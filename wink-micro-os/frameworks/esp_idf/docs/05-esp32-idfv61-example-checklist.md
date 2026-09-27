# ESP-IDF v6.1 官方示例全量仿真适配核对清单 (Checklist)

> **权威上游路径**：`D:\software\embedded-tools\esp-idf\.espressif\v6.1\esp-idf\examples`  
> **参考标准规范**：[`PLAN-20260922-ESP-IDF-SIM-MASTER`](../../implementation-plans/esp32/2026-09-22-esp-idf-simulation-interception-master-plan.md) §7.1.1（三层证据塔与精选规则）  
> **对标基线**：[`docs/vendors/Cmsemicon/CMS8S78XX_EXAMPLE_CHECKLIST.md`](../Cmsemicon/CMS8S78XX_EXAMPLE_CHECKLIST.md)（业界最高保真审计基线）  
> **生成日期**：2026-09-27 | **版本**：v1.1 (全量连续编号普查版)  

---

## 一、 总体适配进度与全量普查统计

- **官方功能大类总数**：**19 个大类**
- **官方独立示例工程总数**：**478 个**（地毯式 100% 全量建档，严格按编号 `#001 ~ #478` 连续顺排）
- **全景状态分布统计**：
  - `[x]` **已完成适配并实证 (Passed)**：**9 项**（M0~M4 核心黄金外设与网络连接代表，Host/Wasm 双向编译与行为实证 100% 通过）
  - `[ ]` **待适配排期中 (Pending In-Scope)**：**286 项**（ADC、RMT/RGB、SPI Master、NVS、WebSocket 等纯软件可模拟用例）
  - `[-]` **纯物理硬件专用 / 声明 Out-of-Scope**：**183 项**（遵循 [ADR-0012 合约诚实原则](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)，明确标明因缺乏物理射频波形、外部 PHY 变压器硬件或物理熔丝而免于失真模拟的项）
  - `🚫` **存在前置阻断 (Blocked)**：**0 项**

### 大类索引与编号导航总览

| 序号 | 功能大类 | 包含示例数 | 编号跨度 | 已实证项数 | 范围定位 |
| :---: | :--- | :---: | :---: | :---: | :--- |
| 01 | [基础快速起步 (Get-Started)](#get-started) | 2 项 | `#001 ~ #002` | 1 项 | `examples/get-started/` |
| 02 | [片上与总线外设 (Peripherals)](#peripherals) | 114 项 | `#003 ~ #116` | 4 项 | `examples/peripherals/` |
| 03 | [操作系统与核心系统调用 (System & OS)](#system) | 68 项 | `#117 ~ #184` | 0 项 | `examples/system/` |
| 04 | [网络与应用层通信协议 (Protocols)](#protocols) | 35 项 | `#185 ~ #219` | 2 项 | `examples/protocols/` |
| 05 | [Wi-Fi 无线局域网 (Wi-Fi)](#wifi) | 24 项 | `#220 ~ #243` | 1 项 | `examples/wifi/` |
| 06 | [蓝牙协议栈 (Bluetooth)](#bluetooth) | 147 项 | `#244 ~ #390` | 1 项 | `examples/bluetooth/` |
| 07 | [片上存储与文件系统 (Storage)](#storage) | 27 项 | `#391 ~ #417` | 0 项 | `examples/storage/` |
| 08 | [底层网络与接口 (Network)](#network) | 5 项 | `#418 ~ #422` | 0 项 | `examples/network/` |
| 09 | [C++ 运行时与语言特性 (C++)](#cxx) | 3 项 | `#423 ~ #425` | 0 项 | `examples/cxx/` |
| 10 | [构建系统与组件组织 (Build System)](#build_system) | 19 项 | `#426 ~ #444` | 0 项 | `examples/build_system/` |
| 11 | [硬件加密与芯片安全特性 (Security - 硬件物理特性)](#security) | 10 项 | `#445 ~ #454` | 0 项 | `examples/security/` |
| 12 | [定制引导加载程序 (Custom Bootloader)](#custom_bootloader) | 4 项 | `#455 ~ #458` | 0 项 | `examples/custom_bootloader/` |
| 13 | [有线以太网 (Ethernet - 外部 PHY 硬件)](#ethernet) | 3 项 | `#459 ~ #461` | 0 项 | `examples/ethernet/` |
| 14 | [超低功耗与 ULP 协处理器 (Low Power & ULP)](#lowpower) | 2 项 | `#462 ~ #463` | 0 项 | `examples/lowpower/` |
| 15 | [Wi-Fi 空间自组网 (Mesh)](#mesh) | 3 项 | `#464 ~ #466` | 0 项 | `examples/mesh/` |
| 16 | [OpenThread 802.15.4 线程网络 (OpenThread)](#openthread) | 6 项 | `#467 ~ #472` | 0 项 | `examples/openthread/` |
| 17 | [Zigbee 2.4G 射频网络 (Zigbee)](#zigbee) | 3 项 | `#473 ~ #475` | 0 项 | `examples/zigbee/` |
| 18 | [IEEE 802.15.4 原始射频 (IEEE 802.15.4)](#ieee802154) | 1 项 | `#476 ~ #476` | 0 项 | `examples/ieee802154/` |
| 19 | [射频物理层与工厂校准 (PHY & Calibration)](#phy) | 2 项 | `#477 ~ #478` | 0 项 | `examples/phy/` |

---

## 二、 符号与分类说明

### 1. 状态符号
- `[x]` **已完成实证**：已建立独立工程，通过 Host/Wasm 双 Target 编译门禁，并在 UniSim/Headless 中获得数据实证。
- `[ ]` **待适配**：功能在仿真规划范围内，待随着后续里程碑（如 M5）逐步翻牌实施。
- `[-]` **声明 Out-of-Scope / 硬件专用**：遵循 [ADR-0012 合约诚实原则](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)，明确标明因缺乏物理射频波形、外部 PHY 硬件或物理熔丝而暂不适配。
- `🚫` **前置阻断**：依赖尚未就绪的平台抽象契约。

### 2. 可观测性分级 (Observability Level)
- 🎯 **Level 1（界面直观可视）**：在 UniSim 画布或 Inspector 拥有直观控件（如 LED、数码管、Virtual BLE Inspector 调试面板）。
- 📜 **Level 2（控制台日志/网络数据）**：虚拟串口终端或网络通信控制台有格式化数据流（UART、MQTT、HTTP）。
- ⚡ **Level 3（IO 打点/波形探测）**：硬件定时器中断、GPIO 边沿触发波形。
- ⚙️ **Level 4（纯内部静默逻辑）**：内存管理、错误码捕获或寄存器级状态校验。

---

## 三、 478 个官方示例逐项核对总账

<a id="get-started"></a>
### 基础快速起步 (Get-Started)（共 2 项 | 编号 `#001 ~ #002` | 已实证: 1 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [x] | 001 | `get-started/blink` | 🎯 Level 1 | P0 | `esp_idfv61_blink_gpio` | M0 标杆已落地。GPIO 输出 + FreeRTOS 延时驱动板载 LED 呼吸闪烁。 |
| [ ] | 002 | `get-started/hello_world` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |

---

<a id="peripherals"></a>
### 片上与总线外设 (Peripherals)（共 114 项 | 编号 `#003 ~ #116` | 已实证: 4 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [ ] | 003 | `peripherals/adc/continuous_read` | 🎯 Level 1 | P1 | `待适配` | 排期中。拟对接 PAL pal_adc，支持 ADC 模拟量转换与虚拟电位器/光敏电阻控件。 |
| [ ] | 004 | `peripherals/adc/oneshot_read` | 🎯 Level 1 | P1 | `待适配` | 排期中。拟对接 PAL pal_adc，支持 ADC 模拟量转换与虚拟电位器/光敏电阻控件。 |
| [ ] | 005 | `peripherals/analog_comparator/auto_scan` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 006 | `peripherals/analog_comparator/etm_periodic_scan` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 007 | `peripherals/bitscrambler` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [-] | 008 | `peripherals/camera/dvp_dsi` | 🎯 Level 1 | P3 | `-` | 声明 Out-of-Scope。MIPI-CSI/DVP 摄像头图像传感器与硬件编解码加速器。 |
| [-] | 009 | `peripherals/camera/dvp_isp_dsi` | 🎯 Level 1 | P3 | `-` | 声明 Out-of-Scope。MIPI-CSI/DVP 摄像头图像传感器与硬件编解码加速器。 |
| [-] | 010 | `peripherals/camera/mipi_isp_dsi` | 🎯 Level 1 | P3 | `-` | 声明 Out-of-Scope。MIPI-CSI/DVP 摄像头图像传感器与硬件编解码加速器。 |
| [ ] | 011 | `peripherals/dac/dac_continuous/dac_audio` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 012 | `peripherals/dac/dac_continuous/signal_generator` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 013 | `peripherals/dac/dac_cosine_wave` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 014 | `peripherals/dac/dac_oneshot` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 015 | `peripherals/dedicated_gpio/soft_i2c` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 016 | `peripherals/dedicated_gpio/soft_spi` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 017 | `peripherals/dedicated_gpio/soft_uart` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 018 | `peripherals/dma/async_color_convert` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 019 | `peripherals/dma/async_crc` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 020 | `peripherals/gpio/generic_gpio` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 021 | `peripherals/gpio/matrix_keyboard` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [-] | 022 | `peripherals/h264` | 🎯 Level 1 | P3 | `-` | 声明 Out-of-Scope。MIPI-CSI/DVP 摄像头图像传感器与硬件编解码加速器。 |
| [x] | 023 | `peripherals/i2c/i2c_basic` | 📜 Level 2 | P0 | `esp_idfv61_i2c_basic` | M2 已落地。现代对象式 I2C Master 驱动，总线与器件二级句柄，读写传感器寄存器。 |
| [ ] | 024 | `peripherals/i2c/i2c_eeprom` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 025 | `peripherals/i2c/i2c_slave_network_sensor` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 026 | `peripherals/i2c/i2c_tools` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 027 | `peripherals/i2c/i2c_u8g2` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [-] | 028 | `peripherals/i2s/i2s_advance/i2s_usb` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [ ] | 029 | `peripherals/i2s/i2s_basic/i2s_pdm` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 030 | `peripherals/i2s/i2s_basic/i2s_std` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 031 | `peripherals/i2s/i2s_basic/i2s_tdm` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 032 | `peripherals/i2s/i2s_codec/i2s_es7210_tdm` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 033 | `peripherals/i2s/i2s_codec/i2s_es8311` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 034 | `peripherals/i2s/i2s_recorder` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 035 | `peripherals/i3c/i3c_i2c_basic` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 036 | `peripherals/i3c/i3c_lsm6dscx` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [-] | 037 | `peripherals/isp/multi_pipelines` | 🎯 Level 1 | P3 | `-` | 声明 Out-of-Scope。MIPI-CSI/DVP 摄像头图像传感器与硬件编解码加速器。 |
| [-] | 038 | `peripherals/jpeg/jpeg_decode` | 🎯 Level 1 | P3 | `-` | 声明 Out-of-Scope。MIPI-CSI/DVP 摄像头图像传感器与硬件编解码加速器。 |
| [-] | 039 | `peripherals/jpeg/jpeg_encode` | 🎯 Level 1 | P3 | `-` | 声明 Out-of-Scope。MIPI-CSI/DVP 摄像头图像传感器与硬件编解码加速器。 |
| [ ] | 040 | `peripherals/lcd/i2c_oled` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 041 | `peripherals/lcd/i80_controller` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 042 | `peripherals/lcd/mipi_dsi` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 043 | `peripherals/lcd/parlio_simulate` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 044 | `peripherals/lcd/rgb_panel` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 045 | `peripherals/lcd/spi_lcd_touch` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 046 | `peripherals/lcd/tjpgd` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [x] | 047 | `peripherals/ledc/ledc_basic` | 🎯 Level 1 | P0 | `esp_idfv61_ledc_basic` | M2 已落地。LEDC 定时器与通道配置，定点 PWM 占空比平滑渐变，无浮点。 |
| [ ] | 048 | `peripherals/ledc/ledc_dimmer` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 049 | `peripherals/ledc/ledc_fade` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 050 | `peripherals/ledc/ledc_gamma_curve_fade` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 051 | `peripherals/mcpwm/mcpwm_bdc_speed_control` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 052 | `peripherals/mcpwm/mcpwm_bldc_hall_control` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 053 | `peripherals/mcpwm/mcpwm_capture_hc_sr04` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 054 | `peripherals/mcpwm/mcpwm_foc_svpwm_open_loop` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 055 | `peripherals/mcpwm/mcpwm_servo_control` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 056 | `peripherals/mcpwm/mcpwm_sync` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 057 | `peripherals/parlio/parlio_rx/logic_analyzer` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 058 | `peripherals/parlio/parlio_tx/advanced_rgb_led_matrix` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 059 | `peripherals/parlio/parlio_tx/simple_rgb_led_matrix` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 060 | `peripherals/pcnt/rotary_encoder` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [-] | 061 | `peripherals/ppa/ppa_rgb_lcd` | 🎯 Level 1 | P3 | `-` | 声明 Out-of-Scope。MIPI-CSI/DVP 摄像头图像传感器与硬件编解码加速器。 |
| [ ] | 062 | `peripherals/rmt/dshot_esc` | 🎯 Level 1 | P1 | `待适配` | 排期中。拟实现 RMT 脉冲引擎与 led_strip 门面，驱动虚拟 WS2812 RGB 彩灯。 |
| [ ] | 063 | `peripherals/rmt/ir_nec_transceiver` | 🎯 Level 1 | P1 | `待适配` | 排期中。拟实现 RMT 脉冲引擎与 led_strip 门面，驱动虚拟 WS2812 RGB 彩灯。 |
| [ ] | 064 | `peripherals/rmt/led_strip` | 🎯 Level 1 | P1 | `待适配` | 排期中。拟实现 RMT 脉冲引擎与 led_strip 门面，驱动虚拟 WS2812 RGB 彩灯。 |
| [ ] | 065 | `peripherals/rmt/led_strip_simple_encoder` | 🎯 Level 1 | P1 | `待适配` | 排期中。拟实现 RMT 脉冲引擎与 led_strip 门面，驱动虚拟 WS2812 RGB 彩灯。 |
| [ ] | 066 | `peripherals/rmt/musical_buzzer` | 🎯 Level 1 | P1 | `待适配` | 排期中。拟实现 RMT 脉冲引擎与 led_strip 门面，驱动虚拟 WS2812 RGB 彩灯。 |
| [ ] | 067 | `peripherals/rmt/onewire` | 🎯 Level 1 | P1 | `待适配` | 排期中。拟实现 RMT 脉冲引擎与 led_strip 门面，驱动虚拟 WS2812 RGB 彩灯。 |
| [ ] | 068 | `peripherals/rmt/stepper_motor` | 🎯 Level 1 | P1 | `待适配` | 排期中。拟实现 RMT 脉冲引擎与 led_strip 门面，驱动虚拟 WS2812 RGB 彩灯。 |
| [-] | 069 | `peripherals/sdio/host` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。SDIO 从机高速硬件总线。 |
| [-] | 070 | `peripherals/sdio/slave` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。SDIO 从机高速硬件总线。 |
| [ ] | 071 | `peripherals/sigma_delta/sdm_dac` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 072 | `peripherals/sigma_delta/sdm_led` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 073 | `peripherals/spi_master/hd_eeprom` | 📜 Level 2 | P1 | `待适配` | 排期中。拟对接 SPI Master 轮询与中断事务传输。 |
| [ ] | 074 | `peripherals/spi_master/lcd` | 📜 Level 2 | P1 | `待适配` | 排期中。拟对接 SPI Master 轮询与中断事务传输。 |
| [ ] | 075 | `peripherals/spi_slave/receiver` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 076 | `peripherals/spi_slave/sender` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 077 | `peripherals/spi_slave_hd/append_mode/master` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 078 | `peripherals/spi_slave_hd/append_mode/slave` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 079 | `peripherals/spi_slave_hd/segment_mode/seg_master` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 080 | `peripherals/spi_slave_hd/segment_mode/seg_slave` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 081 | `peripherals/temperature_sensor/temp_sensor` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 082 | `peripherals/temperature_sensor/temp_sensor_monitor` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [x] | 083 | `peripherals/timer_group/gptimer` | ⚡ Level 3 | P0 | `esp_idfv61_gptimer_alarm` | M2 已落地。高精度通用硬件定时器 Alarm 回调与自动重载机制。 |
| [ ] | 084 | `peripherals/timer_group/gptimer_capture_hc_sr04` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 085 | `peripherals/timer_group/wiegand_interface` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 086 | `peripherals/touch_sensor/touch_sens_basic` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 087 | `peripherals/touch_sensor/touch_sens_sleep` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [-] | 088 | `peripherals/twai/cybergear` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。TWAI/CAN 工业汽车差分总线物理收发器。 |
| [-] | 089 | `peripherals/twai/twai_error_recovery` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。TWAI/CAN 工业汽车差分总线物理收发器。 |
| [-] | 090 | `peripherals/twai/twai_network/twai_listen_only` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。TWAI/CAN 工业汽车差分总线物理收发器。 |
| [-] | 091 | `peripherals/twai/twai_network/twai_sender` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。TWAI/CAN 工业汽车差分总线物理收发器。 |
| [-] | 092 | `peripherals/twai/twai_utils` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。TWAI/CAN 工业汽车差分总线物理收发器。 |
| [ ] | 093 | `peripherals/uart/nmea0183_parser` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 094 | `peripherals/uart/uart_async_rxtxtasks` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 095 | `peripherals/uart/uart_dma_ota` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [x] | 096 | `peripherals/uart/uart_echo` | 📜 Level 2 | P0 | `esp_idfv61_uart_echo` | M2 已落地。UART 阻塞读/写回环，环形缓冲区与多任务调度。 |
| [ ] | 097 | `peripherals/uart/uart_echo_rs485` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 098 | `peripherals/uart/uart_events` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 099 | `peripherals/uart/uart_repl` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 100 | `peripherals/uart/uart_select` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [-] | 101 | `peripherals/usb/device/cherryusb_serial_device` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [-] | 102 | `peripherals/usb/device/tusb_cdc_acm_wakeup` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [-] | 103 | `peripherals/usb/device/tusb_composite_msc_serialdevice` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [-] | 104 | `peripherals/usb/device/tusb_console` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [-] | 105 | `peripherals/usb/device/tusb_hid` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [-] | 106 | `peripherals/usb/device/tusb_midi` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [-] | 107 | `peripherals/usb/device/tusb_msc` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [-] | 108 | `peripherals/usb/device/tusb_ncm` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [-] | 109 | `peripherals/usb/device/tusb_serial_device` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [-] | 110 | `peripherals/usb/host/cdc` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [-] | 111 | `peripherals/usb/host/cherryusb_host` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [-] | 112 | `peripherals/usb/host/hid` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [-] | 113 | `peripherals/usb/host/msc` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [-] | 114 | `peripherals/usb/host/usb_host_lib` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [-] | 115 | `peripherals/usb/host/uvc` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [-] | 116 | `peripherals/usb_serial_jtag/usb_serial_jtag_echo` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |

---

<a id="system"></a>
### 操作系统与核心系统调用 (System & OS)（共 68 项 | 编号 `#117 ~ #184` | 已实证: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [ ] | 117 | `system/app_trace_basic` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 118 | `system/app_trace_to_plot` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 119 | `system/base_mac_address` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 120 | `system/console/advanced` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 121 | `system/console/basic` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 122 | `system/deep_sleep` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 123 | `system/deep_sleep_wake_stub` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 124 | `system/efuse` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 125 | `system/esp_event/default_event_loop` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 126 | `system/esp_event/user_event_loops` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 127 | `system/esp_timer` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 128 | `system/esp_trace` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 129 | `system/eventfd` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 130 | `system/flash_suspend` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 131 | `system/freertos/basic_freertos_smp_usage` | 📜 Level 2 | P2 | `待适配` | 排期中。FreeRTOS 更多高级并发用例（任务通知、软件定时器等）。 |
| [ ] | 132 | `system/freertos/real_time_stats` | 📜 Level 2 | P2 | `待适配` | 排期中。FreeRTOS 更多高级并发用例（任务通知、软件定时器等）。 |
| [ ] | 133 | `system/gcov` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 134 | `system/gdbstub` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 135 | `system/heap_task_tracking/advanced` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 136 | `system/heap_task_tracking/basic` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 137 | `system/himem` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 138 | `system/ipc/ipc_isr/riscv` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 139 | `system/ipc/ipc_isr/xtensa` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 140 | `system/light_sleep` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 141 | `system/nmi_isr` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 142 | `system/ota/advanced_https_ota` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 143 | `system/ota/native_ota_example` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 144 | `system/ota/otatool` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 145 | `system/ota/partitions_ota` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 146 | `system/ota/simple_ota_example` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 147 | `system/perfmon` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 148 | `system/pthread` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 149 | `system/rt_mqueue` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 150 | `system/select` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 151 | `system/startup_time` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 152 | `system/sysview_tracing` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 153 | `system/sysview_tracing_heap_log` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 154 | `system/task_watchdog` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 155 | `system/ulp/lp_core/build_system` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 156 | `system/ulp/lp_core/debugging` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 157 | `system/ulp/lp_core/gpio` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 158 | `system/ulp/lp_core/gpio_intr_pulse_counter` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 159 | `system/ulp/lp_core/gpio_wakeup` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 160 | `system/ulp/lp_core/inter_cpu_critical_section` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 161 | `system/ulp/lp_core/interrupt` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 162 | `system/ulp/lp_core/lp_adc` | 🎯 Level 1 | P1 | `待适配` | 排期中。拟对接 PAL pal_adc，支持 ADC 模拟量转换与虚拟电位器/光敏电阻控件。 |
| [ ] | 163 | `system/ulp/lp_core/lp_i2c` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 164 | `system/ulp/lp_core/lp_mailbox` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 165 | `system/ulp/lp_core/lp_spi` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 166 | `system/ulp/lp_core/lp_timer_interrupt` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 167 | `system/ulp/lp_core/lp_touch` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 168 | `system/ulp/lp_core/lp_uart/lp_uart_char_seq_wakeup` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 169 | `system/ulp/lp_core/lp_uart/lp_uart_echo` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 170 | `system/ulp/lp_core/lp_uart/lp_uart_print` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 171 | `system/ulp/ulp_fsm/ulp_adc` | 🎯 Level 1 | P1 | `待适配` | 排期中。拟对接 PAL pal_adc，支持 ADC 模拟量转换与虚拟电位器/光敏电阻控件。 |
| [ ] | 172 | `system/ulp/ulp_fsm_riscv_combined/counter` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 173 | `system/ulp/ulp_riscv/adc` | 🎯 Level 1 | P1 | `待适配` | 排期中。拟对接 PAL pal_adc，支持 ADC 模拟量转换与虚拟电位器/光敏电阻控件。 |
| [ ] | 174 | `system/ulp/ulp_riscv/ds18b20_onewire` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 175 | `system/ulp/ulp_riscv/gpio` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 176 | `system/ulp/ulp_riscv/gpio_interrupt` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 177 | `system/ulp/ulp_riscv/gpio_pulse_counter` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 178 | `system/ulp/ulp_riscv/i2c` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 179 | `system/ulp/ulp_riscv/interrupts` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 180 | `system/ulp/ulp_riscv/touch` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 181 | `system/ulp/ulp_riscv/uart_print` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 182 | `system/unit_test` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 183 | `system/unit_test/test` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 184 | `system/xip_from_psram` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |

---

<a id="protocols"></a>
### 网络与应用层通信协议 (Protocols)（共 35 项 | 编号 `#185 ~ #219` | 已实证: 2 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [ ] | 185 | `protocols/dns_over_https` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [x] | 186 | `protocols/esp_http_client` | 📜 Level 2 | P0 | `esp_idfv61_http_client` | M4-2 已落地。HTTP 客户端请求隧道桥接，支持 GET/POST 响应流式解析与分块传输。 |
| [ ] | 187 | `protocols/esp_http_client_mutual_auth` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 188 | `protocols/esp_local_ctrl` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 189 | `protocols/http_request` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 190 | `protocols/http_server/advanced_tests` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 191 | `protocols/http_server/async_handlers` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 192 | `protocols/http_server/captive_portal` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 193 | `protocols/http_server/file_serving` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 194 | `protocols/http_server/persistent_sockets` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 195 | `protocols/http_server/restful_server` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 196 | `protocols/http_server/simple` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 197 | `protocols/http_server/ws_echo_server` | 📜 Level 2 | P2 | `待适配` | 排期中。WebSocket 长连接协议客户端。 |
| [ ] | 198 | `protocols/https_mbedtls` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 199 | `protocols/https_request` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 200 | `protocols/https_server/simple` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 201 | `protocols/https_server/wss_server` | 📜 Level 2 | P2 | `待适配` | 排期中。WebSocket 长连接协议客户端。 |
| [ ] | 202 | `protocols/https_x509_bundle` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 203 | `protocols/icmp/pmtu_probe` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 204 | `protocols/icmp_echo` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 205 | `protocols/l2tap` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [x] | 206 | `protocols/mqtt` | 📜 Level 2 | P0 | `esp_idfv61_mqtt_tcp` | M4-2 已落地。ESP-MQTT 客户端连接内存虚拟轻量 Broker，Pub/Sub 实时闭环。 |
| [ ] | 207 | `protocols/mqtt5` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 208 | `protocols/smtp_client` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 209 | `protocols/sntp` | 📜 Level 2 | P2 | `待适配` | 排期中。SNTP 网络时间协议，校准虚拟 RTC 时钟。 |
| [ ] | 210 | `protocols/sockets/icmpv6_ping` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 211 | `protocols/sockets/non_blocking` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 212 | `protocols/sockets/tcp_client` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 213 | `protocols/sockets/tcp_client_multi_net` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 214 | `protocols/sockets/tcp_server` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 215 | `protocols/sockets/tcp_transport_client` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 216 | `protocols/sockets/udp_client` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 217 | `protocols/sockets/udp_multicast` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 218 | `protocols/sockets/udp_server` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 219 | `protocols/static_ip` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |

---

<a id="wifi"></a>
### Wi-Fi 无线局域网 (Wi-Fi)（共 24 项 | 编号 `#220 ~ #243` | 已实证: 1 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [ ] | 220 | `wifi/espnow` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 221 | `wifi/fast_scan` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 222 | `wifi/ftm` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 223 | `wifi/getting_started/softAP` | 📜 Level 2 | P2 | `待适配` | 排期中。扩展 Wi-Fi 接入点热点 (SoftAP) 广播与虚拟 DHCP Server 租约。 |
| [x] | 224 | `wifi/getting_started/station` | 📜 Level 2 | P0 | `esp_idfv61_wifi_sta` | M4-1 已落地。虚拟 AP 状态机与 DHCP 虚拟 IP 分配，esp_event 事件循环派发。 |
| [ ] | 225 | `wifi/iperf` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 226 | `wifi/itwt` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 227 | `wifi/power_save` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 228 | `wifi/roaming/roaming_11kvr` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 229 | `wifi/roaming/roaming_app` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 230 | `wifi/scan` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 231 | `wifi/smart_config` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 232 | `wifi/softap_sta` | 📜 Level 2 | P2 | `待适配` | 排期中。扩展 Wi-Fi 接入点热点 (SoftAP) 广播与虚拟 DHCP Server 租约。 |
| [ ] | 233 | `wifi/wifi_aware/nan_console` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 234 | `wifi/wifi_aware/nan_publisher` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 235 | `wifi/wifi_aware/nan_subscriber` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 236 | `wifi/wifi_aware/usd_publisher` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 237 | `wifi/wifi_aware/usd_subscriber` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 238 | `wifi/wifi_eap_fast` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 239 | `wifi/wifi_easy_connect/dpp-enrollee` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 240 | `wifi/wifi_enterprise` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 241 | `wifi/wifi_nvs_config` | ⚙️ Level 4 | P1 | `待适配` | 排期中。拟对接 UniSim 虚拟键值存储，支持 Flash 掉电保存。 |
| [ ] | 242 | `wifi/wps` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 243 | `wifi/wps_softap_registrar` | 📜 Level 2 | P2 | `待适配` | 排期中。扩展 Wi-Fi 接入点热点 (SoftAP) 广播与虚拟 DHCP Server 租约。 |

---

<a id="bluetooth"></a>
### 蓝牙协议栈 (Bluetooth)（共 147 项 | 编号 `#244 ~ #390` | 已实证: 1 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 244 | `bluetooth/ble_get_started/bluedroid/Bluedroid_Beacon` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 245 | `bluetooth/ble_get_started/bluedroid/Bluedroid_Connection` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 246 | `bluetooth/ble_get_started/bluedroid/Bluedroid_GATT_Server` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [ ] | 247 | `bluetooth/ble_get_started/nimble/NimBLE_Beacon` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 248 | `bluetooth/ble_get_started/nimble/NimBLE_Connection` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 249 | `bluetooth/ble_get_started/nimble/NimBLE_GATT_Server` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 250 | `bluetooth/ble_get_started/nimble/NimBLE_Security` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 251 | `bluetooth/ble_uart_service` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [-] | 252 | `bluetooth/bluedroid/ble/ble_acl_latency/cent` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 253 | `bluetooth/bluedroid/ble/ble_acl_latency/periph` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 254 | `bluetooth/bluedroid/ble/ble_ancs` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 255 | `bluetooth/bluedroid/ble/ble_compatibility_test` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 256 | `bluetooth/bluedroid/ble/ble_eddystone_receiver` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 257 | `bluetooth/bluedroid/ble/ble_eddystone_sender` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 258 | `bluetooth/bluedroid/ble/ble_enc_adv_data/enc_adv_data_cent` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 259 | `bluetooth/bluedroid/ble/ble_enc_adv_data/enc_adv_data_prph` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 260 | `bluetooth/bluedroid/ble/ble_hid_device_demo` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 261 | `bluetooth/bluedroid/ble/ble_ibeacon` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 262 | `bluetooth/bluedroid/ble/ble_multi_conn/ble_multi_conn_cent` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 263 | `bluetooth/bluedroid/ble/ble_multi_conn/ble_multi_conn_prph` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 264 | `bluetooth/bluedroid/ble/ble_spp_client` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 265 | `bluetooth/bluedroid/ble/ble_spp_server` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 266 | `bluetooth/bluedroid/ble/ble_throughput/throughput_client` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 267 | `bluetooth/bluedroid/ble/ble_throughput/throughput_server` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 268 | `bluetooth/bluedroid/ble/gatt_client` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 269 | `bluetooth/bluedroid/ble/gatt_security_client` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 270 | `bluetooth/bluedroid/ble/gatt_security_server` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 271 | `bluetooth/bluedroid/ble/gatt_server` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 272 | `bluetooth/bluedroid/ble/gatt_server_service_table` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 273 | `bluetooth/bluedroid/ble/gattc_multi_connect` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 274 | `bluetooth/bluedroid/ble_50/ble50_security_client` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 275 | `bluetooth/bluedroid/ble_50/ble50_security_server` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 276 | `bluetooth/bluedroid/ble_50/ble50_throughput/throughput_client` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 277 | `bluetooth/bluedroid/ble_50/ble50_throughput/throughput_server` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 278 | `bluetooth/bluedroid/ble_50/ble_conn_subrating_central` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 279 | `bluetooth/bluedroid/ble_50/ble_conn_subrating_peripheral` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 280 | `bluetooth/bluedroid/ble_50/ble_connection_central_with_cte` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 281 | `bluetooth/bluedroid/ble_50/ble_connection_peripheral_with_cte` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 282 | `bluetooth/bluedroid/ble_50/ble_pawr_advertiser` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 283 | `bluetooth/bluedroid/ble_50/ble_pawr_advertiser_conn` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 284 | `bluetooth/bluedroid/ble_50/ble_pawr_synchronizer` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 285 | `bluetooth/bluedroid/ble_50/ble_periodic_adv_with_cte` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 286 | `bluetooth/bluedroid/ble_50/ble_periodic_sync_with_cte` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 287 | `bluetooth/bluedroid/ble_50/ble_power_control_central` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 288 | `bluetooth/bluedroid/ble_50/ble_power_control_peripheral` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 289 | `bluetooth/bluedroid/ble_50/multi-adv` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 290 | `bluetooth/bluedroid/ble_50/periodic_adv` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 291 | `bluetooth/bluedroid/ble_50/periodic_sync` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 292 | `bluetooth/bluedroid/bluedroid_host_only/bluedroid_host_only_uart` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 293 | `bluetooth/bluedroid/classic_bt/a2dp_sink_stream` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 294 | `bluetooth/bluedroid/classic_bt/a2dp_sink_stream_aac` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 295 | `bluetooth/bluedroid/classic_bt/a2dp_source` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 296 | `bluetooth/bluedroid/classic_bt/a2dp_source_aac` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 297 | `bluetooth/bluedroid/classic_bt/avrcp_absolute_volume` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 298 | `bluetooth/bluedroid/classic_bt/avrcp_ct_cover_art` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 299 | `bluetooth/bluedroid/classic_bt/avrcp_ct_metadata` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 300 | `bluetooth/bluedroid/classic_bt/bt_discovery` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 301 | `bluetooth/bluedroid/classic_bt/bt_hid_mouse_device` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 302 | `bluetooth/bluedroid/classic_bt/bt_l2cap_client` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 303 | `bluetooth/bluedroid/classic_bt/bt_l2cap_server` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 304 | `bluetooth/bluedroid/classic_bt/bt_spp_acceptor` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 305 | `bluetooth/bluedroid/classic_bt/bt_spp_initiator` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 306 | `bluetooth/bluedroid/classic_bt/bt_spp_vfs_acceptor` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 307 | `bluetooth/bluedroid/classic_bt/bt_spp_vfs_initiator` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 308 | `bluetooth/bluedroid/classic_bt/hfp_ag` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 309 | `bluetooth/bluedroid/classic_bt/hfp_hf` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 310 | `bluetooth/bluedroid/coex/a2dp_gatts_coex` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 311 | `bluetooth/bluedroid/coex/gattc_gatts_coex` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [ ] | 312 | `bluetooth/blufi` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [-] | 313 | `bluetooth/esp_ble_audio/bap/broadcast_sink` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。LE Audio 等时信道与 LC3 专用音频硬件编解码。 |
| [-] | 314 | `bluetooth/esp_ble_audio/bap/broadcast_source` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。LE Audio 等时信道与 LC3 专用音频硬件编解码。 |
| [-] | 315 | `bluetooth/esp_ble_audio/bap/unicast_client` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。LE Audio 等时信道与 LC3 专用音频硬件编解码。 |
| [-] | 316 | `bluetooth/esp_ble_audio/bap/unicast_server` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。LE Audio 等时信道与 LC3 专用音频硬件编解码。 |
| [-] | 317 | `bluetooth/esp_ble_audio/cap/acceptor` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。LE Audio 等时信道与 LC3 专用音频硬件编解码。 |
| [-] | 318 | `bluetooth/esp_ble_audio/cap/initiator` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。LE Audio 等时信道与 LC3 专用音频硬件编解码。 |
| [-] | 319 | `bluetooth/esp_ble_audio/tmap/bmr` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。LE Audio 等时信道与 LC3 专用音频硬件编解码。 |
| [-] | 320 | `bluetooth/esp_ble_audio/tmap/bms` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。LE Audio 等时信道与 LC3 专用音频硬件编解码。 |
| [-] | 321 | `bluetooth/esp_ble_audio/tmap/central` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。LE Audio 等时信道与 LC3 专用音频硬件编解码。 |
| [-] | 322 | `bluetooth/esp_ble_audio/tmap/peripheral` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。LE Audio 等时信道与 LC3 专用音频硬件编解码。 |
| [-] | 323 | `bluetooth/esp_ble_iso/big_broadcaster` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。LE Audio 等时信道与 LC3 专用音频硬件编解码。 |
| [-] | 324 | `bluetooth/esp_ble_iso/big_receiver` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。LE Audio 等时信道与 LC3 专用音频硬件编解码。 |
| [-] | 325 | `bluetooth/esp_ble_iso/cis_central` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。LE Audio 等时信道与 LC3 专用音频硬件编解码。 |
| [-] | 326 | `bluetooth/esp_ble_iso/cis_peripheral` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。LE Audio 等时信道与 LC3 专用音频硬件编解码。 |
| [-] | 327 | `bluetooth/esp_ble_mesh/aligenie_demo` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [-] | 328 | `bluetooth/esp_ble_mesh/directed_forwarding/df_client` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [-] | 329 | `bluetooth/esp_ble_mesh/directed_forwarding/df_server` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [-] | 330 | `bluetooth/esp_ble_mesh/fast_provisioning/fast_prov_client` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [-] | 331 | `bluetooth/esp_ble_mesh/fast_provisioning/fast_prov_server` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [-] | 332 | `bluetooth/esp_ble_mesh/onoff_models/onoff_client` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [-] | 333 | `bluetooth/esp_ble_mesh/onoff_models/onoff_server` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [-] | 334 | `bluetooth/esp_ble_mesh/provisioner` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [-] | 335 | `bluetooth/esp_ble_mesh/remote_provisioning/rpr_client` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [-] | 336 | `bluetooth/esp_ble_mesh/remote_provisioning/rpr_server` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [-] | 337 | `bluetooth/esp_ble_mesh/remote_provisioning/unprov_dev` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [-] | 338 | `bluetooth/esp_ble_mesh/sensor_models/sensor_client` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [-] | 339 | `bluetooth/esp_ble_mesh/sensor_models/sensor_server` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [-] | 340 | `bluetooth/esp_ble_mesh/vendor_models/vendor_client` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [-] | 341 | `bluetooth/esp_ble_mesh/vendor_models/vendor_server` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [-] | 342 | `bluetooth/esp_ble_mesh/wifi_coexist` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [ ] | 343 | `bluetooth/esp_hid_device` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 344 | `bluetooth/esp_hid_host` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [-] | 345 | `bluetooth/hci/ble_adv_scan_combined` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。裸控制器 HCI 物理 UART 传输层协议。 |
| [-] | 346 | `bluetooth/hci/controller_hci_uart_esp32` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。裸控制器 HCI 物理 UART 传输层协议。 |
| [-] | 347 | `bluetooth/hci/controller_hci_uart_esp32c3_and_esp32s3` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。裸控制器 HCI 物理 UART 传输层协议。 |
| [-] | 348 | `bluetooth/hci/controller_vhci_ble_adv` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。裸控制器 HCI 物理 UART 传输层协议。 |
| [ ] | 349 | `bluetooth/nimble/ble_ancs` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 350 | `bluetooth/nimble/ble_chan_sound_initiator` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 351 | `bluetooth/nimble/ble_chan_sound_reflector` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 352 | `bluetooth/nimble/ble_cte/ble_periodic_adv_with_cte` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 353 | `bluetooth/nimble/ble_cte/ble_periodic_sync_with_cte` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 354 | `bluetooth/nimble/ble_cts/cts_cent` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 355 | `bluetooth/nimble/ble_cts/cts_prph` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 356 | `bluetooth/nimble/ble_dynamic_service` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 357 | `bluetooth/nimble/ble_enc_adv_data/enc_adv_data_cent` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 358 | `bluetooth/nimble/ble_enc_adv_data/enc_adv_data_prph` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 359 | `bluetooth/nimble/ble_gattc_gatts_coex` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 360 | `bluetooth/nimble/ble_htp/htp_cent` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 361 | `bluetooth/nimble/ble_htp/htp_prph` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 362 | `bluetooth/nimble/ble_l2cap_coc/coc_blecent` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 363 | `bluetooth/nimble/ble_l2cap_coc/coc_bleprph` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 364 | `bluetooth/nimble/ble_multi_adv` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 365 | `bluetooth/nimble/ble_multi_conn/ble_multi_conn_cent` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 366 | `bluetooth/nimble/ble_multi_conn/ble_multi_conn_prph` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 367 | `bluetooth/nimble/ble_pawr_adv/ble_pawr_adv` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 368 | `bluetooth/nimble/ble_pawr_adv/ble_pawr_sync` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 369 | `bluetooth/nimble/ble_pawr_adv_conn/ble_pawr_adv_conn` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 370 | `bluetooth/nimble/ble_pawr_adv_conn/ble_pawr_sync_conn` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 371 | `bluetooth/nimble/ble_periodic_adv` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 372 | `bluetooth/nimble/ble_periodic_sync` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 373 | `bluetooth/nimble/ble_phy/phy_cent` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 374 | `bluetooth/nimble/ble_phy/phy_prph` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 375 | `bluetooth/nimble/ble_proximity_sensor/proximity_sensor_cent` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 376 | `bluetooth/nimble/ble_proximity_sensor/proximity_sensor_prph` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 377 | `bluetooth/nimble/ble_spi_slave` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 378 | `bluetooth/nimble/ble_spp/spp_client` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 379 | `bluetooth/nimble/ble_spp/spp_server` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 380 | `bluetooth/nimble/blecent` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 381 | `bluetooth/nimble/blecsc` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 382 | `bluetooth/nimble/blehr` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 383 | `bluetooth/nimble/blemesh` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [x] | 384 | `bluetooth/nimble/bleprph` | 🎯 Level 1 | P0 | `esp_idfv61_bleprph` | M4-3/M4-4 已落地。NimBLE GATT 静态属性池、特征值读写回调与 Virtual BLE Inspector 交互面板。 |
| [ ] | 385 | `bluetooth/nimble/bleprph_host_only` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 386 | `bluetooth/nimble/bleprph_wifi_coex` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [-] | 387 | `bluetooth/nimble/hci` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。裸控制器 HCI 物理 UART 传输层协议。 |
| [ ] | 388 | `bluetooth/nimble/power_save` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 389 | `bluetooth/nimble/throughput_app/blecent_throughput` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 390 | `bluetooth/nimble/throughput_app/bleprph_throughput` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |

---

<a id="storage"></a>
### 片上存储与文件系统 (Storage)（共 27 项 | 编号 `#391 ~ #417` | 已实证: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [ ] | 391 | `storage/custom_flash_driver` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 392 | `storage/emmc` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 393 | `storage/fatfs/bdl_wl` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 394 | `storage/fatfs/ext_flash` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 395 | `storage/fatfs/fatfsgen` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 396 | `storage/fatfs/fs_operations` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 397 | `storage/fatfs/getting_started` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 398 | `storage/littlefs` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 399 | `storage/nvs/nvs_bootloader` | ⚙️ Level 4 | P1 | `待适配` | 排期中。拟对接 UniSim 虚拟键值存储，支持 Flash 掉电保存。 |
| [ ] | 400 | `storage/nvs/nvs_console` | ⚙️ Level 4 | P1 | `待适配` | 排期中。拟对接 UniSim 虚拟键值存储，支持 Flash 掉电保存。 |
| [ ] | 401 | `storage/nvs/nvs_iteration` | ⚙️ Level 4 | P1 | `待适配` | 排期中。拟对接 UniSim 虚拟键值存储，支持 Flash 掉电保存。 |
| [ ] | 402 | `storage/nvs/nvs_rw_blob` | ⚙️ Level 4 | P1 | `待适配` | 排期中。拟对接 UniSim 虚拟键值存储，支持 Flash 掉电保存。 |
| [ ] | 403 | `storage/nvs/nvs_rw_value` | ⚙️ Level 4 | P1 | `待适配` | 排期中。拟对接 UniSim 虚拟键值存储，支持 Flash 掉电保存。 |
| [ ] | 404 | `storage/nvs/nvs_rw_value_cxx` | ⚙️ Level 4 | P1 | `待适配` | 排期中。拟对接 UniSim 虚拟键值存储，支持 Flash 掉电保存。 |
| [ ] | 405 | `storage/nvs/nvs_statistics` | ⚙️ Level 4 | P1 | `待适配` | 排期中。拟对接 UniSim 虚拟键值存储，支持 Flash 掉电保存。 |
| [ ] | 406 | `storage/nvs/nvsgen` | ⚙️ Level 4 | P1 | `待适配` | 排期中。拟对接 UniSim 虚拟键值存储，支持 Flash 掉电保存。 |
| [ ] | 407 | `storage/partition_api/partition_find` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 408 | `storage/partition_api/partition_mmap` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 409 | `storage/partition_api/partition_ops` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 410 | `storage/parttool` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 411 | `storage/perf_benchmark` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 412 | `storage/sd_card/sdmmc` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 413 | `storage/sd_card/sdspi` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 414 | `storage/semihost_vfs` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 415 | `storage/spiffs` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 416 | `storage/spiffsgen` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 417 | `storage/wear_levelling` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |

---

<a id="network"></a>
### 底层网络与接口 (Network)（共 5 项 | 编号 `#418 ~ #422` | 已实证: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [ ] | 418 | `network/bridge` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 419 | `network/eth2ap` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 420 | `network/simple_sniffer` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 421 | `network/sta2eth` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 422 | `network/vlan_support` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |

---

<a id="cxx"></a>
### C++ 运行时与语言特性 (C++)（共 3 项 | 编号 `#423 ~ #425` | 已实证: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [ ] | 423 | `cxx/exceptions` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 424 | `cxx/pthread` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 425 | `cxx/rtti` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |

---

<a id="build_system"></a>
### 构建系统与组件组织 (Build System)（共 19 项 | 编号 `#426 ~ #444` | 已实证: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 426 | `build_system/cmake/component_manager` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 427 | `build_system/cmake/idf_as_lib` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 428 | `build_system/cmake/import_lib` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 429 | `build_system/cmake/import_prebuilt` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 430 | `build_system/cmake/import_prebuilt/prebuilt` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 431 | `build_system/cmake/multi_config` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 432 | `build_system/cmake/plugins` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 433 | `build_system/cmakev2/features/component_manager` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 434 | `build_system/cmakev2/features/conditional_component` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 435 | `build_system/cmakev2/features/idf_as_lib` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 436 | `build_system/cmakev2/features/import_lib` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 437 | `build_system/cmakev2/features/import_lib_direct` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 438 | `build_system/cmakev2/features/import_prebuilt` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 439 | `build_system/cmakev2/features/import_prebuilt/prebuilt` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 440 | `build_system/cmakev2/features/multi_binary` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 441 | `build_system/cmakev2/features/multi_config` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 442 | `build_system/cmakev2/features/plugins` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 443 | `build_system/cmakev2/get-started/hello_world` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 444 | `build_system/wrappers` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |

---

<a id="security"></a>
### 硬件加密与芯片安全特性 (Security - 硬件物理特性)（共 10 项 | 编号 `#445 ~ #454` | 已实证: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 445 | `security/flash_encryption` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。涉及硬件 eFuse 烧死熔断与物理 Flash 硬件解密引擎。 |
| [-] | 446 | `security/hmac_soft_jtag` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。涉及硬件 eFuse 烧死熔断与物理 Flash 硬件解密引擎。 |
| [-] | 447 | `security/key_manager` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。涉及硬件 eFuse 烧死熔断与物理 Flash 硬件解密引擎。 |
| [-] | 448 | `security/nvs_encryption_hmac` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。涉及硬件 eFuse 烧死熔断与物理 Flash 硬件解密引擎。 |
| [-] | 449 | `security/psa_its_custom_backend` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。涉及硬件 eFuse 烧死熔断与物理 Flash 硬件解密引擎。 |
| [-] | 450 | `security/security_features_app` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。涉及硬件 eFuse 烧死熔断与物理 Flash 硬件解密引擎。 |
| [-] | 451 | `security/tee/tee_attestation` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。涉及硬件 eFuse 烧死熔断与物理 Flash 硬件解密引擎。 |
| [-] | 452 | `security/tee/tee_basic` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。涉及硬件 eFuse 烧死熔断与物理 Flash 硬件解密引擎。 |
| [-] | 453 | `security/tee/tee_secure_ota` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。涉及硬件 eFuse 烧死熔断与物理 Flash 硬件解密引擎。 |
| [-] | 454 | `security/tee/tee_secure_storage` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。涉及硬件 eFuse 烧死熔断与物理 Flash 硬件解密引擎。 |

---

<a id="custom_bootloader"></a>
### 定制引导加载程序 (Custom Bootloader)（共 4 项 | 编号 `#455 ~ #458` | 已实证: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 455 | `custom_bootloader/bootloader_extra_dir` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。芯片二级引导程序，纯软件仿真直接进入 app_main。 |
| [-] | 456 | `custom_bootloader/bootloader_hooks` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。芯片二级引导程序，纯软件仿真直接进入 app_main。 |
| [-] | 457 | `custom_bootloader/bootloader_multiboot` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。芯片二级引导程序，纯软件仿真直接进入 app_main。 |
| [-] | 458 | `custom_bootloader/bootloader_override` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。芯片二级引导程序，纯软件仿真直接进入 app_main。 |

---

<a id="ethernet"></a>
### 有线以太网 (Ethernet - 外部 PHY 硬件)（共 3 项 | 编号 `#459 ~ #461` | 已实证: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 459 | `ethernet/basic` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。依赖板外物理变压器及外部 PHY 芯片（RMII/SMI 总线）。 |
| [-] | 460 | `ethernet/iperf` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。依赖板外物理变压器及外部 PHY 芯片（RMII/SMI 总线）。 |
| [-] | 461 | `ethernet/ptp` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。依赖板外物理变压器及外部 PHY 芯片（RMII/SMI 总线）。 |

---

<a id="lowpower"></a>
### 超低功耗与 ULP 协处理器 (Low Power & ULP)（共 2 项 | 编号 `#462 ~ #463` | 已实证: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 462 | `lowpower/power_management` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。ULP 独立微功耗 FSM/RISC-V 协处理器硬件运行环境。 |
| [-] | 463 | `lowpower/vbat` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。ULP 独立微功耗 FSM/RISC-V 协处理器硬件运行环境。 |

---

<a id="mesh"></a>
### Wi-Fi 空间自组网 (Mesh)（共 3 项 | 编号 `#464 ~ #466` | 已实证: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 464 | `mesh/internal_communication` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。多机空间电磁跳步网络，需分布式拓扑模拟。 |
| [-] | 465 | `mesh/ip_internal_network` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。多机空间电磁跳步网络，需分布式拓扑模拟。 |
| [-] | 466 | `mesh/manual_networking` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。多机空间电磁跳步网络，需分布式拓扑模拟。 |

---

<a id="openthread"></a>
### OpenThread 802.15.4 线程网络 (OpenThread)（共 6 项 | 编号 `#467 ~ #472` | 已实证: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 467 | `openthread/ot_br` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。依赖 2.4GHz 空间物理射频网卡与硬件基带解调。 |
| [-] | 468 | `openthread/ot_cli` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。依赖 2.4GHz 空间物理射频网卡与硬件基带解调。 |
| [-] | 469 | `openthread/ot_rcp` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。依赖 2.4GHz 空间物理射频网卡与硬件基带解调。 |
| [-] | 470 | `openthread/ot_sleepy_device/deep_sleep` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。依赖 2.4GHz 空间物理射频网卡与硬件基带解调。 |
| [-] | 471 | `openthread/ot_sleepy_device/light_sleep` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。依赖 2.4GHz 空间物理射频网卡与硬件基带解调。 |
| [-] | 472 | `openthread/ot_trel` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。依赖 2.4GHz 空间物理射频网卡与硬件基带解调。 |

---

<a id="zigbee"></a>
### Zigbee 2.4G 射频网络 (Zigbee)（共 3 项 | 编号 `#473 ~ #475` | 已实证: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 473 | `zigbee/esp_zigbee_gateway` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。依赖 2.4GHz 空间物理射频网卡与硬件基带解调。 |
| [-] | 474 | `zigbee/light_sample/HA_on_off_light` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。依赖 2.4GHz 空间物理射频网卡与硬件基带解调。 |
| [-] | 475 | `zigbee/light_sample/HA_on_off_switch` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。依赖 2.4GHz 空间物理射频网卡与硬件基带解调。 |

---

<a id="ieee802154"></a>
### IEEE 802.15.4 原始射频 (IEEE 802.15.4)（共 1 项 | 编号 `#476 ~ #476` | 已实证: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 476 | `ieee802154/ieee802154_cli` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。依赖 2.4GHz 空间物理射频网卡与硬件基带解调。 |

---

<a id="phy"></a>
### 射频物理层与工厂校准 (PHY & Calibration)（共 2 项 | 编号 `#477 ~ #478` | 已实证: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 477 | `phy/antenna` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。芯片工厂模拟射频电气校准与功率表。 |
| [-] | 478 | `phy/cert_test` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。芯片工厂模拟射频电气校准与功率表。 |

---
