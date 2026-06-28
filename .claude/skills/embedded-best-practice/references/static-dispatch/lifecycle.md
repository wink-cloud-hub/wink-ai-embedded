# 资源所有权与生命周期模型 (Lifecycle Model)

静态分发（方案 B）通过去除动态内存管理，实现了极高确定性的生命周期。然而，如果不理清静态全局实例的“读写权限”与“所有权”，依然会引入并发冲突、配置篡改等致命 bug。

项目规定以下五个维度的生命周期和资源控制准则：

---

## 1. 零动态分配原则 (No-malloc Policy)

*   **核心原则**：本项目在固件运行期（初始化及运行）**严禁使用任何形式的动态堆内存分配**（如 `malloc`, `free`, `realloc` 等）。
*   **代偿方案**：所有 DAL 外设句柄实例（如 `front_radar`）和 PAL 缓冲区必须在编译期静态展开，并存放于 `.data` 或 `.bss` 段。
*   **AI 检查点**：AI 生成的任何驱动实现均不可包含内存释放逻辑（即不设计 `dal_xxx_destroy` 或 `free` 的 API），只能包含 `init` 与 `deinit`。

---

## 2. 配置与状态显式分离 (Const Config & Mutable State)

为规避 AI 误操作或业务层代码意外篡改外设引脚配置，外设 POD 结构体在设计时必须显式区分为“只读配置区”和“可变状态区”，并在 Codegen 实例化时将配置声明为 `const`。

### 推荐的设计模式：

> ⚠ **目标形态（尚未落地）**：当前实际头文件（`dal_servo.h` / `dal_ultrasonic.h`）是**扁平字段、非 `const`**（见 README 偏差框）。下面是 ADR 目标形态；迁移示例见 evolution.md §1.4。写新代码按此目标形态，读写现有实例仍用扁平字段（`dev->last_distance`）。

```c
/* dal_servo.h 中的定义（目标形态；现状为扁平字段） */
typedef struct {
    // --- 1. 静态只读配置区（由 Codegen 填入） ---
    const uint8_t  pwm_channel;   /* const 锁定物理通道，防篡改 */
    const float    min_pulse_ms;
    const float    max_pulse_ms;

    // --- 2. 运行期可变状态区（由 DAL 驱动修改，外部只读） ---
    struct {
        float         current_angle;  /* 缓存的当前角度 */
        wink_status_t last_status;    /* 器件上一次运行状态 */
    } state;
} dal_servo_t;
```

```c
/* device_tree.c 中的 Codegen 初始化定义 */
#include "device_tree.h"

// 编译期绑定引脚配置，将 const 字段完全锁定
dal_servo_t neck_servo = {
    .pwm_channel  = 0,
    .min_pulse_ms = 0.5f,
    .max_pulse_ms = 2.5f,
    .state = {
        .current_angle = 90.0f,
        .last_status   = WINK_OK
    }
};
```

---

## 3. 初始化幂等性 (Idempotency of Init)

在静态设备树初始化中，各外设可能由于复杂的依赖关系被多次触发 `init`。因此，所有的 `dal_xxx_init` 函数必须是**幂等（Idempotent）**的：
1. **防止二次破坏**：多次调用同一实例的 `init`，若状态已就绪，应直接返回 `WINK_OK`，而不重新重置物理引脚状态。
2. **状态锁保护**：可以在 `state` 中加入 `bool initialized` 标记。如果为 `true`，直接快速返回。
3. **错误重试**：如果 `init` 失败，实例状态应当允许被多次重试，而不锁死或泄露物理资源。

---

## 4. 指针逃逸与局部变量缓存限制 (Pointer Escape Rules)

由于设备实例是全局静态变量，其指针（例如 `&front_radar`）是永久有效的（所有权归属系统设备树所有）。
*   **指针共享**：可以安全地将 `front_radar` 指针传递给任何消费任务，无需担心“悬空指针 (Dangling Pointer)”。
*   **逃逸禁令**：**严禁将任何临时局部变量（Stack 变量）的指针缓存到全局实例中**。一旦函数调用结束，栈上的临时变量被销毁，全局实例中指向该栈地址的指针将变为野指针。

```c
/* ❌ 严重错误示范：缓存栈上临时数据指针（dev->state.* 为目标形态示意，现状扁平） */
wink_status_t dal_temp_cache_data(dal_temp_t *dev) {
    float temp_data = 25.4f;
    dev->state.last_reading_ptr = &temp_data; // ❌ 逃逸！temp_data 在出栈后失效
    return WINK_OK;
}
```

---

## 5. 回调上下文与 user_data 生命周期

在静态分发下，异步消息和硬件中断一般通过函数指针回调。为了保持面向对象的隔离：
1. **回调签名规范**：所有的回调函数必须遵循 `void (*cb)(void *user_data)` 的签名格式。
2. **user_data 所有权**：`user_data` 指针通常由 Codegen 在静态绑定时填入对应外设实例的指针。禁止传递任何栈上临时对象的地址作为 `user_data`。
3. **中断限制**：若回调是在 ISR（中断服务程序）上下文中执行，则该回调内绝对禁止再调用任何带阻塞的 API，必须保证回调生命周期在微秒级结束。

---

## 6. 器件健康状态机（Health Model）

静态分发下，App 需要统一查询「这个器件现在 OK / 降级 / 故障」，以决定走正常逻辑还是保守降级——否则只能为每个器件写特判，违背表驱动 / DRY。每个器件的可变状态区（§2 的 `state`）统一含一个 `health` 字段：

```c
typedef enum {
    DAL_HEALTH_OK       = 0,   /* 初始化成功、运行正常 */
    DAL_HEALTH_DEGRADED = 1,   /* 部分功能受限（如 NVS 损坏用默认值、单通道失效） */
    DAL_HEALTH_FAULTED  = 2,   /* init 失败或运行期故障，App 必须隔离 / 停用该器件 */
} dal_health_t;

/* 每个器件 state 区统一含： */
dal_health_t health;
```

**与错误码的关系**（ADR-0005）：`init` / 读返回 `WINK_ERR_CONFIG_CORRUPT_DEGRADED(-50)` → 置 `health = DAL_HEALTH_DEGRADED`；返回 `WINK_ERR_FAILED_INIT(-51)` 或致命码 → 置 `health = DAL_HEALTH_FAULTED`。返回码承载**瞬时信号**，`health` 字段承载**持续状态**——两者互补。

**App 统一巡检**（复用 templates.md 形态 5 的 X-macro 批量遍历，无需为每个器件写特判）：

```c
#define X_CHECK_HEALTH(name) \
    do { \
        if ((name).state.health >= DAL_HEALTH_FAULTED) { app_quarantine(&(name)); } \
        else if ((name).state.health == DAL_HEALTH_DEGRADED) { app_conservative(&(name)); } \
    } while (0)
ULTRASONIC_DEVICES(X_CHECK_HEALTH)   /* 遍历所有超声波，跳过故障、对降级走保守逻辑 */
#undef X_CHECK_HEALTH
```

> 现状（扁平结构体）下 `health` 是顶层字段 `dev->health`；目标 config/state 分离后归入 `dev->state.health`（见 §2）。
