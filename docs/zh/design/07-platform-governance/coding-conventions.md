# Coding Conventions & Pragma Rule Matrix

> **Source of Truth**: Platform conventions.  
> Blocking pragma matrix: [ADR-0025](../../decisions/core/0025-app-blocking-api-honesty-pragma-convention.md).  
> Role/BAL operation naming (A/B/C): [ADR-0032](../../decisions/core/0032-bal-role-operation-naming-classes.md) → **§3 below**.  
> BAL 域 / 文件命名 / 依赖 / CI：[06-bal-layer.md](../02-wink-micro-os/06-bal-layer.md)（SSOT）← [ADR-0038](../../decisions/core/0038-bal-naming-hard-cut-and-layer-ssot.md) → **§4 below**.

---

## 1. Pragma Rule Matrix for Blocking APIs

To ensure honesty in blocking API usage and prevent silencing real bugs, compiler warning suppressions (pragmas) must strictly follow the matrix below.

| Location | Blocking Allowed? | Pragma Usage | Required Comments & Usage |
|:---|:---:|:---|:---|
| **BAL `.c` Entire TU**<br>(e.g., `wink_ultrasonic_poll.c` calling `dal_ultrasonic_request_measurement`) | ✅ Yes (Legitimate) | File-level `WINK_INTERNAL_BLOCKING_REGION_BEGIN/END` | Wrap implementation section **after** includes.<br>Must document with:<br>`/* ADR-0017 BAL-exception: periodic MAY_BLOCK path calls WINK_BLOCKING API */` |
| **BAL `.c` LIGHT Path**<br>(e.g., `wink_button_events.c`, `wink_led_blink.c`) | ❌ No | **No pragma allowed** | If there are blocking calls, they must be refactored or moved to `MAY_BLOCK`. |
| **Runtime `.c` Internal**<br>(e.g., `wink_selftest.c`, `wink_runtime.c`) | ✅ Yes | `WINK_INTERNAL_BLOCKING_REGION_BEGIN/END` | For internal runtime blocking paths. |
| **`app_init_status()` / `app_on_fault_status()`**<br>(e.g., `wink_selftest_run`, I2C scan) | ✅ Yes | Block-level `WINK_INIT_BLOCKING_REGION_BEGIN/END` | Wrap only the specific blocking diagnostic lines.<br>Must document with:<br>`/* ADR-0017 init-phase exception: selftest 在同步启动阶段运行 */` |
| **App Event Callbacks**<br>(e.g., `on_button_click`, event handlers) | ❌ No | **Prohibited** | Blocking here is a bug. Do not suppress warnings. |
| **`app_loop()`** | ❌ No | **Prohibited** | Must use BAL helper for asynchronous execution. |
| **Bringup/Selftest Instrumentation**<br>(e.g., `wink_sim_ultrasonic_echo`) | ✅ Yes | `WINK_INTERNAL_BLOCKING_REGION_BEGIN/END` | Must be isolated inside `runtime/selftest/` and wrapped with `#ifndef WINK_STRICT_NONBLOCKING`. |

---

## 2. Platform CI Gates & Lint Checks

To prevent architectural degradation, CI gates enforce the following boundaries:
1. **BAL Layer Boundary (分层红线)**:
   - Grep constraint: `bal/include/**/*.h` must **never** contain `#include "pal_*.h"` or `#include <pal_*.h>` (except `pal_log.h` because log macros do not leak OSAL/HAL types).
   - Additional BAL gates (math 禁 DAL、禁 `*_helper.h`/`*_controller.h`、禁公开 `sonar`、PUBLIC include 仅 `bal/include`、`src/` 镜像)：见 [06-bal-layer.md §6](../02-wink-micro-os/06-bal-layer.md)（硬切割合入后启用）。
2. **App Layer Pragma Restriction**:
   - Grep constraint: `samples/*/app_callbacks.c` (and `wink-micro-app/*/app_callbacks.c`) must **never** contain raw `#pragma GCC diagnostic ignored "-Wdeprecated-declarations"`. Block-level suppressions must only use the semantic macros `WINK_INIT_BLOCKING_REGION_BEGIN/END`.
3. **Simulation Strict Mode**:
   - The WASM simulation target must compile with `-DWINK_STRICT_NONBLOCKING=1` by default. Under this mode, any blocking APIs are removed from headers to cause compile-time failures on invalid blocking calls.

---

## 3. App Role / BAL 操作三类命名（A · B · C）

> **权威活规范（SSOT）**。决策：[ADR-0032](../../decisions/core/0032-bal-role-operation-naming-classes.md)。  
> App Role 动词清单：[01-app-business-logic.md](../03-app-codegen/01-app-business-logic.md) §2.7。  
> 按键事件通路：[ADR-0031](../../decisions/core/0031-button-event-drive-config.md)。事件队列契约：[ADR-0022](../../decisions/core/0022-event-queue-mbox-async-primitives.md)。

### 3.0 口诀

```text
主交付 = 打开后往事件队列投、业务主要在 app_on_event 消化
    → B：enable_* / disable_*
否则
    → 有周期/会话后台 → A：start / stop
    → 一次读写下完     → C：set / get / request / is / …
```

分类对象是**操作**，不是「一种外设只能进一类」。同一外设可同时拥有 **A + B + C**  
（例：舵机 `set_angle` + `sweep_start`；按钮 `is_active` + `enable_events`）。

### 3.1 三类定义

| 类 | 名称 | 动词 | 主交付心智 | 与 `wink_event` 队列 |
|:--|:-----|:-----|:-----------|:---------------------|
| **A** | 活动 | `start` / `stop`（可 `_start_ex`） | 「跑起来一个后台活动」；`stop` 后不再跑 | **通常不经过**队列 |
| **B** | 能力 | `enable_*` / `disable_*` | 「打开某种产出能力」；消费者在别处 | **主路径进入**队列 → `app_on_event` |
| **C** | 动作 | `set_*` / `get_*` / `request_*` / `is_*` / `read_*` / `clear` / `draw_*`… | 「此刻做一次」；无会话 | 一般无关开/关通路 |

**为何队列场景用 B 而非 A：**  
App 是队列消费者时，`enable_events` =「开始向我投递」。`start` 更像再起一个与队列平级的服务，易与 blink / ultrasonic poll 等 **A 类活动**混淆。

**为何默认不用 `register_*`：**  
`register` 暗示挂私有回调 `register(cb, ctx)`。本平台默认是**全局队列 + 统一 `on_event`**，打开生产者用 `enable_*`。仅当 API 真的登记 handler 时才用 `register_*`。

**词序：** `enable_events`，禁用倒装 `events_enable`。可细化宾语：`enable_distance_events`、`enable_fault_events`（仍属 B）。

**3.1.1 谓词约定（P 类）**：返回 bool 的查询函数统一 `is_*` / `has_*` / `can_*` 前缀（例：`wink_button_events_is_debouncing`、`wink_button_has_irq_backend`）。祖父名 `*_supported`、`*_ready` 保留不迁移（新代码不再仿写）。谓词不属于 A/B/C 中任何一类——它不改状态，只回答一次"是否/能否"。

### 3.2 跨层同构（Role ↔ BAL）

同一操作必须**同动词**（允许不同前缀 / 是否传指针）：

```c
/* Role — App / AI 主表面 */
user_button_enable_events();

/* BAL — codegen static inline 转发；禁止改成 *_start 表达同一路径 */
wink_button_enable_events(&user_button, &cfg);
wink_button_disable_events(&user_button);
```

| 层 | 形式 | 调用方 |
|:---|:-----|:-------|
| Role | `{instance}_{verb}()` | App |
| BAL | `wink_<type>_{verb}(dev, …)` | Role 包装 / L2 |
| 过渡兼容 | `wink_button_events_start/stop` | `WINK_DEPRECATED` 薄包装 → 两个 minor 后删除 |

过渡兼容名：`wink_button_events_start` → **deprecated** 薄包装（`wink_button_helper_*` 已于 pre-1.0 直接删除）。  
新代码与 codegen **只**生成 / 文档化 `enable` / `disable`。

### 3.3 示例矩阵（Role 动词 · BAL 符号）

| 外设 | A（活动） | B（能力 → 队列） | C（动作） |
|:-----|:----------|:-----------------|:----------|
| LED | `blink_start` ↔ `wink_led_blink_start` | —（LED 为 indicator，非事件源） | `activate` ↔ `dal_led_on` |
| Ultrasonic | `proximity_start` ↔ `wink_ultrasonic_poll_start` | **`enable_distance_events` ↔ `wink_ultrasonic_enable_distance_events`**（ADR-0033） | `request_measurement` / `read_distance` |
| Servo | `sweep_start` ↔ `wink_rc_servo_sweep_start` | （可选）`enable_fault_events`（future） | `set_angle` ↔ `wink_rc_servo_set_angle` |
| Button | —（一般不需 A） | **`enable_events` ↔ `wink_button_enable_events`** | `is_active` / `was_active` |
| Telemetry | — ↔ `wink_telemetry_default_start` | `enable_fault_events`（future） | — |

### 3.4 选型决策树

```text
Q1: 主交付是「打开后往 wink_event 投、业务主要在 app_on_event」？
    │
    ├─ YES → B: enable_<capability> / disable_<capability>
    │         （边沿用 enable_events；测距完成用 enable_distance_events …）
    │
    └─ NO  → Q2: stop/关掉之后，是否还有后台自动在做事？
              │
              ├─ YES（周期闪、周期测、后台上报） → A: start / stop
              │
              └─ NO  → C: set / get / request / is / read / clear / draw …
```

| 情况 | 命名 |
|:-----|:-----|
| A 类活动**顺带**偶尔 post 一条事件 | 主 API 仍用 **A `start`**；主路径改成「听完成事件」时再**另提供** B `enable_*_events` |
| 同一路径两套名字并存 | 禁止；旧名必须 deprecated → 删除 |
| 为和 blink 整齐，把 button 事件叫 `*_start` | 禁止（Q1） |
| 为和 Role 整齐，把 blink 改成 `enable_blink` | 禁止（Q2 / A） |

### 3.5 硬性规则

1. 新增 Role/BAL API 必须标明 **A / B / C**。  
2. Role ↔ BAL **同操作同动词**；codegen 薄包装不得换一类动词。  
3. 进队列主路径 → **B**；周期/会话活动 → **A**；一次做完 → **C**。  
4. ADR-0038 硬切割后，BAL 公开名以 [06-bal-layer.md](../02-wink-micro-os/06-bal-layer.md) 为准；A/B/C **动词**不变。  
5. 默认不用 `register_*`（除非签名含回调登记）。  
6. 禁用词序 `events_enable`；用 `enable_events` / `enable_<noun>_events`。  
7. 谓词/查询用 `is_*` / `has_*` / `can_*` 前缀（祖父保留 `*_supported` / `*_ready`）。  
8. 禁用词序增加 `events_disable`（正名 `disable_events`）。  
9. **视角陷阱**：A/B/C 看 App 主交付，不看实现——B 类底下可以有 timer/daemon/ISR；A 类可以顺带 post 事件。Q1/Q2 决策树按 App 视角回答。

### 3.6 层边界

- **DAL**（`dal_*`）：仍按 [ADR-0004](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)；本节目只约束 **App Role + BAL**。  
- **PAL**（如 `pal_gpio_enable_interrupt`）：平台能力可用 `enable`；不要求与 BAL A 类对齐。  
- **Runtime 队列**：谁 post、谁 `on_event`——见 ADR-0022；按键后端配置——见 ADR-0031；本节目只约束**开/关生产者**时的动词。

---

## 4. BAL 域划分与文件命名（指针）

> **正文 SSOT**：[06-bal-layer.md](../02-wink-micro-os/06-bal-layer.md)。决策：[ADR-0037](../../decisions/core/0037-bal-domain-partition-and-closed-loop-motor.md)（三域）、[ADR-0038](../../decisions/core/0038-bal-naming-hard-cut-and-layer-ssot.md)（硬切割）。

**口诀（勿在本文件复制整表）：**

```text
无 DAL/Runtime → math/
跨器件闭环或编排 → control/
单 DAL 增强 → input|output|sensor|actuator|display|comm（对齐主 DAL）

文件：禁 *_helper / *_controller；stem = API 前缀；词表对齐 DAL（ultrasonic）
动词：仍按 §3 A/B/C
```

硬切割后仓库不得残留旧公开名（见 ADR-0038 映射表）。新增/改 BAL 组件用 [06-bal-layer.md §7](../02-wink-micro-os/06-bal-layer.md) 评审清单。

---

## 5. C++ Subset & Arduino Compat Sandbox Guidelines

To support Arduino sketches and third-party libraries (e.g. Adafruit) while preserving WinkMicroOS's C-based kernel efficiency and memory safety, C++ usage must strictly adhere to the following conventions (ADR-0035 / ADR-0036).

### 5.1 Compiler Flags & Feature Exclusions
The following C++ flags are globally enforced for C++ compilation and cannot be bypassed:
*   **`-fno-exceptions`**: Exceptions and `try-catch` are prohibited. The build will fail if compile-time exceptions are used.
*   **`-fno-rtti`**: Run-Time Type Information is disabled; `dynamic_cast` and `typeid` are prohibited.
*   **`-fno-threadsafe-statics`**: Thread-safe local static variables are disabled to avoid implicit mutex allocation.
*   **`-nostdlib++`**: No C++ standard library (`libstdc++`/`libc++`) may be linked. Container structures (`std::vector`, etc.) are prohibited.

### 5.2 Target and Include Isolation
*   **Leaf Target**: `wink_arduino_compat` is a leaf target. No kernel target (`pal`, `dal`, `wink_runtime`, `wink_bal`) may depend on or link against `wink_arduino_compat`.
*   **Include Paths**: Header directories for the Arduino Core API must never be exposed to kernel include paths.
*   **Forbidden Headers**: No `.c` or `.h` files under `pal/`, `dal/`, or `osal/` may include `<Arduino.h>`, `<Wire.h>`, `<SPI.h>`, or C++ standard library headers. This is enforced mechanically in CI pipelines.

### 5.3 Memory Allocation (Arena Heap)
*   **Sandbox Heap**: C++ allocations (via `new`/`delete`) must run on a dedicated static arena `arduino_arena_heap` (32KB default on ESP32).
*   **OOM Fail-Fast**: Memory exhaustion in the sandboxed heap must trigger `pal_panic(WINK_ERR_OUT_OF_MEMORY)` immediately. Returning `nullptr` is prohibited to prevent silent crashes or execution memory corruptions.
*   **Custom Placement New**: Placement new must be overloaded locally without including `<new>` to prevent runtime library pollution:
    ```cpp
    inline void* operator new(size_t, void* __p) noexcept { return __p; }
    inline void* operator new[](size_t, void* __p) noexcept { return __p; }
    ```

### 5.4 Bus Concurrency on Shared Buses (I2C/SPI)
*   **Mutex Protection**: Shared buses (I2C, SPI) accessed by the Arduino Task and the Wink Event Loop must be protected by PAL-level mutexes (e.g., `pal_i2c_lock`/`unlock`).
*   **Locking Hierarchy**: Arduino class overrides (like `TwoWire` transactions) must obtain the corresponding PAL bus lock and release it on transmission completion. Both native DAL drivers and Arduino compatibility drivers must block on the same locks to serialize requests and prevent hardware line collisions.



