# DAL 外设执行分计划：output/buzzer

| 项 | 内容 |
|---|---|
| **计划名称** | `output/buzzer` 蜂鸣器外设驱动与 Codegen 落地计划 |
| **所属批次** | P0 批次 (核心 HMI 声学提示外设) |
| **映射 Wokwi 组件** | `buzzer` (两引脚压电蜂鸣器/发声模块) |
| **驱动文件路径** | `wink-micro-os/dal/include/output/dal_buzzer.h`<br>`wink-micro-os/dal/src/output/dal_buzzer.c` |
| **Codegen 描述** | `wink-micro-os/codegen/drivers/buzzer.yaml`（**ADR-0051：YAML 为 SSOT**） |
| **关联规范** | [`dal-role-architecture-spec.md`](../../../../../wink-micro-os/docs/dal-development-guide/dal-role-architecture-spec.md) §4 (#12 `tone_generator` 与 #2 `binary_indicator`)、[`dal-api-consistency-spec.md`](../../../../../wink-micro-os/docs/dal-development-guide/dal-api-consistency-spec.md) (v3.4.3)、[`dal-best-practices.md`](../../../../../wink-micro-os/docs/dal-development-guide/dal-best-practices.md) §3 (拓扑枚举原则)、[ADR-0056](../../decisions/core/0056-cross-profile-quantity-ab-class-and-scaled-integers.md)、[ADR-0051](../../decisions/tools/0051-scannable-codegen-extension-roots.md) |
| **前置依赖任务** | 1. PAL 增补 `pal_pwm_set_freq(channel, freq_hz)` 接口及其 ESP32 / Host / Wasm 封装；<br>2. Wasm 桥接暴露 `js_pal_pwm_set_freq` 与 Web Audio API 发声支持 |
| **计划状态** | 🔄 Revise Completed（2026-08-07 根据编码前评审意见完成 11 项 P0 + P1/P2/专家补充修正，待编码） |

---

## 1. 需求分析与硬件语义映射

### 1.1 Wokwi 元件与控制语义
- **`buzzer`** 属于 HMI 声学提示与蜂鸣发声执行器。
- **底层物理控制**：
  - **无源蜂鸣器 / 压电扬声器 (Passive Piezo)**：使用指定频率 (Hz) 的 PWM 矩形波驱动线圈/压电片震动发声。
  - **有源蜂鸣器 (Active Buzzer)**：内部自带震荡源，GPIO 高/低电平即可触发固频鸣叫。
- **DAL 抽象控制语义**：**声学发声与频率控制 (Tone Generation & Binary Sound)**。
  - `play_tone` / `play_tone_hz`：以指定 Hz 频率发声（如 2000Hz 提示音、440Hz A4 音符）。
  - `stop_tone` / `off`：停止发声并关闭 PWM/GPIO 输出，防止 DC 直流偏置线圈发热。
  - `on` / `set` / `toggle`：按默认频率开关发声（映射 Role `binary_indicator` 契约）。

### 1.2 量纲与 A/B 分类（ADR-0056）
- `buzzer` 属于 **A 类 actuator_command**（App 控制输出 $\rightarrow$ 硬件，`is_actuator: true`）。
- 声学控制信号，量纲标记为：
  - `quantity: frequency`（主量纲）
  - 对 Role 动词细分：`play_tone_hz` 标 `frequency (unit: hz)`；`activate` / `deactivate` / `toggle` 标 `binary`。
  - `quantity_class: actuator_command`

---

## 2. 硬件拓扑分类与 Variant 架构分析 (依据 `dal-best-practices.md` §3)

### 2.1 架构设计原则
遵照 [`dal-best-practices.md`](../../../../../wink-micro-os/docs/dal-development-guide/dal-best-practices.md) §3「同类芯片 / 模块：语义不变 + 拓扑枚举」设计心法：
1. **对外 API 绝对冻结**：统一暴露 `on` / `off` / `set` / `toggle` / `play_tone` / `stop_tone` / `is_on`，绝不因有源/无源蜂鸣器改变 API。
2. **拓扑为一等公民 (`variant`)**：使用内部拓扑枚举 `dal_buzzer_variant_t` 消化无源 PWM 驱动与有源 GPIO 直驱的逻辑差异。
3. **C 头文件绝对无具体芯片型号名**：具体型号仅作为 Codegen 设备树别名，由 `app_codegen` 自动映射为 `variant` + 默认频率与极性配置。

### 2.2 市面主流蜂鸣器产品/模块全量盘点

| 硬件拓扑分类 (Variant) | 市面典型产品/模块型号 | 电气驱动机制 | 对应 Wokwi 元件 | 默认驱动动作 |
|---|---|---|---|---|
| **`PASSIVE_PWM` (无源蜂鸣器/压电片, 默认)** | 压电蜂鸣片、5V 无源蜂鸣器模块、小型喇叭/扬声器 | MCU PWM 输出方波震动发声，调节 PWM 频率改变音调 (Pitch) | Wokwi `buzzer` (物理对应无源压电发声) | 调用 `pal_pwm_init_ex`，`on()` 使用 `default_freq_hz` 50% 占空比发声；`active_high` 在此变体忽略 |
| **`ACTIVE_GPIO` (有源蜂鸣器)** | 5V/3.3V 有源蜂鸣器模块、高分贝报警电笛 | 内部自带多谐振荡器，MCU GPIO 直驱拉高/拉低控制开关 | Wokwi `buzzer` (仿真中以高低电平控制) | 依赖 `pal_gpio_write()` 控制高/低电平，`play_tone(f)` 当 `f>0` 退化为 `on()`，`f==0` 退化为 `off()` |

### 2.3 Codegen 市场型号别名映射表 (`aliases`)

在 `wink-micro-os/codegen/drivers/buzzer.yaml` 中提供别名映射（统一使用 `active_high` 极性名）：

```yaml
aliases:
  buzzer:         { variant: passive_pwm, default_freq_hz: 2000, active_high: true }
  passive_buzzer: { variant: passive_pwm, default_freq_hz: 2000, active_high: true }
  active_buzzer:  { variant: active_gpio, default_freq_hz: 0,    active_high: true }
  piezo_speaker:  { variant: passive_pwm, default_freq_hz: 1000, active_high: true }
```

---

## 3. 数据结构设计 (`dal_buzzer.h`)

遵循 [`dal-api-consistency-spec.md`](../../../../../wink-micro-os/docs/dal-development-guide/dal-api-consistency-spec.md) §2 结构体规范及 `DAL-S-001` / `DAL-S-006` / `DAL-S-011` / `DAL-S-014` / `DAL-HDR-NO-HAL`：

```c
#ifndef DAL_BUZZER_H
#define DAL_BUZZER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"
/* 严格遵循 DAL-HDR-NO-HAL: 禁止 include "pal_hal.h" */

#ifdef __cplusplus
extern "C" {
#endif

#define DAL_BUZZER_DEFAULT_FREQ_HZ 2000u
#define DAL_BUZZER_MIN_FREQ_HZ     20u
#define DAL_BUZZER_MAX_FREQ_HZ     8000u /* 对应 ESP32 13-bit AUTO 分辨率下上限，如需更高频率可降分辨率 */

/**
 * @brief 蜂鸣器拓扑变体枚举 (Topology Variant - 一等公民)
 */
typedef enum {
    DAL_BUZZER_VARIANT_PASSIVE_PWM = 0, /**< 无源蜂鸣器 (PWM 频率驱动, 默认) */
    DAL_BUZZER_VARIANT_ACTIVE_GPIO = 1, /**< 有源蜂鸣器 (GPIO 高低电平开关) */
} dal_buzzer_variant_t;

/**
 * @brief 蜂鸣器配置结构体 (POD config_t)
 * 寻址模型隔离：ACTIVE_GPIO 使用 pin(uint16_t)；PASSIVE_PWM 使用 pwm_channel(uint8_t)；
 * 可选使能脚使用 enable_pin(int16_t, -1 哨兵)。
 */
typedef struct {
    const char          *owner;               /**< 资源占用者名称 (DAL-S-001: 必须为首成员指针) */
    uint32_t             default_freq_hz;     /**< 默认发声频率 Hz (默认 2000Hz) */
    uint16_t             pin;                 /**< ACTIVE_GPIO: 必填 GPIO 引脚 (DAL-S-006) */
    int16_t              enable_pin;          /**< 可选电源/使能引脚 (-1 表示未绑定) */
    uint8_t              pwm_channel;         /* PASSIVE_PWM: 逻辑通道 [0, PAL_PWM_CHANNELS) */
    bool                 active_high;         /**< 触发极性: true=高电平有效 (PASSIVE_PWM 下忽略) */
    bool                 enable_active_high;  /**< 使能脚极性: true=高电平使能 */
    uint8_t              _pad0;               /**< 内存对齐填充 */
    dal_buzzer_variant_t variant;             /**< 拓扑变体枚举 */
} dal_buzzer_config_t;

/**
 * @brief 蜂鸣器句柄结构体 (POD instance_t)
 */
typedef struct {
    dal_buzzer_config_t config;               /**< 配置副本 (DAL-S-011: 值副本且 offsetof == 0) */
    uint32_t            current_freq_hz;      /**< 当前发声频率 Hz (0 表示静音) */
    bool                is_on;                /**< 当前是否处于发声状态 */
    bool                initialized;          /**< 初始化状态标记 (DAL-L-004) */
    uint8_t             _pad0[2];             /**< 内存对齐填充 */
} dal_buzzer_t;

/* Static assertions for ABI freeze and first-member guard (DAL-S-011 / DAL-S-014) */
_Static_assert(offsetof(dal_buzzer_t, config) == 0, "config must be the first member");

#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_buzzer_config_t) == 20, "ABI break: config size changed on 32-bit target");
_Static_assert(offsetof(dal_buzzer_t, initialized) == 25, "ABI break: initialized offset changed on 32-bit");
_Static_assert(sizeof(dal_buzzer_t) == 28, "ABI break: handle size changed on 32-bit target");
#else                         /* LP64 / LLP64: 64-bit Host Simulation */
_Static_assert(sizeof(dal_buzzer_config_t) == 24, "ABI break: config size changed on 64-bit host");
_Static_assert(offsetof(dal_buzzer_t, initialized) == 29, "ABI break: initialized offset changed on 64-bit host");
_Static_assert(sizeof(dal_buzzer_t) == 32, "ABI break: handle size changed on 64-bit host");
#endif

WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_init(dal_buzzer_t *dev, const dal_buzzer_config_t *cfg);

wink_status_t dal_buzzer_deinit(dal_buzzer_t *dev);

WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_on(dal_buzzer_t *dev);

WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_off(dal_buzzer_t *dev);

WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_set(dal_buzzer_t *dev, bool on);

WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_toggle(dal_buzzer_t *dev);

WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_play_tone(dal_buzzer_t *dev, uint32_t freq_hz);

WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_stop_tone(dal_buzzer_t *dev);

WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_is_on(const dal_buzzer_t *dev, bool *out_on);

/* Actuator safe_off 契约: 返回 status，无 WINK_WARN_UNUSED_RESULT (DAL-L-021) */
wink_status_t dal_buzzer_safe_off(dal_buzzer_t *dev);

#ifdef __cplusplus
}
#endif

/* Compile-time pruning stubs */
#if !defined(WINK_USE_BUZZER) || !WINK_USE_BUZZER
#define WINK_BUZZER_DISABLED_MSG \
    "Buzzer driver not enabled; add a \"buzzer\" device to wink-app.json " \
    "(or set -DWINK_USE_BUZZER=ON)."
WINK_UNAVAILABLE_MSG(WINK_BUZZER_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_init(dal_buzzer_t *dev, const dal_buzzer_config_t *cfg);
WINK_UNAVAILABLE_MSG(WINK_BUZZER_DISABLED_MSG)
wink_status_t dal_buzzer_deinit(dal_buzzer_t *dev);
WINK_UNAVAILABLE_MSG(WINK_BUZZER_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_on(dal_buzzer_t *dev);
WINK_UNAVAILABLE_MSG(WINK_BUZZER_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_off(dal_buzzer_t *dev);
WINK_UNAVAILABLE_MSG(WINK_BUZZER_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_set(dal_buzzer_t *dev, bool on);
WINK_UNAVAILABLE_MSG(WINK_BUZZER_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_toggle(dal_buzzer_t *dev);
WINK_UNAVAILABLE_MSG(WINK_BUZZER_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_play_tone(dal_buzzer_t *dev, uint32_t freq_hz);
WINK_UNAVAILABLE_MSG(WINK_BUZZER_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_stop_tone(dal_buzzer_t *dev);
WINK_UNAVAILABLE_MSG(WINK_BUZZER_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_is_on(const dal_buzzer_t *dev, bool *out_on);
WINK_UNAVAILABLE_MSG(WINK_BUZZER_DISABLED_MSG)
wink_status_t dal_buzzer_safe_off(dal_buzzer_t *dev);
#endif /* !WINK_USE_BUZZER */

#endif /* DAL_BUZZER_H */
```

---

## 4. C 驱动核心逻辑与 API 规则 (`dal_buzzer.c`)

### 4.1 初始化与上电时序 (`dal_buzzer_init`)
1. 校验指针非空及 `owner` 非空；防重复初始化校验（已初始化返 `WINK_ERR_ALREADY_INITIALIZED`）。
2. **资源申请与校验**：
   - **`PASSIVE_PWM`**：校验 `pwm_channel < PAL_PWM_CHANNELS`，索取资源 `pal_resource_claim(PAL_RESOURCE_PWM_CHANNEL, pwm_channel, owner)`。
   - **`ACTIVE_GPIO`**：索取资源 `pal_resource_claim(PAL_RESOURCE_GPIO_PIN, pin, owner)`。
   - 若 `enable_pin >= 0`，额外索取 `PAL_RESOURCE_GPIO_PIN`（冲突时回滚先前资源）。
3. **上电防爆音/防死锁时序 (Power-On Sequencing)**：
   - 第一步：若有 `enable_pin`，配置为 GPIO 输出，并拉到**无效（关断）电平**。
   - 第二步：配置主驱动输出为**零能量静音状态**：
     - `PASSIVE_PWM`：配置 `pal_pwm_config_t`（`freq_hz = default_freq_hz`, `resolution_bits = 0 (AUTO)`），调 `pal_pwm_init_ex(dev->config.pwm_channel, &pwm_cfg)`；显式设 duty 0.0f；并拉低脚防 DC 偏置。
     - `ACTIVE_GPIO`：配置为 `PAL_GPIO_OUTPUT_PUSH_PULL`，输出无效（OFF）电平。
   - 第三步：若有 `enable_pin`，拉到**使能电平**。
4. 状态归零：`current_freq_hz = 0`, `is_on = false`, `initialized = true`。

### 4.2 发声与改频逻辑 (`dal_buzzer_play_tone` / `dal_buzzer_on`)
- **`play_tone(freq_hz)` 状态机与边界**：
  - **`freq_hz == 0`**：等价于 `stop_tone()`，静音并返回 `WINK_OK`。
  - **`freq_hz < DAL_BUZZER_MIN_FREQ_HZ` 或 `> DAL_BUZZER_MAX_FREQ_HZ`**：返回 `WINK_ERR_OUT_OF_RANGE`。
  - **`PASSIVE_PWM` 变体**：调用 PAL 原语 `pal_pwm_set_freq(dev->config.pwm_channel, freq_hz)`；更新 duty 为 0.5f (50% 方波发声)；设置 `dev->current_freq_hz = freq_hz`, `dev->is_on = true`。
  - **`ACTIVE_GPIO` 变体（退化处理）**：有源蜂鸣器无法调节频率。当 `freq_hz > 0` 时退化为 `dal_buzzer_on(dev)`（拉高/低 GPIO 发声，`current_freq_hz` 记为 `default_freq_hz`）。
- **`on()` 语义**：`on()` 是 `play_tone(dev->config.default_freq_hz)` 的语义糖。若之前调过 `play_tone(440)`，再次调 `on()` 会重置回默认频率发声。
- **`off()` / `stop_tone()` & DC 偏置防护**：
  - `PASSIVE_PWM` 变体设 PWM duty 为 0.0f，停止发声，并确保 GPIO 处于低电平（避免直流过流发热）。
  - `ACTIVE_GPIO` 变体写无效电平。
  - 更新 `dev->current_freq_hz = 0`, `dev->is_on = false`。

### 4.3 三段式 Safe-Off 与 Deinit (DAL-L-014/015/021/022)
- **`dal_buzzer_safe_off(dev)`**：
  - NULL 指针返 `WINK_ERR_INVALID_ARG`；未初始化返 `WINK_OK`（幂等）。
  - Best-effort 停止发声、设 duty 0.0f、拉低 GPIO、拉低使能脚。
- **`dal_buzzer_deinit(dev)`**：
  1. 调用 `dal_buzzer_safe_off(dev)`；
  2. 解绑底层硬件：`PASSIVE_PWM` 调 `pal_pwm_deinit(pwm_channel)`；`ACTIVE_GPIO` 调 `pal_gpio_reset_pin(pin)`；
  3. 释放资源：`pal_resource_release`（LOGW 记录失败但不中断流程）；
  4. 句柄清零：`memset(dev, 0, sizeof(*dev))`。

---

## 5. 运行时防线 (5 Safety Guards) 落实

| 防线 | 条款标准 | 落地实现方案 |
|---|---|---|
| **Guard A: 低功耗** | 关断省电 | `deinit()` 或 `safe_off()` 时强制设 PWM Duty 为 0、拉低物理引脚与 `enable_pin`，防止漏电与发热 |
| **Guard B: Bus 共享** | 资源独占 | `init()` 时按变体 claim `PAL_RESOURCE_PWM_CHANNEL` 或 `PAL_RESOURCE_GPIO_PIN`，冲突自动回滚 |
| **Guard C: Zero-as-Default** | 零值推导 | `variant` 默认 `PASSIVE_PWM`；`default_freq_hz == 0` 时自动推导为默认 2000Hz |
| **Guard D: 硬件托底** | DC偏置与防音爆 | 调 `pal_pwm_set_freq()` 依赖影子寄存器平滑更新；关闭发声时强制拉低 GPIO，切断直流偏置电流 |
| **Guard E: 非阻塞** | `WINK_STRICT_NONBLOCKING` | 严禁使用死等 delay 控制发声持续时间！旋律/提示音持续时间由 BAL 软定时器调度 |

---

## 6. Codegen 驱动描述 (`buzzer.yaml`)

新建 `wink-micro-os/codegen/drivers/buzzer.yaml`（ Schema 1.1 ）：

```yaml
codegen_schema: "1.1"
type: buzzer
category: output
is_actuator: true
experimental: false
default_role: tone_generator

aliases:
  buzzer:         { variant: passive_pwm, default_freq_hz: 2000, active_high: true }
  passive_buzzer: { variant: passive_pwm, default_freq_hz: 2000, active_high: true }
  active_buzzer:  { variant: active_gpio, default_freq_hz: 0,    active_high: true }
  piezo_speaker:  { variant: passive_pwm, default_freq_hz: 1000, active_high: true }

quantity: frequency
quantity_class: actuator_command

fields:
  owner:
    tier: stable
    type: string
    emit: none
  default_freq_hz:
    tier: advanced
    type: int
    default: 2000
  gpio_pin:
    tier: advanced
    type: int
    default: 0
    c: pin
  enable_pin:
    tier: advanced
    type: int
    default: -1
  pwm_channel:
    tier: advanced
    type: int
    default: 0
  active_high:
    tier: advanced
    type: bool
    default: true
  enable_active_high:
    tier: advanced
    type: bool
    default: true
  variant:
    tier: advanced
    type: enum
    enum: [passive_pwm, active_gpio]
    default: passive_pwm
    map:
      passive_pwm: DAL_BUZZER_VARIANT_PASSIVE_PWM
      active_gpio: DAL_BUZZER_VARIANT_ACTIVE_GPIO
  role:
    tier: stable
    type: string
    emit: none

quantities:
  owner:              { quantity_class: actuator_command }
  default_freq_hz:    { quantity: frequency, unit: hz, quantity_class: actuator_command }
  gpio_pin:           { quantity_class: actuator_command }
  enable_pin:         { quantity_class: actuator_command }
  pwm_channel:        { quantity_class: actuator_command }
  active_high:        { quantity_class: actuator_command }
  enable_active_high: { quantity_class: actuator_command }
  variant:            { quantity_class: actuator_command }
  role:               { quantity_class: actuator_command }

config:
  c_type: dal_buzzer_t
  config_type: dal_buzzer_config_t
  headers: [output/dal_buzzer.h]
  init_fn: dal_buzzer_init
  deinit_fn: dal_buzzer_deinit
  init_template_file: templates/buzzer_init.c.j2
  safe_off_fn: dal_buzzer_safe_off

role_bindings:
  tone_generator:
    covers_contract: full
    headers: []
    verbs:
      play_tone_hz:
        template: "static inline void {{ name }}_play_tone_hz(uint32_t freq_hz) { WINK_IGNORE_RESULT(dal_buzzer_play_tone(&{{ name }}, freq_hz)); }"
      stop_tone:
        template: "static inline void {{ name }}_stop_tone(void) { WINK_IGNORE_RESULT(dal_buzzer_stop_tone(&{{ name }})); }"

  binary_indicator:
    covers_contract: full
    headers: []
    verbs:
      activate:
        template: "static inline void {{ name }}_activate(void) { WINK_IGNORE_RESULT(dal_buzzer_on(&{{ name }})); }"
      deactivate:
        template: "static inline void {{ name }}_deactivate(void) { WINK_IGNORE_RESULT(dal_buzzer_off(&{{ name }})); }"
      toggle:
        template: "static inline void {{ name }}_toggle(void) { WINK_IGNORE_RESULT(dal_buzzer_toggle(&{{ name }})); }"
```

---

## 7. Codegen 初始化模板 (`buzzer_init.c.j2`)

新建 `wink-micro-os/codegen/drivers/templates/buzzer_init.c.j2`：

```jinja2
    static const dal_buzzer_config_t {{ name }}_cfg = {
        .owner = "{{ name }}",
        .default_freq_hz = {{ default_freq_hz | default(2000) }}u,
        .pin = {{ gpio_pin | default(0) }}u,
        .enable_pin = {{ enable_pin | default(-1) }},
        .pwm_channel = {{ pwm_channel | default(0) }}u,
        .active_high = {% if active_high | default(true) %}true{% else %}false{% endif %},
        .enable_active_high = {% if enable_active_high | default(true) %}true{% else %}false{% endif %},
        .variant = DAL_BUZZER_VARIANT_{{ variant | default('passive_pwm') | upper }},
    };
    WINK_TRY(dal_buzzer_init(&{{ name }}, &{{ name }}_cfg));
```

---

## 8. Wasm / Wokwi 仿真映射契约

- **Wokwi 元件**：`buzzer`（压电发声模块，对应 `PASSIVE_PWM` 变体）。
- **前置扩展依赖**：
  - 在 PAL wasm 层 (`pal_hal_wasm.c` / `wasm_bridge.h`) 补充 `js_pal_pwm_set_freq(channel, freq_hz)` 桥接导出；
  - 前端 Web Audio API 绑定 PWM 通道频率变化，实现浏览器发声。
- **降级支持**：若 wasm 音频桥接未就绪，wasm 环境下 `pal_pwm_set_freq` 走静音 stub，不影响 CI / Host 单测编译。

---

## 9. 交付 Checklist 与验证步骤

- [ ] **1. PAL 前置原语扩展**：在 `pal_hal.h` 声明并实现 `pal_pwm_set_freq(channel, freq_hz)`（ESP32 封装 `ledc_set_freq`，Host/Wasm 提供 stub）。
- [ ] **2. 脚手架搭建**：创建 `dal_buzzer.h` / `dal_buzzer.c` / `buzzer.yaml` / `buzzer_init.c.j2`。
- [ ] **3. C 驱动实现**：
  - 区分 `PASSIVE_PWM` / `ACTIVE_GPIO` 拓扑路由与资源 claim；
  - 上电防爆音时序；
  - DC 偏置低电平防护；
  - `play_tone(0)` 幂等与频率范围校验。
- [ ] **4. 单元测试编写 (`test_dal_buzzer.c`)**：
  - 测试用例全覆盖：① PWM / GPIO 双拓扑路由校验；② 极性与使能脚时序；③ 频率边界（0->off, 19/8001->OUT_OF_RANGE, 20/2000->OK）；④ init 资源冲突回滚；⑤ 重复 init / 未 init 错误码；⑥ `safe_off` 幂等；⑦ `deinit` 三段式资源释放。
- [ ] **5. ABI 断言核验**：使用编译产物实测并核对 `sizeof` 与 `offsetof` 断言。
- [ ] **6. 静态 Lint 门禁**：运行 `python wink-tools/wink.py lint arch --pack layering --pack api` 全绿无告警。
- [ ] **7. 闭环状态更新**：更新 `00-master-execution-plan.md` 进度状态。

