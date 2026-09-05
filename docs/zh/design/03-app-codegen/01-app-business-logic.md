# 02. 通用低代码 AI 嵌入式开发平台：应用层 (App) 运行时规范

| 项 | 内容 |
|---|---|
| **关联 ADR** | ADR-0004（静态分发）、ADR-0011（State Struct）、ADR-0013/0014（协作调度器）、ADR-0018（IRQ 三级收窄）、ADR-0048（执行器语义） |
| **关联技术设计** | [app-layer-lowcode-unification-design.md](../../tech-designs/tools/app-layer-lowcode-unification-design.md)；[user-surface-insulation-design.md](../../tech-designs/tools/2026-07-28-user-surface-insulation-design.md)（用户稳定面 SSOT） |
| **关联实施计划** | [2026-07-28-user-surface-phase1-plan.md](../../implementation-plans/frontend/2026-07-28-user-surface-phase1-plan.md) |
| **关联评审** | [dal-control-semantic-completeness-review §10](../../reviews/core/2026-07-28-dal-control-semantic-completeness-review.md)；[user-surface-phase1-plan-review.md](../../reviews/frontend/2026-07-28-user-surface-phase1-plan-review.md) |
| **Codegen 实现** | `wink gen app` / `wink gen device-tree` (`wink-tools/tools/codegen/`, [ADR-0051](../../decisions/tools/0051-scannable-codegen-extension-roots.md) / [ADR-0059](../../decisions/tools/0059-wink-tools-cli-hybrid-verb-first-architecture.md)) |
| **首个样板 sample** | L1：`oled_dashboard`（按键事件）、`avoidance_car`（测距事件 ADR-0033）；QA：`devkitc_smoke` |

在面向低代码 (Low-Code) 与 AI 辅助生成的开发场景中，系统设计的核心在于**屏蔽繁琐的技术细节，将开发者的关注点聚焦于“业务流与控制算法”**。

本文件详细定义了**应用层 (App, Application Layer)** 的运行规范、C 代码自动生成契约、以及其与底层硬件逻辑的完全解耦形式。

> **术语澄清**：
> - ✅ **App 层**：本文件描述的「用户代码 / AI 生成的一次性业务逻辑」，包含 `app_init` / `app_loop` / `app_on_fault`
> - ✅ **BAL 层**：Business Abstraction Layer（[ADR-0023](../../decisions/core/0023-bal-business-abstraction-layer.md) / [ADR-0038](../../decisions/core/0038-bal-naming-hard-cut-and-layer-ssot.md)），可复用业务服务（如 `wink_led_blink`、`wink_button_events`、`wink_telemetry_default`、`wink_pid` 等），随 WinkMicroOS 发布

---

## 1. App 层的核心职责与设计原则

### 1.1 核心职责
*   **业务流程调度**：定义设备的工作流、调用 BAL 算法库、事件触发响应、以及不同器件之间的协作规则（如“超声波距离小于 15cm 时转动舵机并亮红灯”）。
*   **状态机生命周期管理**：控制系统初始化、常态循环（Main Loop）、异常挂起、自我保护与恢复等状态机的流转。
*   **人机/云端交互**：处理来自 Web 前端虚拟控制面板的参数输入，或将器件采集的状态打包上传给云端监控层。

### 1.2 架构约束原则
为了确保生成的业务逻辑具备 100% 的平台无关性和高可移植性，App 代码必须严格遵守以下约束：
1.  **禁止包含硬件级头文件**：App 代码中**绝对不允许**直接 `#include "pal_hal.h"`，也不允许使用任何 `pal_gpio_write`、`pal_i2c_transfer` 等总线级 API。
2.  **禁止硬编码物理引脚与通道**：所有的引脚分配（如 Pin 2、PWM Channel 0）必须交由静态设备树分配。App 中只能包含通过业务命名的设备实例句柄指针（如 `&front_radar`、`&neck_servo`）。
3.  **通过业务语义接口交互**：App 与硬件的所有信息交换，必须通过调用 DAL (器件层) 或 BAL (算法层) 提供的只读、只写业务级别接口（例如 `dal_ultrasonic_read`、`bal_pid_compute`、`dal_rc_servo_set_angle`）来实现。
4.  **同源同构编译**：同一段 App 业务代码无需任何改动，即可在 WebWorker (wasm) 的仿真沙箱或 ESP32/STM32 真机固件中正常运行。

---

## 2. 低代码 / AI 自动生成 C 代码架构规范

在低代码拖拽编排前端或 AI 生成引擎中，用户的可视化积木块、状态机流程图或自然语言描述，最终会被翻译成如下结构的标准 C 语言代码文件：

```text
generated_app/
├── app_config.h              # 业务层参数与宏定义
├── device_tree.h             # 前端生成的设备实例逻辑声明
├── device_tree.c             # 前端生成的设备物理参数静态分配
└── app_main.c                # 业务核心状态机与主循环逻辑 (App)
```

### 2.1 业务代码的生命周期契约
生成的业务代码必须提供三个标准的接口，由 WinkMicroOS 的运行时引擎在启动和主循环中进行挂载调用：

```c
/**
 * @brief 系统初始化入口 (由 OS 启动后调用一次)
 * 负责设置外设的初始默认状态、配置控制算法参数等
 */
void app_init(void);

/**
 * @brief 周期性主循环函数 (在 OS 专有业务线程中被 while(1) 循环调用)
 * 包含状态机转换逻辑、周期采样与策略执行
 */
void app_loop(void);

/**
 * @brief 系统遇到紧急异常（如急停、器件断线）时的安全保护回调
 */
void app_on_fault(uint32_t fault_code);
```

### 2.2 Codegen 入口：`wink-app.json`（静态物理世界宣言）

设备实例与物理布局由单一 JSON 描述文件 `wink-app.json` 驱动，codegen 于构建期读取该文件并产出 `device_tree.c/h` 和 `app_options.cmake` 等胶水代码。

🚨 **分层契约（ADR-0023）：JSON 只描述静态物理世界，不包含任何业务状态或启动行为。** 
因此，所有服务自动启动（`services`）、状态变量（`state_variables`）和应用回调注册（`callbacks`）字段均已从 schema 中彻底删除。所有服务启动/停止（例如 `wink_button_enable_events`）全部在用户 C 代码中显式调用。

简化后的 JSON 格式示例（L1，默认路径）：

```json
{
  "app_name": "devkitc_smoke",
  "board": "esp32_devkitc",
  "devices": {
    "board_led":   { "type": "led",      "pin": 2,  "active_high": true },
    "boot_button": { "type": "button",   "pin": 0,  "active_low": true,
                     "long_press_ms": 3000, "isr_counter": true,
                     "auto_poll_ms": 10 },
    "smoke_ultrasonic": { "type": "ultrasonic", "trig_pin": 18, "echo_pin": 19,
                     "use_rmt": true }
  }
}
```

| 字段 | 类型 | 说明 |
|---|---|---|
| `app_name` | string | 构建产物名，对应 CMake target |
| `board` | string | 板级预设（引用 `boards/<board>.json`） |
| `devices` | map<name, DeviceSpec> | 静态设备定义。`type` 决定实例化哪类外设驱动。引脚、极性、周期等物理参数在此声明。 |

#### 2.2.1 渐进披露 `advanced`（L2，ADR-0034）

L1 只写语义字段。专家可选字段**唯一**放在 `advanced` 对象中；codegen 默认不生成、不向 AI 暴露。

```json
"boot_button": {
  "type": "button",
  "pin": 0,
  "active_low": true,
  "auto_poll_ms": 10,
  "advanced": { "pull": "none" }
},
"neck_servo": {
  "type": "rc_servo",
  "pwm_channel": 0,
  "min_pulse_ms": 0.5,
  "max_pulse_ms": 2.5,
  "advanced": {
    "resolution_bits": 10,
    "clock_requirement": "stable_required"
  }
}
```

| `advanced` 键 | 适用 type | 合法值 | 缺省 |
|---|---|---|---|
| `pull` | button | `auto` \| `up` \| `down` \| `none`（小写 string） | 省略 → C `AUTO`（与今日上下拉行为一致） |
| `resolution_bits` | servo | 正整数（target 权威校验可实现性）；禁止 bool/float | 省略 → 0 → 平台默认 13-bit |
| `clock_requirement` | servo | `auto` \| `stable_required` | 省略 → AUTO |

**校验规则（codegen）：** `advanced` 必须为 object；未知子键报错；`pull`/`clock_requirement` 大小写严格。不支持顶层 `pull` 与 `advanced.pull` 双写（若保留 deprecated 顶层 alias，双写一律 ERROR）。**禁止**在 JSON 中出现 `system_clock_hz` / APB / XTAL / 外拉阻值 Ω。

**配置所有权（SSOT）：** Registry/schema 管字段名与可见性；Python plugin 管校验与 C 枚举映射；DAL 管语义→PAL 映射（不泄漏 `pal_*`）；PAL target 管 AUTO→effective 与硬件可实现性。
---

### 2.3 设备树生成契约（`device_tree.h/c` 与宏导出）

Codegen 产出的 `device_tree.h/c` 为系统提供了静态物理资源的实例化与宏配置绑定：

#### 1. 实例容量上限宏 (Capacity Limits)
Codegen 自动统计 `wink-app.json` 中各类设备的实例数，在 `device_tree.h` 中导出 `WINK_APP_MAX_<DEV>_INSTANCES` 计数宏（若某设备为 0 实例，则该宏输出为 `0u`）：
```c
#define WINK_APP_MAX_LED_INSTANCES          1u
#define WINK_APP_MAX_BUTTON_INSTANCES       1u
#define WINK_APP_MAX_ULTRASONIC_INSTANCES   1u
#define WINK_APP_MAX_SERVO_INSTANCES        0u
```
BAL 层的静态 slot 池数组大小将以此宏为驱动。当某类设备为 `0u` 时，对应的 BAL 服务编译为空 stub 且控制 API 挂载 `WINK_UNAVAILABLE_MSG`，**达成零 RAM 浪费与编译期安全**。

#### 2. 设备参数常量宏 (Config Constants)
Codegen 通过驱动插件提供的 `render_config_macros()` 钩子，将 JSON 中声明的参数导出为强类型宏：
```c
#define BOOT_BUTTON_AUTO_POLL_MS   10u
#define BOOT_BUTTON_LONG_PRESS_MS  3000u
```
这避免了 C 代码中硬编码“auto_poll_ms=10”等魔数，直接实现“JSON 声明参数 ⇢ C 层编译消费”的链路。

#### 3. WINK_MAX_PERIODIC 周期容量自动计算
为了让开发者无需手动计算 `wink_periodic`（RTOS 任务数/定时器数）的上限，Codegen 在 `app_options.cmake` 中生成自动求和表达式：
$$\text{WINK\_MAX\_PERIODIC} = \sum \text{instances} + 4$$
（多出的 4 槽作为系统 telemetry、shell、selftest 以及预留的余裕），防止运行时触发 `RESOURCE_EXHAUSTED`。

#### 4. Bus-Owner 静态初始化序
若 JSON 中声明了多个使用相同物理 I2C/SPI 端口（如 `i2c_port`）的设备，Codegen 会在 `device_tree.c` 中生成静态的 bus-owner 节点。
时序要求：
- `wink_device_tree_init()` 优先调用 `pal_i2c_bus_init(port, sda, scl, hz)`，然后按拓扑序依次调用各外设的 `dal_xxx_init()`。
- `wink_device_tree_deinit()` 逆序依次调用外设的 `dal_xxx_deinit()`，最后再调用 `pal_i2c_bus_deinit(port)` 释放物理总线。

---

### 2.4 `app_support.c` 废弃与删除

🚨 **重大重构决议**：为了捍卫“JSON 只描述静态物理世界，所有启动行为均属于 C 代码”这一原则，自动生成的 `app_support.c` （包含 `wink_app_services_start`）已**彻底废弃并删除**。

- 按钮辅助的 auto-poll 任务等，不再被 Codegen 隐式启动；
- 应用层必须在其手写的 `app_init_status()` 中显式调用 `wink_button_enable_events()` 等 API 来启动相应服务。


### 2.5 `wink_button_events` 上下文约束（重要 ⚠️）

`wink_button_enable_events(&btn, &cfg)`（B 类，`WINK_BUTTON_DRIVE_SOFT_POLL`）通过 soft_timer 周期回调 `dal_button_poll()`，解放 `app_loop` 中的手动轮询。Soft_timer 回调在 Runtime tick 内同步触发，具备**类 ISR 约束**：

- ⛔ **禁止**调用任何可能 yield/block 的 API（`pal_os_delay_ms`、`pal_os_mutex_lock`、阻塞 I2C/SPI 传输、大量 `printf`/`LOG_I` 等）。
- ⛔ **禁止**在回调内执行超过 ~100 μs 的计算。
- ✅ 应做的：设置 volatile 标志位、往 ringbuf/队列投递事件、调用非阻塞 DAL 读。
- 若需重处理，请在事件回调中 post 到业务 task，或保持 `app_loop` 手动 poll（不启用 `auto_poll_ms`）。

Host/Debug 构建下 soft_timer dispatch 已集成 WCET 检测（超过 tick 50% 阈值触发 trace warning）。

### 2.6 BAL API 错误处理三级分级

| 级别 | 标记 | 适用 API | 调用方 |
|---|---|---|---|
| **Fatal** | `WINK_WARN_UNUSED_RESULT` | `_init`、`wink_runtime_spawn_*`、`wink_actuator_register`、`wink_button_enable_events` | 必须 `WINK_CHECK` 或显式处理 |
| **Normal** | `WINK_WARN_UNUSED_RESULT` | `_request_measurement`、`_start`（服务启动）、`wink_telemetry_default_start`、`wink_sim_ultrasonic_echo_start`、`dal_button_on_event` | 必须接收返回值；可用 `WINK_IGNORE_RESULT` 显式忽略 |
| **Fire-and-forget** | 无 warn_unused_result | `wink_led_blink_start/stop`、`wink_trace_*` | 可直接调用；BAL 服务内部用 `LOG_D` 自记失败 |

### 2.7 Role-Interface-Based Codegen (基于标准抽象接口的 Codegen)

为了消除 App 业务层代码中低级的 C 指针符号（`&`）、减少冗余的 `(void)` 强转、并彻底使业务代码与底层驱动型号（如 `ssd1306`）解耦，WinkMicroOS 引入了**编译期抽象角色接口（Role Interface）**。

> **Howto（实现/扩展）**：挂 Role 是 **codegen** 工作（`wink-tools/tools/codegen/drivers/`），与 DAL 本体分属不同包；完整步骤见 [`role-interface-codegen.md`](../../../wink-micro-os/docs/dal-development-guide/role-interface-codegen.md)。本节为角色/动词 **SSOT**。

#### 1. 概念与工作原理
在 `wink-app.json` 中定义器件时，可为其指定一个抽象角色 `"role"`。若未指定，系统将自动使用该器件驱动声明的 `default_role`：
- `led` $\rightarrow$ `binary_indicator`
- `button` $\rightarrow$ `binary_sensor`
- `ultrasonic` $\rightarrow$ `distance_sensor`
- `ssd1306` $\rightarrow$ `text_display`
- `rc_servo` $\rightarrow$ `angular_actuator`
- `dc_motor` $\rightarrow$ `open_loop_actuator`
- `encoder` $\rightarrow$ `pulse_counter`

> **命名维度**：`angular_actuator` 按运动输出命名；`open_loop_actuator` 按开环策略命名（防与闭环 BAL 混淆）；`pulse_counter` 返回原始脉冲计数，本 Phase 无 CPR 换算。

编译系统根据设备实例名和角色，在生成的 `device_tree.h` 中动态注入一系列 `static inline` 的业务级封装 API。

#### 2. 标准角色与其能力动词定义

| 角色 (`role`) | 动词 (`verb`) | 错误层级 | C 生成接口示例 | 说明 |
|---|---|---|---|---|
| **`binary_indicator`** | `activate` | Fire-and-forget | `void {name}_activate(void)` | 开启指示（如 LED 亮/蜂鸣器鸣叫） |
| | `deactivate` | Fire-and-forget | `void {name}_deactivate(void)` | 关闭指示 |
| | `toggle` | Fire-and-forget | `void {name}_toggle(void)` | 翻转状态 |
| **`binary_sensor`** | `is_active` | Convenience (bool) | `bool {name}_is_active(void)` | 返回布尔状态，异常时静默并打日志 |
| | `is_active_status` | Normal | `wink_status_t {name}_is_active_status(bool*)` | 契约诚实接口，带 `WINK_WARN_UNUSED_RESULT` |
| | `was_active` | Convenience (bool) | `bool {name}_was_active(void)` | 边沿上升沿检测，异常时静默并打日志 |
| | `was_active_status` | Normal | `wink_status_t {name}_was_active_status(bool*)` | 边沿上升沿检测契约诚实接口 |
| | `start_auto_poll` | Fatal | `wink_status_t {name}_start_auto_poll(uint32_t)`| 开启软定时器轮询任务，带 `WINK_WARN_UNUSED_RESULT` |
| | `stop_auto_poll` | Fire-and-forget | `void {name}_stop_auto_poll(void)` | 停止软定时器轮询任务 |
| **`distance_sensor`** | `request_measurement` | Normal | `wink_status_t {name}_request_measurement(void)` | 发起非阻塞测量，带 `WINK_WARN_UNUSED_RESULT` |
| | `read_distance` | Convenience (float) | `float {name}_read_distance(void)` | 返回距离 cm 值，发生错误时返回 `-1.0f` |
| | `read_distance_status` | Normal | `wink_status_t {name}_read_distance_status(float*)` | 契约诚实获取测量值接口，带 `WINK_WARN_UNUSED_RESULT` |
| **`text_display`** | `clear` | Fire-and-forget | `void {name}_clear(void)` | 清除帧缓冲 |
| | `draw_text` | Fire-and-forget | `void {name}_draw_text(uint16_t, uint8_t, const char*)` | 绘制文本 |
| | `flush` | Fire-and-forget | `void {name}_flush(void)` | 将帧缓冲物理刷入屏幕 |
| **`angular_actuator`** | `set_angle` | Fire-and-forget | `void {name}_set_angle(float)` | 舵机角度（度）；内部 `WINK_IGNORE_RESULT` |
| **`open_loop_actuator`** | `set_speed` | Normal | `wink_status_t {name}_set_speed(float)` | 开环占空比/符号速度；**必须**检查返回值 |
| | `coast` | Normal | `wink_status_t {name}_coast(void)` | 滑行（dir 低、PWM 0） |
| | `brake` | Normal | `wink_status_t {name}_brake(void)` | 短路制动（需双 dir 引脚） |
| | `safe_off` | Normal | `wink_status_t {name}_safe_off(void)` | 注册表安全关断（ADR-0048 层级） |
| **`pulse_counter`** | `get_count` | Normal | `wink_status_t {name}_get_count(int32_t*)` | 原始脉冲计数；**无 CPR**；物理换算在 BAL |
| | `reset` | Normal | `wink_status_t {name}_reset(void)` | 计数清零 |

#### 3. 开发规范与兼容性（Escape Hatch）
- ✅ **推荐实践**：用户层 App 代码编写时，推荐全面使用生成的 `{instance_name}_{verb}` 接口。这消除了宏警告，并且实现了完全的芯片无关。
- ✅ **保留逃生通道（Escape Hatch）**：`device_tree.h` 依然正常导出设备全局结构体（如 `extern dal_led_t board_led;`），当遇到复杂的多任务通信或需要直接传递指针的高级场景时，高级用户依然可以通过 `&board_led` 调用底层 DAL。`wink lint --pack user_surface` 对直调 `dal_*` 发 **warn**；官方 sample 可升 **error**；allowlist 须写 `reason=`（见绝缘设计 §4.1）。

#### 4. 用户稳定面 vs 驱动面（Phase 1 契约）

> SSOT 机制：[user-surface-insulation-design.md](../../tech-designs/tools/2026-07-28-user-surface-insulation-design.md)。Owner 裁决：**不上板卡模板锁引脚**；接线仍由各 App `wink-app.json` 灵活填写。

| 面 | 写什么 | 谁改 | 绝缘目标 |
|----|--------|------|----------|
| **用户稳定面** | App C → `{name}_{verb}`；JSON 的 `role` 与 stable knobs（如 `long_press_ms`、`max_angle`） | 业务/低代码用户 | DAL 符号/签名变 → 重 codegen 即可；**动词一旦发布即契约** |
| **驱动面** | JSON 的 `type`、引脚/总线、`drive_mode`、`decode_mode`、`enable_pin` 等 advanced | 每 App 自定 | 改脚 = 正常灵活代价，**不是**缺陷 |
| **Escape Hatch** | `&instance` + `dal_*` / 额外 `#include "dal_*.h"` | 专家 | lint warn + allowlist |
| **BAL-backed** | 事件类动词（如 `enable_events`）内部调 `wink_button_*` 等 BAL | App 仍用 Role 动词 | 属稳定面，但 BAL 行为变更须单独 changelog |

**无板卡模板**：近程不引入「模板锁死引脚 / 简单模式只写 role」；若未来画布需要折叠 advanced，另开设计且须保留每 App 覆盖引脚能力。

**Phase 1 语义钉死（穿透 Role，同名动词行为变 = 破坏性变更）：**

| `type` / Role | 钉死内容 |
|---------------|----------|
| `dc_motor` / `open_loop_actuator` | 默认 `drive_mode = in_in`（PWM + IN_A/IN_B，**不是**业界 Phase/Enable）；[IN/IN 真值表](#in-in-真值表)；`phase_enable` / `pwm_on_in` **预留**未实现 → `WINK_ERR_UNSUPPORTED`；`safe_off` 层级见 [ADR-0048](../../decisions/core/0048-actuator-control-semantic-naming.md) 附录 |
| `encoder` / `pulse_counter` | 默认 `decode_mode = x1_rising`；x2/x4 init fail-closed；`invert=true` = **交换 A/B 方向语义（换相极性）**，非 `count = -count`；Role **`get_count` 无 CPR**——物理换算在 BAL |
| `rc_servo` / `angular_actuator` | `pulse_ms = min_pulse + (angle / effective_max_angle) * (max_pulse - min_pulse)`；`max_angle` 缺省/0 → 180° |
| `ssd1306` / `text_display` | JSON **`type` 保留芯片名 `ssd1306`**（本 Phase 不改名）；App 通过 `text_display` Role 动词，不依赖芯片名 |

##### IN/IN 真值表

`dir_pin_a` = A，`dir_pin_b` = B（与 `dal_dc_motor.h` 一致）：

```text
dir_a  dir_b | state
  0      0   | coast
  1      0   | forward
  0      1   | reverse
  1      1   | brake (short)
```

---

## 3. App 层典型业务逻辑示例

以下为一个由低代码系统生成的、用于控制**避障避险小车**的典型 App 业务代码。

> **⚠️ State Struct 强制模式（ADR-0011）：**
> 所有持久化状态必须使用 `WINK_PT_STATE_*` 系列宏，禁止使用 `static` 局部变量。
> 这确保了：1）无栈协程 yield 后状态不丢失；2）支持多实例重入；3）状态可序列化用于调试/休眠唤醒。

### 3.1 业务核心逻辑：`app_main.c`（State Struct 模式）

```c
#include "device_tree.h"
#include "app_config.h"
#include "wink_app.h"

// 定义系统的四种业务状态
typedef enum {
    SYSTEM_STATE_INIT,      // 系统初始化中
    SYSTEM_STATE_RUNNING,   // 正常前行巡航
    SYSTEM_STATE_AVOIDING,  // 发现障碍避障转动中
    SYSTEM_STATE_EMERGENCY  // 紧急刹车故障状态
} system_state_t;

// ============================================================
//  ✅ 推荐：使用 State Struct 存储所有持久化状态
//  Codegen 自动为每个 task 生成唯一前缀，确保命名空间隔离
// ============================================================
WINK_PT_STATE_BEGIN(app_main)
    system_state_t task_001_current_state;  // 状态机当前状态
    uint32_t       task_001_last_scan_tick; // 上次扫描时间戳
    float          task_001_last_distance;  // 上次雷达读数
WINK_PT_STATE_END()

// 全局状态上下文（由 runtime 持有，支持多实例）
static struct app_main_state g_app_state;
static wink_pt_t g_app_pt;

/**
 * @brief 初始化应用状态与外设初始姿态
 */
void app_init(void) {
    // 初始化协程控制块与状态结构体
    WINK_PT_INIT(&g_app_pt);
    memset(&g_app_state, 0, sizeof(g_app_state));
    g_app_state._magic = 0x50545354UL;  // PTST magic

    // 1. 设置指示灯为常规熄灭状态
    if (dal_led_set_state(&status_led, LED_STATE_OFF) != WINK_OK) {
        app_on_fault(FAULT_STATUS_LED_UNAVAILABLE);
        return;
    }
    
    // 2. 将云台舵机复位到前方 90° 居中方向
    if (dal_rc_servo_set_angle(&neck_servo, 90.0f) != WINK_OK) {
        app_on_fault(FAULT_SERVO_CONTROL_FAILED);
        return;
    }
    
    // ✅ 所有状态通过 state 指针访问（多实例安全）
    g_app_state.task_001_current_state = SYSTEM_STATE_RUNNING;
}

/**
 * @brief 核心业务状态机循环（无栈协程模式）
 * 
 * 所有持久化状态通过 state 指针访问，确保：
 * - yield 后状态不丢失
 * - 多实例运行安全
 * - 状态可序列化用于调试和休眠唤醒
 * 
 * 所有的硬件交互全部基于 DAL 提供的语义级指针与函数
 */
void app_loop(void) {
    // ✅ 注入状态指针（编译期生成，零运行时开销）
    WINK_PT_STATE_USE(app_main);

    // 协程入口：yield 后从这里恢复执行
    WINK_PT_BEGIN(&g_app_pt);

    // =====================================================
    // 协程主循环
    // =====================================================
    while (1) {
        // 1. 调用 DAL 读取前方避障雷达的物理距离（单位: cm）
        //    distance 是栈变量，不会跨 yield 访问 → 安全
        float distance = 0.0f;
        wink_status_t distance_status = dal_ultrasonic_read(&front_radar, &distance);
        if (distance_status != WINK_OK) {
            app_on_fault(FAULT_FRONT_RADAR_UNAVAILABLE);
            WINK_PT_DELAY_MS(&g_app_pt, APP_TICK_RATE_MS);
            continue;
        }

        // ✅ 通过 state-> 访问所有持久化状态
        switch (state->task_001_current_state) {
            case SYSTEM_STATE_RUNNING:
                // 正常巡航模式：雷达探测距离大于安全阈值 (如 20.0cm)
                if (distance > 0.0f && distance < OBSTACLE_THRESHOLD_CM) {
                    // 距离过近，切入避障状态
                    state->task_001_current_state = SYSTEM_STATE_AVOIDING;
                    
                    // 亮红灯警示
                    if (dal_led_set_state(&status_led, LED_STATE_ON) != WINK_OK) {
                        app_on_fault(FAULT_STATUS_LED_UNAVAILABLE);
                        break;
                    }
                    // 指令舵机偏转 180° 进行扫描
                    if (dal_rc_servo_set_angle(&neck_servo, 180.0f) != WINK_OK) {
                        app_on_fault(FAULT_SERVO_CONTROL_FAILED);
                        break;
                    }
                }
                break;
                
            case SYSTEM_STATE_AVOIDING:
                // 避障状态：判断距离是否恢复
                if (distance >= OBSTACLE_THRESHOLD_CM) {
                    // 障碍物清除，返回巡航
                    state->task_001_current_state = SYSTEM_STATE_RUNNING;
                    // 关闭警报灯
                    if (dal_led_set_state(&status_led, LED_STATE_OFF) != WINK_OK) {
                        app_on_fault(FAULT_STATUS_LED_UNAVAILABLE);
                        break;
                    }
                    // 舵机复位
                    if (dal_rc_servo_set_angle(&neck_servo, 90.0f) != WINK_OK) {
                        app_on_fault(FAULT_SERVO_CONTROL_FAILED);
                        break;
                    }
                } else if (distance > 0.0f && distance < EMERGENCY_THRESHOLD_CM) {
                    // 如果在避障过程中距离继续缩短到极危险限值，触发紧急刹车
                    state->task_001_current_state = SYSTEM_STATE_EMERGENCY;
                    app_on_fault(FAULT_OBSTACLE_COLLISION);
                }
                break;
                
            case SYSTEM_STATE_EMERGENCY:
                // 紧急状态：持续闪烁 LED，不恢复，需人工复位或彻底清除障碍
                if (dal_led_set_state(&status_led, LED_STATE_FLASHING) != WINK_OK) {
                    app_on_fault(FAULT_STATUS_LED_UNAVAILABLE);
                    break;
                }
                if (distance > OBSTACLE_THRESHOLD_CM) {
                    // 自动修复机制：障碍清除后允许解锁
                    state->task_001_current_state = SYSTEM_STATE_RUNNING;
                    if (dal_led_set_state(&status_led, LED_STATE_OFF) != WINK_OK) {
                        app_on_fault(FAULT_STATUS_LED_UNAVAILABLE);
                        break;
                    }
                    if (dal_rc_servo_set_angle(&neck_servo, 90.0f) != WINK_OK) {
                        app_on_fault(FAULT_SERVO_CONTROL_FAILED);
                        break;
                    }
                }
                break;
                
            default:
                break;
        }

        // ✅ 协程延时：yield 并在下一个 tick 恢复
        //    栈变量在此之后变为未定义，但我们已将所有状态存入 state struct
        WINK_PT_DELAY_MS(&g_app_pt, APP_TICK_RATE_MS);
    }

    WINK_PT_END(&g_app_pt);
}

/**
 * @brief 异常处理逻辑
 */
void app_on_fault(uint32_t fault_code) {
    // 强制切入安全限位，fault handler 内只做安全动作与记录
    (void)dal_rc_servo_set_angle(&neck_servo, 90.0f);
    (void)dal_led_set_state(&status_led, LED_STATE_ON);
    wink_trace_fault(fault_code);
}
```

---

## 3.2 State Struct 模式的架构优势

| 特性 | static 局部变量 | WINK_PT_STATE 结构体 |
|------|----------------|---------------------|
| **跨 yield 安全** | ✅ 安全（静态存储） | ✅ 安全 |
| **多实例重入** | ❌ 所有实例共享 | ✅ 每个实例独立状态 |
| **Web 仿真调试** | ❌ 无法枚举和监控 | ✅ 可读取完整状态树 |
| **休眠唤醒持久化** | ❌ 无法序列化 | ✅ 直接 dump/restore |
| **故障诊断** | ❌ crash 时无法定位 | ✅ 状态快照用于事后分析 |
| **单元测试** | ❌ 测试间状态污染 | ✅ 每个测试用例独立状态 |

---

## 4. 前端低代码到 App C 语言的生成映射 (Blockly/DSL Codegen)

低代码平台的图形化逻辑（如 Blockly 积木或逻辑流程图节点）可按如下映射规则输出 C 代码。**所有持久化变量必须自动加入 State Struct，禁止使用 `static`：**

```
[ 图形块：读取 "front_radar" 的距离并存入临时变量 "dist" ]
                    │
                    ▼ (Codegen)
// 临时变量在栈上，不跨 yield → 安全
float dist = 0.0f;
wink_status_t status = dal_ultrasonic_read(&front_radar, &dist);
```

```
[ 图形块：持久化变量 "current_state" 存储状态机当前状态 ]
                    │
                    ▼ (Codegen 自动处理)
// 1. 自动在 State Struct 中声明
WINK_PT_STATE_BEGIN(app_main)
    system_state_t task_001_current_state;  // ← 自动生成
WINK_PT_STATE_END()

// 2. 代码中通过 state 指针访问
state->task_001_current_state = SYSTEM_STATE_RUNNING;
```

```
[ 图形块：控制 "neck_servo" 旋转到 120 度 ]
                    │
                    ▼ (Codegen)
`dal_rc_servo_set_angle(&neck_servo, 120.0f);`
```

```
[ 状态机连线：从 "巡航" -> "避障" (条件: 雷达距离 < 20.0) ]
                    │
                    ▼ (Codegen)
`if (distance < 20.0f) { state->task_001_current_state = SYSTEM_STATE_AVOIDING; }`
```

### 4.1 Codegen 命名空间规则（强制）

为避免跨协程的变量命名冲突，Codegen 必须遵守以下命名规则：

| 元素 | 命名格式 | 示例 |
|------|---------|------|
| State Struct 名 | `{task_name}_state` | `app_main_state` |
| 状态成员变量 | `task_{NNN}_{var_name}` | `task_001_current_state` |
| 协程控制块 | `g_{task_name}_pt` | `g_app_main_pt` |

其中 `{NNN}` 是低代码图形化编辑器中节点的唯一 ID，确保每个协程的状态成员在全局命名空间中绝对唯一。

---

### 4.2 架构优势总结

通过这种标准的生成模板，我们达成了以下架构优势：
1. **语法绝对安全**：由于底层的寄存器配置、总线状态机等高危、易崩溃的逻辑全部被死死封装在 DAL/PAL 内部，AI 生成器只需按规则组装应用 C 代码，即使生成的逻辑发生语法错误，也绝对不会损坏底层的固件驱动层（即 OS 核心）。
2. **测试打桩（Mock）极度友好**：测试框架只需简单的创建一个与 `device_tree.h` 同接口的模拟桩文件（例如提供虚拟的 `dal_ultrasonic_read`），就可以在主机上对业务逻辑层进行完整的 CI/CD 单元测试。
3. **多实例原生支持**：所有状态通过指针访问，同一个协程函数可以同时运行多个独立实例（例如多个相同的避障算法运行在不同的机器人上）。
4. **调试与诊断能力**：Web 仿真端可以直接读取完整的状态结构体，在浏览器中展示实时状态树；crash 时可以 dump 完整状态快照用于事后分析。

