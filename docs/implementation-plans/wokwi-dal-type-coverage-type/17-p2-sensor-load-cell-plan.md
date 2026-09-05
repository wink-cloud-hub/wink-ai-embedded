# DAL load_cell 称重传感器外设重构实施计划 (v2.0.0 终态设计版)

| 项 | 内容 |
|---|---|
| **文档名称** | DAL `load_cell` 称重传感器外设重构实施计划 |
| **文档路径** | `docs/implementation-plans/frontend/17-p2-sensor-load-cell-plan.md` |
| **版本** | v2.0.0 (终态标准版：整合专家评审全量修订、ADR-0017 非阻塞解耦、GPIO 沿中断、中位值去皮滤波与 Unisim WASM 协议仿真) |
| **日期** | 2026-08-11 |
| **上级计划** | [`00-master-execution-plan.md`](00-master-execution-plan.md), [`00.1-category-type-variant-wokwi-ssot.md`](00.1-category-type-variant-wokwi-ssot.md) |
| **参考规范** | `wink_status.h`, `pal_hal.h`, `pal_uart.h`, ADR-0001, ADR-0004, ADR-0008, ADR-0017 (非阻塞), ADR-0043 (分层门禁) |

---

## 1. 需求与硬件架构诊断

称重传感器系统（Load Cell System）在物理结构上包含 **电阻应变电桥（物理传感器主体）** 与 **24-bit 模数转换/信号调理芯片 (AFE)** 两部分。通用单片机（MCU）直接通过数字总线控制并读取 AFE 芯片的数据：

$$\text{MCU / 开发板} \xleftrightarrow[\text{4Pin/6Pin/UART}]{\text{数字控制与数据线}} \text{[AFE 模块 (HX711/CS1237)]} \xleftrightarrow[\text{E+, E-, S+, S-}]{\text{微伏级模拟电桥}} \text{[称重应变片主体]}$$

### 1.1 变体枚举与 AFE 芯片映射表 (`dal_load_cell_variant_t`)

依据 SSOT §1.1 变体设计规范与 **§4.4 芯片别名 (IC Aliases) 归一化原则**，穷举 4 种底层硬件接口与通信协议变体：

| 变体枚举名称 | 物理引脚拓扑 | `affects_pins` | 底层通信协议与时序特征 | 芯片别名 (Aliases) | 仿真适配 (Wokwi@1.9.2) |
|---|---|:---:|---|---|:---:|
| `DAL_LOAD_CELL_VARIANT_HX711_TWO_WIRE` (0) | `[VCC, GND, DT, SCK]` (4Pin) | `true` | **单向 2 线脉冲同步**：SCK 24~27 个脉冲读取数据并设置 128/64/32 增益 | `HX711`, `TM7711`, `NA770`, `HX710` | 🟢 Native (`wokwi-hx711`) |
| `DAL_LOAD_CELL_VARIANT_CS1237_TWO_WIRE` (1) | `[VCC, GND, OUT_IN, SCLK]` (4Pin) | `true` | **半双工 2 线读写**：OUT 线可切换双向，支持读 ADC 数据及写配置寄存器 | `CS1237`, `CS1238` | 🔴 Custom (`wink-custom-cs1237`) |
| `DAL_LOAD_CELL_VARIANT_ADS1232_SPI` (2) | `[VCC, GND, SCLK, DOUT, GAIN0, GAIN1]` (6Pin) | `true` | **2 线移位+硬控脚** (SSOT 保留 `_SPI` 命名，本质走 GPIO bit-bang，因 PAL 无 `pal_spi_*`) | `ADS1232`, `ADS1234` | 🔴 Custom (`wink-custom-ads1232`) |
| `DAL_LOAD_CELL_VARIANT_MODBUS_RTU` (3) | `[VCC, GND, TX, RX]` (4Pin UART) | `true` | **Modbus RTU 串口协议**：工业称重变送器/仪表直接输出标量重量 | 工业称重仪表, `JY-S60` | 🔴 Custom (`wink-custom-scale-modbus`) |

> **实施阶段界定 (Phase Scope)**：Phase 1 落地 `hx711_two_wire` 全功能支持；`cs1237_two_wire` / `ads1232_spi` / `modbus_rtu` 变体在 init 时校验脚位后返回 `WINK_ERR_UNSUPPORTED`，但其 C 枚举、POD 结构体尺寸、ABI Padding 与 Codegen YAML 全面一次性定稿，防止后续破坏 ABI 尺寸断言。

---

## 2. 详细设计规范

### 2.1 C API 头文件设计 (`wink-micro-os/dal/include/sensor/dal_load_cell.h`)

#### (1) 头文件标准骨架与裁剪桩
```c
/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Wink AI Project
 */

#ifndef DAL_LOAD_CELL_SENSOR_H
#define DAL_LOAD_CELL_SENSOR_H

#include "wink_status.h"
#include "pal_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(WINK_USE_LOAD_CELL) || !WINK_USE_LOAD_CELL

/* --- 编译期裁剪 fallback 桩段 --- */
#define WINK_LOAD_CELL_UNAVAILABLE WINK_UNAVAILABLE_MSG("DAL load_cell driver is disabled in build config")

static inline wink_status_t dal_load_cell_init(void *dev, const void *config) { (void)dev; (void)config; return WINK_LOAD_CELL_UNAVAILABLE; }
static inline wink_status_t dal_load_cell_deinit(void *dev) { (void)dev; return WINK_LOAD_CELL_UNAVAILABLE; }

#else

/**
 * @brief Load cell AFE hardware interface variant (affects_pins: true)
 */
typedef enum {
    DAL_LOAD_CELL_VARIANT_HX711_TWO_WIRE  = 0, /**< Default: 4Pin 2-wire pulse bit-bang (HX711/TM7711/NA770) */
    DAL_LOAD_CELL_VARIANT_CS1237_TWO_WIRE = 1, /**< 4Pin 2-wire half-duplex register read/write (CS1237/CS1238) */
    DAL_LOAD_CELL_VARIANT_ADS1232_SPI     = 2, /**< 6Pin 2-wire shift with hardware gain control (ADS1232/ADS1234) */
    DAL_LOAD_CELL_VARIANT_MODBUS_RTU      = 3, /**< 4Pin UART RS485 Modbus RTU weighing transmitter */
    DAL_LOAD_CELL_VARIANT_COUNT            = 4, /**< Total variant count for static assertion */
} dal_load_cell_variant_t;

/**
 * @brief Programmable Gain Amplifier (PGA) setting
 * @note GAIN_32 represents Channel B Gain 32 on HX711; ADS1232 uses hardware pins; Modbus ignores this field.
 */
typedef enum {
    DAL_LOAD_CELL_GAIN_128 = 0, /**< Channel A gain 128 (Default for HX711) */
    DAL_LOAD_CELL_GAIN_64  = 1, /**< Channel A gain 64 (HX711) */
    DAL_LOAD_CELL_GAIN_32  = 2, /**< Channel B gain 32 (HX711) */
} dal_load_cell_gain_t;

/**
 * @brief Load cell configuration struct (Flat POD layout with sentinel trimming & ABI padding)
 */
typedef struct {
    const char              *owner;              /**< Instance owner static tag string */
    float                    calibration_factor; /**< Scale factor (counts per gram, default 1.0f) */
    int32_t                  zero_offset;        /**< Tare offset count (raw zero reading) */
    uint32_t                 timeout_us;         /**< Measurement DRDY timeout in µs (default 150000us) */
    uint32_t                 baud_rate;          /**< UART baud rate (modbus_rtu only, default 9600) */
    dal_load_cell_variant_t  variant;            /**< AFE interface variant */
    dal_load_cell_gain_t     gain;               /**< PGA gain selection */
    wink_pin_t               dt_pin;             /**< HX711 DT / DOUT pin (-1 if unused) */
    wink_pin_t               sck_pin;            /**< HX711 SCK / PD_SCK pin (-1 if unused) */
    wink_pin_t               out_in_pin;         /**< CS1237 bi-directional OUT_IN pin (-1 if unused) */
    wink_pin_t               sclk_pin;           /**< CS1237/ADS1232 SCLK pin (-1 if unused) */
    wink_pin_t               dout_pin;           /**< ADS1232 DOUT pin (-1 if unused) */
    wink_pin_t               gain0_pin;          /**< ADS1232 GAIN0 pin (-1 if unused) */
    wink_pin_t               gain1_pin;          /**< ADS1232 GAIN1 pin (-1 if unused) */
    uint8_t                  modbus_addr;        /**< Modbus slave address (modbus_rtu only, default 1) */
    uint8_t                  uart_port;          /**< Logical UART port index (modbus_rtu only) */
    uint8_t                  _reserved[4];       /**< ABI alignment padding & future expansion reservation */
} dal_load_cell_config_t;

/**
 * @brief Load cell device handle
 */
typedef struct {
    dal_load_cell_config_t   config;             /**< Device configuration POD (Offset 0) */
    int32_t                  last_raw;           /**< Last raw 24-bit ADC reading */
    float                    last_weight_g;      /**< Last calculated weight in grams */
    dal_load_cell_gain_t     pending_gain;       /**< Next-frame pending gain setting */
    volatile bool            initialized;        /**< Initialization flag */
} dal_load_cell_t;

/* --- SSOT §5.1 Mandatory Double Static Assertions & ABI Freeze Checks --- */
_Static_assert(offsetof(dal_load_cell_t, config) == 0, "ABI break: config struct must be at offset 0 of handle");
_Static_assert(DAL_LOAD_CELL_VARIANT_COUNT == 4, "Variant count mismatch with SSOT §2 and codegen YAML");
_Static_assert(DAL_LOAD_CELL_VARIANT_MODBUS_RTU + 1 == DAL_LOAD_CELL_VARIANT_COUNT, "Sequential variant ordering error");

#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_load_cell_config_t) == 48, "ABI break: config size changed on 32-bit target");
_Static_assert(offsetof(dal_load_cell_t, initialized) == 60, "ABI break: initialized offset changed on 32-bit target");
_Static_assert(sizeof(dal_load_cell_t) == 64, "ABI break: handle size changed on 32-bit target");
#else                         /* LP64 / LLP64: 64-bit Host Simulation */
_Static_assert(sizeof(dal_load_cell_config_t) == 56, "ABI break: config size changed on 64-bit host");
_Static_assert(offsetof(dal_load_cell_t, initialized) == 68, "ABI break: initialized offset changed on 64-bit host");
_Static_assert(sizeof(dal_load_cell_t) == 72, "ABI break: handle size changed on 64-bit host");
#endif

/* --- API 函数声明 --- */
WINK_WARN_UNUSED_RESULT wink_status_t dal_load_cell_init(dal_load_cell_t *dev, const dal_load_cell_config_t *config);
WINK_WARN_UNUSED_RESULT wink_status_t dal_load_cell_deinit(dal_load_cell_t *dev);

/* --- ADR-0017 非阻塞解耦 API 契约 --- */
WINK_WARN_UNUSED_RESULT wink_status_t dal_load_cell_is_data_ready(const dal_load_cell_t *dev, bool *out_ready);
WINK_WARN_UNUSED_RESULT wink_status_t dal_load_cell_request_read(dal_load_cell_t *dev);
WINK_WARN_UNUSED_RESULT wink_status_t dal_load_cell_get_cached_raw(const dal_load_cell_t *dev, int32_t *out_raw);
WINK_WARN_UNUSED_RESULT wink_status_t dal_load_cell_get_cached_weight_g(const dal_load_cell_t *dev, float *out_g);

#ifndef WINK_STRICT_NONBLOCKING
/**
 * @brief Synchronous blocking read (Waits for DRDY up to timeout_us)
 */
WINK_BLOCKING WINK_WARN_UNUSED_RESULT wink_status_t dal_load_cell_read_weight_g(dal_load_cell_t *dev, float *out_g);
#endif

/* --- 业务辅助 API --- */
WINK_WARN_UNUSED_RESULT wink_status_t dal_load_cell_tare(dal_load_cell_t *dev);
WINK_WARN_UNUSED_RESULT wink_status_t dal_load_cell_set_calibration_factor(dal_load_cell_t *dev, float factor);
WINK_WARN_UNUSED_RESULT wink_status_t dal_load_cell_apply_override(void *dev, const uint8_t *params, uint16_t len);

#endif /* WINK_USE_LOAD_CELL */

#ifdef __cplusplus
}
#endif

#endif /* DAL_LOAD_CELL_SENSOR_H */
```

---

### 2.2 C 驱动核心算法与协议细节 (`wink-micro-os/dal/src/sensor/dal_load_cell.c`)

在 `dal_load_cell.c` 落地实现中，严格遵守以下工程规范：

1. **初始化双重检查修复**：
   ```c
   if (dev == NULL || config == NULL) return WINK_ERR_INVALID_ARG;
   if (dev->initialized) return WINK_ERR_ALREADY_INITIALIZED; // 正确顺序
   ```
2. **资源申请与回滚 (Resource Claim & Rollback)**：
   *   对活跃脚逐一调用 `pal_resource_claim(PAL_RESOURCE_GPIO_PIN, pin, config->owner)`；
   *   HX711 变体下：`dt_pin` 设为 `PAL_GPIO_INPUT`；`sck_pin` 设为 `PAL_GPIO_OUTPUT_PUSH_PULL` 并初始拉低；
   *   Deinit 时依次调用 `pal_gpio_reset_pin` 与 `pal_resource_release`。
3. **HX711 时序与增益脉冲控制**：
   *   读数据：循环 24 次，拉高 SCK ($t_{\text{high}} > 0.2\,\mu\text{s}$)，读取 DT 位，拉低 SCK；
   *   补发增益脉冲：根据 `dev->pending_gain` 补发第 25 个 (Gain 128)、26 个 (Gain 32 CH-B) 或 27 个 (Gain 64) 脉冲，更新 `dev->pending_gain`；
   *   **掉电防护**：严禁 SCK 保持高电平超过 $60\,\mu\text{s}$（否则 HX711 将误入 Power-Down 掉电模式）。Deinit 或异常退出时确保 SCK 输出低电平。
4. **24-bit 符号扩展 (Sign Extension)**：
   ```c
   int32_t raw24 = 0;
   // ... bit-bang 24 bits into raw24 ...
   if (raw24 & 0x800000u) {
       raw24 |= (int32_t)0xFF000000u; // 符号扩展到有符号 int32
   }
   ```
5. **`tare` 去皮滤波算法 (Median-Average Filter)**：
   为防止工频干扰与机械震动抖动导致去皮漂移，`dal_load_cell_tare()` 连续采样 8 帧数据，剔除 2 个最大值与 2 个最小值后取均值作为 `zero_offset`。若采样数据标准差过大，返回 `WINK_ERR_BUSY`。
6. **GPIO 负边沿中断与事件驱动支持**：
   驱动中可选挂载 `pal_gpio_attach_interrupt(dev->config.dt_pin, PAL_GPIO_INTR_NEGEDGE, ...)`，支持在 DT 拉低时触发事件通知。

---

### 2.3 Codegen Driver YAML 契约 (`wink-tools/tools/codegen/drivers/load_cell.yaml`)

遵循现网 `mono_oled.yaml` 规格，采用 Schema List 模式编写：

```yaml
codegen_schema: "1.1"
type: load_cell
category: sensor
source_stem: load_cell
driver_header: "sensor/dal_load_cell.h"
handle_type: "dal_load_cell_t"
config_type: "dal_load_cell_config_t"
init_fn: "dal_load_cell_init"
deinit_fn: "dal_load_cell_deinit"

fields:
  - name: variant
    type: enum
    enum: [hx711_two_wire, cs1237_two_wire, ads1232_spi, modbus_rtu]
    map:
      hx711_two_wire: DAL_LOAD_CELL_VARIANT_HX711_TWO_WIRE
      cs1237_two_wire: DAL_LOAD_CELL_VARIANT_CS1237_TWO_WIRE
      ads1232_spi: DAL_LOAD_CELL_VARIANT_ADS1232_SPI
      modbus_rtu: DAL_LOAD_CELL_VARIANT_MODBUS_RTU
    affects_pins: true
    variant_fields:
      hx711_two_wire: [dt_pin, sck_pin, gain, calibration_factor, zero_offset]
      cs1237_two_wire: [out_in_pin, sclk_pin, gain, calibration_factor, zero_offset]
      ads1232_spi: [sclk_pin, dout_pin, gain0_pin, gain1_pin, calibration_factor, zero_offset]
      modbus_rtu: [uart_port, baud_rate, modbus_addr, calibration_factor]

  - name: dt_pin
    type: pin
    default: -1
  - name: sck_pin
    type: pin
    default: -1
  - name: out_in_pin
    type: pin
    default: -1
  - name: sclk_pin
    type: pin
    default: -1
  - name: dout_pin
    type: pin
    default: -1
  - name: gain0_pin
    type: pin
    default: -1
  - name: gain1_pin
    type: pin
    default: -1
  - name: calibration_factor
    type: float
    default: 1.0
  - name: zero_offset
    type: int
    default: 0
  - name: gain
    type: enum
    enum: [gain_128, gain_64, gain_32]
    map:
      gain_128: DAL_LOAD_CELL_GAIN_128
      gain_64: DAL_LOAD_CELL_GAIN_64
      gain_32: DAL_LOAD_CELL_GAIN_32
    default: gain_128

ic_aliases:
  HX711: { variant: hx711_two_wire }
  TM7711: { variant: hx711_two_wire }
  NA770: { variant: hx711_two_wire }
  HX710: { variant: hx711_two_wire }
  CS1237: { variant: cs1237_two_wire }
  ADS1232: { variant: ads1232_spi }
```

---

### 2.4 跨仓前端与 WASM 协议仿真模型 (TS & Unisim Axis A Bridge)

#### 1. 前端 TS 外设包交付边界
前端 TS 外设包代码位于外部 monorepo `wink-ai/packages/embedded-frontend/` 下的 `peripherals/builtin/load_cell/v1/`，由前端框架独立渲染 `wokwi-hx711` UI。

#### 2. WASM HX711 协议行为模型 (`targets/wasm/pal_wasm_gpio_sim.c`)
C 驱动在 WASM Target 下会真实对 GPIO 进行 bit-bang 移位。为了保证仿真连贯性，在 WASM 物理桥接层搭建 HX711 协议行为模型：
*   **物理量激励映射**：Unisim 标量通道将面板重量 $W_{\text{sim}}$ 换算为仿真 ADC Raw Count：
    $$N_{\text{raw\_sim}} = W_{\text{sim}} \times K_{\text{sim}} + Z_{\text{sim}}$$
*   **时序响应状态机**：
    1. 当转换就绪时，模拟器将 DT 脚拉低（发出 DRDY 负边沿）；
    2. WASM GPIO 监听到 SCK 的 24 个上升沿时，按 MSB-first 依次将 $N_{\text{raw\_sim}}$ 的 24 个 bit 送出至 DT 引脚；
    3. 监听到第 25/26/27 个脉冲时，更新模拟器下一帧的 Gain 模式。

---

## 3. Checklist 与落地实施路径

- [x] **Step 0: SSOT 与规范审计**：完成 SSOT 表格演进与专家评审意见吸收（✅ Complete）。
- [ ] **Step 1: C 头文件与 ABI 冻结校验**：
  - 创建 `wink-micro-os/dal/include/sensor/dal_load_cell.h`，植入静态断言与非阻塞 API。
  - 在 `wink-micro-os/test/unit/dal/test_dal_abi_freeze.c` 追加 `#include "sensor/dal_load_cell.h"` 并通过 ABI 门禁。
- [ ] **Step 2: C 驱动与资源管理**：
  - 创建 `wink-micro-os/dal/src/sensor/dal_load_cell.c`。
  - 实现资源申请/释放、HX711 24bit 脉冲移位、符号扩展、掉电保护及 `tare` 中位值平均滤波。
  - 在 `wink-micro-os/dal/CMakeLists.txt` 注册并增加 `WINK_USE_LOAD_CELL` 编译宏。
- [ ] **Step 3: Codegen Driver YAML 契约**：
  - 创建 `wink-tools/tools/codegen/drivers/load_cell.yaml`。
  - 运行 `python -m tools.codegen.schema.yaml_schema` 校验 Schema。
- [ ] **Step 4: 单元测试套件**：
  - 创建 `wink-micro-os/test/unit/dal/test_dal_load_cell.c`。
  - 覆盖 NULL 指针、重重复 init、哨兵校验、正负符号扩展、DRDY 超时、Modbus UNSUPPORTED 及 `tare` 滤波用例。
- [ ] **Step 5a: 跨仓 TS 外设包集成**：在外部 monorepo `wink-ai` 中交付 `load_cell/v1` 外设包。
- [ ] **Step 5b: WASM HX711 协议数字仿真**：在 `targets/wasm/` 部署 SCK/DT 移位时序桥，完成 Unisim E2E 重量注入验证。

---

## 4. 全自动化验证计划 (Verification Plan)

1. **ABI 与双 Target 编译**：
   使用 CMake 分别编译 WASM32 与 ESP32 双目标，确保 `_Static_assert` 尺寸校验 100% 通过：
   ```bash
   python wink-tools/wink.py build host --app infrastructure_smoke
   python wink-tools/wink.py build esp32 --app infrastructure_smoke
   ```
2. **Codegen Schema 校验**：
   ```bash
   python -m tools.codegen.schema.yaml_schema
   pytest tools/codegen/tests/
   ```
3. **架构分层检查 (ADR-0043)**：
   ```bash
   python wink-tools/wink.py lint arch --pack layering --pack api
   ```
4. **DAL 单元测试**：
   运行 CTest 执行 `test_dal_load_cell.c` 与 `test_dal_abi_freeze.c`：
   ```bash
   ctest -R test_dal_load_cell --output-on-failure
   ```
5. **Unisim E2E 仿真验证**：
   在 Web 仿真画板中拖入 `wokwi-hx711` 组件，滑动重量滑块，验证 WASM 固件中的 `dal_load_cell_get_cached_weight_g()` 能精准读出对应的重量数值。
