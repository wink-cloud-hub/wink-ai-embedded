# DAL led & rgb_led 外设重构与架构解耦实施计划

| 项 | 内容 |
|---|---|
| **文档名称** | DAL led 外设重构与 rgb_led 独立拆分实施计划 |
| **文档路径** | `docs/implementation-plans/frontend/07-output-led-refactor-plan.md` |
| **版本** | v1.0.0 |
| **日期** | 2026-08-07 |
| **上级计划** | [`00-master-execution-plan.md`](00-master-execution-plan.md), [`00.1-category-type-variant-wokwi-ssot.md`](00.1-category-type-variant-wokwi-ssot.md) |

---

## 1. 重构背景与边界 A 裁决诊断

在现网 `dal_led.h` 及 SSOT 规范中，关于 `led` 外设存在以下严重的架构边界混淆：

1. **单色 LED vs RGB LED 的 API 与控制量破坏**：
   - **单色 LED (`led`)**：物理控制量为 **bool 开关状态 (On/Off)**，核心 C API 签名维系在 `dal_led_set(dev, bool)` / `dal_led_toggle(dev)`。
   - **RGB LED (`rgb_led`)**：物理控制量为 **24位 R/G/B 三原色亮度和 PWM 占空比**，核心 C API 签名必然要求 `dal_rgb_led_set_rgb(dev, r, g, b)` 或 `dal_rgb_led_set_color(dev, hex_color)`。
2. **边界 A (Variant vs Type) 强制判定**：
   - SSOT 边界 A 明确规定：“若物理控制量单位改变或核心 C API 签名遭到破坏，**必须新建 Type**。”
   - 现网 SSOT 尝试把 `rgb_common_anode` / `rgb_common_cathode` 作为 Variant 强行塞入 `led` Type，导致 `dal_led_config_t` 结构体被迫膨胀 4 个引脚字段（`pin_r`, `pin_g`, `pin_b`, `pin_com`），且使得 `dal_led_set(bool)` 函数在 RGB Variant 下语义失效。
3. **架构解耦决定**：
   - **`led` Type**：专注于单色 GPIO 开关 LED（及 Active High / Active Low 极性变体）。
   - **`rgb_led` Type (新建 Type)**：独立提炼为 3 通道 PWM/GPIO 颜色控制外设，提供独立的 `dal_rgb_led` API 签名与结构体。

---

## 2. 详细设计规范

### 2.1 `led` DAL Type 规范 (单色 LED)

#### C API 头文件设计 (`dal_led.h`)
```c
/**
 * @brief Single color LED variant (affects_pins: false)
 */
typedef enum {
    DAL_LED_VARIANT_SINGLE_COLOR = 0,  /**< Standard single-pin GPIO LED */
} dal_led_variant_t;

/**
 * @brief LED configuration struct
 */
typedef struct {
    const char       *owner;      /**< Instance owner static string */
    dal_led_variant_t variant;    /**< LED variant */
    uint16_t          pin;        /**< Logical GPIO pin */
    bool              active_high;/**< true: Active High, false: Active Low */
} dal_led_config_t;
```

---

### 2.2 `rgb_led` 新建 DAL Type 规范 (三色 RGB LED)

#### C API 头文件设计 (`dal_rgb_led.h`)
```c
/**
 * @brief RGB LED variant — determines electrical polarity (affects_pins: false)
 */
typedef enum {
    DAL_RGB_LED_VARIANT_COMMON_ANODE   = 0, /**< Common Anode (COM to VCC, Low active) */
    DAL_RGB_LED_VARIANT_COMMON_CATHODE = 1, /**< Common Cathode (COM to GND, High active) */
} dal_rgb_led_variant_t;

/**
 * @brief RGB LED configuration struct (3-channel PWM/GPIO)
 */
typedef struct {
    const char            *owner;     /**< Instance owner static string */
    dal_rgb_led_variant_t  variant;   /**< Polarity variant */
    uint16_t               pin_r;     /**< Red channel GPIO pin */
    uint16_t               pin_g;     /**< Green channel GPIO pin */
    uint16_t               pin_b;     /**< Blue channel GPIO pin */
    uint16_t               pin_com;   /**< Optional Common pin (-1 if tied to VCC/GND directly) */
    bool                   use_pwm;   /**< true: PWM dimming (0-255), false: GPIO On/Off */
} dal_rgb_led_config_t;

/**
 * @brief Set RGB LED color values (0-255 per channel)
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_rgb_led_set_rgb(dal_rgb_led_t *dev, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Set RGB LED 24-bit HEX color (0xRRGGBB)
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_rgb_led_set_hex(dal_rgb_led_t *dev, uint32_t hex_color);
```

---

## 3. Codegen Driver YAML 契约

### (1) `led.yaml`
```yaml
codegen_schema: "1.1"
type: led
category: output
source_stem: led

fields:
  - name: variant
    type: enum
    enum: [single_color]
    default: single_color
    map:
      single_color: DAL_LED_VARIANT_SINGLE_COLOR
    affects_pins: false

  - name: pin
    type: int
  - name: active_high
    type: bool
    default: true

wokwi_binding:
  element: wokwi-led
```

### (2) `rgb_led.yaml` (新建 Type)
```yaml
codegen_schema: "1.1"
type: rgb_led
category: output
source_stem: rgb_led

fields:
  - name: variant
    type: enum
    enum: [common_anode, common_cathode]
    default: common_anode
    map:
      common_anode: DAL_RGB_LED_VARIANT_COMMON_ANODE
      common_cathode: DAL_RGB_LED_VARIANT_COMMON_CATHODE
    affects_pins: false

  - name: pin_r
    type: int
  - name: pin_g
    type: int
  - name: pin_b
    type: int
  - name: pin_com
    type: int
    default: -1
  - name: use_pwm
    type: bool
    default: true

wokwi_binding:
  element: wokwi-rgb-led
```

---

## 4. 任务拆解与 SSOT 关联

1. **`led` 与 `rgb_led` 拆分**：在 SSOT §2.2 中将 RGB LED 从 `led` Type 的 Variant 中移出，新建 `rgb_led` Type 行。
2. **C 代码保持纯粹**：`dal_led.h` 仅维护 `single_color` 逻辑，消除 ABI 膨胀风险。
3. **新建 `dal_rgb_led.h/.c`**：实现 3 通道 PWM 调光与开关映射。
