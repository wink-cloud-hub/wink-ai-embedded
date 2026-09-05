# DAL ultrasonic 外设重构与 Wokwi 拓扑解耦实施计划

| 项 | 内容 |
|---|---|
| **文档名称** | DAL ultrasonic 外设重构与 Wokwi 拓扑解耦实施计划 |
| **文档路径** | `docs/implementation-plans/frontend/06-sensor-ultrasonic-refactor-plan.md` |
| **版本** | v1.1.0 (修正对齐 SSOT v2.3.3 规则门禁、对象式 YAML 模式与前端外设包拓扑任务) |
| **日期** | 2026-08-09 |
| **上级计划** | [`00-master-execution-plan.md`](00-master-execution-plan.md), [`00.1-category-type-variant-wokwi-ssot.md`](00.1-category-type-variant-wokwi-ssot.md) |

---

## 1. 重构背景与问题诊断

现网 `dal_ultrasonic.h` 及底层驱动实现存在以下架构缺陷与脱节问题：

1. **变体枚举缺失与模式单一**：
   - 现网 C 头文件 `dal_ultrasonic_config_t` 中只有 `trig_pin` 与 `echo_pin`，仅支持最简单的 4Pin 驱动模式，甚至完全没有 `dal_ultrasonic_variant_t` 枚举类型。
   - 现实硬件中超声波传感器存在 4 种典型硬件接口与协议形态：4Pin 双脚脉冲式 (`hcsr04`)、3Pin 单脚半双工脉冲式 (`single_pin_ping`)、串口数据帧式 (`uart_stream`) 以及 I2C 寄存器式 (`i2c`)。
2. **SOC 硬件解调引擎泄漏至变体**：
   - 过去设计中混入了 `use_rmt` 或 `rmt_hcsr04` 变体的概念。将 MCU 内部解调机制（ESP32 RMT vs GPIO 轮询 vs Timer 输入捕获）提升为 Variant 属于架构误区（违反 SSOT 边界 C）。
   - 传感器硬件本身无感知 MCU 是如何进行脉冲宽度的微秒级捕获的。因此 `use_rmt` 必须下沉为 `config.backend`（如 `dal_ultrasonic_backend_t`）。
3. **wink-tools 平铺配置与 -1 Sentinel 适配**：
   - 采用平铺配置结构体 `dal_ultrasonic_config_t`，利用 `wink-tools` (Python Codegen `emit_config.py`) 的 `variant_fields` 机制：当选择 `hcsr04` 变体时，未使用的单脚 `sig_pin` 或 `uart_port` / `i2c_port` 等字段由工具链自动裁剪并初始化赋值为 `-1`，确保 ABI 稳定性与驱动层零风险识别。

---

## 2. 详细设计规范

### 2.1 C API 头文件设计 (`dal_ultrasonic.h`)

#### (1) Variant 与 Backend 枚举定义
```c
/**
 * @brief Ultrasonic sensor physical variant (affects pinout, affects_pins: true)
 */
typedef enum {
    DAL_ULTRASONIC_VARIANT_HCSR04          = 0, /**< Default: 4Pin dual-pin pulse (HC-SR04/HY-SRF05) */
    DAL_ULTRASONIC_VARIANT_SINGLE_PIN_PING = 1, /**< 3Pin single-pin bidirectional pulse (Parallax PING))) */
    DAL_ULTRASONIC_VARIANT_UART_STREAM     = 2, /**< 4Pin UART serial stream mode (US-100/A02YYUW) */
    DAL_ULTRASONIC_VARIANT_I2C             = 3, /**< 4Pin I2C register mode (Devantech SRF02/SRF08) */
    DAL_ULTRASONIC_VARIANT_COUNT           = 4, /**< Total variant count for static assertion */
} dal_ultrasonic_variant_t;

/**
 * @brief MCU pulse capture demodulation backend (affects_pins: false)
 */
typedef enum {
    DAL_ULTRASONIC_BACKEND_AUTO      = 0, /**< Auto select (ESP32 RMT hardware capture if available, GPIO poll fallback) */
    DAL_ULTRASONIC_BACKEND_GPIO_POLL = 1, /**< Standard GPIO polling / ISR capture */
    DAL_ULTRASONIC_BACKEND_ESP32_RMT = 2, /**< ESP32 RMT peripheral hardware pulse capture */
} dal_ultrasonic_backend_t;
```

#### (2) Config 结构体与 ABI 静态断言 (`_Static_assert`)
```c
/**
 * @brief Ultrasonic sensor configuration struct (Flat layout with sentinel trimming)
 */
typedef struct {
    const char               *owner;      /**< Instance owner static string */
    uint32_t                  baud_rate;  /**< Serial baud rate (typically 9600) */
    uint32_t                  timeout_us; /**< Measurement timeout threshold in µs (default 30000us) */
    dal_ultrasonic_variant_t  variant;    /**< Hardware interface variant */
    dal_ultrasonic_backend_t  backend;    /**< MCU pulse capture backend (pulse modes only) */
    int16_t                   trig_pin;   /**< Trigger pin (-1 if unused) */
    int16_t                   echo_pin;   /**< Echo pin (-1 if unused) */
    int16_t                   sig_pin;    /**< Bidirectional SIG pin (-1 if unused) */
    uint16_t                  i2c_addr;   /**< 7-bit I2C slave address (e.g. 0x70) */
    uint8_t                   uart_port;  /**< Logical UART port index */
    uint8_t                   i2c_port;   /**< Logical I2C port index */
    uint8_t                   _reserved[2];/**< ABI alignment padding */
} dal_ultrasonic_config_t;

/* --- SSOT §5.1 Mandatory Static Assertions --- */
_Static_assert(DAL_ULTRASONIC_VARIANT_COUNT == 4, 
               "Variant count mismatch with SSOT §2 and codegen YAML");
_Static_assert(DAL_ULTRASONIC_VARIANT_I2C + 1 == DAL_ULTRASONIC_VARIANT_COUNT, 
               "Sequential variant ordering error: last member index check failed");

#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_ultrasonic_config_t) == 32, "ABI break: config size changed on 32-bit target");
_Static_assert(offsetof(dal_ultrasonic_t, initialized) == 48, "ABI break: initialized offset changed on 32-bit");
_Static_assert(sizeof(dal_ultrasonic_t) == 52, "ABI break: handle size changed on 32-bit target");
#else                         /* LP64 / LLP64: 64-bit Host Simulation */
_Static_assert(sizeof(dal_ultrasonic_config_t) == 40, "ABI break: config size changed on 64-bit host");
_Static_assert(offsetof(dal_ultrasonic_t, initialized) == 56, "ABI break: initialized offset changed on 64-bit host");
_Static_assert(sizeof(dal_ultrasonic_t) == 64, "ABI break: handle size changed on 64-bit host");
#endif
```

---

### 2.2 Codegen Driver YAML 契约 (`ultrasonic.yaml`)

> ⚠️ **SSOT §5.1 门禁约束**：Codegen Driver YAML **严禁包含 `wokwi_binding` 字段**。仿真 Element 映射与引脚覆盖完全由前端外设包（§2.3）承担。YAML 仅维护对象式字段定义、`tier` 属性、`role_bindings` 动词模板与 `device_specific_apis` 隔离列表。

```yaml
codegen_schema: "1.1"
type: ultrasonic
category: sensor
is_actuator: false
experimental: false
default_role: distance_sensor

quantity: distance
quantity_class: sensor_measurement

fields:
  variant:
    tier: advanced
    type: enum
    enum: [hcsr04, single_pin_ping, uart_stream, i2c]
    default: hcsr04
    map:
      hcsr04: DAL_ULTRASONIC_VARIANT_HCSR04
      single_pin_ping: DAL_ULTRASONIC_VARIANT_SINGLE_PIN_PING
      uart_stream: DAL_ULTRASONIC_VARIANT_UART_STREAM
      i2c: DAL_ULTRASONIC_VARIANT_I2C
    affects_pins: true
    variant_fields:
      hcsr04: [trig_pin, echo_pin, backend, timeout_us]
      single_pin_ping: [sig_pin, backend, timeout_us]
      uart_stream: [uart_port, baud_rate, timeout_us]
      i2c: [i2c_port, i2c_addr, timeout_us]

  backend:
    tier: advanced
    type: enum
    enum: [auto, gpio_poll, esp32_rmt]
    default: auto
    map:
      auto: DAL_ULTRASONIC_BACKEND_AUTO
      gpio_poll: DAL_ULTRASONIC_BACKEND_GPIO_POLL
      esp32_rmt: DAL_ULTRASONIC_BACKEND_ESP32_RMT

  trig_pin:
    tier: advanced
    type: int
    required: true
    min: 0
    max: 39

  echo_pin:
    tier: advanced
    type: int
    required: true
    min: 0
    max: 39

  sig_pin:
    tier: advanced
    type: int
    min: 0
    max: 39

  uart_port:
    tier: advanced
    type: int
    default: 0

  baud_rate:
    tier: advanced
    type: int
    default: 9600

  i2c_port:
    tier: advanced
    type: int
    default: 0

  i2c_addr:
    tier: advanced
    type: int
    default: 112 # 0x70

  timeout_us:
    tier: advanced
    type: int
    default: 30000

  auto_poll_ms:
    tier: stable
    type: int
    default: 50
    min: 50
    emit: macro
    c_suffix: u

  role:
    tier: stable
    type: string
    emit: none

config:
  c_type: dal_ultrasonic_t
  config_type: dal_ultrasonic_config_t
  headers: [dal_ultrasonic.h]
  init_fn: dal_ultrasonic_init
  deinit_fn: dal_ultrasonic_deinit
  safe_off_fn: ""

device_specific_apis:
  - apply_override

role_bindings:
  distance_sensor:
    headers: [sensor/wink_ultrasonic_distance_events.h]
    verbs:
      request_measurement:
        template: "WINK_WARN_UNUSED_RESULT static inline wink_status_t {{ name }}_request_measurement(void) { return dal_ultrasonic_request_measurement(&{{ name }}); }"
      read_distance:
        template: "static inline float {{ name }}_read_distance(void) { float d = -1.0f; WINK_IGNORE_RESULT(dal_ultrasonic_get_cached_distance(&{{ name }}, &d)); return d; }"
      read_distance_status:
        template: "WINK_WARN_UNUSED_RESULT static inline wink_status_t {{ name }}_read_distance_status(float *out_dist_cm) { return dal_ultrasonic_get_cached_distance(&{{ name }}, out_dist_cm); }"
      enable_distance_events:
        template: "WINK_WARN_UNUSED_RESULT static inline wink_status_t {{ name }}_enable_distance_events(void) { static const wink_ultrasonic_distance_event_config_t cfg = { .period_ms = {{ auto_poll_ms }}u }; return wink_ultrasonic_enable_distance_events(&{{ name }}, &cfg); }"
      disable_distance_events:
        template: "static inline void {{ name }}_disable_distance_events(void) { wink_ultrasonic_disable_distance_events(&{{ name }}); }"
```

---

### 2.3 前端 UI 外设包拓扑与变体契约 (`variants.ts`)

依据 SSOT §4.6，拓扑图、物理脚 Overlay 与 Wokwi 适配 Element 的映射集中放在前端外设包：
`peripherals/builtin/ultrasonic/1.0.0/src/variants.ts`

```typescript
export type UltrasonicVariantKey = 'hcsr04' | 'single_pin_ping' | 'uart_stream' | 'i2c';

export const ULTRASONIC_TOPOLOGIES = Object.freeze({
  hcsr04: Object.freeze({
    variant: 'hcsr04',
    pinsOverlay: HCSR04_OVERLAY,
    defaultAppearanceId: 'ultrasonic_hcsr04',
  }),
  single_pin_ping: Object.freeze({
    variant: 'single_pin_ping',
    pinsOverlay: PING_OVERLAY,
    defaultAppearanceId: 'ultrasonic_ping',
  }),
  uart_stream: Object.freeze({
    variant: 'uart_stream',
    pinsOverlay: UART_OVERLAY,
    defaultAppearanceId: 'ultrasonic_uart',
  }),
  i2c: Object.freeze({
    variant: 'i2c',
    pinsOverlay: I2C_OVERLAY,
    defaultAppearanceId: 'ultrasonic_i2c',
  }),
});

export const ULTRASONIC_APPEARANCES = Object.freeze({
  ultrasonic_hcsr04: Object.freeze({
    appearanceId: 'ultrasonic_hcsr04',
    variant: 'hcsr04' as const,
    displayName: 'HC-SR04 Ultrasonic Sensor (4-Pin)',
    elementTag: 'wokwi-hc-sr04', // 🟢 Native
  }),
  ultrasonic_ping: Object.freeze({
    appearanceId: 'ultrasonic_ping',
    variant: 'single_pin_ping' as const,
    displayName: 'Parallax PING))) Ultrasonic (3-Pin)',
    elementTag: 'wink-custom-ping', // 🔴 Custom
  }),
  ultrasonic_uart: Object.freeze({
    appearanceId: 'ultrasonic_uart',
    variant: 'uart_stream' as const,
    displayName: 'US-100 UART Ultrasonic Sensor',
    elementTag: 'wink-custom-ultrasonic-uart', // 🔴 Custom
  }),
  ultrasonic_i2c: Object.freeze({
    appearanceId: 'ultrasonic_i2c',
    variant: 'i2c' as const,
    displayName: 'Devantech SRF02/SRF08 I2C Ultrasonic Sensor',
    elementTag: 'wink-custom-ultrasonic-i2c', // 🔴 Custom
  }),
});
```

---

## 3. 修改计划与任务拆解

### 任务 1：C DAL 驱动层更新
- [ ] 修改 `wink-micro-os/dal/include/sensor/dal_ultrasonic.h`
  - 增加 `dal_ultrasonic_variant_t` 枚举（含 `DAL_ULTRASONIC_VARIANT_COUNT = 4`）
  - 增加 `dal_ultrasonic_backend_t` 枚举，下沉解调机制（下构 `use_rmt` 字段）
  - 重构 `dal_ultrasonic_config_t` 结构体，优化内存对齐
  - 添加 SSOT §5.1 强约束的双重 `_Static_assert`（枚举计数校验 + 顺序递增校验 + 32-bit/64-bit 尺寸断言）
- [ ] 修改 `wink-micro-os/dal/src/sensor/dal_ultrasonic.c`
  - 实现按 4 种 Variant 分发的 `init()`、`request_measurement()` 与 `read()` 底层处理逻辑

### 任务 2：Codegen YAML 契约与 Python 工具链适配
- [ ] 更新 `wink-micro-os/codegen/drivers/ultrasonic.yaml`
  - 补充对象式字典格式的 `variant` 字段与 `backend` 字段，声明 `tier: advanced` 和 `affects_pins: true`
  - 设置 `variant_fields` 在选取 `hcsr04` 模式时，非活跃引脚置 `-1` 哨兵
  - **严禁放入 `wokwi_binding` 节**，保留现网 `role_bindings` 与 `device_specific_apis` 规范字段
- [ ] 校验 `pytest tools/codegen/tests/` 自动化规则门禁通过

### 任务 3：前端外设包多变体与 Custom 组件拓扑扩充
- [ ] 修改 `peripherals/builtin/ultrasonic/1.0.0/src/variants.ts`
  - 扩充 `ULTRASONIC_TOPOLOGIES` 与 `ULTRASONIC_APPEARANCES`
  - 为 `hcsr04`, `single_pin_ping`, `uart_stream`, `i2c` 4 种变体分别添加 `PinsOverlayMap` 引脚映射
  - 映射原生 Element (`wokwi-hc-sr04`) 与 3 个 Custom 仿真组件 (`wink-custom-ping`, `wink-custom-ultrasonic-uart`, `wink-custom-ultrasonic-i2c`)

### 任务 4：SSOT 文档落地状态更新
- [ ] 重构代码与测试验证全量落地后，更新 `00.1-category-type-variant-wokwi-ssot.md` §3 状态矩阵表格：
  - 将第 `#13` 行 `ultrasonic` 的状态标记从 `🚧 Refactor Planned` 改为 `✅ Completed`

---

## 4. 验证计划

1. **单元测试与 ABI 校验**：编译 DAL `dal_ultrasonic`，确保 `_Static_assert` 门禁断言无警告通过。
2. **Codegen 生成测试**：运行 `pytest tools/codegen/tests/`，验证选择 `hcsr04` 变体时 `sig_pin`, `uart_port`, `i2c_port` 被赋 `-1` 哨兵。
3. **前端外设包与 Wokwi 拓扑连线验证**：在前端画布放置 4 种变体，校验引脚 Overlay 重排与 Custom Component 连线正常。
4. **WASM 仿真测距验证**：在 WASM/Wokwi 仿真环境中验证 HC-SR04 声波测距逻辑及非阻塞事件推送。

