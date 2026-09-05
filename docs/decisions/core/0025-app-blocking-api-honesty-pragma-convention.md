# ADR-0025：App 层阻塞 API 诚实化约定（Pragma 最小化 + Init/Fault 阻塞合法 + Sim 严格模式）

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已接受）** |
| 日期 | 2026-07-06 |
| 触发 | [2026-07-06 BAL/DCST 架构重构方案](../../zh/tech-designs/core/2026-07-06-bal-dcst-architecture-refactor.md) v5；当前 `samples/devkitc_smoke/app_callbacks.c` file-scope pragma 把所有 deprecated warning 一刀切关掉，掩盖真 bug |
| 影响范围 | `runtime/`（新增 `wink_blocking_region.h`）；所有 BAL `.c`；app 层 `app_callbacks.c`（pragma 重写）；sim target CMake（强制 `WINK_STRICT_NONBLOCKING=1`）；CI 卡口 |
| 决策者 | 项目 Owner |
| 关联 ADR | [ADR-0017 阻塞 API 硬隔离](0017-blocking-api-hard-isolation.md)、[ADR-0023 BAL 分层](0023-bal-business-abstraction-layer.md)、[ADR-0024 Fault 三阶段](0024-fault-three-phase-model-and-dal-deinit-contract.md) |
| 关联设计规范 | （Accepted 后回写：`02-wink-micro-os/01-pal-platform-abstraction.md`、`04-wasm-simulation/*`、`07-platform-governance/coding-conventions.md`） |

---

## 背景（Context）

### ADR-0017 已建立三层硬隔离

ADR-0017（2026-07-02 Accepted）已经建立了阻塞 API 的三层防护：
1. **编译期**：`WINK_BLOCKING` 属性触发 GCC/Clang `-Wdeprecated-declarations` 警告（`-Werror` 下失败）；
2. **链接期**：`-DWINK_STRICT_NONBLOCKING=1` 下符号从头文件消失，阻塞 API 调用点变 undefined reference；
3. **运行期**：`WINK_PT_DEBUG` 下 `WINK_ASSERT_NONBLOCKING()` 检测 PT 上下文误调，升级为 fault。

### ADR-0017 遗留的三个问题

1. **抑制手段粗糙且不跨编译器**：现有的应对手段是在 `app_callbacks.c` 顶部写 file-scope `#pragma GCC diagnostic ignored "-Wdeprecated-declarations"`：
   - 粒度太粗——整个文件里所有 deprecated warning 都被关掉，包括真正的 bug（如业务回调里误调 `pal_os_sleep_ms`）；
   - 不兼容 MSVC——MSVC 上 `WINK_BLOCKING` 映射到 `__declspec(deprecated(msg))`，产生 C4996 警告，GCC pragma 覆盖不到。

2. **"什么位置允许阻塞"没有诚实的语义化标注**：
   - BAL helper 内部的 MAY_BLOCK task 函数调 blocking API（如 `dal_ultrasonic_request_measurement`）是**合法**的；
   - `app_init` 里的一次性 selftest/I2C scan/diagnostic（如 `wink_selftest_run`）是**合法**的（init 是同步启动阶段，不是 PT 协作上下文）；
   - `app_on_fault` 在 fault task 上下文（Phase 2，≤500ms）调阻塞 stop 是**合法**的（ADR-0024）；
   - 但 app 业务回调（`on_button_click`、`app_loop`、event handler）里调阻塞 API 是 **bug**，pragma 遮住会把真 bug 压下去。

3. **sim target 严格模式尚未默认开启**：
   - `WINK_STRICT_NONBLOCKING=1` 在 PAL 头里已实现（阻塞 API 符号消失），但大部分 sample 和 sim 默认构建未开启；
   - 带来"两端不同源"风险：代码在 sim 下因为 Asyncify 等机制"看起来能跑"，真机协作调度器下饿死 WDT（ADR-0002 双 target 同源承诺被违反）；
   - 若现在直接打开，现有 `wink_sim_ultrasonic_echo` 等 bringup 模块会链接失败，需先分级处理。

### 诊断：问题的根源不是 pragma 本身，而是**位置与诚实性**

当前 file-scope pragma 的问题不在"用了 pragma"，而在：
- 把"合法的 blocking"（BAL helper 内部、init 阶段一次性诊断）和"非法的 blocking"（业务回调里误调）一刀切；
- 没有 MSVC 支持；
- 没有语义化命名让 code review 一眼看出"这是合法例外"还是"遮住 bug"。

**本 ADR 的目标不是"应用层 0-pragma"**（教条目标，反而逼出各种绕路），而是**pragma 作用域最小化 + 语义诚实 + 非法位置零容忍**。

---

## 方案比选（Options）

### 方案 A：追求"应用层 0-pragma"（教条目标）

强制 app 层零 pragma，所有 blocking 调用都必须下放到 BAL helper 或新抽象。selftest 这类一次性诊断也必须封装成非阻塞或放入独立 task。

- ✅ 视觉上最干净；
- ❌ **违背诚实性**：init 阶段阻塞是合法的，强行走非阻塞包装反而绕路（selftest 本就在同步启动阶段，做成异步状态机是过度设计）；
- ❌ 强制把 selftest 等 bringup 工具移入独立 task 增加调度复杂度，且这些工具本就是一次性诊断，不值得。

### 方案 B：统一 file-scope pragma（当前状态的延续）

所有需要调 blocking API 的文件顶部都加 file-scope pragma。

- ✅ 最简单；
- ❌ **遮住真 bug**：业务回调里误调 `pal_os_sleep_ms` 也被压住，协作调度链崩塌（LIGHT 路径阻塞导致整个系统停摆）时最难 debug；
- ❌ MSVC 下 C4996 漏出（无对应 pragma）；
- ❌ code review 无法区分"这是合法例外"还是"遮住了 bug"。

### 方案 C：语义化阻塞区域宏 + 位置白名单 + sim 严格模式（**推荐**）

提供两个语义明确、跨编译器的宏，收敛 pragma 到**最小必要作用域**：
- `WINK_INTERNAL_BLOCKING_REGION_BEGIN/END`：BAL/Runtime 内部文件级使用（整个 TU 合法 blocking）；
- `WINK_INIT_BLOCKING_REGION_BEGIN/END`：应用层 `app_init`/`app_on_fault` 里小块一次性阻塞诊断使用（push/pop 最小范围）；
- **业务回调 / `app_loop` / event handler 里禁止任何 pragma**（deprecated warning 在此出现即 bug，CI 卡口）；
- sim target 强制 `WINK_STRICT_NONBLOCKING=1`（先普查违规清单，再分级处理：LIGHT 回调内的阻塞是 bug、selftest 放入 MAY_BLOCK task、bringup 模块条件编译隔离）。

- ✅ pragma 位置诚实：`WINK_INIT_BLOCKING_REGION` 自带注释语义，code review 一眼看出"init 一次性诊断合法"；
- ✅ 跨编译器（GCC/Clang/MSVC）三端统一，MSVC C4996 被正确抑制；
- ✅ 业务回调零 pragma，真 bug 不被遮住；
- ✅ sim strict mode 默认开启，fail-fast 抓阻塞 bug，兑现 ADR-0017 链接期硬隔离承诺；
- ⚠️ 需要一次普查（约 0.5 天）+ 分级处理 bringup 模块。

---

## 决策结论（Decision）

**采纳方案 C**。核心设计点：

### 1. 阻塞区域宏（跨编译器、语义化）

新建 **`runtime/include/wink_blocking_region.h`**（保持 `wink_status.h` 洁癖），提供两对宏：

```c
/* ── (A) BAL/Runtime 内部：文件级抑制（BAL .c 顶部用） ────── */
#if defined(__GNUC__) || defined(__clang__)
#  define WINK_INTERNAL_BLOCKING_REGION_BEGIN \
    _Pragma("GCC diagnostic push") \
    _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#  define WINK_INTERNAL_BLOCKING_REGION_END  _Pragma("GCC diagnostic pop")
#elif defined(_MSC_VER)
#  define WINK_INTERNAL_BLOCKING_REGION_BEGIN  __pragma(warning(push)) __pragma(warning(disable:4996))
#  define WINK_INTERNAL_BLOCKING_REGION_END    __pragma(warning(pop))
#else
#  define WINK_INTERNAL_BLOCKING_REGION_BEGIN
#  define WINK_INTERNAL_BLOCKING_REGION_END
#endif

/* ── (B) 应用层：app_init / app_on_fault 里的 init-phase exception 小块 ── */
#define WINK_INIT_BLOCKING_REGION_BEGIN  WINK_INTERNAL_BLOCKING_REGION_BEGIN
#define WINK_INIT_BLOCKING_REGION_END    WINK_INTERNAL_BLOCKING_REGION_END
```

**设计说明**：
- 两对宏底层展开相同（都是 push/disable/pop），但**名字不同是刻意的**——code review 和 grep 时能立刻区分"BAL 内部实现" vs "app init 一次性诊断"；
- 宏在 `#include` **之后**展开，防止抑制泄漏到 PAL/DAL 头内部；
- 三编译器兼容：GCC/Clang 用 `_Pragma`，MSVC 用 `__pragma(warning(push/disable:4996/pop))`，其他编译器为空。

### 2. 三位置合法/非法矩阵（pragma 规则）

| 位置 | blocking 合法？ | pragma 用法 |
|---|---|---|
| **BAL `.c` 整个 TU**（如 `wink_sonar_helper.c` MAY_BLOCK task 调 `dal_ultrasonic_request_measurement`） | ✅ 合法 | 文件级 `WINK_INTERNAL_BLOCKING_REGION_BEGIN/END` 包裹**include 之后**的实现段，**必配注释**：`/* ADR-0017 BAL-exception: helper 内部通过 wink_periodic MAY_BLOCK 路径调用 WINK_BLOCKING API */` |
| **BAL `.c` LIGHT 路径**（如 `wink_button_helper.c`、`wink_led_blink_helper.c`） | ❌ 非法 | **不加** blocking region；若 `dal_button_poll` 等确无阻塞（已走查三 target），不加 pragma；若未来有 blocking 调用必须重构或改 MAY_BLOCK |
| **Runtime `.c` 内部**（如 `wink_selftest.c`、`wink_runtime.c` 里合法的 blocking 段） | ✅ 合法 | 同 BAL 内部，用 `WINK_INTERNAL_BLOCKING_REGION_BEGIN/END` |
| **`app_init_status()` / `app_on_fault_status()` 里的一次性诊断块**（如 `wink_selftest_run`、I2C scan） | ✅ 合法（init 是同步启动阶段，不在 PT 协作上下文） | **push/pop 小块**用 `WINK_INIT_BLOCKING_REGION_BEGIN/END` 包裹**仅**阻塞调用语句块，**必配注释**：`/* ADR-0017 init-phase exception: selftest 在同步启动阶段运行 */` |
| **App 业务回调**（`on_xxx_click`、event handler、sensor on_data 回调——当 on_data ADR 落地后） | ❌ 非法（bug） | **禁止任何 pragma**；出现 deprecated warning 必须修代码（改走 BAL/wink_periodic） |
| **`app_loop()`** | ❌ 非法（PT 协作上下文） | **禁止任何 pragma**；阻塞操作必须放 BAL helper |
| **Bringup/selftest 仪器**（`wink_sim_ultrasonic_echo`、GPIO 短路测试等） | ✅ 合法但仅限 bringup 场景 | 迁移到 `runtime/selftest/`，文件级 `WINK_INTERNAL_BLOCKING_REGION_BEGIN/END`，加 `#ifndef WINK_STRICT_NONBLOCKING` 条件编译隔离（sim strict 下不编译主体） |

### 3. App 层 `app_init_status()` 标准模板

应用层 `app_callbacks.c` 遵循：

```c
#define LOG_TAG "smoke_app"

#include "device_tree.h"
#include "wink_app.h"
#include "wink_fault.h"
#include "wink_led_blink_helper.h"
#include "wink_button_helper.h"
#include "wink_sonar_helper.h"
#include "wink_telemetry_helper.h"
#include "wink_blocking_region.h"   /* 只在 init 用 selftest 时才 include */
#include "wink_selftest.h"
#include "pal_log.h"                /* 唯一直接 include 的 PAL 头（LOG 宏不引入 OSAL/HAL 类型） */
/* 注意：零 pal_osal.h / pal_hal.h include */

static void on_boot_button(dal_button_event_t evt, void *ctx) {
    (void)ctx;
    /* 业务回调里严禁阻塞，不允许任何 pragma */
    if (evt == DAL_BUTTON_EVT_LONG_PRESS) {
        LOG_I("Long press detected!");
    }
}

static wink_status_t app_init_status(void) {
    WINK_TRY(wink_device_tree_init());
    WINK_TRY(dal_button_on_event(&boot_button, on_boot_button, NULL));

    /* BAL 启动（非阻塞，无 pragma） */
    WINK_TRY(wink_led_blink_start(&board_led, 1000));
    WINK_TRY(wink_button_helper_start(&boot_button, BOOT_BUTTON_AUTO_POLL_MS));
    WINK_TRY(wink_sonar_helper_start(&smoke_sonar, 500));
    WINK_TRY(wink_telemetry_default_start(&smoke_sonar, &boot_button));

    /* ADR-0017 init-phase exception: selftest 在同步启动阶段运行，
     * 不在 cooperative PT 上下文，允许阻塞调用（内部含 I2C scan / RMT wait）。
     * 用 WINK_INIT_BLOCKING_REGION 宏包裹最小必要语句块。 */
    WINK_INIT_BLOCKING_REGION_BEGIN
    wink_selftest_result_t results[8]; size_t n = 0;
    WINK_IGNORE_RESULT(wink_selftest_run("*", results, 8, &n));
    WINK_INIT_BLOCKING_REGION_END

    LOG_I("App init done.");
    return WINK_OK;
}

static void app_loop(void) {
    /* app_loop 跑在 PT 上下文，严禁阻塞 */
}

static wink_status_t app_on_fault_status(uint32_t code) {
    (void)code;
    /* 初学者场景：直接返回 LOCKED 让 WDT 兜底，无需处理 */
    return WINK_ERR_LOCKED;
}

const wink_app_callbacks_t *wink_app_get_callbacks(void) {
    static const wink_app_callbacks_t cb = {
        .init_status     = app_init_status,
        .loop            = app_loop,
        .on_fault_status = app_on_fault_status,
        .on_boot         = app_on_boot,
    };
    return &cb;
}
```

**关键变化**（vs 当前 devkitc_smoke）：
- **零 file-scope pragma**：整文件顶部不再有 `#pragma GCC diagnostic ignored "-Wdeprecated-declarations"`；
- **零 `pal_osal.h` include**：通过 BAL `wink_bal_core_t` / `WINK_HELPER_OPTS` 隔离核亲和类型；
- **零手写 task wrapper**：`sonar_poll_task` 这类手写壳子被 `wink_sonar_helper_start` 取代；
- **零 `extern wink_app_services_start`**：codegen 不再自动启动服务，button auto_poll 由 C 代码显式 `wink_button_helper_start(&boot_button, BOOT_BUTTON_AUTO_POLL_MS)`；
- **selftest 块有明确语义标注**：`WINK_INIT_BLOCKING_REGION` + ADR 注释，code review 一眼看出"这是合法例外"。

### 4. Sim Target 严格模式强制开启

sim（wasm/fiber）target CMake **默认**配置 `-DWINK_STRICT_NONBLOCKING=1`，兑现 ADR-0017 链接期硬隔离承诺：
- **BAL LIGHT 回调内不得调 blocking**——这本来就不该有，有则是 bug，必须修；
- **selftest 必须在独立 MAY_BLOCK task 里跑**，不在 LIGHT 上下文；
- **bringup 仪器**（如 `wink_sim_ultrasonic_echo`）：迁移到 `runtime/selftest/`，用 `#ifndef WINK_STRICT_NONBLOCKING` 条件隔离（硬件 ISR/task 创建在 sim 下本身返回 UNSUPPORTED，这些模块主体在 sim strict 下为空 no-op）；
- **Sim PAL 桩里阻塞 API**：被 `#ifndef WINK_STRICT_NONBLOCKING` 包围的声明消失，直接链接失败（fail-fast），而不是靠 Asyncify 让它"看起来能跑"。
- **防范未使用变量警告**：在开启 `-DWINK_STRICT_NONBLOCKING=1` 编译时，部分只在阻塞路径下使用的配置变量或中间结果可能会触发编译器的 `unused-variable` 警告。为了在 `-Werror` 下通过编译，编写代码时需通过 `(void)var;` 或条件编译宏来屏蔽这些无用变量警告。

**开关顺序硬约束**：Stage 5 开启 strict mode 之前必须先完成违规普查（0.5 天）并分级处理所有 call site，避免大面积 breakage。

### 5. 运行期 LIGHT 上下文断言（必做，非可选）

作为编译期/链接期防线的兜底，运行期也必须能抓漏网之鱼：

1. 在 `wink_periodic` LIGHT 分发入口/出口维护一个"在 LIGHT 上下文"标志（sim/host 下用 thread-local 或全局）；
2. 与 `wink_pt_debug.h` 里已有的 `WINK_ASSERT_NONBLOCKING()` 宏打通——当 LIGHT 回调内调用了任何 `WINK_BLOCKING` API（包括新 DAL/PAL API 忘记挂 `WINK_BLOCKING` 标记的漏网违规）时：
   - 开发构建/WINK_PT_DEBUG 下 hard fault（调用 `wink_trace_fault(WINK_ERR_PANIC)` + assert），打印 helper 名/回调地址定位具体违规点；
   - Release 构建保留 LOG_E 警告；
3. **测量抖动余量**：在抢占式 RTOS（ESP32）上 `pal_os_get_us()` 包含 ISR 抢占时间，WCET 硬阈值需预留 2-5× 安全余量（建议 LIGHT 100µs 预算配 200-500µs hard limit），在升级为 fault 前打印警告日志及堆栈，以便排查是否受中断抢占影响，避免误伤。

### 6. CI / Code Review 卡口

- **简单 grep 卡口**：`app_callbacks.c`（以及 samples 里所有 app 层文件）如果出现 `-Wdeprecated-declarations` pragma 或 `WINK_INTERNAL_BLOCKING_REGION`（注意不是 `WINK_INIT_BLOCKING_REGION`），CI 拒绝合入；
- 允许出现 `WINK_INIT_BLOCKING_REGION` 但必须配套 `ADR-0017 init-phase exception` 注释（review 卡口，CI 可加简单正则）；
- `bal/include/**/*.h` 里 grep `#include.*pal_` 拒绝合入（分层红线，见 ADR-0023）；
- sim target CI build 默认带 `-DWINK_STRICT_NONBLOCKING=1`，链接失败即 CI 失败。

### 7. 与 Self-set_period 重入的协作

本 ADR 与 ADR-0023 §11 self-set_period 重入语义配合：
- 从 BAL LIGHT/MAY_BLOCK 回调内部对**自身**句柄调 `wink_periodic_change_period` 是合法的（LIGHT 侧原子写、MAY_BLOCK 侧循环顶部读）；
- 这不涉及任何 blocking API（`change_period` 是非阻塞的原子操作），不触发 `WINK_BLOCKING` 警告，也不违反本 ADR。

---

## 后果与约束（Consequences & Constraints）

### 正面后果

1. **Bug 不被遮住**：业务回调/app_loop 零 pragma，误调 blocking API 在编译期即报警（`-Werror` 下失败），不会混过 code review。
2. **诚实的代码**：`WINK_INIT_BLOCKING_REGION` 宏自带语义和 ADR 注释，读者一眼看出"这是 init 一次性合法阻塞"，而非"作者粗暴地关掉了警告"。
3. **跨编译器一致**：MSVC C4996 被正确处理，不再有"GCC 下干净、MSVC 下 C4996 刷屏"的不一致。
4. **Sim fail-fast**：sim 强制严格非阻塞模式，阻塞 bug 在仿真阶段就被链接失败或运行期 assert 抓住，不再是"sim 看起来对、真机 WDT 饿死"的两端不同源事故。
5. **BAL helper 内部收敛**：所有 blocking pragma 集中在 BAL `.c` 文件顶部，应用层 include BAL 头看不到任何 deprecated 声明警告（BAL 头本身不 include 被 WINK_BLOCKING 标记的 PAL/DAL API 声明，除了非 blocking 的 `pal_log.h`）。
6. **对初学者友好**：初学者模板里零 pragma（selftest 块是可选高级功能），不需要理解 RTOS 阻塞概念即可写可运行固件。

### 约束 / 代价

1. **Stage 5 需普查**：开启 sim strict mode 前需 0.5 天普查全仓库 `-Wdeprecated-declarations` pragma 和 WINK_BLOCKING API call site，分级处理（BAL/selftest/bringup 各有处理方式）。
2. **Bringup 模块迁移**：`wink_sim_ultrasonic_echo` 等 bringup 仪器需从 `samples/common/` 迁移到 `runtime/selftest/`，加 `#ifndef WINK_STRICT_NONBLOCKING` 条件隔离。
3. **旧 sample 迁移成本**：avoidance_car / dual_task_demo / oled_dashboard 等现有 sample 在 Stage 4-5 分批迁移到新模式（旧 API 过渡期保留，转发头一个 release 周期）。
4. **BAL LIGHT 回调行数约束**：所有新 BAL LIGHT 回调 PR 里 code review 必须检查体长度 ≤20 行，过长需拆分或改 MAY_BLOCK。

### 对默认参数的影响

BAL helper 默认 LIGHT/MAY_BLOCK 分类（见 ADR-0023 §4 表格）直接决定 pragma 是否加：
- `wink_led_blink_helper` / `wink_button_helper`：LIGHT，**不加** `WINK_INTERNAL_BLOCKING_REGION`（已走查三 target `pal_gpio_read/dal_button_poll` 仅做寄存器级读 <1µs）；
- `wink_sonar_helper` / `wink_servo_helper` / `wink_telemetry_helper` / `wink_oled_helper`：MAY_BLOCK，**加**文件级 `WINK_INTERNAL_BLOCKING_REGION_BEGIN/END`。

---

## 遵循与后续（Compliance & Follow-up）

### 立即执行（Accepted 后）

1. **回写设计规范**：
   - `02-wink-micro-os/01-pal-platform-abstraction.md`：加 WINK_BLOCKING pragma 诚实化约定、跨编译器宏说明；
   - `04-wasm-simulation/`：加 sim 默认开启 `WINK_STRICT_NONBLOCKING=1` 的设计决策；
   - `07-platform-governance/coding-conventions.md`：加 pragma 使用规则矩阵（§2）、CI 卡口说明。
2. **Stage 1 落地阻塞区域宏**：新建 `runtime/include/wink_blocking_region.h`（本 ADR §1 代码原样落地）。

### 实施期必做

- BAL `.c` 迁移时严格按 §2 矩阵决定加不加 blocking region，每个文件顶部的宏必配 ADR 注释；
- Stage 5 普查前不得打开 sim strict mode（避免 breakage），普查后再打开；
- 运行期 LIGHT 上下文断言在 Stage 1 #6 必做，不能推到 Stage 5（否则 LIGHT 里的阻塞 bug 直到 sim strict mode 才能发现，漏掉真机路径）。

### 不做（Out of Scope）

- 追求"应用层 0-pragma"教条（方案 A 已否决）——init 阶段 selftest 等一次性诊断用 `WINK_INIT_BLOCKING_REGION` 小块包裹是合法的、诚实的；
- App 层业务回调允许 blocking（已明确禁止）；
- 自动 pragma 插入工具（YAGNI，code review 卡口足够）。

---

*本 ADR 状态变更请在此记录：*
- 2026-07-06：Proposed（基于 tech-design v5 Owner 决策版 + 当前 devkitc_smoke file-scope pragma 实查起草）
- 2026-07-06：Accepted（Owner 审阅并采纳，并融入了防范严格非阻塞模式下未使用变量警告的优化建议）

