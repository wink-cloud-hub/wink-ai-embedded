# BAL/DCST 架构重构方案：静态硬件树 + 动态控制层

> **状态**：v5（2026-07-06 Owner 审阅决策版，落地 WINK_PERIODIC_INVALID / WINK_ERR_CANCELED、LIGHT/MAY_BLOCK 拍板、共享总线 bus-owner、专家示例修正、自 set_period 重入语义、stub 一致性、init 失败回滚契约、术语 Phase/Stage 区分、绝对行号引用符号化等 14 项扫雷修订）
> **日期**：2026-07-06（v5 由 Owner 决策 + AI 执行修订）
> **作者**：Claude Code（基于原始 DCST 方案的批判性评审与重构，审阅人：项目 Owner）
> **前置 ADR**：ADR-0001、ADR-0002、ADR-0004、ADR-0007、ADR-0013/0014、ADR-0016、ADR-0017
> **关联设计规范**：（ADR Accepted 后即刻回写；实施前完成）
> **关联实施计划**：（ADR Accepted 后写 `implementation-plans/2026-07-06-bal-dcst-refactor-plan.md`）

---

## 0. TL;DR

本方案在现有 codegen + device_tree + `wink_periodic` + helper 雏形的基础上，正式建立 **BAL（Business Abstraction Layer，业务抽象层）**，实现：

> **静态 JSON 声明物理硬件（不可变资产）；C 代码通过强类型 BAL Helper 编写动态逻辑（可变行为）。**

核心架构名 **DCST（Dynamic-Control Static-Tree）**，命名沿用原方案。

**与原方案的关键差异（本方案的修正）：**

| 议题 | 原方案 | 本方案 |
|---|---|---|
| 服务生命周期 | 全局自注册链表，fault 时自动遍历 stop | 静态 slot 池 + handle，fault 路径**不**自动 stop 服务，只做 actuator safe-off |
| Helper 默认参数 | 硬编码 2048 栈/ANY 核，无覆盖入口 | 双轨 API：`_start`（初学者默认值）/ `_start_ex`（专家可覆盖栈/优先级/核） |
| pragma 警告抑制 | "应用层 0-Warning"（教条目标） | pragma 收敛到最小作用域；init 阶段允许 blocking 是**合法**的，不追求 0-pragma |
| BAL 层位置 | 未明说 | 正式建 `wink-micro-os/bal/`，从 `samples/common/` 迁移成熟 helper |
| 异步 on_data 回调 | 方案内提出 `dal_xxx_on_data` | 不在本次范围，留待后续独立 ADR（通知上下文/ISR defer 是独立议题） |
| Fault 资源释放 | "用户完全无需手动 stop" | 三阶段 fault 模型：safe-off（非阻塞，立即）→ fault 回调 → WDT/复位兜底；软件层**不承诺**完全资源回收 |
| Codegen 与 BAL 关系 | codegen 自动启动 helper 服务 | codegen 生成设备 init/deinit + **设备实例计数宏 + 配置常量宏**（驱动 BAL 槽位大小与默认参数）；codegen driver plugin 删掉 `get_service_headers/render_service_starts` 钩子；`app_support.c` 删除；所有服务启动/停止留在用户 C 代码里（保持心智模型纯粹："JSON 只描述静态世界，所有启动行为属于 C 层"） |
| Slot 池容量 | 未提及（固定魔数） | codegen 解析 `wink-app.json` 自动生成 `WINK_APP_MAX_<DEVICE>_INSTANCES`，BAL 静态数组以此为大小，100% 避免 RESOURCE_EXHAUSTED 同时零内存浪费；所有 BAL helper 统一走 `wink_periodic_start_ex`（LIGHT/MAY_BLOCK 双路径），不直接用 `wink_soft_timer`，以简化 `WINK_MAX_PERIODIC/SOFT_TIMERS` 容量计算 |
| BAL 对 PAL 类型依赖 | 未明确（示例 include `pal_osal.h`） | BAL 头文件**不得** expose PAL 类型；`wink_helper_opts_t` 使用 BAL 自有的 `wink_bal_core_t` 枚举，BAL `.c` 内部转换为 `pal_os_core_id_t`，彻底守住"BAL ⇢ PAL" 分层红线 |
| 0 实例 stub | 自造 `__attribute__((error(...)))` | 复用现有 `WINK_UNAVAILABLE_MSG` 机制（已支持 GCC/Clang/MSVC）；控制 API 编译报错，`stop` 静默 no-op 方便通用清理路径 |
| Slot 并发安全 | 未考虑（或用全局锁） | 三态状态机（FREE/STARTING/RUNNING）+ TOCTOU 二次校验自回滚，返回 `WINK_ERR_CANCELED` 表并发撤销；临界区只保护元数据，阻塞操作全在 CS 外 |
| LIGHT 路径契约 | 未明确警告 | 血红色文档警告：LIGHT 回调内**严禁**阻塞/浮点重算/mutex/复杂 printf，否则整个协作调度链式崩塌 |
| DAL deinit 质量 | 未明确标准 | **强制**对称 deinit + `gpio_reset_pin` 撤销 reservation；DMA 描述符/未决中断标志必须彻底清零；I2C/SPI 共享 bus 由 codegen 生成的 bus-owner 静态节点管理，单器件 deinit 不得销毁 bus；保证低功耗唤醒后状态一致 |
| Init 失败清理 | 假定 runtime 全部回滚 | "谁启动、谁回滚"契约：runtime 只做 Phase 1 safe-off，已启动 BAL 服务由调用方显式 stop，或交给 WDT 硬件复位兜底；不猜测依赖顺序 |
| Sim 非阻塞模式 | 未明确 | `WINK_STRICT_NONBLOCKING=1` 强制开启，fail-fast 抓阻塞 bug（阶段 5 修补 sim 侧问题） |

---

## 1. 背景与问题陈述

### 1.1 当前 `app_callbacks.c` 的真实痛点

以 `samples/devkitc_smoke/app_callbacks.c` 为观测样本：

```c
/* 问题 1：file-scope 粗粒度 pragma，把所有 deprecated warning 都关了 */
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

/* 问题 2：应用层直接调 pal_os_task_create 形态的 wrapper，携带栈/优先级/核 */
WINK_IGNORE_RESULT(wink_runtime_spawn_periodic(
    "sonar_poll", 2048, 500, sonar_poll_task, &smoke_sonar, 1, PAL_OS_CORE_ANY));

/* 问题 3：用户需要自己写 task 函数壳子 */
static void sonar_poll_task(void *ctx) {
    WINK_IGNORE_RESULT(dal_ultrasonic_request_measurement((dal_ultrasonic_t *)ctx));
}

/* 问题 4：wink_app_services_start 是 codegen 产生的 extern，但无对应 stop */
extern wink_status_t wink_app_services_start(void);

/* 问题 5：wink_led_blink_start 的返回值需要用户自己理解 "handle vs fire-and-forget" */
wink_led_blink_start(&board_led, 1000);   /* 返回 int32_t，用户被鼓励忽略 */

/* 问题 6：selftest 阻塞调用混在 init 里，没有明确标注 "init 阶段合法 blocking" */
wink_selftest_run("*", results, 8, &n);
```

### 1.2 目标人群的差异化需求

| 人群 | 核心诉求 | 必须规避 |
|---|---|---|
| **初学者** | 5 分钟写出跑起来的固件；不理解 RTOS 栈/优先级/核绑定 | 暴露 `pal_os_task_create` 参数；出现"玄学"崩溃 |
| **AI 生成** | API 命名可预测、参数类型严格、模板规整 | void* / 变参 API；需要 pragma 的警告；需要理解 target 差异 |
| **嵌入式专家** | 可动态调整周期/优先级/核；可低功耗启停；可错误恢复 | 一刀切封装锁死底层能力；helper 隐藏了关键时序约束 |

### 1.3 非目标（明确划界）

- ❌ **不**做把业务逻辑写进 JSON 的 DSL（拒绝无代码歧途）。
- ❌ **不**在 BAL 层引入 C++/虚表/运行期多态（与 ADR-0004 冲突）。
- ❌ **不**让 BAL 层依赖/调用 PAL 直接（BAL 只能用 DAL + runtime + `wink_periodic`）。
- ❌ **不**在本次引入 DAL 异步数据回调（`on_data` 模式）——需要独立 ADR 解决通知上下文、ISR defer、回调上下文规则。
- ❌ **不**追求应用层 0-pragma——init 阶段允许 blocking 是合法设计，要做的是 pragma 作用域最小化与位置诚实。
- ❌ **不**承诺 fault 路径自动释放所有资源——硬件复位 + 板级安全电路是最终兜底。

---

## 2. 架构总览

### 2.1 分层图

```
┌────────────────────────────────────────────────────────────────────┐
│ 应用层 (app_callbacks.c — 手写，AI/初学者/专家共用)                │
│   - 事件回调 (on_button_click, on_sonar_ready)                     │
│   - 状态机 / 条件分支 / 业务时序                                   │
│   - 零引脚号 / 零驱动参数，但**可以**访问设备实例字段              │
├────────────────────────────────────────────────────────────────────┤
│ BAL 业务抽象层 (wink-micro-os/bal/ — 本方案新增正式层)            │
│   - 强类型 Helper API：wink_led_blink_start/stop/set_period        │
│                    wink_sonar_helper_start/stop/set_period         │
│                    wink_servo_helper_start/stop/set_angle           │
│                    ...                                              │
│   - 双轨设计：_start (默认参数) / _start_ex (专家可覆盖)           │
│   - 内部分配：slot 池管理 + wink_periodic_start_ex 托管            │
│   - WINK_BLOCKING 相关 pragma 收敛到 BAL .c 内部                    │
├────────────────────────────────────────────────────────────────────┤
│ Runtime 内核运行时                                                 │
│   - wink_periodic (LIGHT/MAY_BLOCK 双路径调度)                     │
│   - wink_soft_timer (tick 上下文，非阻塞)                          │
│   - wink_actuator_registry (safe-off 静态表)                       │
│   - 协作调度器 (ADR-0013/0014, sim fiber / FreeRTOS task)          │
│   - Fault 三阶段处理 (safe-off → callback → WDT 兜底)              │
├────────────────────────────────────────────────────────────────────┤
│ DAL 器件抽象层 (dal/)                                              │
│   - POD 静态分发：dal_led_on/off/toggle, dal_button_poll, ...     │
│   - 同步语义（调用即阻塞到硬件动作完成，或返回 BUSY 让上层调度）    │
│   - 每个驱动在 init 阶段注册自己语义正确的 safe-off 到 actuator_reg│
├────────────────────────────────────────────────────────────────────┤
│ PAL 平台抽象层 (pal/) + targets/ (esp32/host/wasm/baremetal)       │
│   - OSAL (task/sem/mutex/sleep) / HAL (gpio/pwm/i2c/rmt/irq)      │
│   - WINK_BLOCKING 标记所有可能阻塞/让出的 API                      │
└────────────────────────────────────────────────────────────────────┘
        ▲
        │ 构建期 codegen：从 wink-app.json 生成 device_tree.c/h
        │
┌────────────────────────────────────────────────────────────────────┐
│ 静态硬件树 (wink-app.json + tools/codegen/)                        │
│   - 仅描述物理拓扑：有哪些器件、接在哪些引脚、电气极性              │
│   - 构建期生成：静态实例 + init/deinit 拓扑序                      │
│   - **不**声明任何业务动作（周期、blink、telemetry 均不进 JSON）  │
└────────────────────────────────────────────────────────────────────┘
```

### 2.2 依赖规则（严格层向，禁止反向依赖）

```
app → BAL → { DAL, runtime }
BAL ⇢ PAL （禁止：BAL 不得直接 include pal_*.h；阻塞 API 必须经由 runtime 的 wink_periodic 封装）
BAL 公共头 ⇢ PAL 类型 （禁止：pal_os_core_id_t 等 PAL 类型不得出现在任何 BAL 公共头里，使用 BAL 自有 wink_bal_core_t 代替）
DAL → PAL （允许：驱动直接操作硬件）
runtime → PAL （允许：runtime_tasks.c 是 ADR-0017 合法例外）
codegen 输出 → BAL （允许：device_tree.h 可被 BAL .c include 拿 WINK_APP_MAX_XXX_INSTANCES 宏；但 device_tree.h 自身不得 include BAL 头，防止循环依赖）
```

> **类型隔离细节（重要）**：`wink_bal_core_t` 是 BAL 自有的核亲和枚举 `{ WINK_BAL_CORE_ANY = 0, WINK_BAL_CORE_0 = 1, WINK_BAL_CORE_1 = 2 }`，在 `bal/include/wink_helper_opts.h` 中定义；BAL `.c` 内部在调用 `wink_periodic_start_ex` 前把它映射到 `pal_os_core_id_t`。这样应用层用 BAL 时完全不需要 include 任何 PAL 头文件，分层边界清晰。

---

## 3. 五大支柱设计

### 支柱 1：`wink-app.json` 严格约束在物理层（继承原方案，加强约束）

#### 3.1.1 允许的字段

```json
{
  "app_name": "devkitc_smoke",
  "board": "esp32_devkitc",
  "devices": {
    "board_led":   { "type": "led",        "pin": 2,  "active_high": true },
    "boot_button": { "type": "button",     "pin": 0,  "active_low":  true,
                     "long_press_ms": 3000, "isr_counter": true,
                     "auto_poll_ms": 10 },
    "smoke_sonar": { "type": "ultrasonic", "trig_pin": 18, "echo_pin": 19,
                     "use_rmt": true }
  }
}
```

#### 3.1.2 禁止的字段

- ❌ `"services": { ... }`（blink/telemetry/poll 周期不进 JSON）
- ❌ `"callbacks": { ... }`（业务回调必须在 C 里手写）
- ❌ `"state_variables": [...]`（状态是 C 代码的事）
- ❌ 任何带 `"period_ms"` / `"priority"` / `"stack"` / `"on_xxx"` 语义的键

**理由**：JSON 只描述"长什么样的硬件"，不描述"要干什么"。后者是图灵完备 C 语言的职责。

#### 3.1.3 Codegen 动态化 BAL 槽位容量（关键设计）

既然 codegen 是唯一掌握"应用实际用了几个同类型设备"真相的组件，**槽位容量应由 codegen 动态生成，而不是 BAL 内硬编码魔数**：

**device_tree.h 新增 codegen 输出：**
```c
/* 自动生成，不要手改：每个 BAL helper 的槽位容量 = 该类型设备在 JSON 里的实例数 */
#define WINK_APP_MAX_LED_INSTANCES         1u
#define WINK_APP_MAX_BUTTON_INSTANCES      1u
#define WINK_APP_MAX_ULTRASONIC_INSTANCES  1u
#define WINK_APP_MAX_SERVO_INSTANCES       0u  /* 未使用的类型 = 0 */
/* ... */

/* 若用户在 JSON 中声明同类型多个器件（如 2 路超声波），宏自动变为 2 */
```

**BAL helper `.c` 使用这些宏作为静态数组大小：**
```c
/* bal/src/sensor/wink_sonar_helper.c */
#include "device_tree.h"   /* 拿 WINK_APP_MAX_ULTRASONIC_INSTANCES */

#ifndef WINK_APP_MAX_ULTRASONIC_INSTANCES
#define WINK_APP_MAX_ULTRASONIC_INSTANCES 2u   /* 非 codegen 构建（如单测）的 fallback */
#endif

static sonar_slot_t s_slots[WINK_APP_MAX_ULTRASONIC_INSTANCES];
/* 若 JSON 有 2 路超声波 → 数组长度 2，零浪费；
 * 若 0 路 → 数组长度 0（不占 RAM），start 返回 RESOURCE_EXHAUSTED */
```

**好处：**
- 100% 避免 `RESOURCE_EXHAUSTED`：槽位数量 = 设备实例数 + 0；
- 零内存浪费：不用按全局最大 4/8 开数组；
- 未使用的 BAL helper 其数组长度为 0（或被链接器 GC），在小资源 MCU 上尤为关键。

> **注**：LIGHT 路径的 soft_timer 句柄也走同样机制；但 `wink_soft_timer` / `wink_periodic` 是 runtime 全局池，容量由编译期 `-DWINK_MAX_SOFT_TIMERS=N` / `-DWINK_MAX_PERIODIC=N` 控制，codegen 可在 `app_options.cmake` 里按需计算并设值（如按所有 BAL helper 的总槽位 + 4 余量）。

#### 3.1.4 行动项

1. 从 `tools/codegen/app_codegen.py` 现有 schema 中移除 `services`/`callbacks`/`state_variables` 三个未实际使用的字段（代码确认：当前只是 schema 校验允许、模板未使用，无外部 JSON 依赖，直接移除不做 deprecation 宽限）。
2. **删除** `app_support.c.j2` 整个文件——其核心产物 `wink_app_services_start()` 不再存在（贯彻"JSON 只描述静态世界，所有启动行为属于 C 层"原则）。同步从 `DriverBase` 移除 `get_service_headers()` / `render_service_starts()` 两个钩子；button driver 插件删掉对应实现。`app_callbacks.c` 模板/示例里显式写 `wink_button_helper_start(&boot_button, BOOT_BUTTON_AUTO_POLL_MS)` 这一行，保证运行期行为在 `app_init` 里一目了然。
3. **加固（非新增）** `wink_device_tree_deinit()`：当前 codegen 已生成（符号 `wink_device_tree_deinit` 在 `device_tree.c.j2` 的 deinit 块中），签名 `void`、unregister 在前、deinit 逆序在后——保持该语义。需修正/核实的点：(a) unregister actuator thunk 顺序必须与 register 严格反向（当前是 forward init 序 unregister = reverse register 序，保持）；(b) deinit 调用必须全部用 `WINK_IGNORE_RESULT` 链式 best-effort（当前已如此），不能因单个 deinit 失败而阻断后续清理（fault 路径不允许被单设备阻塞）。
4. device_tree.h.j2 新增两类宏：
   - **设备实例计数宏** `WINK_APP_MAX_<DEVICE>_INSTANCES`（驱动 BAL 槽位大小，见 §3.1.3）；
   - **配置常量宏**（如 `BOOT_BUTTON_AUTO_POLL_MS / BOOT_BUTTON_LONG_PRESS_MS / SMOKE_SONAR_USE_RMT`），由 driver plugin 新增 `render_config_macros()` 钩子按字段选择性导出（避免全量字段生成宏导致 device_tree.h 膨胀）。
5. app_options.cmake 新增 codegen 计算的 `WINK_MAX_PERIODIC` 最小值配置（统一走 `wink_periodic` 后，soft_timer 仅保留 4 个余量给 selftest/用户自定义）。容量估算规则：`WINK_MAX_PERIODIC ≥ Σ(WINK_APP_MAX_<DEV>_INSTANCES) + 4`，其中 Σ 覆盖所有可能被用户启用的 BAL helper（对应启用了 WINK_USE_XXX 的设备类型）。

---

### 支柱 2：BAL Helper 强类型双轨 API（修正原方案的单轨假设）

#### 3.2.1 命名拓扑

统一遵循：

```
wink_<device>_<service>_start       (&dev, ...service_params)        → wink_status_t
wink_<device>_<service>_start_ex    (&dev, ...service_params,        → wink_status_t
                                     const wink_helper_opts_t *opts)
wink_<device>_<service>_stop        (&dev)                            → void
wink_<device>_<service>_set_<attr>  (&dev, <attr_value>)              → void / wink_status_t
wink_<device>_<service>_is_running  (&dev)                            → bool
```

- `<device>` ：DAL 类型名（led / button / ultrasonic / servo / motor / oled / ...）
- `<service>` ：该器件上的业务模式（blink / breath / poll / sweep / telemetry / ...）
- 一对多关系天然成立：`wink_led_blink_start` / `wink_led_breath_start` / `wink_led_marquee_start` 是同一个 led 器件上不同的 BAL 服务——这正是反对统一 `wink_dev_start` 的根本理由（见 §3.2.5）。

#### 3.2.2 默认值策略（初学者 API：`_start`）

每个 helper 自己定义"合理默认"，在 `.h` 顶部文档化。参考当前 codebase 惯例与踩坑经验（见 [[freertos-same-priority-pulse-stretch]] / [[dal-eager-init-pattern]]）：

| Helper | 默认栈 | 默认优先级 | 默认核 | 默认周期 | 执行路径 | 备注 |
|---|---|---|---|---|---|---|
| `wink_led_blink_start(&led, ms)` | 无独立栈 | —（tick 上下文） | ANY | 入参 | **LIGHT** | 软定时器 tick 路径，非阻塞 |
| `wink_button_helper_start(&btn, poll_ms)` | 无独立栈 | —（tick 上下文） | ANY | 入参（建议 10ms） | **LIGHT** | tick 上下文；ISR 边沿计数 + LIGHT poll 去抖（`isr_counter: true` in JSON）；回调 ≤100µs；已走查 host/esp32/wasm 三 target `dal_button_poll` 与 `pal_gpio_read` 仅做寄存器级读（<1µs），无阻塞，安全归入 LIGHT |
| `wink_sonar_helper_start(&sonar, ms)` | 3072 | 5 | ANY | 入参（≥50ms） | MAY_BLOCK | RMT 脉冲捕获需高优先级，栈略大（已决 Q1） |
| `wink_servo_sweep_start(&sv, min, max, ms)` | 2048 | 3 | ANY | 入参 | MAY_BLOCK | PWM set_duty 是快操作 |
| `wink_telemetry_default_start(...)` | 2048 | 1 | ANY | 2000ms | MAY_BLOCK | 低优先级后台 LOG_I |
| `wink_oled_animation_start(...)` | 3072 | 2 | ANY | 33ms (30fps) | MAY_BLOCK | I2C 阻塞传输，要独立任务 |

> **默认值原则**：时序敏感（RMT/脉冲捕获）类 helper 默认高优先级、可钉核；后台遥测/日志类默认低优先级；tick 级轻量工作默认 LIGHT 无独立栈。
>
> **BAL Helper 默认常量契约（强制）**：每个 helper 的 `.h` 必须暴露以下一组 `#define WINK_<DEV>_HELPER_DEFAULT_*` 宏，作为 `_start()` 的默认值、codegen 容量参考和文档化入口；`_start_ex()` 传 NULL 时即使用这些宏：
> - `WINK_<DEV>_HELPER_DEFAULT_STACK`（栈字节，LIGHT 路径为 0）
> - `WINK_<DEV>_HELPER_DEFAULT_PRIO`（FreeRTOS 优先级，LIGHT 路径为 0 或留空）
> - `WINK_<DEV>_HELPER_DEFAULT_CORE`（`WINK_BAL_CORE_ANY` 等）
> - `WINK_<DEV>_HELPER_MIN_PERIOD_MS`（最小合法周期，防配置过短炸时序）
> - `WINK_<DEV>_HELPER_DEFAULT_FLAGS`（`WINK_PERIODIC_LIGHT` 或 `WINK_PERIODIC_MAY_BLOCK`，驱动 `opts->flags == 0` 时的默认行为）

#### 3.2.3 专家覆盖 API：`_start_ex` + `wink_helper_opts_t`

`wink_helper_opts_t` **统一定义在 `bal/include/wink_helper_opts.h`**（每个 helper `.h` 不得重复定义同名 struct，否则类型冲突）。该头同时定义 BAL 自有的核亲和枚举，杜绝 BAL 公共头对 `pal_osal.h` 的依赖，守住 §2.2 分层红线：

```c
/* bal/include/wink_helper_opts.h */
#ifndef WINK_HELPER_OPTS_H
#define WINK_HELPER_OPTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BAL 自有的核亲和枚举（不依赖 pal_os_core_id_t，避免 BAL 公共头 include PAL）。
 *
 * BAL 内部在调用 wink_periodic_start_ex 时映射到 pal_os_core_id_t；
 * 不支持多核的 target（host/wasm/baremetal）将 WINK_BAL_CORE_0/1 视为 ANY。
 */
typedef enum {
    WINK_BAL_CORE_ANY = 0,   /* runtime/scheduler 决定 */
    WINK_BAL_CORE_0   = 1,
    WINK_BAL_CORE_1   = 2,
    WINK_BAL_CORE_INVALID = -1,
} wink_bal_core_t;

/**
 * @brief BAL Helper 调度覆盖选项（专家用）。
 *
 * 传 NULL 给 _start_ex 等价于调 _start（全用默认值）。
 * 零值/默认值字段表示"使用 helper 自带默认"。
 */
typedef struct {
    uint32_t        stack_bytes;  /* 0 = use default */
    int32_t         priority;     /* <0 = use default (WINK_PERIODIC_DEFAULT_PRIORITY 或 helper 自有默认) */
    wink_bal_core_t core_id;      /* WINK_BAL_CORE_INVALID = use default */
    uint32_t        flags;        /* WINK_PERIODIC_LIGHT / WINK_PERIODIC_MAY_BLOCK / 0 = use helper default */
} wink_helper_opts_t;

/* 便捷初始化宏：指定栈+优先级+核；其他字段为 0（默认） */
#define WINK_HELPER_OPTS(stack, prio, core) \
    ((wink_helper_opts_t){ .stack_bytes = (stack), .priority = (prio), .core_id = (core), .flags = 0u })

/* 默认选项初始化宏（推荐使用，防止零初始化时将优先级/核绑定错误覆盖为 0/ANY） */
#define WINK_HELPER_OPTS_DEFAULT \
    ((wink_helper_opts_t){ .stack_bytes = 0, .priority = -1, .core_id = WINK_BAL_CORE_INVALID, .flags = 0u })

#ifdef __cplusplus
}
#endif
#endif /* WINK_HELPER_OPTS_H */
```

使用示例（专家场景）：

```c
/* 超声波：钉到 CORE_1，优先级拉到 7，栈加大到 4096 应对 -O0 LOG_D 场景 */
wink_sonar_helper_start_ex(&smoke_sonar, 50 /*period_ms*/,
    &WINK_HELPER_OPTS(4096, 7, WINK_BAL_CORE_1));
```

#### 3.2.4 典型 Helper 头文件（以超声波为例）

```c
/* bal/include/sensor/wink_sonar_helper.h */
#ifndef WINK_SONAR_HELPER_H
#define WINK_SONAR_HELPER_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"
#include "dal_ultrasonic.h"
#include "wink_helper_opts.h"   /* wink_helper_opts_t / wink_bal_core_t / WINK_HELPER_OPTS — 唯一入口 */

#ifdef __cplusplus
extern "C" {
#endif

/* ── 默认参数（文档化，专家可参考；BAL Helper 默认常量契约要求） ──── */
#define WINK_SONAR_HELPER_DEFAULT_STACK    3072U
#define WINK_SONAR_HELPER_DEFAULT_PRIO     5
#define WINK_SONAR_HELPER_DEFAULT_CORE     WINK_BAL_CORE_ANY
#define WINK_SONAR_HELPER_DEFAULT_FLAGS    WINK_PERIODIC_MAY_BLOCK
#define WINK_SONAR_HELPER_MIN_PERIOD_MS    50U   /* RMT 单次测量 ~25-30ms，周期不可短于此 */

/* ── 初学者 API ──────────────────────────────────────────────── */
/**
 * @brief 启动超声波周期测量（默认栈 3KB / prio=5 / 任意核 / MAY_BLOCK 路径）。
 * @param dev       已 init 的 dal_ultrasonic_t
 * @param period_ms 测量周期 (≥ WINK_SONAR_HELPER_MIN_PERIOD_MS)
 * @return WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_RESOURCE_EXHAUSTED(槽位慢) /
 *         WINK_ERR_BUSY(该 dev 已在跑同一 helper) / WINK_ERR_UNSUPPORTED(目标未启用 ultrasonic)
 * @note  测量结果通过 dal_ultrasonic_get_cached_distance() 轮询读取；
 *        on_data 异步回调模式待后续版本（独立 ADR）。
 */
wink_status_t wink_sonar_helper_start(dal_ultrasonic_t *dev, uint32_t period_ms);

/* ── 专家 API ────────────────────────────────────────────────── */
wink_status_t wink_sonar_helper_start_ex(dal_ultrasonic_t *dev, uint32_t period_ms,
                                          const wink_helper_opts_t *opts);

/* ── 公共 API（初学者/专家共用） ─────────────────────────────── */
void         wink_sonar_helper_stop(dal_ultrasonic_t *dev);   /* NULL 安全、幂等 */
wink_status_t wink_sonar_helper_set_period(dal_ultrasonic_t *dev, uint32_t period_ms);
bool         wink_sonar_helper_is_running(const dal_ultrasonic_t *dev);

#ifdef __cplusplus
}
#endif
#endif /* WINK_SONAR_HELPER_H */

#### 3.2.5 为什么**不**统一为 `wink_dev_start`（继承原方案论证，补充一条与 ADR-0004 的冲突）

原方案三点理由全部成立：
1. **C 无重载**：void*/变参会丧失编译期类型检查，AI 传错 float/int 真机炸栈。
2. **DAL→BAL 一对多**：同一 led 可以 blink/breath/marquee，统一名无法在编译期区分服务。
3. **AI 强类型导航**：LLM 靠命名拓扑预测参数，泛化 API 反增加 AI 错率。

**补充第 4 条（与 ADR-0004 对齐）**：统一 `wink_dev_start` 本质是通过枚举+void* 从后门把虚表/运行期多态请回来，和项目"编译期静态分发、无 void* opaqueness"的核心范式直接冲突。

#### 3.2.6 BAL 目录布局

```
wink-micro-os/bal/
├── include/
│   ├── wink_helper_opts.h        /* wink_helper_opts_t + wink_bal_core_t + WINK_HELPER_OPTS 宏（BAL 公共头唯一入口） */
│   ├── output/                   /* 输出器件 helper：LED blink/breath/marquee/buzzer 等 */
│   │   └── wink_led_blink_helper.h
│   ├── input/                    /* 输入器件 helper：button poll/encoder 等 */
│   │   └── wink_button_helper.h
│   ├── sensor/                   /* 传感器 helper：ultrasonic 周期测量/IMU 读取等 */
│   │   └── wink_sonar_helper.h
│   ├── actuator/                 /* 执行器 helper：servo sweep/motor PID 等 */
│   │   └── wink_servo_helper.h
│   ├── display/                  /* 显示 helper：OLED 动画/数码管刷新等 */
│   │   └── wink_oled_helper.h
│   └── comm/                     /* 通信/遥测 helper：default telemetry/MQTT 上报等 */
│       └── wink_telemetry_helper.h
├── src/
│   └── （镜像 include 布局，每个 helper 一个 .c 文件）
└── tests/
    └── （host Unity 单测，一个 helper 一个 test 文件）
```

CMake：bal 作为一个静态库 `wink_bal`，链接到 sample/固件，依赖 `wink_runtime` + `wink_dal`。`wink_bal` 自身**不直接** link PAL 除外的 target-specific 物件（保持跨 target 同源编译）；BAL 公共头 include 路径下只出现 `wink_*`、`dal_*` 头，不得出现任何 `pal_*` 头（§2.2 类型隔离）。

**Helper 实现约定**：
- 每个 helper `.c` 内一个 static slot 数组，大小 `WINK_APP_MAX_<DEV>_INSTANCES`（0 实例时用 `WINK_UNAVAILABLE_MSG` stub）；
- 所有周期调度统一走 `wink_periodic_start_ex`（LIGHT/MAY_BLOCK），不直接调用 `wink_soft_timer_create`——LIGHT 路径底层仍由 soft_timer 承载，但通过 `wink_periodic` 统一入口便于容量计算与 set_period 分发；
- 所有 helper `.c` 顶部用 `WINK_INTERNAL_BLOCKING_REGION_BEGIN/END`（见 §3.5）收敛 pragma。

#### 3.2.7 ⚠️ LIGHT 路径强契约（血红色警告）

所有默认走 `WINK_PERIODIC_LIGHT` 路径的 BAL helper（blink / button poll / 未来的 tick 级采样），其回调在 **runtime 主 tick 的 soft_timer 分发上下文**里执行。此上下文有**极度严苛**的契约：

> 🩸 **CRITICAL — LIGHT 回调铁律**
>
> LIGHT 回调内**严禁**执行以下任何操作：
> 1. **阻塞/让出**：调用任何 `WINK_BLOCKING` API（`pal_os_sleep_ms` / `pal_os_mutex_lock` / `pal_os_sem_take` / `pal_os_task_create` / 阻塞式 I2C/SPI/RMT wait）；
> 2. **耗时运算**：浮点三角函数、复杂 PID、JSON 序列化、printf-heavy 日志（单次 LOG_I 超过 100µs 都要警惕）；
> 3. **等外部事件**：busy-wait 超过 10µs、轮询硬件寄存器超过极少量次数；
> 4. **重入调用 BAL/Runtime 的非 ISR-safe API**。
>
> **违反后果**：LIGHT 回调在协作调度上下文里运行，一旦阻塞/长耗时，**整个调度链全部停摆**——包括其他 soft_timer、app_loop、WCET 监控本身。表现为：系统"看起来没死机但什么都不动"、所有定时器延迟、按钮失灵、遥测停止、（sim target 下）整个 UI 冻结。这是比单任务崩溃更难调试的灾难级故障。
>
> **执行保障（三道防线，全在必做）**：
> - **编译期**：LIGHT 路径下 `WINK_BLOCKING` API 触发 deprecated 警告（`-Werror` 下编译失败）；sim target 强制 `WINK_STRICT_NONBLOCKING=1`（已决 Q5），符号从 PAL 头消失，直接链接失败。
> - **运行期（阶段 1 必做，非可选项）**：
>   - soft_timer 已有 WCET 监控（回调 >50% tick 即 `WINK_WARN_WCET_EXCEEDED` 软警告）保留，并在 `WINK_PT_DEBUG`/开发构建下升级为 hard fault（触发 `wink_trace_fault`）；
>   - 在 `wink_periodic` LIGHT 分发入口/出口维护一个"在 LIGHT 上下文"标志（sim/host 下为 thread-local 或全局），复用 `wink_pt_debug.h` 里已有的 `WINK_ASSERT_NONBLOCKING()` 宏——这能抓住"DAL/PAL 新增 API 忘记挂 `WINK_BLOCKING` 标记"的漏网违规（编译期防线绕得过、运行期绕不过）；
>   - 断言触发后打印 LOG_E 带 helper 名/回调地址并 fault，定位到具体违规调用。
>   - **⚠️ 测量抖动警告**：由于运行在支持硬件中断抢占的 RTOS 上，`pal_os_get_us()` 测量的 wall-clock 时间包含了抢占式高优先级 ISR 的执行时间。为避免由于偶然的中断抖动导致 LIGHT 路径误触发 hard fault，WCET 超时阈值应预留足够的安全余量（建议 LIGHT 100µs 限制在开发构建中配为 200-500µs 的 hard limit），并在升级为 fault 之前打印警告日志及堆栈，以便排查是否受中断抢占影响。
> - **Code review**：所有新 BAL helper 在 PR 里必须标注其执行路径（LIGHT/MAY_BLOCK），LIGHT 的回调体长度建议不超过 20 行。
>
> **不能满足此契约的 BAL helper**（如 I2C OLED 动画、RMT 读超声波、SD 卡写入）必须强制走 `WINK_PERIODIC_MAY_BLOCK` 路径，用独立任务/独立栈。

---

### 支柱 3：静态 slot 池 + handle 生命周期（**替代原方案的全局自注册链表**）

#### 3.3.1 为什么原方案的链表有三处硬伤

回顾评审意见：
1. **单实例假设炸多实例**：静态节点里硬编码 `ctx = &smoke_sonar`，同一 helper 不能服务两个 HC-SR04。
2. **Fault 路径 blocking stop 违反安全约束**：`wink_periodic_stop` 等信号最长 500ms，fault 路径是要尽快进安全态的。
3. **双删竞态**：用户手动 stop 与 fault 路径 stop 并发时链表损坏。

#### 3.3.2 本方案方案：per-helper 静态 slot 池

每个 BAL helper 在自己的 `.c` 里维护一个静态 slot 数组，模式对齐已经在 `wink_button_helper.c` / `wink_runtime_tasks.c` 验证过的 slot 池范式：

```c
/* bal/src/sensor/wink_sonar_helper.c */
#define LOG_TAG "bal.sonar"

#include "wink_sonar_helper.h"
#include "wink_periodic.h"          /* wink_periodic_start_ex / stop / change_period */
#include "wink_blocking_region.h"   /* WINK_INTERNAL_BLOCKING_REGION_BEGIN/END */
#include "pal_irq.h"
#include "pal_log.h"
#include "pal_osal.h"               /* pal_os_core_id_t — BAL .c 内部可以 include PAL；只有 BAL 公共头禁止 */
#include "device_tree.h"            /* codegen 生成的 WINK_APP_MAX_ULTRASONIC_INSTANCES */

/* ADR-0017 BAL-exception: helper 内部通过 wink_periodic MAY_BLOCK 路径调用
 * WINK_BLOCKING API（dal_ultrasonic_request_measurement）。pragma/警告抑制
 * 用统一的 WINK_INTERNAL_BLOCKING_REGION 宏收敛在此 TU 内，应用层 include
 * BAL 头文件不会看到 deprecated 声明警告。 */
WINK_INTERNAL_BLOCKING_REGION_BEGIN

/* 非 codegen 构建（如 host 单测）的 fallback 容量 */
#ifndef WINK_APP_MAX_ULTRASONIC_INSTANCES
#define WINK_APP_MAX_ULTRASONIC_INSTANCES 2u
#endif

#if WINK_APP_MAX_ULTRASONIC_INSTANCES == 0
/* 无此类型设备时：提供 WINK_UNAVAILABLE_MSG 标注的 stub。
 * 复用现有 WINK_UNAVAILABLE_MSG 机制（GCC/Clang 编译期 error、MSVC 升级为 error 的 warning），
 * 用户/AI 在未配置硬件的情况下误调用，直接在编译期报错并给出修复指引。
 *
 * Stub 约定（所有 BAL Helper 统一遵守）：
 *   - 控制/状态类 API（start/start_ex/set_period/is_running）：标记 WINK_UNAVAILABLE_MSG，
 *     编译期强制报错，避免在无设备板型上悄然 no-op 掩盖配置错误；
 *   - 清理类 API（stop）：保持静默 no-op，**不**挂 WINK_UNAVAILABLE_MSG。理由：通用的
 *     故障/低功耗清理代码路径会无差别调用所有 helper 的 stop，如果 stop 也编译报错，
 *     应用层就必须写大量 #ifdef 分支来判断板型——这违反了"stop 幂等/NULL 安全"的统一
 *     心智模型。stop 即使无设备也安全 no-op。*/
#define _SONAR_UNAVAIL WINK_UNAVAILABLE_MSG(     "No ultrasonic devices defined in wink-app.json — add a '"'type'"': '"'ultrasonic'"'' device first")

_SONAR_UNAVAIL wink_status_t wink_sonar_helper_start(dal_ultrasonic_t *d, uint32_t p);
_SONAR_UNAVAIL wink_status_t wink_sonar_helper_start_ex(dal_ultrasonic_t *d, uint32_t p, const wink_helper_opts_t *o);
_SONAR_UNAVAIL wink_status_t wink_sonar_helper_set_period(dal_ultrasonic_t *d, uint32_t p);
_SONAR_UNAVAIL bool         wink_sonar_helper_is_running(const dal_ultrasonic_t *d);

wink_status_t wink_sonar_helper_start(dal_ultrasonic_t *d, uint32_t p) { (void)d; (void)p; return WINK_ERR_UNSUPPORTED; }
wink_status_t wink_sonar_helper_start_ex(dal_ultrasonic_t *d, uint32_t p, const wink_helper_opts_t *o) { (void)d; (void)p; (void)o; return WINK_ERR_UNSUPPORTED; }
void         wink_sonar_helper_stop(dal_ultrasonic_t *dev) { (void)dev; }   /* stop 始终静默 no-op，见上文 stub 约定 */
wink_status_t wink_sonar_helper_set_period(dal_ultrasonic_t *d, uint32_t p) { (void)d; (void)p; return WINK_ERR_UNSUPPORTED; }
bool         wink_sonar_helper_is_running(const dal_ultrasonic_t *d) { (void)d; return false; }

#undef _SONAR_UNAVAIL
#else

typedef enum {
    WINK_BAL_SLOT_FREE = 0,
    WINK_BAL_SLOT_STARTING,
    WINK_BAL_SLOT_RUNNING
} wink_bal_slot_state_t;

typedef struct {
    dal_ultrasonic_t      *dev;
    wink_periodic_handle_t periodic_h; /* 占用句柄。无效初始值为 WINK_PERIODIC_INVALID */
    uint32_t               period_ms;
    wink_bal_slot_state_t  state;
    uint32_t               generation; /* 世代计数器，防范 ABA 竞态 */
} sonar_slot_t;

static sonar_slot_t s_slots[WINK_APP_MAX_ULTRASONIC_INSTANCES];

/* 所有对 s_slots 的访问在 runtime tick 协作模型下是单线程的，
 * 但 stop 可能从另一个 preemptive task 调用 → 用临界区保护元数据。 */

/* 内部：BAL 核枚举 → PAL 核枚举的转换（隔离 BAL 头对 PAL 类型的暴露） */
static pal_os_core_id_t map_core(wink_bal_core_t c) {
    switch (c) {
    case WINK_BAL_CORE_0:   return PAL_OS_CORE_0;
    case WINK_BAL_CORE_1:   return PAL_OS_CORE_1;
    default:                return PAL_OS_CORE_ANY;
    }
}

/* 内部：周期 task 函数 */
static void sonar_periodic_task(void *ctx) {
    sonar_slot_t *slot = (sonar_slot_t *)ctx;
    /* WINK_BLOCKING API 可以在这里安全调用：这是 MAY_BLOCK 路径（整个 TU 被 WINK_INTERNAL_BLOCKING_REGION 包裹） */
    WINK_IGNORE_RESULT(dal_ultrasonic_request_measurement(slot->dev));
}

wink_status_t wink_sonar_helper_start_ex(dal_ultrasonic_t *dev, uint32_t period_ms,
                                          const wink_helper_opts_t *opts) {
    if (dev == NULL || period_ms < WINK_SONAR_HELPER_MIN_PERIOD_MS)
        return WINK_ERR_INVALID_ARG;

    /* 解析 opts，NULL 或字段为零值 → 用 helper 默认值（由 WINK_SONAR_HELPER_DEFAULT_* 宏定义） */
    uint32_t stack = (opts && opts->stack_bytes) ? opts->stack_bytes : WINK_SONAR_HELPER_DEFAULT_STACK;
    int32_t  prio  = (opts && opts->priority >= 0) ? opts->priority : WINK_SONAR_HELPER_DEFAULT_PRIO;
    wink_bal_core_t core_sel = (opts && opts->core_id != WINK_BAL_CORE_INVALID)
                            ? opts->core_id : WINK_SONAR_HELPER_DEFAULT_CORE;
    pal_os_core_id_t core = map_core(core_sel);
    uint32_t flags = (opts && opts->flags) ? opts->flags : WINK_SONAR_HELPER_DEFAULT_FLAGS;

    /* 临界区：使用实际存在的 pal_irq_save_rtos_safe / pal_irq_restore 保护槽位分配 */
    uint32_t mask = pal_irq_save_rtos_safe();
    /* 幂等：已在跑 → BUSY */
    for (uint32_t i = 0; i < WINK_APP_MAX_ULTRASONIC_INSTANCES; i++) {
        if (s_slots[i].dev == dev) { pal_irq_restore(mask); return WINK_ERR_BUSY; }
    }
    /* 找空闲 slot（注意：必须扫描全数组找 FREE 槽位，不能用环形游标——
     * stop 要能原地回收 slot，避免 blink_helper 现有的 s_next++ 环形耗尽 bug） */
    sonar_slot_t *slot = NULL;
    for (uint32_t i = 0; i < WINK_APP_MAX_ULTRASONIC_INSTANCES; i++) {
        if (s_slots[i].state == WINK_BAL_SLOT_FREE) { slot = &s_slots[i]; break; }
    }
    if (!slot) { pal_irq_restore(mask); return WINK_ERR_RESOURCE_EXHAUSTED; }
    slot->dev = dev;               /* 先占 slot，状态标为 STARTING，防同 dev 并发 start */
    slot->state = WINK_BAL_SLOT_STARTING;
    slot->periodic_h = WINK_PERIODIC_INVALID;
    slot->period_ms = period_ms;
    uint32_t expected_gen = ++slot->generation; /* 世代计数器自增，用于防范 ABA 竞态 */
    pal_irq_restore(mask);

    char name[16];
    (void)snprintf(name, sizeof(name), "sonar_%p", (void*)dev);
    wink_periodic_handle_t h = wink_periodic_start_ex(
        name, stack, period_ms, sonar_periodic_task, slot, flags, prio, core);
    
    mask = pal_irq_save_rtos_safe();
    if (h < 0) {
        /* 仅在 generation 未改变时回滚，如果已改变说明被新任务占用，我们不应触碰 slot */
        if (slot->generation == expected_gen) {
            slot->dev = NULL;
            slot->state = WINK_BAL_SLOT_FREE;
        }
        pal_irq_restore(mask);
        return (wink_status_t)h;
    }
    
    /* 解决 TOCTOU 竞态：如果在临界区外 start_ex 期间有并发 stop 将 slot 清空，
     * slot->dev 会变为 NULL 或被其他设备重新占用。我们需要二次校验设备、状态和世代计数器。 */
    if (slot->dev == dev && slot->state == WINK_BAL_SLOT_STARTING && slot->generation == expected_gen) {
        slot->periodic_h = h;
        slot->state = WINK_BAL_SLOT_RUNNING;
        pal_irq_restore(mask);
    } else {
        /* 期间发生了并发 stop 撤销。执行回滚，仅在 generation 未被复用改变时清理 slot，并在临界区外调用 periodic_stop 销毁新任务 */
        if (slot->generation == expected_gen) {
            slot->dev = NULL;
            slot->periodic_h = WINK_PERIODIC_INVALID;
            slot->state = WINK_BAL_SLOT_FREE;
        }
        pal_irq_restore(mask);
        wink_periodic_stop(h);
        return WINK_ERR_CANCELED;
    }
    return WINK_OK;
}

/* _start 是 3 行 wrapper：opts=NULL 转调 _start_ex */
wink_status_t wink_sonar_helper_start(dal_ultrasonic_t *dev, uint32_t period_ms) {
    return wink_sonar_helper_start_ex(dev, period_ms, NULL);
}

void wink_sonar_helper_stop(dal_ultrasonic_t *dev) {
    if (!dev) return;
    uint32_t mask = pal_irq_save_rtos_safe();
    sonar_slot_t *slot = NULL;
    for (uint32_t i = 0; i < WINK_APP_MAX_ULTRASONIC_INSTANCES; i++) {
        if (s_slots[i].dev == dev) { slot = &s_slots[i]; break; }
    }
    if (!slot) { pal_irq_restore(mask); return; }
    
    wink_bal_slot_state_t old_state = slot->state;
    wink_periodic_handle_t h = slot->periodic_h;
    
    /* 统一清空槽位元数据，并自增 generation 促使任何未决 start 因世代比对失败自回滚 */
    slot->dev = NULL;
    slot->periodic_h = WINK_PERIODIC_INVALID;
    slot->state = WINK_BAL_SLOT_FREE;
    slot->generation++;
    pal_irq_restore(mask);
    
    /* 只有当先前是运行态且句柄有效时，才在临界区外发起 stop 阻塞调用 */
    if (old_state == WINK_BAL_SLOT_RUNNING && h != WINK_PERIODIC_INVALID) {
        wink_periodic_stop(h);  /* stop 在临界区外调用：可能阻塞等 sem */
    }
}

#endif /* WINK_APP_MAX_ULTRASONIC_INSTANCES > 0 */

WINK_INTERNAL_BLOCKING_REGION_END
```

**关键设计点：**
- **槽位容量 codegen 驱动**：`WINK_APP_MAX_ULTRASONIC_INSTANCES` 由 codegen 根据 JSON 中实际设备数生成，0 实例时整个 helper 编译为空 stub，零 RAM 开销。
- **三态 Slot 状态机（FREE/STARTING/RUNNING）+ TOCTOU 二次校验**：临界区内预留 slot → 临界区外启动 task → 临界区内二次校验 dev 与 state，若期间被并发 stop 撤销则自回滚（返回 `WINK_ERR_CANCELED`）；从设计上消灭 start/stop 并发的空指针窗口。
- **多实例安全**：静态槽数组 + 临界区保护元数据；**必须扫描全数组找 FREE 槽位**（不用环形游标 `s_next++`），避免现有 `wink_blink_helper.c` 里 `s_next++` 导致 stop 无法回收、4 次 start 即 RESOURCE_EXHAUSTED 的 LIFO 耗尽 bug。
- **幂等 + NULL 安全**：`stop(NULL)` 无害；start 同 dev 两次返回 `WINK_ERR_BUSY`；stop 在 STARTING 态的 slot 上也安全（状态机自动处理撤销回滚）。
- **临界区只保护 slot 元数据**，`wink_periodic_start_ex`（任务创建）/`wink_periodic_stop`（可能最长阻塞 500ms）都在临界区外调用，不影响 ISR 延迟；临界区采用真实存在的 `pal_irq_save_rtos_safe/pal_irq_restore` 原语。
- **不做跨 helper 全局链表**——没有必要，也没有"一次性停所有 BAL 服务"的合法场景（见 §3.4）。
- **BAL 内部核枚举映射**：`wink_bal_core_t → pal_os_core_id_t` 转换在 BAL `.c` 内部完成，BAL 公共头不依赖 PAL 类型。
- **阻塞 API 警告抑制收敛**：整个 TU 用 `WINK_INTERNAL_BLOCKING_REGION_BEGIN/END` 包裹（替代手写 GCC pragma），同时兼容 MSVC C4996。
- **0 实例 stub 约定**：控制/状态类 API 挂 `WINK_UNAVAILABLE_MSG` 编译报错，`stop` 静默 no-op（见 §3.3.2 stub 块注释）。

#### 3.3.3 `set_period` 运行期动态调整（零停摆）

已决 **Q2 = 方案 B**：runtime 层直接实现 `wink_periodic_change_period()`，下个周期生效，零停摆、零丢拍（对伺服电机/PID 闭环等场景是硬要求）。该 API **当前不存在**，需要在阶段 1 分三个子任务实现：

- **1.4a LIGHT 侧（soft_timer 路径）**：给 `wink_soft_timer` 加 `wink_soft_timer_change_period(h, new_ticks)`，原子更新 timer 的 period 字段；下个 tick dispatch 时即以新频率触发。BAL helper 的 blink（half-period toggle）/ button poll 等 LIGHT 类 helper 经此路径生效（注意 blink 还要同步更新它自己的 half-period 缓存）。
- **1.4b MAY_BLOCK 侧（独立 task 路径）**：
  - 给 runtime 内部的 periodic task 控制块加 `period_ms` 原子字段 + `wakeup_generation` 计数；
  - task 主循环里用 `xTaskDelayUntil(&last_wake, period_ms)` 或 `pal_os_sem_take(..., timeout=period_ms)` 休眠前读最新 period；
  - ESP32 下**必须配合 `xTaskAbortDelay()`** 才能让"长周期改短周期"立即生效（否则从 10s 改 100ms 必须等完 10s，违反"零停摆"承诺）；sim/host target 下 fiber 休眠机制同样需要加类似打断点（检查 generation 标志）。
- **1.4c 统一入口**：在 `wink_periodic_start_ex` 之上封装 `wink_periodic_change_period(h, period_ms)`，根据 h 对应的执行路径分派到 1.4a/1.4b；对非法 h 返回 `WINK_ERR_INVALID_ARG`。

```c
/* runtime/include/wink_tasks.h 新增 */
/**
 * @brief 无效 periodic 句柄标记（Stage 1 新增常量）。
 * @note  历史约定为"负值即无效"（见旧版注释"Negative = invalid"），本方案统一为
 *        具名常量 (-1)，所有 slot/句柄初始化和比较都必须使用该宏，禁止裸写 -1/0。 */
#define WINK_PERIODIC_INVALID ((wink_periodic_handle_t)-1)

/**
 * @brief 运行期动态修改 periodic 的周期（下个周期生效，零停摆）。
 * @param h         periodic 句柄
 * @param period_ms 新周期（同 _start_ex 语义：LIGHT 路径须是 WINK_RUNTIME_TICK_MS 倍数）
 * @return WINK_OK / WINK_ERR_INVALID_ARG(h 非法)
 * @note 线程安全：内部原子更新 next_wake/period 字段。
 *       若当前周期回调已经在执行，本次修改从下一个完整周期开始生效。
 *       LIGHT 路径下个 tick 立即以新频率生效；MAY_BLOCK 路径使用
 *       xTaskAbortDelay / fiber-wake 打断当前休眠，保证新周期尽快生效。
 *
 *       **Self set_period 重入语义**：在 LIGHT/MAY_BLOCK 回调内部对**自身**句柄
 *       调用 wink_periodic_change_period 是安全的。LIGHT 侧原子写入 period 字段，
 *       当前 callback 返回后下一个 tick 即按新频率派发；MAY_BLOCK 侧在 task 主循环
 *       顶部读 period，当前迭代完成后即按新周期休眠。不允许从回调内对**其他**
 *       periodic 句柄做跨 helper 的 set_period（这属于业务层状态机职责）。
 */
wink_status_t wink_periodic_change_period(wink_periodic_handle_t h, uint32_t period_ms);
```

> **专业排雷预警：MAY_BLOCK 路径的休眠打断**
> 如果 `MAY_BLOCK` 底层是 FreeRTOS `vTaskDelayUntil` 等，仅仅修改内部控制块的 `period_ms` **无法立即唤醒正在休眠的任务**——例如从 10s 突然改为 100ms，任务必须等完这 10s 才会进入新周期。本方案要求 ESP32 target 使用 `xTaskAbortDelay()` 配合原子 generation 标志实现即时生效，sim/host fiber 路径同样需要显式检查。这是 Q2 "零停摆"承诺的硬要求，不能退化为"stop+restart"。

BAL helper 内的 set_period 直接调用：

```c
wink_status_t wink_sonar_helper_set_period(dal_ultrasonic_t *dev, uint32_t period_ms) {
    if (!dev || period_ms < WINK_SONAR_HELPER_MIN_PERIOD_MS) return WINK_ERR_INVALID_ARG;
    /* 临界区：使用 pal_irq_save_rtos_safe / pal_irq_restore，与 start_ex/stop 保持一致 */
    uint32_t mask = pal_irq_save_rtos_safe();
    sonar_slot_t *slot = NULL;
    for (uint32_t i = 0; i < WINK_APP_MAX_ULTRASONIC_INSTANCES; i++) {
        if (s_slots[i].dev == dev) { slot = &s_slots[i]; break; }
    }
    if (!slot) { pal_irq_restore(mask); return WINK_ERR_NOT_FOUND; }
    if (slot->state != WINK_BAL_SLOT_RUNNING) {
        pal_irq_restore(mask);
        return WINK_ERR_INVALID_STATE;   /* 尚未 start 或正在 start/stop 中 */
    }
    slot->period_ms = period_ms;
    wink_periodic_handle_t h = slot->periodic_h;   /* 在 CS 内快照句柄 */
    pal_irq_restore(mask);

    return wink_periodic_change_period(h, period_ms);   /* 零停摆；CS 外调用以保持"临界区只护元数据"原则 */
}
```

---

### 支柱 4：诚实的 Fault 三阶段（Phase）模型（**替代原方案的"fault 自动 stop 所有服务"**）

#### 3.4.1 为什么"fault 时自动 stop 所有 BAL 服务"是伪需求

1. **Fault 上下文不能 blocking**：`wink_periodic_stop` 等 sem 最长 500ms，期间系统卡住；如果 fault 本身是某个 task 死锁/栈溢出引起的，等 sem 永远超时。
2. **硬件复位是最终兜底**：真正致命 fault（HardFault/WDT/Panic）根本走不到软件链表遍历那一步，CPU 直接复位，所有资源清零。板级电路（引脚 Hi-Z、执行器使能脚默认关断、电源门控）是硬安全态，软件只补软闭环（这是 `wink_actuator_registry` 设计里已经写清楚的哲学，见 `wink_actuator_registry.h:12-13`）。
3. **Stop 顺序有依赖**：如果停 OLED helper 之前就停 I2C bus helper，OLED deinit 会总线错误。通用链表无法表达依赖顺序。

#### 3.4.2 Fault 三阶段（Phase）模型

> **术语约定**：工程交付里程碑用"阶段 -1 / 0 / 1 / 2 / 3 / 4 / 5"（见 §7），Fault 处理用"Phase 1 / 2 / 3"，避免同一词"阶段"过载引起混淆。

```
Fault 触发 (wink_runtime_raise_fault / HardFault / WDT NMI)
       │
       ▼
┌──────────────────────────────────────────────────────────┐
│ Phase 1：fault-detect 上下文（非阻塞，必须 ≤ 100µs）      │
│   - 允许使用 ISR-safe 的 SDK API（例如 gpio_set_level）  │
│   - wink_trace_fault(code)  记录故障码                   │
│   - wink_actuator_safe_off_all()  关断所有执行器        │
│     （现有机制：静态表遍历，回调必须非阻塞，失败继续）    │
│   - 绝对禁止：stop 任务、等 sem、动态分配、printf         │
│   - 注意：此时运行在普通 task 或 ISR 上下文，非 CPU panic/ │
│     HardFault/NMI 异常中断向量上下文。                   │
└──────────────────────────────────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────────────────────────┐
│ Phase 2：Fault task 上下文（可短阻塞，≤ 500ms）           │
│   - 调 app_on_fault(code) 回调                           │
│   - 应用层在这里做：LED 闪故障码、日志上报、尝试停BAL服务 │
│   - Runtime 不自动 stop 任何 BAL 服务                    │
│   - 如果 app_on_fault 内需要停服务，必须显式调用          │
│     wink_xxx_helper_stop(&dev)；Runtime 不代劳           │
└──────────────────────────────────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────────────────────────┐
│ Phase 3：决策点                                           │
│   - 如果 on_fault 内用户调了 wink_runtime_recover() →    │
│     回到主循环继续运行（可恢复故障）                      │
│   - 否则：让 WDT 到期 → 硬件复位 → boot safe-lock 逻辑   │
│     (ADR-0010: 异常启动计数 ≥3 进 boot lockout)          │
└──────────────────────────────────────────────────────────┘
```

**Init 失败的回滚契约（应用层责任）**：
若 `app_init_status()` 返回非 WINK_OK（包括 WINK_TRY 触发的 fault 跳转），runtime 会执行 Phase 1 的 `safe-off_all()`，但**不**自动 stop 已启动的 BAL 服务——遵循"谁启动、谁回滚"原则。应用若需要在 init 失败路径释放已 start 的 helper，应在返回错误码前显式调用对应 `_stop`；否则直接返回错误码让 Phase 3 的 WDT 复位兜底，板级安全电路把所有引脚置 Hi-Z。这样的设计避免 runtime 猜测依赖顺序（参考 §3.4.1 第 3 条）。

**这意味着：**
- ✅ actuator safe-off（把电机停了/LED 关了）是 runtime 的责任，fault 立即做。
- ❌ BAL 服务生命周期（停后台 task、删定时器）**不是** runtime fault 路径的责任。
- ✅ 如果应用想在 fault 时优雅停服务，在 `app_on_fault` 里自己 stop（此时在 fault task 上下文，阻塞是允许的）。
- ✅ 对初学者场景，`app_on_fault` 留空即可——WDT 硬件复位会把一切清零，板级安全电路兜底。
- ✅ BAL helper 不需要全局注册到任何 runtime 链表——它们只是各自的 slot 池。

> **专业排雷预警：WDT 脏复位与总线挂死**
> 绝大多数 MCU（如 ESP32）的看门狗复位是系统级软复位，**通常不会复位外部器件的状态**。如果系统在读取 I2C 传感器时发生死锁并触发 WDT，复位后 I2C 从设备可能仍拉低 SDA。必须在拓扑序的 `wink_device_tree_init()` 阶段或 DAL Init 中加入 **总线恢复逻辑（Bus Recovery）**（例如 I2C 发现 SDA 为低时，主机手动 toggle SCL 9 个时钟周期释放总线），以应对 WDT 脏复位带来的状态不同步，防止系统无限重启。
>
> **异常启动与锁死前置判断**：系统在极早期（Early Boot，执行 `wink_device_tree_init()` 之前）必须先读取 WDT 复位标志并校验 boot 计数器。若已经达到 ADR-0010 的锁死阈值，必须立刻进入 Safe-lock 挂起状态，**禁止执行任何 DAL 驱动初始化或总线恢复操作**，防止物理故障导致初始化死锁或多次损坏硬件。

#### 3.4.3 低功耗场景的 deinit（专家模式，非 fault 路径）

对于用户主动进入低功耗（非 fault）的场景：

```c
/* app_callbacks.c — 专家场景 */
void enter_deep_sleep_10s(void) {
    /* 1. 显式停所有 BAL 服务 */
    wink_sonar_helper_stop(&smoke_sonar);
    wink_led_blink_stop(&board_led);
    /* telemetry_helper_stop（wink_default_telemetry 升级后提供 stop） */

    /* 2. 逆序 deinit 硬件树（codegen 生成的 deinit 会做 gpio_reset_pin 等） */
    wink_device_tree_deinit();

    /* 3. PAL 进入 light sleep（待实现；PAL 层扩展，非本方案范围） */
    /* pal_enter_light_sleep(10000); */

    /* 4. 唤醒后：wink_device_tree_init() + BAL 重启即可恢复所有状态。
     *    前提：所有 DAL _deinit 必须彻底清场（见 §3.4.4）。 */
}
```

> **Q3 已决：必须生成**。这不仅服务低功耗场景，软重启（不引起外部硬件抖动）也依赖拓扑逆序 deinit。

#### 3.4.4 DAL `_deinit` 质量铁律（唤醒后状态一致性的前提）

低功耗唤醒（RAM 保持）后通过 `deinit → init` 恢复硬件状态，**要求 deinit 必须彻底清场**，不能留下任何"未来得及复位的硬件状态"，否则唤醒后外设会以奇怪的中间态工作。

> **现状核查（2026-07-06 代码走查结果）**：
> - `dal_led_deinit` / `dal_button_deinit` / `dal_ultrasonic_deinit` 三个已存在，但**只调用 `pal_resource_release()`**（纯软件 owner 字符串表，见 `pal_resource_esp32.c:78-98`），**没有** `gpio_reset_pin`、没有停 PWM/RMT 外设、没有断 GPIO 路由、没有 ISR 注销，属于"伪 deinit"；
> - `dal_servo_deinit` / `dal_ssd1306_deinit` / `dal_eeprom_deinit` / `dal_gps_deinit` 完全**不存在**，header 里也没有声明；
> - 因此阶段 0 的工作量不是"补几个缺失的 deinit"，而是**3 个重写 + 4 个新建**，详见 §7。

每个 DAL 驱动的 `dal_xxx_deinit()` **必须**满足以下清场检查单：

| 检查项 | 说明 | 对应 ESP32 铁律 |
|---|---|---|
| 硬件停止 | 停 PWM/RMT/I2C 等外设输出，执行器回 safe-off 或 Hi-Z | `ledc_stop` / `rmt_rx_stop` / `rmt_tx_stop` / `i2c_driver_delete` |
| **GPIO reservation 撤销（硬要求）** | **必须**调用 `gpio_reset_pin(pin)` 撤销 `esp_gpio_reserve` 位图、断开 GPIO 矩阵路由、复位 pad 为默认 Hi-Z 输入态；**仅调 `pal_resource_release()` 是不够的**——那只是软件字符串表，不触碰 IDF 层 | [[memory:esp32-idf-gpio-reset-pattern]]：漏调将导致"重复 init 报 GPIO 占用"或"重启后引脚状态粘连"等玄学 bug |
| 中断注销 | 注销 ISR handler、禁用外设中断源、释放中断分配资源。<br>**顺序**：先关外设中断源 → 再 `gpio_isr_handler_remove` → 最后关外设时钟，防中断风暴 | `gpio_isr_handler_remove` + 驱动专属 interrupt disable |
| DMA/描述符清理 | 释放 DMA 描述符链表、reset 接收 FIFO、清 pending 中断标志。<br>停 DMA 时必须**等 burst 完成或硬 reset 通道**，防总线挂死（WDT 复位后 I2S/RMT 可能遗留半帧状态） | RMT/I2S/ADC 驱动专属 |
| I2C 总线恢复（WDT 脏复位） | I2C 驱动 deinit 前若检测到 SDA 被从设备拉低（WDT 复位后从设备状态未清），**手动 toggle SCL 9 个时钟**释放总线，避免 WDT 复位后 I2C 永久挂死（见 §3.4.2 WDT 脏复位预警） | `i2c_master_clear_bus()` 或手动 GPIO toggle |
| **共享 Bus 所有权** | I2C/SPI 等可挂多器件的共享总线，**bus 本身**的 init/deinit 由 codegen 生成的 **bus-owner 静态节点**统一管理（拓扑序保证 bus 先于 client init、逆序晚于 client deinit）；单个器件 DAL `_deinit` 只清理该 client 的软件状态，**不得**调用 `i2c_driver_delete`/`spi_bus_free` 等销毁 bus 的操作，避免 double-free。stage 0 必须先补 bus-owner 抽象（ssd1306 + eeprom 共享 I2C 是首个触发场景） | codegen 在 device_tree.c 里生成 bus 节点 + 按拓扑序 init/deinit；单器件 deinit 只做 client 级清理 |
| 软件态复位 | 把 `dal_xxx_t` 实例恢复到 init 前状态（如 `dev->initialized = false`、清 config 副本指针、清 buffer 计数），不保留任何运行期状态 | 保证 re-init 行为与冷启动一致；init→deinit→init 幂等单测可验证 |
| 幂等 | 调用多次 deinit 必须安全；对 NULL dev 返回 `WINK_ERR_INVALID_ARG`；对未 init（`initialized==false`）实例返回 `WINK_OK`（no-op） | 类似 `free(NULL)` 的容错语义；fault 路径链式调用时不能因单设备状态异常阻断其他设备清理 |
| 不阻塞 | deinit 不得等待信号量超过 50ms；快速路径优先，必要时用强制 abort（如 `rmt_rx_stop` 不等 DMA 完成直接 reset）而非优雅等待 | 满足 fault 路径潜在调用需求 |
| 签名统一 | `wink_status_t dal_xxx_deinit(dal_xxx_t *dev);`（返回状态，非 void）；`wink_device_tree_deinit()` 里用 `WINK_IGNORE_RESULT` 链式 best-effort 调用（当前 codegen 模板已是此模式，保持） | 便于 host 单测精确断言每个 deinit 返回值 |
| **Leak 检测（Debug）** | **必须**在 `wink_device_tree_deinit()` 入口处检查是否存在未停止的周期性服务。 | `WINK_PT_DEBUG` 构建下断言 `wink_periodic_active_count() == 0`，避免耦合特定 BAL Helper 类型的同时确保清场干净。 |

> **关键后果**：deinit 做不干净 → 低功耗唤醒后出现"DMA 搬了旧数据"、"GPIO 路由冲突报 err"、"引脚被 reserve 住 init 失败"、"WDT 复位后 I2C 从设备拉死 SDA"等难以复现的玄学 bug。这是本重构的前置硬依赖，**阶段 0 必须全部 7 个驱动走完检查单并通过 host init→deinit→init 幂等单测**，不能边写 BAL 边补。

---

### 支柱 5：阻塞 API pragma 诚实化（**不追求 0-pragma，追求位置最小化与语义诚实**）

#### 3.5.1 诊断结论

当前 `app_callbacks.c` 的 file-scope pragma 是粗粒度的"关掉所有 deprecated warning"，这才是问题——不是 pragma 本身。应用层调用 blocking API 有三类来源：

| 来源 | 是否合法 | 处理方式 |
|---|---|---|
| BAL helper 内部的 task 函数（如 `sonar_periodic_task`）调用 `dal_ultrasonic_request_measurement` | ✅ 合法（MAY_BLOCK 路径） | pragma 写在 `bal/src/.../*.c` 文件顶部，应用层看不到 |
| `app_init` 里的 selftest / 一次性诊断（如 `wink_selftest_run`） | ✅ 合法（init 是同步启动阶段，不是 PT 上下文） | push/pop 缩小到**调用点所在函数**或**调用语句块**，配合注释标注 "ADR-0017 init-phase exception" |
| 应用层直接调 `pal_os_task_create` / `pal_os_sleep_ms` | ❌ 不合法（应该用 BAL/wink_periodic） | 不允许；迁移后应用层不应出现这种调用 |

#### 3.5.2 pragma 规范模板

**（A）BAL `.c` 文件（整个 TU 是 helper 实现，已知使用 blocking API）**

BAL `.c` 用 `WINK_INTERNAL_BLOCKING_REGION_BEGIN/END` 包裹**整个 include 之后的实现段**（不是裸写 #pragma，以兼容 MSVC C4996）。宏放在 `#include` 之后，避免头文件内的声明被意外包裹：

```c
/* bal/src/sensor/wink_sonar_helper.c */
#define LOG_TAG "bal.sonar"

#include "wink_sonar_helper.h"
#include "wink_periodic.h"
#include "wink_blocking_region.h"
#include "pal_irq.h"
#include "pal_log.h"
#include "pal_osal.h"        /* BAL .c 内部可以 include PAL；只有 BAL 公共头不可以 */
#include "device_tree.h"

/* ADR-0017 BAL-exception: helper 内部通过 wink_periodic MAY_BLOCK 路径调用
 * WINK_BLOCKING API（dal_ultrasonic_request_measurement）。抑制宏收敛在此 TU 内，
 * 应用层 include BAL 头文件不会看到 deprecated 声明警告。
 * 注意：宏在所有 #include 之后展开，防止抑制泄漏到 PAL/DAL 头内部。 */
WINK_INTERNAL_BLOCKING_REGION_BEGIN

/* ... 实际 helper 实现 ... */

WINK_INTERNAL_BLOCKING_REGION_END
```

**（B）app_init 里一次性阻塞调用（push/pop 最小范围）**

```c
static void app_init(void) {
    WINK_CHECK(wink_device_tree_init(), WINK_FAULT_APP(0));

    /* ... BAL 启动（非阻塞，无 pragma）... */
    WINK_CHECK(wink_sonar_helper_start(&smoke_sonar, 500), WINK_FAULT_APP(1));
    WINK_IGNORE_RESULT(wink_led_blink_start(&board_led, 1000));

    /* ADR-0017 init-phase exception: selftest 在同步启动阶段运行，不在
     * cooperative PT 上下文，允许阻塞调用（内部含 I2C scan / RMT wait）。
     * 用 WINK_INIT_BLOCKING_REGION 宏（GCC/Clang/MSVC 三编译器兼容）包裹。 */
    WINK_INIT_BLOCKING_REGION_BEGIN
    wink_selftest_result_t results[8];
    size_t n = 0;
    WINK_IGNORE_RESULT(wink_selftest_run("*", results, 8, &n));
    WINK_INIT_BLOCKING_REGION_END
    /* ...后续非阻塞代码... */
}
```

**（C）应用层业务回调 / app_loop / app_on_fault 里：禁止任何 pragma**
- 这些位置运行在 PT 或 tick 上下文，调用 blocking API 是 bug，pragma 遮住会把真 bug 压下去。
- Code review / CI 规则：app_callbacks.c 里如果出现 `-Wdeprecated-declarations` pragma，必须有配套注释说明是 init/on_fault 等允许阶段，否则拒绝合入。

#### 3.5.3 阻塞区域宏（MSVC 兼容 + 语义清晰）

当前 `WINK_BLOCKING` 在 MSVC 上映射到 `__declspec(deprecated(msg))`，会产生 C4996 警告而非 GCC 的 `-Wdeprecated-declarations`。原 file-scope pragma 只处理 GCC/Clang，MSVC 下 C4996 会漏出。

**Q4 已决**：宏放 `runtime/include/wink_blocking_region.h`（保持 `wink_status.h` 洁癖）。提供两个语义明确的宏：

```c
/* runtime/include/wink_blocking_region.h */
#ifndef WINK_BLOCKING_REGION_H
#define WINK_BLOCKING_REGION_H

/* ── BAL/Runtime 内部使用：文件级抑制（BAL .c 顶部用） ─────── */
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

/* ── 应用层使用：app_init 里的 init-phase exception 小块 ───── */
#define WINK_INIT_BLOCKING_REGION_BEGIN  WINK_INTERNAL_BLOCKING_REGION_BEGIN
#define WINK_INIT_BLOCKING_REGION_END    WINK_INTERNAL_BLOCKING_REGION_END

#endif /* WINK_BLOCKING_REGION_H */
```

**用法边界**：
- `WINK_INTERNAL_BLOCKING_REGION_BEGIN/END`：只在 BAL `.c` / runtime `.c` 内部文件级使用，应用层不应直接用；
- `WINK_INIT_BLOCKING_REGION_BEGIN/END`：应用层 `app_init` / `app_on_fault`（fault task 上下文允许阻塞）里包裹一次性阻塞诊断（selftest）；
- **应用层业务回调（on_xxx）/ `app_loop` 里禁止使用这两个宏**——这些位置跑在 PT/tick 上下文，出现 deprecated warning 是 bug 而不是 exception。

---

## 4. 完整示例

### 4.1 初学者/AI 生成场景（最简模式）

```c
/**
 * @file app_callbacks.c — 初学者/AI 生成版
 * 功能：按键长按打日志、LED 1Hz 闪烁、超声波 500ms 周期测量、默认遥测
 * 代码行数：~50 行，零手写 pragma，零 RTOS 参数，零 pal_* 头
 */
#define LOG_TAG "smoke_app"

#include "device_tree.h"             /* 生成的：board_led, boot_button, smoke_sonar 句柄 + WINK_APP_MAX_* 宏 + 配置宏 */
#include "wink_app.h"
#include "wink_fault.h"              /* WINK_TRY / WINK_FAULT_APP */
#include "wink_led_blink_helper.h"   /* BAL：LED 周期闪烁 */
#include "wink_button_helper.h"     /* BAL：按键软定时器轮询 */
#include "wink_sonar_helper.h"      /* BAL：超声波周期测量 */
#include "wink_telemetry_helper.h"  /* BAL：默认遥测 */
#include "wink_blocking_region.h"   /* WINK_INIT_BLOCKING_REGION */
#include "wink_selftest.h"
#include "pal_log.h"

/* ── 业务回调 ─────────────────────────────────────────────────── */
static void on_boot_button(dal_button_event_t evt, void *ctx) {
    (void)ctx;
    if (evt == DAL_BUTTON_EVT_LONG_PRESS) {
        LOG_I("Long press detected!");
    }
}

/* ── 生命周期回调 ─────────────────────────────────────────────── */
static void app_on_boot(const wink_boot_info_t *info) {
    LOG_I("Boot OK, reset_reason=%d", (int)info->reset_reason);
}

/* 使用 init_status（wink_status_t 返回）新签名——失败直接 return 错误码，
 * runtime 会自动执行 Phase 1 safe-off 并进入 fault 流程，比 void-init + WINK_CHECK longjmp 更干净。
 * 注意：runtime 不会自动 stop 已启动的 BAL 服务（"谁启动、谁回滚"契约，见 §3.4.2）；
 * 初学者 app_on_fault 返回 WINK_ERR_LOCKED 让 WDT 硬件复位兜底即可，无需手动回滚。 */
static wink_status_t app_init_status(void) {
    /* 1. 初始化硬件树（阻塞允许，这是 init 阶段）。device_tree_init 自身内部
     *    不会调 WINK_BLOCKING 的 runtime-light API（它是同步一次性硬件 init），
     *    不需要 pragma 包裹。 */
    WINK_TRY(wink_device_tree_init());

    /* 2. 绑定事件 */
    WINK_TRY(dal_button_on_event(&boot_button, on_boot_button, NULL));

    /* 3. 启动 BAL 服务（全部用默认参数，零 RTOS 细节）。
     *    注意：即使是 button auto_poll 这种"基础设施"也由 C 代码显式启动——
     *    codegen 只生成 BOOT_BUTTON_AUTO_POLL_MS 常量宏，所有运行期行为
     *    都在 app_init 里可见，贯彻"JSON 只描述静态世界"原则。 */
    WINK_TRY(wink_led_blink_start(&board_led, 1000));                         /* 1Hz */
    WINK_TRY(wink_button_helper_start(&boot_button, BOOT_BUTTON_AUTO_POLL_MS)); /* 10ms 轮询 */
    WINK_TRY(wink_sonar_helper_start(&smoke_sonar, 500));                     /* 500ms 测距 */
    WINK_TRY(wink_telemetry_default_start(&smoke_sonar, &boot_button));

    /* 4. 可选：selftest（一次性诊断，init 阶段阻塞合法）——用 WINK_INIT_BLOCKING_REGION 包裹 */
    WINK_INIT_BLOCKING_REGION_BEGIN
    wink_selftest_result_t results[8]; size_t n = 0;
    WINK_IGNORE_RESULT(wink_selftest_run("*", results, 8, &n));
    WINK_INIT_BLOCKING_REGION_END

    LOG_I("App init done.");
    return WINK_OK;
}

static void app_loop(void) {
    /* 空 loop——所有周期工作都由 BAL 后台服务处理。
     * 保留空函数以维持 runtime heartbeat（可放低优先级任务如喂狗）。*/
}

/* 使用 on_fault_status 新签名：返回 WINK_OK 表示"已恢复"、WINK_ERR_LOCKED 表示交给 WDT。
 * 初学者返回 WINK_ERR_LOCKED 即可（WDT 复位兜底）。 */
static wink_status_t app_on_fault_status(uint32_t code) {
    (void)code;
    /* 初学者无需做任何处理：
     * - 阶段 1 runtime 已经做过 actuator safe-off_all()
     * - 返回 LOCKED 让 WDT 硬件复位清理一切，板级电路兜底
     */
    return WINK_ERR_LOCKED;
}

/* ── Callback 工厂（binary decoupling） ──────────────────────── */
const wink_app_callbacks_t *wink_app_get_callbacks(void) {
    static const wink_app_callbacks_t cb = {
        .init_status     = app_init_status,     /* ✅ 新签名：返回 wink_status_t */
        .loop            = app_loop,
        .on_fault_status = app_on_fault_status, /* ✅ 新签名：返回是否恢复 */
        .on_boot         = app_on_boot,
    };
    return &cb;
}
```

**对初学者/AI 的友好点：**
- 没有任何 `stack/priority/core_id` 参数；
- 除 selftest 外零 pragma（selftest 那块用明确语义的 `WINK_INIT_BLOCKING_REGION` 宏，初学者通常不需要写 selftest）；
- 所有 BAL 调用返回 `wink_status_t`，错误时 `WINK_CHECK` 直接 fault，不用 if 分支；
- 设备实例名（`board_led`/`boot_button`/`smoke_sonar`）直接来自 JSON 的 key；
- 周期参数（如 `BOOT_BUTTON_AUTO_POLL_MS`）由 codegen 从 JSON 生成宏，AI 不用去记魔法数字；
- 所有运行期启动行为集中在 `app_init` 里可见可查，没有"潜伏"在 codegen 里的启动动作。

### 4.2 专家场景（动态控制 + 低功耗 + 故障恢复）

```c
/**
 * @file app_callbacks.c — 专家版：看门狗恢复模式 + 按键动态调频 + 休眠
 *
 * 注意：即使专家模式仍然不 include 任何 pal_*.h；核亲和优先级通过
 * BAL 自有的 WINK_HELPER_OPTS / wink_bal_core_t 指定，保持分层边界。
 */
#define LOG_TAG "expert_app"

#include "device_tree.h"
#include "wink_app.h"
#include "wink_runtime.h"
#include "wink_fault.h"            /* WINK_TRY / WINK_FAULT_APP */
#include "wink_helper_opts.h"     /* WINK_HELPER_OPTS / WINK_BAL_CORE_1 */
#include "wink_led_blink_helper.h"
#include "wink_button_helper.h"
#include "wink_sonar_helper.h"
#include "wink_telemetry_helper.h"
#include "wink_blocking_region.h" /* WINK_INIT_BLOCKING_REGION */
#include "wink_selftest.h"
#include "pal_log.h"              /* 唯一直接 include 的 PAL 头：LOG 宏，不引入 OSAL/HAL 类型 */

static bool g_low_power_mode = false;
static wink_reset_reason_t g_reset_reason = WINK_RESET_REASON_UNKNOWN;

/* ── 业务回调 ─────────────────────────────────────────────────── */
static void on_boot_button(dal_button_event_t evt, void *ctx) {
    (void)ctx;
    switch (evt) {
    case DAL_BUTTON_EVT_CLICK: {
        /* 专家操作：动态提升超声波采集频率（从 500ms → 100ms）。
         * 常规场景（只改周期）优先用 set_period 零停摆切换；
         * 若需要同时改优先级/钉核（如本例 pin 到 CORE_1 + prio=7），
         * 则必须走 stop + _start_ex 完整重启（period/prio/core 属进程属性，
         * 周期内改需要重建 task，不提供"热迁移"API 以避免复杂度爆炸）。 */
        static bool fast_mode = false;
        fast_mode = !fast_mode;
        if (fast_mode) {
            /* 配置升级：需钉核 + 升优先级 → 完整重启（零停摆承诺仅适用于同 task 内改周期） */
            wink_sonar_helper_stop(&smoke_sonar);
            WINK_IGNORE_RESULT(wink_sonar_helper_start_ex(
                &smoke_sonar, 100,
                &WINK_HELPER_OPTS(4096, 7, WINK_BAL_CORE_1)));
            LOG_I("Sonar FAST mode (100ms, prio=7, core=1)");
        } else {
            /* 降回常规：仅改周期即可，set_period 零停摆；
             * 因为快模式时我们 pin 到了 CORE_1，想回 ANY 也必须重启——所以这里走 stop+start。
             * 若只是等频内改周期，直接调 set_period 即可，例如：
             *   WINK_TRY(wink_sonar_helper_set_period(&smoke_sonar, 200)); */
            wink_sonar_helper_stop(&smoke_sonar);
            WINK_IGNORE_RESULT(wink_sonar_helper_start(&smoke_sonar, 500));
            LOG_I("Sonar NORMAL mode (500ms, defaults)");
        }
        break;
    }
    case DAL_BUTTON_EVT_LONG_PRESS:
        LOG_I("Long press → entering sleep in 1s");
        g_low_power_mode = true;  /* 通知 app_loop 触发休眠 */
        break;
    default: break;
    }
}

/* ── 生命周期回调 ─────────────────────────────────────────────── */
static void app_on_boot(const wink_boot_info_t *info) {
    g_reset_reason = info->reset_reason;   /* 缓存 boot 信息给 init_status 做分支启动 */
    if (info->abnormal_boot_count > 0) {
        LOG_W("Recovered from abnormal boot, count=%lu",
              (unsigned long)info->abnormal_boot_count);
    }
}

static wink_status_t app_init_status(void) {
    WINK_TRY(wink_device_tree_init());
    WINK_TRY(dal_button_on_event(&boot_button, on_boot_button, NULL));
    WINK_TRY(wink_button_helper_start(&boot_button, BOOT_BUTTON_AUTO_POLL_MS));

    /* 专家决策：根据复位原因分支启动（boot_info 通过 on_boot 缓存到 g_reset_reason，
     * 无需引入 wink_runtime_get_boot_info()——on_boot 是 runtime 已提供的唯一入口）。 */
    if (g_reset_reason == WINK_RESET_REASON_WATCHDOG) {
        /* WDT 恢复：先不启超声波，LED 快闪告警，让人工干预 */
        WINK_TRY(wink_led_blink_start(&board_led, 100));
        LOG_W("WDT recovery: sensors disabled, LED warning blink");
    } else {
        /* 正常上电：满服务启动 */
        WINK_TRY(wink_led_blink_start(&board_led, 1000));
        WINK_TRY(wink_sonar_helper_start(&smoke_sonar, 500));
        WINK_TRY(wink_telemetry_default_start(&smoke_sonar, &boot_button));
    }

    /* selftest（init 阶段阻塞合法，用 WINK_INIT_BLOCKING_REGION 包裹） */
    WINK_INIT_BLOCKING_REGION_BEGIN
    wink_selftest_result_t results[8]; size_t n = 0;
    WINK_IGNORE_RESULT(wink_selftest_run("*", results, 8, &n));
    for (size_t i = 0; i < n; i++) {
        if (results[i].status != WINK_OK && results[i].status != WINK_ERR_UNSUPPORTED) {
            LOG_E("%s: FAIL metric=%lu", results[i].name, (unsigned long)results[i].metric);
        }
    }
    WINK_INIT_BLOCKING_REGION_END
    return WINK_OK;
}

static void app_loop(void) {
    if (g_low_power_mode) {
        g_low_power_mode = false;
        /* 低功耗流程：停服务 → deinit 硬件树 → 休眠。
         * 注意：wink_device_tree_deinit() 返回 void，best-effort 链式清理，
         * 单个设备 deinit 失败不会阻断其他设备（fault 容错语义）。 */
        LOG_I("Stopping services before sleep...");
        wink_sonar_helper_stop(&smoke_sonar);
        wink_led_blink_stop(&board_led);     /* 按设备指针停（Q7 决策），隐藏内部 handle */
        wink_button_helper_stop(&boot_button);
        wink_telemetry_default_stop();
        wink_device_tree_deinit();
        /* pal_enter_light_sleep(10000); */  /* TODO: PAL 层扩展后启用 */
        LOG_I("Wake from sleep, re-init...");
        /* 简单做法：WDT 软复位，让 boot 流程重新跑；
         * 高级做法：手动调 wink_device_tree_init() + 重启 BAL 服务（依赖阶段 0 deinit 质量）。 */
        wink_runtime_trigger_wdt_test(100);
    }
}

static wink_status_t app_on_fault_status(uint32_t code) {
    /* 专家 fault 处理：记录日志、尝试优雅停特定服务。
     * 注意：runtime 阶段 1 已经 actuator_safe_off_all() 过了，执行器已安全。*/
    LOG_E("Fault code=%lu", (unsigned long)code);
    /* 谨慎停 telemetry（避免 fault 风暴），但保留 sonar 以便 WDT 复位前的最后读数 */
    wink_telemetry_default_stop();
    /* 不调其他 stop：返回 LOCKED 让 WDT 硬件复位兜底（阶段 3） */
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

**专家自由度：**
- 可以动态 stop/start 服务并切换参数（栈/优先级/核）；
- 可以根据 `reset_reason` 做条件启动；
- 可以在 loop 里做状态机（如低功耗进入条件）；
- 可以在 on_fault 里做差异化资源清理；
- 仍然没有直接 include `pal_osal.h` 或 `pal_hal.h`，保持了 BAL 分层。

---

## 5. 跨 Target 兼容性（Wasm 仿真）

### 5.1 必须保证的同源编译

| 问题 | 在 sim target (Wasm/fiber) 下的语义 | 方案处理 |
|---|---|---|
| `stack_bytes` 参数 | fiber 调度下，MAY_BLOCK task 仍有独立 fiber 栈，stack_hint 生效 | 复用现有 `targets/wasm/pal_osal_wasm.c` 的 fiber 创建逻辑，BAL 不需要特殊处理 |
| `priority` 参数 | sim 是协作式单虚拟核（ADR-0013/0014），优先级被忽略 | BAL 默认值设高不影响 sim；在 BAL opts 头文档里注明"sim 下优先级无效" |
| `core_id` 参数 | sim 无多核概念，`WINK_BAL_CORE_ANY` 是唯一合法值 | BAL 内部 `map_core()` 在 sim target 下将 CORE_0/1 映射为 ANY（不报错，因为 pin-core 在 sim 上是"尽力而为"而非"必须"）；若用户真的需要检测错误，可在 runtime 层 warning |
| LIGHT vs MAY_BLOCK | sim 下 soft_timer 和独立 fiber 都是协作调度，不会抢占，但 MAY_BLOCK 的 fiber 有独立栈 | 语义保持；BAL 默认把 I2C/RMT 类 helper 设成 MAY_BLOCK，在 sim 下自动 fiber 化；BAL helper 内部统一走 `wink_periodic_start_ex` 不直接调 `wink_soft_timer`，sim 层只需适配一处 |
| `WINK_BLOCKING` pragma | sim 下强制 `WINK_STRICT_NONBLOCKING=1`，阻塞符号从头文件消失，违反者编译期即失败 | **Q5 已决：强制开启**。simulator 的核心价值之一是 fail-fast 抓 bug，协作 fiber 里阻塞会导致整个仿真挂起，严格隔离省掉巨大排查成本 |

### 5.2 测试辅助/bringup 工具归属（`wink_sim_ultrasonic_echo` 等）

`wink_sim_ultrasonic_echo` 这类模块（S10 超声波回波仿真、GPIO 短路测试等）直接调用 `pal_os_task_create/pal_os_sem_take/pal_gpio_enable_interrupt/pal_os_busy_wait_us` 等底层 WINK_BLOCKING API，属于 **bringup/selftest 仪器**，不是业务 BAL helper。

归属原则（**已决**：统一放入 `wink-micro-os/runtime/selftest/`，不新开 `testing/` 目录，避免目录膨胀）：
- 迁移到 `wink-micro-os/runtime/selftest/`；
- 不进 BAL 目录；
- 允许文件级 `WINK_INTERNAL_BLOCKING_REGION_BEGIN/END`；
- 在 sim target 下用 `#ifndef WINK_STRICT_NONBLOCKING` 条件编译（硬件 ISR/task 创建在 sim 下本身返回 UNSUPPORTED，这些模块的主体代码在 sim 下为空 no-op）；
- 禁止被业务 BAL helper 或用户 app_callbacks.c 直接 include（只在 smoke/selftest 场景用）。

阶段 5 开启 `WINK_STRICT_NONBLOCKING=1` 前，必须先把这类 bringup 模块都迁移到合适目录并加好条件编译隔离。

### 5.3 Host 单元测试

每个 BAL helper 必须有 host target 的单元测试（Unity），覆盖：
- start/stop 幂等性
- 多实例并发（占满 slot 池返回 RESOURCE_EXHAUSTED）
- `_start_ex` 参数覆盖生效
- set_period 动态调整
- is_running 状态查询
- NULL 安全

测试目录：`wink-micro-os/bal/tests/`，模式参考 `runtime/selftest/`。

---

## 6. Codegen 变更

### 6.1 输出文件清单

| 文件 | 变更性质 | 内容 |
|---|---|---|
| `device_tree.h` | 增强 | 外部实例句柄 + `wink_device_tree_init/deinit` 声明 + **`WINK_APP_MAX_<DEV>_INSTANCES` 计数宏** + **配置常量宏**（如 `BOOT_BUTTON_AUTO_POLL_MS`，由 driver `render_config_macros()` 钩子选择性导出） |
| `device_tree.c` | 加固（已存在） | 静态实例 + init（拓扑序）+ deinit（逆拓扑序，void 返回，best-effort 链式清理）+ actuator thunk 注册/反注册 |
| `app_options.cmake` | 增强 | `WINK_USE_XXX` 选项 + **`WINK_MAX_PERIODIC` 容量自动计算**（统一走 `wink_periodic` 后，soft_timer 只需留 4 余量给 selftest/用户自定义） |
| `app_support.c` | **删除** | 当前 `wink_app_services_start()` 的唯一内容是 button auto_poll 自动启动——贯彻 Q8 后无任何存在价值。`wink_app_get_callbacks` 已经是 app 层强符号，weak stub 无实际用途；直接删除该模板并从 CMake 移除。所有服务启动在 `app_init` 里由用户显式调用（已决 Q8）。 |

### 6.2 schema 变更 + DriverBase 钩子调整

**从 JSON schema 校验中移除**（不做 deprecation 宽限，代码确认无外部 JSON 使用）：
- `services`
- `callbacks`
- `state_variables`

**DriverBase 钩子移除**（与上面三个字段一起清理）：
- `get_service_headers()`  — 删除
- `render_service_starts()` — 删除

**DriverBase 新增钩子**：
```python
def render_config_macros(self, dev_name: str, spec: dict) -> List[str]:
    """Return C macro lines like `#define <DEV>_<FIELD> <value>` to be
    emitted in device_tree.h. Only fields that the driver deems worth
    promoting to named constants should be returned (avoid bloating
    device_tree.h with every config field)."""
    return []
```
每个 driver plugin 选择性把"业务层经常引用的配置默认值"提升为命名宏（如 button 导出 `BOOT_BUTTON_AUTO_POLL_MS / BOOT_BUTTON_LONG_PRESS_MS`，led 不导出任何因为 blink 周期总是由 BAL `start()` 参数传入，ultrasonic 可导出 `SMOKE_SONAR_USE_RMT` 作为条件编译提示）。

**保留并给予新语义**：
- `auto_poll_ms` 等字段**保留**在 JSON 里（属于硬件/驱动属性），但**不再触发 codegen 自动调用 `wink_button_helper_start`**；codegen 通过 `render_config_macros()` 把它输出为 `#define BOOT_BUTTON_AUTO_POLL_MS 10` 常量宏，供用户 C 代码显式引用。

> 这条规则贯彻设计根本原则：**"JSON 只描述静态硬件世界；所有运行期启动/停止行为完全属于 C 层"**。

### 6.3 device_tree.h 新增输出

```c
/* device_tree.h — 自动生成，不要手改 */

/* ...现有 extern 实例声明不变... */
extern dal_led_t        board_led;
extern dal_button_t     boot_button;
extern dal_ultrasonic_t smoke_sonar;

wink_status_t wink_device_tree_init(void);
void            wink_device_tree_deinit(void);   /* void 返回：best-effort 链式清理，见 §3.4.4 / §6.4 */

/* ── 实例计数宏（驱动 BAL 槽位大小） ────────────────────────── */
#define WINK_APP_MAX_LED_INSTANCES         1u
#define WINK_APP_MAX_BUTTON_INSTANCES      1u
#define WINK_APP_MAX_ULTRASONIC_INSTANCES  1u
#define WINK_APP_MAX_SERVO_INSTANCES       0u
#define WINK_APP_MAX_SSD1306_INSTANCES     0u
#define WINK_APP_MAX_EEPROM_INSTANCES      0u   /* Reserved for future BAL helper expansion */
#define WINK_APP_MAX_GPS_INSTANCES         0u   /* Reserved for future BAL helper expansion */
/* 多实例场景示例（若 JSON 有 sonar_left/sonar_right 两个 ultrasonic）：
 * #define WINK_APP_MAX_ULTRASONIC_INSTANCES  2u */

/* ── 配置常量宏（把 JSON 中的驱动属性提升为命名常量） ──────── */
#define BOOT_BUTTON_AUTO_POLL_MS     10u
#define BOOT_BUTTON_LONG_PRESS_MS    3000u
#define BOARD_LED_ACTIVE_HIGH        1
#define SMOKE_SONAR_USE_RMT          1
/* 这些宏供 BAL 启动参数引用，避免 app_init 里出现魔法数字 */
```

### 6.4 device_tree.c deinit 加固（已存在，非新增）

当前 `device_tree.c.j2` 已生成 `wink_device_tree_deinit()`（符号名 `wink_device_tree_deinit`，在 `device_tree.c.j2` 的 deinit 块中），语义如下（与本方案一致，不需要重写，但需审查保证顺序/错误处理正确）：

```c
void wink_device_tree_deinit(void) {
    /* 1. 先 unregister actuator thunk（forward init 序 = reverse register 序） */
    {% for a in actuators %}
    WINK_IGNORE_RESULT(wink_actuator_unregister({{ a.name }}_safe_off, &{{ a.name }}));
    {% endfor %}
    /* 2. 再按 reverse init 序 deinit 每个设备 */
    {% for d in devices | reverse %}
    WINK_IGNORE_RESULT({{ d.deinit }}(&{{ d.name }}));
    {% endfor %}
}
```

关键约定（**与当前 codegen 模板一致，保持**）：
- 签名 `void`（不是 `wink_status_t`）——deinit 是 best-effort 链式清理，单个设备失败不能阻断其他设备清理；
- 全部用 `WINK_IGNORE_RESULT`（当前模板已是）；
- unregister actuator 必须在 deinit 之前（否则 deinit 时 safe-off 表里还有悬垂指针）；
- deinit 顺序严格 reverse init 序（被依赖方后 deinit，如 I2C bus 必须在 OLED 之后 deinit）。

**前提（已决 Q6）**：
- 所有 DAL 驱动必须补全对称 `dal_xxx_deinit()`（阶段 0）；
- deinit 必须严格满足 §3.4.4 清场检查单（gpio_reset_pin / DMA 清场 / ISR 注销 / I2C 总线恢复 / 幂等 / 不阻塞）；
- 每个 deinit 必须通过 host `init→deinit→init` 幂等单测。

### 6.5 app_options.cmake 容量计算规则

因为所有 BAL helper 统一走 `wink_periodic_start_ex`（LIGHT/MAY_BLOCK 都经过同一 handle 池，见 §3.2.6），容量计算规则简化：

```cmake
# app_options.cmake — 自动生成
set(WINK_USE_LED ON CACHE BOOL "" FORCE)
set(WINK_USE_BUTTON ON CACHE BOOL "" FORCE)
set(WINK_USE_ULTRASONIC ON CACHE BOOL "" FORCE)

# 容量自动计算（codegen 能精确知道每类设备启用了几个实例）：
#   WINK_MAX_PERIODIC = Σ(WINK_APP_MAX_<DEV>_INSTANCES) + 4
# 其中 +4 余量给 selftest / 用户自定义 soft_timer / app_loop protothread。
# soft_timer 底层由 wink_periodic LIGHT 路径承载，无需独立计数。
math(EXPR WINK_COMPUTED_PERIODIC "1 + 1 + 1 + 4")   # = 7，取整到 8
set(WINK_MAX_PERIODIC  8  CACHE STRING "" FORCE)
# 小资源 MCU 上用户可手动 -D 覆盖；默认值确保够 codegen 出的固件使用，
# 且零浪费（不多开 unused slot）。
```

> **为什么 soft_timer 不单独计数？**因为本方案要求 BAL helper 统一通过 `wink_periodic` 抽象进入调度池——LIGHT 路径底层虽然用 soft_timer，但 wink_periodic 内部可以把 LIGHT handle 池和 soft_timer 池合并（或把 LIGHT 路径实现为对 soft_timer 的 thin wrapper），避免两套池独立配额导致容量浪费。阶段 1 实现 `wink_periodic_change_period` 时顺手做这一层统一。

---

## 7. 迁移路径

> **阶段顺序原则**：阶段 -1 → 0 → 1 → 2 → 3 → 4 → 5，前一阶段 host+ESP32 双 target build 0 warn 0 error + 单测全绿方可进入下一阶段。每个阶段控制在 1-2 天内可交付、可验证的粒度。

### 阶段 -1：ADR 前置（0.5 天，实施前必须完成）
1. 起草并提交 Owner 审阅 3 个 ADR（§8）：BAL 正式分层 / Fault 三阶段 / App 层阻塞 API 诚实化
2. ADR Accepted 后即刻回写 `docs/design/01-system-overall/` 与 `02-wink-micro-os/` / `03-app-codegen/` 对应设计规范
3. 基于本 tech-design（v5）撰写实施计划 `implementation-plans/2026-07-06-bal-dcst-refactor-plan.md`，拆到可 1 人 1 天完成的 task 粒度，每个 task 带验收标准

### 阶段 0：DAL deinit 走查与补全（1.5-2 天，**前置硬依赖**）
> **现状核查**：3 个已有 deinit（led/button/ultrasonic）是"伪 deinit"（只调 pal_resource_release，没 gpio_reset_pin），4 个缺失（servo/ssd1306/eeprom/gps）。不要低估工作量。
1. **0a 重写已有 deinit**：led / button / ultrasonic 三个驱动按 §3.4.4 清场检查单重写 `dal_xxx_deinit()`（必须 `gpio_reset_pin` + 停外设 + ISR 注销 + `initialized=false` + 幂等 + ≤50ms 非阻塞）
2. **0b 新建缺失 deinit**：servo / ssd1306 / eeprom / gps 四个驱动补 `dal_xxx_deinit()` 函数 + header 声明 + 各 target 实现
3. **0c host 幂等单测**：每个 DAL 驱动加 `test_dal_xxx_init_deinit_init` 单测，断言 init→deinit→init 无资源泄漏、第二次 init 行为与冷启动一致（config 字段相同、`initialized==true`）；多轮（5-10 次）循环 deinit→init 不累积资源
4. **0d ESP32 真机验证**：smoke S1-S10 在补全 deinit 前后行为一致；加 S11 deinit 循环测试（init→deinit→init 跑 5 轮，不报 GPIO 占用、不 WDT）
5. **0e deinit 代码审查**：对照 §3.4.4 清场单逐行 review（特别注意 I2C 总线恢复、RMT DMA 清理、gpio_reset_pin 覆盖所有引脚）

### 阶段 1：BAL/Runtime 基础设施（1.5-2 天）
1. 新建 `wink-micro-os/bal/` 目录结构（include/{output,input,sensor,actuator,display,comm}/ + src/ + tests/）+ CMake 静态库 `wink_bal`（依赖 wink_runtime + wink_dal，不直接 link PAL 除外的 target 物件）
2. 新建 `bal/include/wink_helper_opts.h`：`wink_bal_core_t` 枚举 + `wink_helper_opts_t` + `WINK_HELPER_OPTS()` 宏（BAL 头不得 include pal_osal.h）
3. 新建 `runtime/include/wink_blocking_region.h`：`WINK_INTERNAL_BLOCKING_REGION_BEGIN/END` + `WINK_INIT_BLOCKING_REGION_BEGIN/END`（GCC/Clang/MSVC 三编译器兼容，Q4 已决）
4. **Runtime 常量与错误码补齐**：
   - 在 `wink_tasks.h` 新增 `#define WINK_PERIODIC_INVALID ((wink_periodic_handle_t)-1)` 具名常量，统一无效句柄表示（替代裸写 `-1`/`0` 魔数）
   - 在 `wink_status.h` 错误码枚举新增 `WINK_ERR_CANCELED = -19`，语义为"并发撤销"（并发 stop 抢占导致 start 回滚，属良性可预测并发事件，与 `WINK_ERR_INVALID_STATE` 编程错误严格区分）
   - 在 `wink_tasks.h`/`wink_tasks.c` 新增 `uint32_t wink_periodic_active_count(void)`（返回当前 RUNNING 态 periodic 句柄数），供 `wink_device_tree_deinit()` 泄漏断言使用，并配 host 单测验证
5. `wink_periodic_change_period()` 实现（拆三子任务，Q2 已决方案 B）：
   - 1.4a：**新增公共 API** `wink_soft_timer_change_period(h, new_ticks)` 到 `wink_soft_timer.h`（LIGHT 侧原子更新，注意这会改公共头，需同步更新所有 call site 与文档）
   - 1.4b：MAY_BLOCK task 侧 period 原子字段 + `xTaskAbortDelay`/fiber-wake 打断休眠（sim/host fiber 路径同样实现）
   - 1.4c：`wink_periodic_change_period(h, period_ms)` 统一入口按路径分发；非法 h 返回 `WINK_ERR_INVALID_ARG`
   - 每个子任务配 host 单测验证"零停拍 + 下个周期生效"，含 **self set_period 重入单测**（从 LIGHT/MAY_BLOCK callback 内调自己句柄 set_period，验证新周期下一迭代生效）
6. **运行期 LIGHT 上下文断言（必做，非可选）**：在 `wink_periodic` LIGHT 入口/出口维护 dispatch 标志，与 `wink_pt_debug.h` 里 `WINK_ASSERT_NONBLOCKING()` 打通，抓住未标记 WINK_BLOCKING 的违规调用
7. **Codegen bus-owner 抽象**（配合 §3.4.4 共享 Bus 所有权规则）：在 device_tree codegen 中识别共享 I2C/SPI bus，生成静态 bus-owner 节点（init 顺序：bus 先于 client，deinit 逆序）；单器件 DAL deinit 不得调用 `i2c_driver_delete`/`spi_bus_free`，bus-owner 的 deinit 才销毁 bus。配合 stage 0 补 ssd1306/eeprom I2C 共享场景验证
8. 更新 codegen：
   - DriverBase 删除 `get_service_headers()` / `render_service_starts()` 钩子，新增 `render_config_macros()` 钩子
   - button.py / led.py / ultrasonic.py 删除 service 相关实现，button.py 加 `render_config_macros()` 导出 `BOOT_BUTTON_AUTO_POLL_MS / BOOT_BUTTON_LONG_PRESS_MS`
   - device_tree.h.j2 加 `WINK_APP_MAX_<DEV>_INSTANCES` 计数宏 + 配置常量宏段
   - device_tree.c.j2 审查 deinit 段（已存在，验证顺序/错误处理正确）
   - **删除** `app_support.c.j2` 模板及相关 CMake 引用；`wink_app_services_start()` 从 runtime 启动流程中移除（如有引用）
   - app_options.cmake.j2 按 §6.5 规则计算 `WINK_MAX_PERIODIC`
   - schema 校验移除 `services/callbacks/state_variables` 三个字段
   - codegen golden test 同步更新
9. 验证：host build 0 warn、codegen golden 单测全过

### 阶段 2：迁移现有 samples/common helper → BAL（1-2 天）
1. 迁移 `wink_blink_helper` → `bal/include/output/wink_led_blink_helper.h`（**重点修复 LIFO bug**）：
   - **Q7 已决：选 (a)** — `wink_led_blink_stop(dal_led_t *led)` 按设备指针停；`wink_led_blink_start` 返回 `wink_status_t`
   - 内部 slot 池**扫描全数组找 NULL dev**（不能沿用现有 `s_next++` 环形游标——那是真 bug，stop 无法回收 slot）
   - 加 `wink_led_blink_set_period()` 时注意同步更新 half-period（toggle 频率 = period_ms/2）；底层走 `wink_periodic_start_ex(WINK_PERIODIC_LIGHT)`（不直接用 wink_soft_timer，统一路径）
   - 双轨 `_start_ex` 让专家强制 LIGHT/MAY_BLOCK（blink 默认 LIGHT）
   - 0 实例 stub 用 `WINK_UNAVAILABLE_MSG` 标注
   - 加 `wink_led_blink_is_running()`
2. 迁移 `wink_button_helper` → `bal/include/input/`：
   - 现有 slot 池范式基本正确，但数组大小改用 `WINK_APP_MAX_BUTTON_INSTANCES`（非固定 4 魔数）
   - 底层从直接调 `wink_soft_timer_create/start` 改为走 `wink_periodic_start_ex(WINK_PERIODIC_LIGHT)`
   - `.c` 顶部**不加** `WINK_INTERNAL_BLOCKING_REGION_BEGIN/END`——已走查 host/esp32/wasm 三 target：`dal_button_poll` 内部仅通过 `pal_gpio_read` 做寄存器级读（<1µs），无任何 `WINK_BLOCKING` 调用；配合 `"isr_counter": true` 的 ISR-defer 模型（ISR 计边沿，LIGHT 回调读 delta + 去抖状态机），满足 LIGHT 契约
   - 加双轨 `_start_ex` + `set_period` + `is_running`
3. 迁移 `wink_default_telemetry` → `bal/include/comm/wink_telemetry_helper.h`：
   - 从全局单例 `static s_ctx` 改为 slot 池（至少 1 实例默认）
   - 补 `wink_telemetry_default_stop()` 接口
   - 加双轨 `_start_ex`（默认栈 2KB/prio=1/ANY/MAY_BLOCK）
4. 迁移 `wink_sim_ultrasonic_echo` → `runtime/selftest/wink_sim_echo.h`（不归 BAL！见 §5.2），加 `#ifndef WINK_STRICT_NONBLOCKING` 条件编译隔离
5. `samples/common/` 旧文件处理：加转发头（`#include "bal/output/wink_led_blink_helper.h"`）保留一个版本周期，下个 release 前彻底移除
6. 删除 `samples/devkitc_smoke/app_callbacks.c` 现有 file-scope pragma，改为 selftest 段用 `WINK_INIT_BLOCKING_REGION`
7. BAL helper host 单测：每个 helper 覆盖 start/stop 幂等、多实例、NULL 安全、set_period、_start_ex 参数覆盖、is_running

### 阶段 3：新建第一批 BAL helper + smoke 迁移（1.5-2 天）
1. `wink_sonar_helper`（新建）：默认 MAY_BLOCK / 栈 3KB / prio=5 / ANY；满足 [[memory:dal-eager-init-pattern]] / [[memory:freertos-same-priority-pulse-stretch]]；替代当前 app_callbacks.c 里手写的 `sonar_poll_task`
2. `wink_servo_helper`（新建）：至少包含 sweep/set_angle 周期模式；默认 MAY_BLOCK / 栈 2KB / prio=3
3. 每个新 helper 配 host 单元测试（5-8 个 Unity 用例）+ 0 实例 stub
4. 重写 `samples/devkitc_smoke/app_callbacks.c` 为 §4.1 初学者版本（`init_status/on_fault_status` 新签名、零手写 pragma、零 pal_osal.h include、显式 `wink_button_helper_start(&boot_button, BOOT_BUTTON_AUTO_POLL_MS)`）
5. **三 target 验证**：
   - host build：0 warning、所有 BAL/runtime/DAL 单测全过
   - ESP32 build：0 warning 0 error、smoke S1-S10（加 S11 deinit 循环）全过
   - wasm sim build：不打开 STRICT_NONBLOCKING 先让它能编译启动（阶段 5 再开），S1-S3/S8/S10 能在仿真中观察到行为

### 阶段 4：扩展 codegen 驱动覆盖与其他 sample 迁移（2-5 天，可分批）
1. 给 servo/ssd1306/eeprom/gps 补 codegen driver 插件（目前只有 button/led/ultrasonic）
2. 把 samples 里其他 sample（avoidance_car / oled_dashboard / dual_task_demo / unisim_smoke / resource_conflict）分批迁移到 codegen + BAL 模式
3. dual_task_demo 的特殊价值是展示"双任务"，保留作为"专家模式"示例，但要求使用 BAL `_start_ex` 而不是直接 `pal_os_task_create`
4. 更新 `docs/design/01-system-overall/02-wink-micro-os/03-app-codegen/04-wasm-simulation/` 对应设计规范（按 CLAUDE.md 规则回写）

### 阶段 5：sim 严格模式修补 + 后续分批迁移（3-6 天）
1. **普查前置**（0.5 天）：grep 全仓库 `-Wdeprecated-declarations` pragma、所有 call site 调 WINK_BLOCKING API 的位置，按文件分级（sim 专用 / selftest / BAL / app 业务层）产出违规清单
2. **sim target 严格非阻塞模式（Q5 已决：强制 `WINK_STRICT_NONBLOCKING=1`）**：
   - sim build CMake 配置默认开启严格模式
   - 按普查清单逐个修复：
     - BAL helper 的 LIGHT 回调内不得调 blocking（这本来就不该有，有则是 bug）
     - selftest 必须在独立 MAY_BLOCK task 里跑，不在 LIGHT 上下文
     - `wink_sim_ultrasonic_echo` 等 bringup 模块用 `#ifndef WINK_STRICT_NONBLOCKING` 条件隔离
     - sim PAL 桩里阻塞 API 被调用时直接 link error 或运行期 assert
   - 运行期 LIGHT 上下文 assert 在 sim 下升级为 hard fault（abort 并打印哪个回调违例）
3. **Host 单测 STRICT_NONBLOCKING 开关**：BAL LIGHT 单测打开 STRICT_NONBLOCKING，确保 helper LIGHT 路径零 blocking 违规；MAY_BLOCK 路径的单测链接独立的 target 配置
4. **其余 sample 迁移**：avoidance_car / oled_dashboard 等手写 device_tree 的 sample 分批迁移到 codegen + BAL；dual_task_demo 作为"专家模式"保留但改用 BAL `_start_ex`
5. **延后项（独立 ADR，不在本次重构范围）**：
   - DAL 异步 `on_data` 回调模式（ISR defer、通知上下文规则）
   - PAL 低功耗扩展（`pal_enter_light_sleep/deep_sleep`）
   - BAL 服务链式依赖（**已决 Q9：强烈不做**，YAGNI——硬件依赖 device_tree 拓扑序保证，软件依赖 app_init 调用顺序解决）
   - BAL helper 模板 macro（`WINK_BAL_HELPER_IMPL`）统一生成 0 实例 stub（可在阶段 2-3 做，非阻塞）

---

## 8. ADR 拆分建议

本 tech-design 通过 v5 审阅后起草以下 3 个 ADR，**ADR Accepted 后再开始阶段 0 编码**（见 §7 阶段 -1）：

| ADR 标题 | 决策点 |
|---|---|
| ADR-0023 BAL 正式分层建立 | BAL 在 DAL/runtime 之上、app 之下；**BAL 公共头禁止 include PAL**（引入 `wink_bal_core_t` 隔离核类型）；双轨 start/start_ex；目录布局；slot 池模式采用三态状态机（FREE/STARTING/RUNNING + TOCTOU 二次校验自回滚），临界区用真实存在的 `pal_irq_save_rtos_safe/pal_irq_restore` 保护元数据；slot 池容量由 codegen 输出的 `WINK_APP_MAX_<DEV>_INSTANCES` 宏驱动；`WINK_PERIODIC_INVALID` 作为统一具名无效句柄；0 实例 stub 约定（控制 API 挂 `WINK_UNAVAILABLE_MSG` 编译报错、stop 静默 no-op 方便通用清理路径）；每个 helper `.h` 强制暴露 `WINK_<DEV>_HELPER_DEFAULT_STACK/PRIO/CORE/FLAGS/MIN_PERIOD_MS` 一组默认宏；`opts->flags == 0` 语义为"use helper default"；LIGHT 契约强约束（编译期+运行期双防线）；所有 BAL helper 统一走 `wink_periodic_start_ex`（不直接用 wink_soft_timer）；LIGHT/MAY_BLOCK 分类中 button 明确为 LIGHT（基于 host/esp32/wasm 三 target `pal_gpio_read` 走查） |
| ADR-0024 Fault 三阶段（Phase）生命周期模型 | Fault 处理用 Phase 1/2/3 术语（和工程里程碑"阶段 -1/0/1/2/3/4/5"区分）；Phase 1 只做 actuator safe-off（非阻塞硬实时，≤100µs，允许 ISR-safe SDK 调用如 `gpio_set_level`，**非 panic/HardFault 向量上下文**）；Phase 2 调 app_on_fault（可阻塞 fault task 上下文，≤500ms）；Phase 3 WDT/复位兜底；runtime 不自动 stop BAL 服务；init 失败采用"谁启动谁回滚"契约，runtime 不猜测依赖顺序；DAL deinit 质量铁律（gpio_reset_pin 硬要求 + I2C 总线恢复 + I2C/SPI 共享 bus 由 codegen 生成的 bus-owner 静态节点管理 + WDT 脏复位处理）；`WINK_ERR_CANCELED` 用于并发撤销的良性事件（与 `WINK_ERR_INVALID_STATE` 编程错语义分离）；`wink_periodic_active_count()` Debug 断言防泄漏；panic/HardFault hook 留作独立 future work（专用 minimal-safe-off 路径） |
| ADR-0025 App 层阻塞 API 诚实化约定 | init 阶段允许 blocking（`WINK_INIT_BLOCKING_REGION`）；BAL .c 用 `WINK_INTERNAL_BLOCKING_REGION` 收敛（替代裸 pragma，MSVC 兼容）；app 业务回调/app_loop/on_event 禁止任何 blocking pragma；app 层统一使用 `init_status/on_fault_status` 新签名；sim 强制 `WINK_STRICT_NONBLOCKING=1`（编译期硬隔离+运行期断言）；self-set_period 重入语义（从 callback 内改自身句柄合法，下个周期生效；跨 helper set_period 禁止） |

---

## 9. 已决问题清单（审阅结论）

Q1-Q9 由项目 Owner 在 v2 审阅中拍板；Q10-Q13 在 v3 现状走查后新增拍板；**Q14-Q19 在 v5 批判性质询后由 Owner 拍板**（与 Owner 达成一致的默认决策，实施时直接落地）：

| # | 议题 | 决策 | 关键理由 |
|---|---|---|---|
| Q1 | BAL helper 默认参数（尤其是 sonar 优先级） | **采纳建议默认值**：sonar prio=5/栈=3KB/MAY_BLOCK，blink/button 走 LIGHT，oled/i2c 栈=3KB/prio=2/MAY_BLOCK，telemetry 栈=2KB/prio=1/MAY_BLOCK | RMT/脉冲类外设高优先级是防时序抖动硬要求（见 [[memory:freertos-same-priority-pulse-stretch]]） |
| Q2 | `wink_periodic_change_period` 实现路径 | **方案 B**：runtime 层直接实现（下个周期生效，零停拍），不做 stop+restart 过渡；分 LIGHT 侧/MAY_BLOCK 侧（需 xTaskAbortDelay）/统一入口三子任务 | 零停摆对伺服/PID 闭环是硬要求；stop+restart 丢拍在专家场景不可接受 |
| Q3 | codegen 生成 `wink_device_tree_deinit()` | **已存在，加固（非新增）**：void 返回、逆拓扑序、best-effort `WINK_IGNORE_RESULT` 链式清理；保持现有语义 | 低功耗唤醒 + 软重启都依赖；DAL deinit 是前置硬依赖 |
| Q4 | 阻塞区域宏位置 | **选项 A**：新建 `runtime/include/wink_blocking_region.h` | `wink_status.h` 是最基础头，保持洁癖；专用头文件语义更清晰；同时兼容 GCC/Clang/MSVC |
| Q5 | sim target 是否强制 `WINK_STRICT_NONBLOCKING=1` | **强制开启**；阶段 5 前先做违规清单普查 | sim 的核心价值是 fail-fast 抓 bug；LIGHT 里阻塞会卡死整个仿真，严格编译期拦截省巨大排查成本；运行期 LIGHT 上下文断言同步必做 |
| Q6 | DAL `_deinit` 标准 | **必须补全 + 强制 `gpio_reset_pin` + I2C 总线恢复 + 幂等单测**；现有 3 个伪 deinit 也要重写 | 阶段 0 是"3 个重写 + 4 个新建"不是"补 4 个缺失"；`pal_resource_release` 只清 bookkeeping 不清 IDF 层 reservation |
| Q7 | `wink_led_blink_stop` 参数形态 | **(a)** `wink_led_blink_stop(dal_led_t *led)` 按设备停；修复现有 blink_helper 的 `s_next++` 环形 LIFO 耗尽 bug | 同一物理 LED 不可能同时 blink+breath；按设备停模型更诚实、AI 更友好；stop 必须能原地回收 slot |
| Q8 | codegen 是否自动启动 button auto_poll | **选项 B：不自动启动，删除 app_support.c**。codegen 只生成 `BOOT_BUTTON_AUTO_POLL_MS` 常量宏，用户在 `app_init` 显式调 `wink_button_helper_start(&boot_button, BOOT_BUTTON_AUTO_POLL_MS)`；DriverBase 删除 `get_service_headers/render_service_starts` 钩子 | 贯彻"JSON 只描述静态世界"纯粹原则；避免潜伏启动；`wink_app_services_start` 删除后无存在价值 |
| Q9 | BAL 是否做服务依赖声明 | **阶段 1 不做**（YAGNI） | 硬件拓扑依赖 device_tree 拓扑序保证；软件依赖 app_init 调用顺序即可 |
| Q10 | BAL 公共头是否依赖 PAL 类型 | **不依赖**：BAL 自定 `wink_bal_core_t` 枚举在 `wink_helper_opts.h`，BAL `.c` 内部映射到 `pal_os_core_id_t` | 守住"BAL ⇢ PAL"分层红线；应用层用 BAL 时零 pal_* 头 include |
| Q11 | 0 实例 BAL helper stub 机制 | **复用 `WINK_UNAVAILABLE_MSG`**（不另造 `__attribute__((error(...)))`） | 统一风格、兼容 GCC/Clang/MSVC；已有基础设施 |
| Q12 | App 回调签名 | **统一用 `init_status`/`on_fault_status`**（返回 `wink_status_t` 的新签名），示例代码不再用 legacy void `init`/`on_fault` | 新签名比 longjmp 式 WINK_CHECK 更干净；legacy 字段保留给旧 sample 过渡，但新代码一律走新签名 |
| Q13 | 阶段顺序 | **加阶段 -1（ADR 前置）**：3 个 ADR Accepted + 设计规范回写后再开始阶段 0 | 按 CLAUDE.md 规则，架构性决策必须 ADR 先立；避免实施中漂移 |
| Q14 | 无效 periodic 句柄表示 | **新增具名常量** `WINK_PERIODIC_INVALID = ((wink_periodic_handle_t)-1)`；禁止裸写 -1/0 魔数 | 高可靠代码基本规范；slot 初始化/比较代码可读性显著提升；列为 Stage 1 交付物 |
| Q15 | 并发撤销错误码 | **新增 `WINK_ERR_CANCELED = -19`**，与 `WINK_ERR_INVALID_STATE`（编程错）严格区分 | 并发 stop 抢占导致 start 回滚是良性可预测并发事件，语义上不能用"状态非法"——应用层按 CANCELED 走正常回滚路径不应触发 fault |
| Q16 | Button helper 分类 | **明确归类为 LIGHT**（已走查 host/esp32/wasm 三 target `dal_button_poll`/`pal_gpio_read` 仅做寄存器级读，<1µs 无阻塞）；配合 `"isr_counter": true` ISR-defer 模型（ISR 计边沿，LIGHT 回调读 delta + 去抖） | 三 target 代码实查证实无阻塞风险；LIGHT 路径节省独立栈/任务资源；Stage 2 迁移时不加 BLOCKING pragma |
| Q17 | I2C/SPI 共享 bus 所有权 | **codegen 生成静态 bus-owner 节点**：bus 按拓扑序先于 client init、逆序晚于 client deinit；单器件 DAL `_deinit` 只清 client 状态，**不得**调用 `i2c_driver_delete/spi_bus_free` | 契合 DCST"静态硬件树"心智模型；codegen 构建期最清楚拓扑；运行期 refcount 增加并发同步复杂度违背静态确定性原则；ssd1306 + eeprom 共享 I2C 是首个触发场景 |
| Q18 | 0 实例 stub 一致性 | 控制/状态类 API（start/start_ex/set_period/is_running）挂 `WINK_UNAVAILABLE_MSG` 编译期报错；**stop 保持静默 no-op** | 通用故障/低功耗清理路径会无差别调所有 helper 的 stop，如果 stop 也编译报错就逼出大量 #ifdef 板型分支；stop 幂等/NULL 安全是统一心智模型，0 实例下也应保持 |
| Q19 | Callback 内 self-set_period 重入语义 | **合法，下个周期生效**：LIGHT 侧原子写 period 字段、当前 callback 返回后下 tick 即按新频率派发；MAY_BLOCK 侧在 task 主循环顶部读 period、当前迭代完成后按新周期休眠 | blink/反馈降速等场景自然需要从回调内改自身频率；跨 helper set_period 禁止（属业务层状态机职责）；列为 Stage 1 单测必覆盖项 |

---

## 10. 风险与回退

| 风险 | 影响 | 缓解措施 |
|---|---|---|
| 阶段 0 DAL deinit 工作量被低估（3 重写+4 新建 vs 原估"补几个缺失"） | 阶段 0 拖期，后续阶段顺延 | 阶段 0 单独排 1.5-2 天；每个驱动 deinit 配 reviewer 对照 §3.4.4 清场单逐行 check；先做 led/button 两个最简单的验证模板再批量铺开 |
| 默认参数（如 sonar 优先级 5）在实际硬件上表现不如预期 | 时序抖动、丢数据 | BAL 默认值集中在各 helper `.h` 顶部宏，专家可 `_start_ex` 覆盖；smoke 测试 S10 真机验证；阶段 3 落地后做 S10 时序回归 |
| BAL 抽象层加厚影响 wasm sim 性能 | 仿真帧率下降 | LIGHT 路径（blink/button poll）零额外栈，开销 ≈ 直接用 soft_timer；MAY_BLOCK 路径开销就是 fiber 本身开销；统一走 wink_periodic 减少间接层 |
| 迁移 samples/common 破坏现有 sample 构建 | 短期 breakage | 旧头文件保留转发 include 一个版本周期，下游 sample 可平滑迁移；CI 跑所有 sample 防回归 |
| 删除 app_support.c / 移除 service_starts 钩子破坏现有 sample | 现有 devkitc_smoke/avoidance_car 等构建失败 | 阶段 2/3 同步迁移所有 in-repo sample；app_support.c 删除前 grep 确认所有引用方已迁移到显式 BAL 启动；模板删除和 sample 迁移在同一 commit |
| 双轨 API（`_start`/`_start_ex`）增加 BAL 维护成本 | 每个 helper 写两个入口 | 实现模式统一：`.c` 内一个 `_start_ex_impl()`，`_start` 是 3 行 wrapper（`opts=NULL` 转调 `_start_ex`）；后续可考虑 BAL 模板 macro（`WINK_BAL_HELPER_IMPL`）自动生成 wrapper/0-stub |
| blink_helper LIFO 环形游标 bug 在迁移中被原样带入 BAL | 多实例场景 4 次 start 即 RESOURCE_EXHAUSTED | 阶段 2 #1 必检项：slot 回收必须"扫描全数组找 NULL dev"，stop 时把 `slot->dev = NULL` 原地释放；配"start/stop 循环 100 次不返回 EXHAUSTED"单测 |
| MAY_BLOCK `wink_periodic_change_period` xTaskAbortDelay 处理不好导致"长改短周期"延迟生效 | servo/PID 动态调频场景响应迟缓 | 1.4b 子任务单测覆盖"10s 周期改 100ms 必须在下一个 100ms 内触发回调"；ESP32 和 sim fiber 路径都要验证 |
| DAL `_deinit` 补全不彻底导致资源泄漏 | ESP32 重复 init/deinit 后 GPIO reservation 泄漏、低功耗唤醒失败 | 阶段 0 走查补全 + init→deinit→init 幂等单测 + [[esp32-idf-gpio-reset-pattern]] checklist 审查（**特别注意 `pal_resource_release ≠ gpio_reset_pin`**）；smoke 加 S11 deinit 循环测试 |
| 0 实例 BAL stub 分支在某个驱动上漏掉导致链接错误 | 构建失败 | 统一使用 `WINK_UNAVAILABLE_MSG`；后续阶段做 BAL 模板 macro `WINK_BAL_HELPER_IMPL` 统一展开 0 实例 stub；codegen 检查每个启用的 DAL 类型都有对应 BAL helper 的宏 |
| sim STRICT_NONBLOCKING=1 打开后 selftest/wink_sim_echo 链接失败 | 阶段 5 时间盒被打破 | 阶段 5 先做违规清单普查，分级处理（BAL/selftest/bringup 各有处理方式）；时间盒 6 天；若发现结构性问题（如某模块大量违反），开独立 ADR 讨论该模块重构，不阻塞主架构上线 |
| BAL 头 include 了 pal_osal.h 等重 PAL 头，破坏分层 | 分层规则被悄悄打破；app 层被迫间接依赖 PAL | CI 加一条简单 grep 检查：`bal/include/**/*.h` 里不得出现 `#include.*pal_`；code review 卡口 |
| `init_status/on_fault_status` 新签名推广时旧 sample 继续用 legacy void 签名 | 两种风格长期并存 | 文档示例统一用新签名；阶段 3-4 迁移所有 in-repo sample 到新签名；runtime 继续兼容 legacy（已有 dual-field 设计），但新代码 PR 卡口禁止用 legacy |

**工作量总估**：阶段 -1 (0.5) + 阶段 0 (2-2.5) + 阶段 1 (2-2.5) + 阶段 2 (1-2) + 阶段 3 (1.5-2) + 阶段 4 (2-5) + 阶段 5 (3-6) = **12.5-21 人天**。相对 v2 估算（7.5-16 天）增加约 5 天，主要来自：阶段 0 从"补 4 个 deinit"升级为"3 重写+4 新建+幂等单测+bus-owner 抽象"、runtime 层新增 `WINK_PERIODIC_INVALID/WINK_ERR_CANCELED/wink_periodic_active_count/wink_soft_timer_change_period`、change_period 拆三子任务且加 self-set_period 重入单测、运行期断言升级为必做、阶段 5 加普查+bringup 条件化。

**回退策略**：所有改动在新目录 `bal/` 中，`samples/common/` 旧文件保留转发头一个版本不删，直到所有 sample 迁移完成且通过验证；runtime 新增 API（`wink_periodic_change_period`/`wink_blocking_region.h`）是纯新增，不修改旧 API 行为。任何阶段发现重大架构问题，只要保留旧文件+旧 API 未被删，就可整体回退而不破坏现有工作固件。

---

## 11. 未来演进与 Hardening (Future Work)

- **长期运行的 Hardening 机制**：在 `WINK_CONFIG_STATIC_TASKS` 宏启用下，评估将 `pal_os_task_create` 映射到 `xTaskCreateStatic` 及静态栈 Slot 分配；或者在 BAL 层实现周期任务句柄的持久化（即采用信号量挂起与唤醒的挂起机制，替代动态任务的销毁与重建）。
- **NMI / HardFault Hook 接入与专用安全关断**：当接入 CPU 级的 panic/HardFault 异常中断向量时，独立设计一套 `wink_panic_minimal_safe_off` 机制（可使用底层寄存器直写），该机制需与常规运行期 `safe_off` 回调分离，防止在 CPU 严重崩溃状态下因 spinlock 死锁而导致复位失败。

---

## 12. 参考

- ADR-0004 静态分发 vs 运行期 ops：避免 void*/虚表，POD + 命名 API
- ADR-0013/0014 协作式确定性调度器：单虚拟核、协作 fiber、零抢占
- ADR-0016 双进入临界区：task/ISR 双入口安全
- ADR-0017 阻塞 API 三层硬隔离：compile-time（deprecated）/ link-time（STRICT_NONBLOCKING 符号消失）/ runtime（PT 上下文断言）
- `wink_actuator_registry.h`：现有 actuator safe-off 静态表哲学——板级安全电路是硬兜底，软件只补软闭环
- `wink_button_helper.c`：现有 slot 池实现参照（静态数组、BSS 零初始即空闲、幂等 NULL 安全 stop）
- `wink_runtime_tasks.c`：LIGHT/MAY_BLOCK 双路径、handle 生命周期、stop sem 超时 500ms、absolute-time drift correction
- `wink_soft_timer.h`：tick 上下文非阻塞约束、WCET 50% 软告警
- [[freertos-same-priority-pulse-stretch]]：RMT/脉冲类时序敏感任务必须优先级高于并发 task + 单段 busy_wait 不 yield + 钉核
- [[esp32-idf-gpio-reset-pattern]]：ESP-IDF v6 deinit 必须 `gpio_reset_pin(pin)` 撤销 reservation + 断路由 + 复位 pad；`ledc_stop` 等不清 reserve 位图
- [[dal-eager-init-pattern]]：helper `_start` 返回时硬件必须 ready，lazy-init 仅适用于开销 < 关键时序窗口的资源
- [[smoke-test-explicit-pass-fail]]：S1-S10 smoke 对 BAL 回归的验证价值
- 原始 DCST 方案：Antigravity IDE 生成的 `architectural_design_scheme.md`，本方案是其批判性重构
