# ADR-0064：Target Capability 平台能力与容量头文件 SSOT 架构

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已通过）** |
| 日期 | 2026-08-24 |
| 影响范围 | `pal/include/hal/pal_target_caps.h`、`wink_status.h`、`pal_resource.h`、各 Target HAL 实现 |
| 决策者 | 架构委员会 / 核心 PAL 维护组 |
| 关联 ADR | [ADR-0002](../unisim/0002-dual-target-compilation.md)（双 Target 编译同源）；[ADR-0012](0012-contract-honesty-over-silent-degradation.md)（合约诚实）；[ADR-0041](0041-hal-osal-directory-orthogonality.md)（HAL/OSAL 正交）。 |

---

## 背景（Context）

在 WinkMicroOS 早期代码中，各类硬件外设的容量上限（如最大 PWM 通道数、最大 I2C 端口数、最大 GPIO 引脚数）四处散落：
- `pal_hal.h:20-22` 宏定义 `#define PAL_PWM_CHANNELS 8`、`PAL_I2C_PORTS 2`；
- `wink_status.h:128` 错误且重复地定义了 `#define PAL_PWM_CHANNELS 8`；
- `pal_resource.h:51-63` 独立定义了 `PAL_PWM_CHANNEL_MAX 8`、`PAL_GPIO_PIN_MAX 50`；
- `pal_wasm_ch1b_pwm.c:17` 局部定义了 `WASM_PWM_MAX_CHANNELS 16`，与公共契约产生冲突（违反 ADR-0012 合约诚实）。

当向不同 SoC（如 ESP32-C3 仅 6 个 LEDC 通道，ESP32-S3 有 8 个，STM32 有不同数量的定时器通道）移植时，公共头文件中硬编码的默认容量会导致严重的能力失真或缓冲区浪费。

---

## 决策（Decision）

1. **建立全局 Target Capabilities SSOT 头文件**：
   在 `pal/include/hal/pal_target_caps.h` 中建立平台能力 SSOT，统一提供以下命名规范的容量宏：
   - `PAL_PWM_CHANNEL_MAX`
   - `PAL_I2C_PORT_MAX`
   - `PAL_GPIO_PIN_MAX`
   - `PAL_PWM_MAX_BITS`
   - `PAL_ADC_CHANNEL_MAX`

2. **平台能力注入原则**：
   - **ESP32**：直接映射自 ESP-IDF 的 `soc/soc_caps.h`（如 `SOC_LEDC_CHANNEL_NUM`）；
   - **Host / Wasm 仿真**：统一在 `pal_target_caps.h` 的条件编译分支中显式约束基线能力（如 PWM 统一为 8，消除 16 vs 8 的漂移）；
   - **公共契约头文件**：严禁在 `pal_gpio.h`、`pal_pwm.h`、`pal_i2c.h`、`wink_status.h` 中再次出现 `#ifndef PAL_* #define 8` 的硬编码 fallback，所有公共头一律包含 `hal/pal_target_caps.h`。

3. **历史散落宏清理**：
   - 彻底删除 `wink_status.h:128` 中的 `PAL_PWM_CHANNELS`；
   - `pal_resource.h` 移除私有容量宏定义，改为直接包含 `hal/pal_target_caps.h`。

---

## 影响（Consequences）

- **正向收益**：全平台外设容量上限具备单一事实来源（SSOT），彻底消灭多头文件宏定义冲突与仿真层容量漂移。
- **治理要求**：后续任何新增外设子系统（如 CAN、DAC）必须首先在 `pal_target_caps.h` 登记容量宏，方可进入公共 HAL 契约。
