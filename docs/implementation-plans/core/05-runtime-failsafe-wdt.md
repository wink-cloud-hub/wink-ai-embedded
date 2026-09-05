# Phase 5: Runtime WDT / Fail-Safe / Actuator Registry 闭环

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.
>
> **核验状态（2026-06-24）：** 已对照 `wink_runtime.h`、`wink_runtime.c` 确认。
>
> **执行序位置（见 00-README）：** `0/1 → 3 → 2 → 4 → 5 → 6`。**前置：Phase 2**（`dal_servo_safe_off` 需 `dal_servo_init` + `initialized` 字段）。

**Goal:** 修复 P0-4：建立软件故障路径、执行器统一关断（Actuator Registry）、reset reason 读取、WDT/Panic 后安全锁定闭环；文档明确**硬件级默认安全态**由板级电路保证，软件只补闭环。

**Architecture:**
- runtime 新增 Actuator Registry；执行器 DAL 在 init 阶段注册 safe-off 回调。
- runtime `fault`/`panic`/`on_fault` 统一调 registry 关断所有执行器。
- PAL 提供 WDT / reset reason 契约；host target 用静态状态模拟。
- 复位期间硬件安全态（引脚 Hi-Z 弱拉、执行器使能脚默认关断、电源门控）**必须由板级硬件保证**——软件无法覆盖 HardFault/总线死锁/CPU 卡死/WDT 硬复位瞬间。

**Tech Stack:** C99, PAL OSAL/HAL, runtime, trace, Unity

## Global Constraints
- 关断路径**无动态分配**（静态注册表）
- fail-safe 回调**不得阻塞**（fault 路径须尽快进入安全态）
- 复位期间硬件安全态由板级电路保证，软件只能补闭环
- 回调型 fault 路径不替代 WDT：真挂死/CPU 卡死靠硬件 WDT 复位兜底

## Sequencing
- 前置：Phase 2（`dal_servo_init`）
- Task 5-1（registry 基础）可最先；5-2（servo safe-off）依赖 5-1 + Phase 2；5-3（runtime fault 路径）依赖 5-1/5-2；5-4（PAL WDT/reset）独立；5-5（boot lock）依赖 5-1/5-4

---

### Task 5-1: Actuator Registry 基础设施

**Files:**
- Create: `wink-micro-os/runtime/include/wink_actuator_registry.h`
- Create: `wink-micro-os/runtime/src/wink_actuator_registry.c`
- Modify: `wink-micro-os/runtime/CMakeLists.txt`
- Create: `wink-micro-os/test/test_actuator_registry.c`
- Modify: `wink-micro-os/test/CMakeLists.txt`

**Interfaces:**
```c
typedef wink_status_t (*wink_actuator_safe_off_fn)(void *ctx);

WINK_WARN_UNUSED_RESULT
wink_status_t wink_actuator_register(wink_actuator_safe_off_fn fn, void *ctx);

void wink_actuator_registry_reset(void);
void wink_actuator_safe_off_all(void);
```
**Rules:** 静态容量 `WINK_ACTUATOR_REGISTRY_CAPACITY=16`；重复 `(fn, ctx)` → `WINK_OK`（幂等）；满 → `WINK_ERR_RESOURCE_EXHAUSTED`；NULL `fn` → `WINK_ERR_INVALID_ARG`；`safe_off_all` 即使单个关断失败也继续遍历全部，失败项 `wink_trace_fault` 记录。

**Tests:**
```c
void test_register_null_returns_invalid_arg(void);
void test_register_duplicate_is_idempotent(void);
void test_safe_off_all_calls_all_registered_actuators(void);
void test_registry_full_returns_resource_exhausted(void);
```

---

### Task 5-2: Servo safe-off hook

**Files:**
- Modify: `wink-micro-os/dal/include/dal_servo.h`
- Modify: `wink-micro-os/dal/src/dal_servo.c`
- Modify: `wink-micro-os/test/test_dal_servo.c`

**Interfaces:**
```c
WINK_WARN_UNUSED_RESULT
wink_status_t dal_servo_safe_off(dal_servo_t *dev);
```
**Rules:** `dev==NULL` → `WINK_ERR_INVALID_ARG`；`!initialized` → `WINK_ERR_NOT_INITIALIZED`；调 `pal_pwm_set_duty(dev->pwm_channel, 0.0f)`；**不 sleep**。

> ⚠️ **safe-off 语义边界（架构师红线）**：duty=0 对**舵机**= 失去保持力（limp）= 安全（无意外运动）。但对**未来 DC 电机 DAL**，duty=0 可能是 coast（滑行）而非 brake（制动）——并非通用安全态。Actuator Registry 的"各执行器自定义 safe-off 回调"模型已为此预留：每个执行器类型须注册**自己语义正确**的关断（电机=制动/断使能，而非简单 duty=0）。本 Task 须在 `dal_servo.h` 与规范中写明"舵机 safe-off = duty 0"的适用范围，不得外推为通用执行器关断范式。

---

### Task 5-3: Runtime fault path uses registry and trace

**Files:**
- Modify: `wink-micro-os/runtime/src/wink_runtime.c`
- Modify: `wink-micro-os/runtime/include/wink_runtime.h`
- Modify: `wink-micro-os/test/test_runtime.c`

**Source-of-truth check:** 当前 `wink_runtime.c` 仅有 `wink_runtime_run`（init → loop → delay），**无任何 fault 路径**；init/loop 回调为 `void`（无法自动捕获 app 错误）——故须暴露显式 fault API。

**Interfaces:**
```c
void wink_runtime_fault(const wink_app_callbacks_t *callbacks, uint32_t fault_code);
```
**Implementation:** `wink_trace_fault(fault_code)` → `wink_actuator_safe_off_all()` → 若 `callbacks && callbacks->on_fault` 非空则调用。

**Runtime error handling:** 当前 `void` 回调设计无法自动捕获 app 错误，故本阶段先暴露**显式** fault API（app/驱动在检测到不可恢复状态时主动调用）。回调返回值迁移到 status 的方案作为 follow-up（不在本阶段）。

---

### Task 5-4: PAL WDT / reset reason contract

**Files:**
- Modify: `wink-micro-os/pal/include/pal_osal.h`
- Modify: `wink-micro-os/targets/host/pal_osal_host.c`
- Modify: `wink-micro-os/targets/wasm/pal_osal_wasm.c`
- Modify: `docs/design/02-wink-micro-os/04-runtime-and-trace.md`
- Modify: `docs/design/07-platform-governance/02-error-fault-model.md`

**Interfaces:**
```c
typedef enum { PAL_RESET_REASON_UNKNOWN=0, PAL_RESET_REASON_POWER_ON=1,
               PAL_RESET_REASON_WATCHDOG=2, PAL_RESET_REASON_PANIC=3 } pal_reset_reason_t;

pal_reset_reason_t pal_get_reset_reason(void);
WINK_WARN_UNUSED_RESULT wink_status_t pal_watchdog_init(uint32_t timeout_ms);
WINK_WARN_UNUSED_RESULT wink_status_t pal_watchdog_feed(void);
```
**Rules:** host 返回可配置 reset reason（供测试）；wasm 返回 `UNKNOWN` + WDT init/feed `WINK_ERR_UNSUPPORTED`（直至浏览器 watchdog 策略确立）；esp32 须映射 `esp_reset_reason()` → `pal_reset_reason_t`（随 P2-6）。

---

### Task 5-5: Boot safe-lock semantics

**Files:**
- Modify: `wink-micro-os/runtime/src/wink_runtime.c`
- Modify: `docs/design/02-wink-micro-os/04-runtime-and-trace.md`

**Rules:**
- 启动调 `pal_get_reset_reason()`
- 若为 `WATCHDOG`/`PANIC`：runtime 记 `wink_trace_fault`，并在 app 显式清除 safety lock 前保持执行器失能——**避免"启动→复位→启动"循环反复驱动执行器**
- 本阶段保守实现：`callbacks->init` 前先 `wink_actuator_safe_off_all()`

> ⚠️ **clear-lock API 是 follow-up**：本阶段实现"锁定"但"清除 safety lock 的 API"未定义 → host 测试中锁定的执行器无法解锁。须登记 follow-up Task 定义 `wink_runtime_clear_safety_lock()`（或等价），否则真机一旦 WDT 复位即永久失能（安全但需现场干预）。文档须写明此为"安全优先"的有意取舍。

### Task 5-6: 预留“分频调度 / 优先级时间轮”架构 (Task Prescaling)

**Files:**
- Modify: `wink-micro-os/runtime/include/wink_runtime.h`
- Modify: `docs/design/02-wink-micro-os/04-runtime-and-trace.md`

**Architecture Extension:**
打破单一 `app_loop` 10ms 强制同步轮询的限制，在 runtime 中规划/预留分频调度（Task Prescaling）或优先级时间轮（Timer Wheel）的扩展接口：
- 允许组件注册不同频率的 tick 回调（例如 1ms 电机 PID 槽、20ms 超声波状态机槽、500ms 日志心跳槽）。
- 本阶段不强制实现复杂的调度器逻辑，但需在 `wink_app_callbacks_t` 或后续设计文档中定义出 multi-rate 回调的模型，并在实现中做好 CPU 负载隔离的铺垫，保障高优先级（如 fail-safe 监控）的实时性。

---

**Verification Gate:**
```powershell
cd wink-micro-os
python wink-tools/wink.py test --clean
```
→ 全绿。新测试确认：fault 路径在 app `on_fault` **之前**调用 safe-off 回调；WDT/PANIC reset reason 下启动时执行器默认失能。

## 出口验收
- [ ] `python wink-tools/wink.py test --clean` 全绿（含 registry 四测试）
- [ ] fault 路径顺序：trace → safe-off-all → on_fault
- [ ] safe-off 语义边界（舵机 duty=0）已写入规范，未外推为通用范式
- [ ] clear-lock follow-up 已登记
- [ ] 整改跟踪表 P0-4 标"软件闭环完成；硬件级默认安全态由板级电路保证（文档化）"
