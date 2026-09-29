<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- ⚠️  此文件由 generate_checklist_v1_1.py 自动生成，严禁人工直接编辑！修改请编辑 checklist.data.json -->
# ESP-IDF v6.1 官方示例全量仿真适配核对清单 (Checklist)

> **数据单一事实源（SSOT）**：[`checklist.data.json`](checklist.data.json)（Spec v2.0.0，多配置实例与五维正交模型）  
> **生成时间**：2026-09-29  
> **分类规范**：[`CLASSIFICATION-SPEC.md`](CLASSIFICATION-SPEC.md) (v2.0)  
> **能力字典**：[`capability-catalog.yaml`](capability-catalog.yaml)  
> **隔离区白名单**：[`.gates/quarantine.yaml`](.gates/quarantine.yaml)（10 项存量债务，14 天 TTL 生效中）  
> **执行手册**：[`PLAYBOOK.md`](PLAYBOOK.md) (v2.0)  

---

## 一、 总体适配进度统计

- **官方独立示例总数**：**478 个**
  - `[x]` **已完成六要素实证 (Verified)**：**0 项**
  - `[?]` **存量隔离待补凭证 (Quarantined Debt)**：**10 项**（14 天 TTL 过期硬阻断，至 `2026-10-13`）
  - `[ ]` **规划中正常排期 (In-Scope Planned)**：**0 项**
  - `[-]` **明确产品排除 / 暂缓投入 (Out-of-Scope / Deferred)**：**183 项**（编译期 `WINK_SLA_ERROR` Fail-Loud 阻断）
  - `?` **待深度审定 (Pending Audit / Unknown Scope)**：**285 项**

### 大类索引导航

| 序号 | 功能大类 | 包含示例数 | 编号跨度 | 已实证 | 隔离待补 |
| :---: | :--- | :---: | :---: | :---: | :---: |
| 01 | [基础快速起步 (Get-Started)](#get-started) | 2 项 | `#001 ~ #002` | 0 项 | 1 项 |
| 02 | [片上与总线外设 (Peripherals)](#peripherals) | 114 项 | `#003 ~ #116` | 0 项 | 4 项 |
| 03 | [操作系统与核心系统调用 (System & OS)](#system) | 68 项 | `#117 ~ #184` | 0 项 | 0 项 |
| 04 | [网络与应用层通信协议 (Protocols)](#protocols) | 35 项 | `#185 ~ #219` | 0 项 | 2 项 |
| 05 | [Wi-Fi 无线局域网 (Wi-Fi)](#wifi) | 24 项 | `#220 ~ #243` | 0 项 | 2 项 |
| 06 | [蓝牙协议栈 (Bluetooth)](#bluetooth) | 147 项 | `#244 ~ #390` | 0 项 | 1 项 |
| 07 | [片上存储与文件系统 (Storage)](#storage) | 27 项 | `#391 ~ #417` | 0 项 | 0 项 |
| 08 | [底层网络与接口 (Network)](#network) | 5 项 | `#418 ~ #422` | 0 项 | 0 项 |
| 09 | [C++ 运行时与语言特性 (C++)](#cxx) | 3 项 | `#423 ~ #425` | 0 项 | 0 项 |
| 10 | [构建系统与组件组织 (Build System)](#build_system) | 19 项 | `#426 ~ #444` | 0 项 | 0 项 |
| 11 | [硬件加密与芯片安全特性 (Security)](#security) | 10 项 | `#445 ~ #454` | 0 项 | 0 项 |
| 12 | [定制引导加载程序 (Custom Bootloader)](#custom_bootloader) | 4 项 | `#455 ~ #458` | 0 项 | 0 项 |
| 13 | [有线以太网 (Ethernet - 外部 PHY 硬件)](#ethernet) | 3 项 | `#459 ~ #461` | 0 项 | 0 项 |
| 14 | [超低功耗与 ULP 协处理器 (Low Power & ULP)](#lowpower) | 2 项 | `#462 ~ #463` | 0 项 | 0 项 |
| 15 | [Wi-Fi 空间自组网 (Mesh)](#mesh) | 3 项 | `#464 ~ #466` | 0 项 | 0 项 |
| 16 | [OpenThread 802.15.4 线程网络 (OpenThread)](#openthread) | 6 项 | `#467 ~ #472` | 0 项 | 0 项 |
| 17 | [Zigbee 2.4G 射频网络 (Zigbee)](#zigbee) | 3 项 | `#473 ~ #475` | 0 项 | 0 项 |
| 18 | [IEEE 802.15.4 原始射频 (IEEE 802.15.4)](#ieee802154) | 1 项 | `#476 ~ #476` | 0 项 | 0 项 |
| 19 | [射频物理层与工厂校准 (PHY & Calibration)](#phy) | 2 项 | `#477 ~ #478` | 0 项 | 0 项 |

---

## 二、 符号与分类说明

- `[x]` **已完成实证 (Verified)**：完全满足六要素合取公式（范围有效、审计覆盖、依赖闭包满足、声明验证、防伪哈希匹配、断言全过）。
- `[?]` **存量隔离待补凭证 (Quarantined)**：早期存量打样条目，已入 `.gates/quarantine.yaml` 隔离区白名单，14 天 TTL 内待补齐哈希与执行凭据。
- `[ ]` **待适配 / 待审定 (Planned / Pending)**：在规划范围内待排期，或处于初始抓取状态待进一步核验源码。
- `[-]` **声明 Out-of-Scope / 暂缓投入**：不可逆物理介质在编译期通过 `WINK_SLA_ERROR` Fail-Loud 显式阻断；或暂缓投入。
- `[~]` **正在构建 (Building)**：WIP 开发中，尚未产出完整自动化测试实证。
- `[!]` **回归失败 / 凭据陈旧 (Regressed / Stale)**：断言失败或工作区资产与登记哈希不一致，严禁打勾。

| 等级 | 符号 | 说明 |
|---|---|---|
| Level 1 | 🎯 | UniSim 画布有直观控件（LED、数码管、BLE Inspector）|
| Level 2 | 📜 | 控制台日志 / 网络数据流（UART、MQTT、HTTP）|
| Level 3 | ⚡ | IO 打点 / GPIO 波形探测 |
| Level 4 | ⚙️ | 纯内部静默逻辑（内存、错误码、寄存器状态）|
| Blocked | 🚫 | 前置阻断，依赖未建模，会导致仿真死锁 |

---

## 三、 478 个官方示例逐项核对总账

<a id="get-started"></a>
### 基础快速起步 (Get-Started)（共 2 项 | 编号 `#001 ~ #002` | 已实证: 0 项 | 隔离待补: 1 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [?] | 001 | `get-started/blink` | 🎯 Level 1 | P1 | `esp_idfv61_blink` | 待补凭证 (存量隔离区债务，14天 TTL 至 2026-10-13) |
| [ ] | 002 | `get-started/hello_world` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |

---

<a id="peripherals"></a>
### 片上与总线外设 (Peripherals)（共 114 项 | 编号 `#003 ~ #116` | 已实证: 0 项 | 隔离待补: 4 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [ ] | 003 | `peripherals/adc/continuous_read` | 🎯 Level 1 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 004 | `peripherals/adc/oneshot_read` | 🎯 Level 1 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 005 | `peripherals/analog_comparator/auto_scan` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 006 | `peripherals/analog_comparator/etm_periodic_scan` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 007 | `peripherals/bitscrambler` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [-] | 008 | `peripherals/camera/dvp_dsi` | 🎯 Level 1 | P4 | `待适配` | 声明 Out-of-Scope。MIPI-CSI/DVP 物理差分摄像头传感器。 |
| [-] | 009 | `peripherals/camera/dvp_isp_dsi` | 🎯 Level 1 | P4 | `待适配` | 声明 Out-of-Scope。MIPI-CSI/DVP 物理差分摄像头传感器。 |
| [-] | 010 | `peripherals/camera/mipi_isp_dsi` | 🎯 Level 1 | P4 | `待适配` | 声明 Out-of-Scope。MIPI-CSI/DVP 物理差分摄像头传感器。 |
| [ ] | 011 | `peripherals/dac/dac_continuous/dac_audio` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 012 | `peripherals/dac/dac_continuous/signal_generator` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 013 | `peripherals/dac/dac_cosine_wave` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 014 | `peripherals/dac/dac_oneshot` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 015 | `peripherals/dedicated_gpio/soft_i2c` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 016 | `peripherals/dedicated_gpio/soft_spi` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 017 | `peripherals/dedicated_gpio/soft_uart` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 018 | `peripherals/dma/async_color_convert` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 019 | `peripherals/dma/async_crc` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 020 | `peripherals/gpio/generic_gpio` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 021 | `peripherals/gpio/matrix_keyboard` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [-] | 022 | `peripherals/h264` | 🎯 Level 1 | P4 | `待适配` | 声明 Out-of-Scope。硬件 H.264 编解码加速器。 |
| [?] | 023 | `peripherals/i2c/i2c_basic` | 📜 Level 2 | P1 | `esp_idfv61_i2c_basic` | 待补凭证 (存量隔离区债务，14天 TTL 至 2026-10-13) |
| [ ] | 024 | `peripherals/i2c/i2c_eeprom` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 025 | `peripherals/i2c/i2c_slave_network_sensor` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 026 | `peripherals/i2c/i2c_tools` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 027 | `peripherals/i2c/i2c_u8g2` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [-] | 028 | `peripherals/i2s/i2s_advance/i2s_usb` | 📜 Level 2 | P4 | `待适配` | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [ ] | 029 | `peripherals/i2s/i2s_basic/i2s_pdm` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 030 | `peripherals/i2s/i2s_basic/i2s_std` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 031 | `peripherals/i2s/i2s_basic/i2s_tdm` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 032 | `peripherals/i2s/i2s_codec/i2s_es7210_tdm` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 033 | `peripherals/i2s/i2s_codec/i2s_es8311` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 034 | `peripherals/i2s/i2s_recorder` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 035 | `peripherals/i3c/i3c_i2c_basic` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 036 | `peripherals/i3c/i3c_lsm6dscx` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [-] | 037 | `peripherals/isp/multi_pipelines` | 🎯 Level 1 | P4 | `待适配` | 声明 Out-of-Scope。图像信号处理器 (ISP) 硬件管线。 |
| [-] | 038 | `peripherals/jpeg/jpeg_decode` | 🎯 Level 1 | P4 | `待适配` | 声明 Out-of-Scope。硬件 JPEG 编解码加速器。 |
| [-] | 039 | `peripherals/jpeg/jpeg_encode` | 🎯 Level 1 | P4 | `待适配` | 声明 Out-of-Scope。硬件 JPEG 编解码加速器。 |
| [ ] | 040 | `peripherals/lcd/i2c_oled` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 041 | `peripherals/lcd/i80_controller` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 042 | `peripherals/lcd/mipi_dsi` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 043 | `peripherals/lcd/parlio_simulate` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 044 | `peripherals/lcd/rgb_panel` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 045 | `peripherals/lcd/spi_lcd_touch` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 046 | `peripherals/lcd/tjpgd` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [?] | 047 | `peripherals/ledc/ledc_basic` | 🎯 Level 1 | P1 | `esp_idfv61_ledc_basic` | 待补凭证 (存量隔离区债务，14天 TTL 至 2026-10-13) |
| [ ] | 048 | `peripherals/ledc/ledc_dimmer` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 049 | `peripherals/ledc/ledc_fade` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 050 | `peripherals/ledc/ledc_gamma_curve_fade` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 051 | `peripherals/mcpwm/mcpwm_bdc_speed_control` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 052 | `peripherals/mcpwm/mcpwm_bldc_hall_control` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 053 | `peripherals/mcpwm/mcpwm_capture_hc_sr04` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 054 | `peripherals/mcpwm/mcpwm_foc_svpwm_open_loop` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 055 | `peripherals/mcpwm/mcpwm_servo_control` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 056 | `peripherals/mcpwm/mcpwm_sync` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 057 | `peripherals/parlio/parlio_rx/logic_analyzer` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 058 | `peripherals/parlio/parlio_tx/advanced_rgb_led_matrix` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 059 | `peripherals/parlio/parlio_tx/simple_rgb_led_matrix` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 060 | `peripherals/pcnt/rotary_encoder` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [-] | 061 | `peripherals/ppa/ppa_rgb_lcd` | 🎯 Level 1 | P4 | `待适配` | 声明 Out-of-Scope。像素处理加速器 (PPA) 硬件管线。 |
| [ ] | 062 | `peripherals/rmt/dshot_esc` | 🎯 Level 1 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 063 | `peripherals/rmt/ir_nec_transceiver` | 🎯 Level 1 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 064 | `peripherals/rmt/led_strip` | 🎯 Level 1 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 065 | `peripherals/rmt/led_strip_simple_encoder` | 🎯 Level 1 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 066 | `peripherals/rmt/musical_buzzer` | 🎯 Level 1 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 067 | `peripherals/rmt/onewire` | 🎯 Level 1 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 068 | `peripherals/rmt/stepper_motor` | 🎯 Level 1 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [-] | 069 | `peripherals/sdio/host` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。SDIO 从机高速差分总线。 |
| [-] | 070 | `peripherals/sdio/slave` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。SDIO 从机高速差分总线。 |
| [ ] | 071 | `peripherals/sigma_delta/sdm_dac` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 072 | `peripherals/sigma_delta/sdm_led` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 073 | `peripherals/spi_master/hd_eeprom` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 074 | `peripherals/spi_master/lcd` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 075 | `peripherals/spi_slave/receiver` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 076 | `peripherals/spi_slave/sender` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 077 | `peripherals/spi_slave_hd/append_mode/master` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 078 | `peripherals/spi_slave_hd/append_mode/slave` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 079 | `peripherals/spi_slave_hd/segment_mode/seg_master` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 080 | `peripherals/spi_slave_hd/segment_mode/seg_slave` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 081 | `peripherals/temperature_sensor/temp_sensor` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 082 | `peripherals/temperature_sensor/temp_sensor_monitor` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [?] | 083 | `peripherals/timer_group/gptimer` | ⚡ Level 3 | P1 | `esp_idfv61_gptimer` | 待补凭证 (存量隔离区债务，14天 TTL 至 2026-10-13) |
| [ ] | 084 | `peripherals/timer_group/gptimer_capture_hc_sr04` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 085 | `peripherals/timer_group/wiegand_interface` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 086 | `peripherals/touch_sensor/touch_sens_basic` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 087 | `peripherals/touch_sensor/touch_sens_sleep` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [-] | 088 | `peripherals/twai/cybergear` | 📜 Level 2 | P4 | `待适配` | 声明 Out-of-Scope。TWAI/CAN 差分总线物理收发器。 |
| [-] | 089 | `peripherals/twai/twai_error_recovery` | 📜 Level 2 | P4 | `待适配` | 声明 Out-of-Scope。TWAI/CAN 差分总线物理收发器。 |
| [-] | 090 | `peripherals/twai/twai_network/twai_listen_only` | 📜 Level 2 | P4 | `待适配` | 声明 Out-of-Scope。TWAI/CAN 差分总线物理收发器。 |
| [-] | 091 | `peripherals/twai/twai_network/twai_sender` | 📜 Level 2 | P4 | `待适配` | 声明 Out-of-Scope。TWAI/CAN 差分总线物理收发器。 |
| [-] | 092 | `peripherals/twai/twai_utils` | 📜 Level 2 | P4 | `待适配` | 声明 Out-of-Scope。TWAI/CAN 差分总线物理收发器。 |
| [ ] | 093 | `peripherals/uart/nmea0183_parser` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 094 | `peripherals/uart/uart_async_rxtxtasks` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 095 | `peripherals/uart/uart_dma_ota` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [?] | 096 | `peripherals/uart/uart_echo` | 📜 Level 2 | P1 | `esp_idfv61_uart_echo` | 待补凭证 (存量隔离区债务，14天 TTL 至 2026-10-13) |
| [ ] | 097 | `peripherals/uart/uart_echo_rs485` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 098 | `peripherals/uart/uart_events` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 099 | `peripherals/uart/uart_repl` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 100 | `peripherals/uart/uart_select` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [-] | 101 | `peripherals/usb/device/cherryusb_serial_device` | 📜 Level 2 | P4 | `待适配` | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [-] | 102 | `peripherals/usb/device/tusb_cdc_acm_wakeup` | 📜 Level 2 | P4 | `待适配` | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [-] | 103 | `peripherals/usb/device/tusb_composite_msc_serialdevice` | 📜 Level 2 | P4 | `待适配` | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [-] | 104 | `peripherals/usb/device/tusb_console` | 📜 Level 2 | P4 | `待适配` | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [-] | 105 | `peripherals/usb/device/tusb_hid` | 📜 Level 2 | P4 | `待适配` | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [-] | 106 | `peripherals/usb/device/tusb_midi` | 📜 Level 2 | P4 | `待适配` | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [-] | 107 | `peripherals/usb/device/tusb_msc` | 📜 Level 2 | P4 | `待适配` | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [-] | 108 | `peripherals/usb/device/tusb_ncm` | 📜 Level 2 | P4 | `待适配` | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [-] | 109 | `peripherals/usb/device/tusb_serial_device` | 📜 Level 2 | P4 | `待适配` | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [-] | 110 | `peripherals/usb/host/cdc` | 📜 Level 2 | P4 | `待适配` | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [-] | 111 | `peripherals/usb/host/cherryusb_host` | 📜 Level 2 | P4 | `待适配` | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [-] | 112 | `peripherals/usb/host/hid` | 📜 Level 2 | P4 | `待适配` | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [-] | 113 | `peripherals/usb/host/msc` | 📜 Level 2 | P4 | `待适配` | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [-] | 114 | `peripherals/usb/host/usb_host_lib` | 📜 Level 2 | P4 | `待适配` | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [-] | 115 | `peripherals/usb/host/uvc` | 📜 Level 2 | P4 | `待适配` | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [-] | 116 | `peripherals/usb_serial_jtag/usb_serial_jtag_echo` | 📜 Level 2 | P4 | `待适配` | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |

---

<a id="system"></a>
### 操作系统与核心系统调用 (System & OS)（共 68 项 | 编号 `#117 ~ #184` | 已实证: 0 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [ ] | 117 | `system/app_trace_basic` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 118 | `system/app_trace_to_plot` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 119 | `system/base_mac_address` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 120 | `system/console/advanced` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 121 | `system/console/basic` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 122 | `system/deep_sleep` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 123 | `system/deep_sleep_wake_stub` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 124 | `system/efuse` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 125 | `system/esp_event/default_event_loop` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 126 | `system/esp_event/user_event_loops` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 127 | `system/esp_timer` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 128 | `system/esp_trace` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 129 | `system/eventfd` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 130 | `system/flash_suspend` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 131 | `system/freertos/basic_freertos_smp_usage` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 132 | `system/freertos/real_time_stats` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 133 | `system/gcov` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 134 | `system/gdbstub` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 135 | `system/heap_task_tracking/advanced` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 136 | `system/heap_task_tracking/basic` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 137 | `system/himem` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 138 | `system/ipc/ipc_isr/riscv` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 139 | `system/ipc/ipc_isr/xtensa` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 140 | `system/light_sleep` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 141 | `system/nmi_isr` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 142 | `system/ota/advanced_https_ota` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 143 | `system/ota/native_ota_example` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 144 | `system/ota/otatool` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 145 | `system/ota/partitions_ota` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 146 | `system/ota/simple_ota_example` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 147 | `system/perfmon` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 148 | `system/pthread` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 149 | `system/rt_mqueue` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 150 | `system/select` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 151 | `system/startup_time` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 152 | `system/sysview_tracing` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 153 | `system/sysview_tracing_heap_log` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 154 | `system/task_watchdog` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 155 | `system/ulp/lp_core/build_system` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 156 | `system/ulp/lp_core/debugging` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 157 | `system/ulp/lp_core/gpio` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 158 | `system/ulp/lp_core/gpio_intr_pulse_counter` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 159 | `system/ulp/lp_core/gpio_wakeup` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 160 | `system/ulp/lp_core/inter_cpu_critical_section` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 161 | `system/ulp/lp_core/interrupt` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 162 | `system/ulp/lp_core/lp_adc` | 🎯 Level 1 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 163 | `system/ulp/lp_core/lp_i2c` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 164 | `system/ulp/lp_core/lp_mailbox` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 165 | `system/ulp/lp_core/lp_spi` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 166 | `system/ulp/lp_core/lp_timer_interrupt` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 167 | `system/ulp/lp_core/lp_touch` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 168 | `system/ulp/lp_core/lp_uart/lp_uart_char_seq_wakeup` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 169 | `system/ulp/lp_core/lp_uart/lp_uart_echo` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 170 | `system/ulp/lp_core/lp_uart/lp_uart_print` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 171 | `system/ulp/ulp_fsm/ulp_adc` | 🎯 Level 1 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 172 | `system/ulp/ulp_fsm_riscv_combined/counter` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 173 | `system/ulp/ulp_riscv/adc` | 🎯 Level 1 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 174 | `system/ulp/ulp_riscv/ds18b20_onewire` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 175 | `system/ulp/ulp_riscv/gpio` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 176 | `system/ulp/ulp_riscv/gpio_interrupt` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 177 | `system/ulp/ulp_riscv/gpio_pulse_counter` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 178 | `system/ulp/ulp_riscv/i2c` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 179 | `system/ulp/ulp_riscv/interrupts` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 180 | `system/ulp/ulp_riscv/touch` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 181 | `system/ulp/ulp_riscv/uart_print` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 182 | `system/unit_test` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 183 | `system/unit_test/test` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 184 | `system/xip_from_psram` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |

---

<a id="protocols"></a>
### 网络与应用层通信协议 (Protocols)（共 35 项 | 编号 `#185 ~ #219` | 已实证: 0 项 | 隔离待补: 2 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [ ] | 185 | `protocols/dns_over_https` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [?] | 186 | `protocols/esp_http_client` | 📜 Level 2 | P1 | `esp_idfv61_esp_http_client` | 待补凭证 (存量隔离区债务，14天 TTL 至 2026-10-13) |
| [ ] | 187 | `protocols/esp_http_client_mutual_auth` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 188 | `protocols/esp_local_ctrl` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 189 | `protocols/http_request` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 190 | `protocols/http_server/advanced_tests` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 191 | `protocols/http_server/async_handlers` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 192 | `protocols/http_server/captive_portal` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 193 | `protocols/http_server/file_serving` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 194 | `protocols/http_server/persistent_sockets` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 195 | `protocols/http_server/restful_server` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 196 | `protocols/http_server/simple` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 197 | `protocols/http_server/ws_echo_server` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 198 | `protocols/https_mbedtls` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 199 | `protocols/https_request` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 200 | `protocols/https_server/simple` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 201 | `protocols/https_server/wss_server` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 202 | `protocols/https_x509_bundle` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 203 | `protocols/icmp/pmtu_probe` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 204 | `protocols/icmp_echo` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 205 | `protocols/l2tap` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [?] | 206 | `protocols/mqtt` | 📜 Level 2 | P1 | `esp_idfv61_mqtt` | 待补凭证 (存量隔离区债务，14天 TTL 至 2026-10-13) |
| [ ] | 207 | `protocols/mqtt5` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 208 | `protocols/smtp_client` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 209 | `protocols/sntp` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 210 | `protocols/sockets/icmpv6_ping` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 211 | `protocols/sockets/non_blocking` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 212 | `protocols/sockets/tcp_client` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 213 | `protocols/sockets/tcp_client_multi_net` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 214 | `protocols/sockets/tcp_server` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 215 | `protocols/sockets/tcp_transport_client` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 216 | `protocols/sockets/udp_client` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 217 | `protocols/sockets/udp_multicast` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 218 | `protocols/sockets/udp_server` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 219 | `protocols/static_ip` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |

---

<a id="wifi"></a>
### Wi-Fi 无线局域网 (Wi-Fi)（共 24 项 | 编号 `#220 ~ #243` | 已实证: 0 项 | 隔离待补: 2 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [ ] | 220 | `wifi/espnow` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 221 | `wifi/fast_scan` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 222 | `wifi/ftm` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 223 | `wifi/getting_started/softAP` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [?] | 224 | `wifi/getting_started/station` | 📜 Level 2 | P1 | `esp_idfv61_station` | 待补凭证 (存量隔离区债务，14天 TTL 至 2026-10-13) |
| [ ] | 225 | `wifi/iperf` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 226 | `wifi/itwt` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 227 | `wifi/power_save` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 228 | `wifi/roaming/roaming_11kvr` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 229 | `wifi/roaming/roaming_app` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [?] | 230 | `wifi/scan` | 📜 Level 2 | P1 | `esp_idfv61_scan` | 待补凭证 (存量隔离区债务，14天 TTL 至 2026-10-13) |
| [ ] | 231 | `wifi/smart_config` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 232 | `wifi/softap_sta` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 233 | `wifi/wifi_aware/nan_console` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 234 | `wifi/wifi_aware/nan_publisher` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 235 | `wifi/wifi_aware/nan_subscriber` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 236 | `wifi/wifi_aware/usd_publisher` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 237 | `wifi/wifi_aware/usd_subscriber` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 238 | `wifi/wifi_eap_fast` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 239 | `wifi/wifi_easy_connect/dpp-enrollee` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 240 | `wifi/wifi_enterprise` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 241 | `wifi/wifi_nvs_config` | ⚙️ Level 4 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 242 | `wifi/wps` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 243 | `wifi/wps_softap_registrar` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |

---

<a id="bluetooth"></a>
### 蓝牙协议栈 (Bluetooth)（共 147 项 | 编号 `#244 ~ #390` | 已实证: 0 项 | 隔离待补: 1 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 244 | `bluetooth/ble_get_started/bluedroid/Bluedroid_Beacon` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 245 | `bluetooth/ble_get_started/bluedroid/Bluedroid_Connection` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 246 | `bluetooth/ble_get_started/bluedroid/Bluedroid_GATT_Server` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [ ] | 247 | `bluetooth/ble_get_started/nimble/NimBLE_Beacon` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 248 | `bluetooth/ble_get_started/nimble/NimBLE_Connection` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 249 | `bluetooth/ble_get_started/nimble/NimBLE_GATT_Server` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 250 | `bluetooth/ble_get_started/nimble/NimBLE_Security` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 251 | `bluetooth/ble_uart_service` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [-] | 252 | `bluetooth/bluedroid/ble/ble_acl_latency/cent` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 253 | `bluetooth/bluedroid/ble/ble_acl_latency/periph` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 254 | `bluetooth/bluedroid/ble/ble_ancs` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 255 | `bluetooth/bluedroid/ble/ble_compatibility_test` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 256 | `bluetooth/bluedroid/ble/ble_eddystone_receiver` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 257 | `bluetooth/bluedroid/ble/ble_eddystone_sender` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 258 | `bluetooth/bluedroid/ble/ble_enc_adv_data/enc_adv_data_cent` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 259 | `bluetooth/bluedroid/ble/ble_enc_adv_data/enc_adv_data_prph` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 260 | `bluetooth/bluedroid/ble/ble_hid_device_demo` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 261 | `bluetooth/bluedroid/ble/ble_ibeacon` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 262 | `bluetooth/bluedroid/ble/ble_multi_conn/ble_multi_conn_cent` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 263 | `bluetooth/bluedroid/ble/ble_multi_conn/ble_multi_conn_prph` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 264 | `bluetooth/bluedroid/ble/ble_spp_client` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 265 | `bluetooth/bluedroid/ble/ble_spp_server` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 266 | `bluetooth/bluedroid/ble/ble_throughput/throughput_client` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 267 | `bluetooth/bluedroid/ble/ble_throughput/throughput_server` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 268 | `bluetooth/bluedroid/ble/gatt_client` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 269 | `bluetooth/bluedroid/ble/gatt_security_client` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 270 | `bluetooth/bluedroid/ble/gatt_security_server` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 271 | `bluetooth/bluedroid/ble/gatt_server` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 272 | `bluetooth/bluedroid/ble/gatt_server_service_table` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 273 | `bluetooth/bluedroid/ble/gattc_multi_connect` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 274 | `bluetooth/bluedroid/ble_50/ble50_security_client` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 275 | `bluetooth/bluedroid/ble_50/ble50_security_server` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 276 | `bluetooth/bluedroid/ble_50/ble50_throughput/throughput_client` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 277 | `bluetooth/bluedroid/ble_50/ble50_throughput/throughput_server` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 278 | `bluetooth/bluedroid/ble_50/ble_conn_subrating_central` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 279 | `bluetooth/bluedroid/ble_50/ble_conn_subrating_peripheral` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 280 | `bluetooth/bluedroid/ble_50/ble_connection_central_with_cte` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 281 | `bluetooth/bluedroid/ble_50/ble_connection_peripheral_with_cte` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 282 | `bluetooth/bluedroid/ble_50/ble_pawr_advertiser` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 283 | `bluetooth/bluedroid/ble_50/ble_pawr_advertiser_conn` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 284 | `bluetooth/bluedroid/ble_50/ble_pawr_synchronizer` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 285 | `bluetooth/bluedroid/ble_50/ble_periodic_adv_with_cte` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 286 | `bluetooth/bluedroid/ble_50/ble_periodic_sync_with_cte` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 287 | `bluetooth/bluedroid/ble_50/ble_power_control_central` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 288 | `bluetooth/bluedroid/ble_50/ble_power_control_peripheral` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 289 | `bluetooth/bluedroid/ble_50/multi-adv` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 290 | `bluetooth/bluedroid/ble_50/periodic_adv` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 291 | `bluetooth/bluedroid/ble_50/periodic_sync` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 292 | `bluetooth/bluedroid/bluedroid_host_only/bluedroid_host_only_uart` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 293 | `bluetooth/bluedroid/classic_bt/a2dp_sink_stream` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 294 | `bluetooth/bluedroid/classic_bt/a2dp_sink_stream_aac` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 295 | `bluetooth/bluedroid/classic_bt/a2dp_source` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 296 | `bluetooth/bluedroid/classic_bt/a2dp_source_aac` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 297 | `bluetooth/bluedroid/classic_bt/avrcp_absolute_volume` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 298 | `bluetooth/bluedroid/classic_bt/avrcp_ct_cover_art` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 299 | `bluetooth/bluedroid/classic_bt/avrcp_ct_metadata` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 300 | `bluetooth/bluedroid/classic_bt/bt_discovery` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 301 | `bluetooth/bluedroid/classic_bt/bt_hid_mouse_device` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 302 | `bluetooth/bluedroid/classic_bt/bt_l2cap_client` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 303 | `bluetooth/bluedroid/classic_bt/bt_l2cap_server` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 304 | `bluetooth/bluedroid/classic_bt/bt_spp_acceptor` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 305 | `bluetooth/bluedroid/classic_bt/bt_spp_initiator` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 306 | `bluetooth/bluedroid/classic_bt/bt_spp_vfs_acceptor` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 307 | `bluetooth/bluedroid/classic_bt/bt_spp_vfs_initiator` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 308 | `bluetooth/bluedroid/classic_bt/hfp_ag` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 309 | `bluetooth/bluedroid/classic_bt/hfp_hf` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 310 | `bluetooth/bluedroid/coex/a2dp_gatts_coex` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 311 | `bluetooth/bluedroid/coex/gattc_gatts_coex` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [ ] | 312 | `bluetooth/blufi` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [-] | 313 | `bluetooth/esp_ble_audio/bap/broadcast_sink` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 314 | `bluetooth/esp_ble_audio/bap/broadcast_source` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 315 | `bluetooth/esp_ble_audio/bap/unicast_client` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 316 | `bluetooth/esp_ble_audio/bap/unicast_server` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 317 | `bluetooth/esp_ble_audio/cap/acceptor` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 318 | `bluetooth/esp_ble_audio/cap/initiator` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 319 | `bluetooth/esp_ble_audio/tmap/bmr` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 320 | `bluetooth/esp_ble_audio/tmap/bms` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 321 | `bluetooth/esp_ble_audio/tmap/central` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 322 | `bluetooth/esp_ble_audio/tmap/peripheral` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 323 | `bluetooth/esp_ble_iso/big_broadcaster` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 324 | `bluetooth/esp_ble_iso/big_receiver` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 325 | `bluetooth/esp_ble_iso/cis_central` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 326 | `bluetooth/esp_ble_iso/cis_peripheral` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 327 | `bluetooth/esp_ble_mesh/aligenie_demo` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 328 | `bluetooth/esp_ble_mesh/directed_forwarding/df_client` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 329 | `bluetooth/esp_ble_mesh/directed_forwarding/df_server` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 330 | `bluetooth/esp_ble_mesh/fast_provisioning/fast_prov_client` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 331 | `bluetooth/esp_ble_mesh/fast_provisioning/fast_prov_server` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 332 | `bluetooth/esp_ble_mesh/onoff_models/onoff_client` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 333 | `bluetooth/esp_ble_mesh/onoff_models/onoff_server` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 334 | `bluetooth/esp_ble_mesh/provisioner` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 335 | `bluetooth/esp_ble_mesh/remote_provisioning/rpr_client` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 336 | `bluetooth/esp_ble_mesh/remote_provisioning/rpr_server` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 337 | `bluetooth/esp_ble_mesh/remote_provisioning/unprov_dev` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 338 | `bluetooth/esp_ble_mesh/sensor_models/sensor_client` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 339 | `bluetooth/esp_ble_mesh/sensor_models/sensor_server` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 340 | `bluetooth/esp_ble_mesh/vendor_models/vendor_client` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 341 | `bluetooth/esp_ble_mesh/vendor_models/vendor_server` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 342 | `bluetooth/esp_ble_mesh/wifi_coexist` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [ ] | 343 | `bluetooth/esp_hid_device` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 344 | `bluetooth/esp_hid_host` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [-] | 345 | `bluetooth/hci/ble_adv_scan_combined` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 346 | `bluetooth/hci/controller_hci_uart_esp32` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 347 | `bluetooth/hci/controller_hci_uart_esp32c3_and_esp32s3` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 348 | `bluetooth/hci/controller_vhci_ble_adv` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [ ] | 349 | `bluetooth/nimble/ble_ancs` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 350 | `bluetooth/nimble/ble_chan_sound_initiator` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 351 | `bluetooth/nimble/ble_chan_sound_reflector` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 352 | `bluetooth/nimble/ble_cte/ble_periodic_adv_with_cte` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 353 | `bluetooth/nimble/ble_cte/ble_periodic_sync_with_cte` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 354 | `bluetooth/nimble/ble_cts/cts_cent` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 355 | `bluetooth/nimble/ble_cts/cts_prph` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 356 | `bluetooth/nimble/ble_dynamic_service` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 357 | `bluetooth/nimble/ble_enc_adv_data/enc_adv_data_cent` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 358 | `bluetooth/nimble/ble_enc_adv_data/enc_adv_data_prph` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 359 | `bluetooth/nimble/ble_gattc_gatts_coex` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 360 | `bluetooth/nimble/ble_htp/htp_cent` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 361 | `bluetooth/nimble/ble_htp/htp_prph` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 362 | `bluetooth/nimble/ble_l2cap_coc/coc_blecent` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 363 | `bluetooth/nimble/ble_l2cap_coc/coc_bleprph` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 364 | `bluetooth/nimble/ble_multi_adv` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 365 | `bluetooth/nimble/ble_multi_conn/ble_multi_conn_cent` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 366 | `bluetooth/nimble/ble_multi_conn/ble_multi_conn_prph` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 367 | `bluetooth/nimble/ble_pawr_adv/ble_pawr_adv` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 368 | `bluetooth/nimble/ble_pawr_adv/ble_pawr_sync` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 369 | `bluetooth/nimble/ble_pawr_adv_conn/ble_pawr_adv_conn` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 370 | `bluetooth/nimble/ble_pawr_adv_conn/ble_pawr_sync_conn` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 371 | `bluetooth/nimble/ble_periodic_adv` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 372 | `bluetooth/nimble/ble_periodic_sync` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 373 | `bluetooth/nimble/ble_phy/phy_cent` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 374 | `bluetooth/nimble/ble_phy/phy_prph` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 375 | `bluetooth/nimble/ble_proximity_sensor/proximity_sensor_cent` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 376 | `bluetooth/nimble/ble_proximity_sensor/proximity_sensor_prph` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 377 | `bluetooth/nimble/ble_spi_slave` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 378 | `bluetooth/nimble/ble_spp/spp_client` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 379 | `bluetooth/nimble/ble_spp/spp_server` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 380 | `bluetooth/nimble/blecent` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 381 | `bluetooth/nimble/blecsc` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 382 | `bluetooth/nimble/blehr` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 383 | `bluetooth/nimble/blemesh` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [?] | 384 | `bluetooth/nimble/bleprph` | 🎯 Level 1 | P1 | `esp_idfv61_bleprph` | 待补凭证 (存量隔离区债务，14天 TTL 至 2026-10-13) |
| [ ] | 385 | `bluetooth/nimble/bleprph_host_only` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 386 | `bluetooth/nimble/bleprph_wifi_coex` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [-] | 387 | `bluetooth/nimble/hci` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [ ] | 388 | `bluetooth/nimble/power_save` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 389 | `bluetooth/nimble/throughput_app/blecent_throughput` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 390 | `bluetooth/nimble/throughput_app/bleprph_throughput` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |

---

<a id="storage"></a>
### 片上存储与文件系统 (Storage)（共 27 项 | 编号 `#391 ~ #417` | 已实证: 0 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [ ] | 391 | `storage/custom_flash_driver` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 392 | `storage/emmc` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 393 | `storage/fatfs/bdl_wl` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 394 | `storage/fatfs/ext_flash` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 395 | `storage/fatfs/fatfsgen` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 396 | `storage/fatfs/fs_operations` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 397 | `storage/fatfs/getting_started` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 398 | `storage/littlefs` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 399 | `storage/nvs/nvs_bootloader` | ⚙️ Level 4 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 400 | `storage/nvs/nvs_console` | ⚙️ Level 4 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 401 | `storage/nvs/nvs_iteration` | ⚙️ Level 4 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 402 | `storage/nvs/nvs_rw_blob` | ⚙️ Level 4 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 403 | `storage/nvs/nvs_rw_value` | ⚙️ Level 4 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 404 | `storage/nvs/nvs_rw_value_cxx` | ⚙️ Level 4 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 405 | `storage/nvs/nvs_statistics` | ⚙️ Level 4 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 406 | `storage/nvs/nvsgen` | ⚙️ Level 4 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 407 | `storage/partition_api/partition_find` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 408 | `storage/partition_api/partition_mmap` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 409 | `storage/partition_api/partition_ops` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 410 | `storage/parttool` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 411 | `storage/perf_benchmark` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 412 | `storage/sd_card/sdmmc` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 413 | `storage/sd_card/sdspi` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 414 | `storage/semihost_vfs` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 415 | `storage/spiffs` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 416 | `storage/spiffsgen` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 417 | `storage/wear_levelling` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |

---

<a id="network"></a>
### 底层网络与接口 (Network)（共 5 项 | 编号 `#418 ~ #422` | 已实证: 0 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [ ] | 418 | `network/bridge` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 419 | `network/eth2ap` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 420 | `network/simple_sniffer` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 421 | `network/sta2eth` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 422 | `network/vlan_support` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |

---

<a id="cxx"></a>
### C++ 运行时与语言特性 (C++)（共 3 项 | 编号 `#423 ~ #425` | 已实证: 0 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [ ] | 423 | `cxx/exceptions` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 424 | `cxx/pthread` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |
| [ ] | 425 | `cxx/rtti` | 📜 Level 2 | P2 | `待适配` | 待深度审定。尚未完成源码调用链与硬件特性审定。 |

---

<a id="build_system"></a>
### 构建系统与组件组织 (Build System)（共 19 项 | 编号 `#426 ~ #444` | 已实证: 0 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 426 | `build_system/cmake/component_manager` | ⚙️ Level 4 | P3 | `待适配` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 427 | `build_system/cmake/idf_as_lib` | ⚙️ Level 4 | P3 | `待适配` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 428 | `build_system/cmake/import_lib` | ⚙️ Level 4 | P3 | `待适配` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 429 | `build_system/cmake/import_prebuilt` | ⚙️ Level 4 | P3 | `待适配` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 430 | `build_system/cmake/import_prebuilt/prebuilt` | ⚙️ Level 4 | P3 | `待适配` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 431 | `build_system/cmake/multi_config` | ⚙️ Level 4 | P3 | `待适配` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 432 | `build_system/cmake/plugins` | ⚙️ Level 4 | P3 | `待适配` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 433 | `build_system/cmakev2/features/component_manager` | ⚙️ Level 4 | P3 | `待适配` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 434 | `build_system/cmakev2/features/conditional_component` | ⚙️ Level 4 | P3 | `待适配` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 435 | `build_system/cmakev2/features/idf_as_lib` | ⚙️ Level 4 | P3 | `待适配` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 436 | `build_system/cmakev2/features/import_lib` | ⚙️ Level 4 | P3 | `待适配` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 437 | `build_system/cmakev2/features/import_lib_direct` | ⚙️ Level 4 | P3 | `待适配` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 438 | `build_system/cmakev2/features/import_prebuilt` | ⚙️ Level 4 | P3 | `待适配` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 439 | `build_system/cmakev2/features/import_prebuilt/prebuilt` | ⚙️ Level 4 | P3 | `待适配` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 440 | `build_system/cmakev2/features/multi_binary` | ⚙️ Level 4 | P3 | `待适配` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 441 | `build_system/cmakev2/features/multi_config` | ⚙️ Level 4 | P3 | `待适配` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 442 | `build_system/cmakev2/features/plugins` | ⚙️ Level 4 | P3 | `待适配` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 443 | `build_system/cmakev2/get-started/hello_world` | ⚙️ Level 4 | P3 | `待适配` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 444 | `build_system/wrappers` | ⚙️ Level 4 | P3 | `待适配` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |

---

<a id="security"></a>
### 硬件加密与芯片安全特性 (Security)（共 10 项 | 编号 `#445 ~ #454` | 已实证: 0 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 445 | `security/flash_encryption` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。硬件 eFuse 不可逆熔丝烧写与加密引擎。 |
| [-] | 446 | `security/hmac_soft_jtag` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。硬件 eFuse 不可逆熔丝烧写与加密引擎。 |
| [-] | 447 | `security/key_manager` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。硬件 eFuse 不可逆熔丝烧写与加密引擎。 |
| [-] | 448 | `security/nvs_encryption_hmac` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。硬件 eFuse 不可逆熔丝烧写与加密引擎。 |
| [-] | 449 | `security/psa_its_custom_backend` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。硬件 eFuse 不可逆熔丝烧写与加密引擎。 |
| [-] | 450 | `security/security_features_app` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。硬件 eFuse 不可逆熔丝烧写与加密引擎。 |
| [-] | 451 | `security/tee/tee_attestation` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。硬件 eFuse 不可逆熔丝烧写与加密引擎。 |
| [-] | 452 | `security/tee/tee_basic` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。硬件 eFuse 不可逆熔丝烧写与加密引擎。 |
| [-] | 453 | `security/tee/tee_secure_ota` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。硬件 eFuse 不可逆熔丝烧写与加密引擎。 |
| [-] | 454 | `security/tee/tee_secure_storage` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。硬件 eFuse 不可逆熔丝烧写与加密引擎。 |

---

<a id="custom_bootloader"></a>
### 定制引导加载程序 (Custom Bootloader)（共 4 项 | 编号 `#455 ~ #458` | 已实证: 0 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 455 | `custom_bootloader/bootloader_extra_dir` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。芯片二级引导程序 ROM 硬件引导链。 |
| [-] | 456 | `custom_bootloader/bootloader_hooks` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。芯片二级引导程序 ROM 硬件引导链。 |
| [-] | 457 | `custom_bootloader/bootloader_multiboot` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。芯片二级引导程序 ROM 硬件引导链。 |
| [-] | 458 | `custom_bootloader/bootloader_override` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。芯片二级引导程序 ROM 硬件引导链。 |

---

<a id="ethernet"></a>
### 有线以太网 (Ethernet - 外部 PHY 硬件)（共 3 项 | 编号 `#459 ~ #461` | 已实证: 0 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 459 | `ethernet/basic` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。外部 PHY 变压器与 RMII/SMI 物理总线。 |
| [-] | 460 | `ethernet/iperf` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。外部 PHY 变压器与 RMII/SMI 物理总线。 |
| [-] | 461 | `ethernet/ptp` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。外部 PHY 变压器与 RMII/SMI 物理总线。 |

---

<a id="lowpower"></a>
### 超低功耗与 ULP 协处理器 (Low Power & ULP)（共 2 项 | 编号 `#462 ~ #463` | 已实证: 0 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 462 | `lowpower/power_management` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。ULP 独立硬件微功耗协处理器环境。 |
| [-] | 463 | `lowpower/vbat` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。ULP 独立硬件微功耗协处理器环境。 |

---

<a id="mesh"></a>
### Wi-Fi 空间自组网 (Mesh)（共 3 项 | 编号 `#464 ~ #466` | 已实证: 0 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 464 | `mesh/internal_communication` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。Wi-Fi Mesh 多机空间电磁拓扑。 |
| [-] | 465 | `mesh/ip_internal_network` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。Wi-Fi Mesh 多机空间电磁拓扑。 |
| [-] | 466 | `mesh/manual_networking` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。Wi-Fi Mesh 多机空间电磁拓扑。 |

---

<a id="openthread"></a>
### OpenThread 802.15.4 线程网络 (OpenThread)（共 6 项 | 编号 `#467 ~ #472` | 已实证: 0 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 467 | `openthread/ot_br` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。802.15.4 2.4GHz 射频物理层。 |
| [-] | 468 | `openthread/ot_cli` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。802.15.4 2.4GHz 射频物理层。 |
| [-] | 469 | `openthread/ot_rcp` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。802.15.4 2.4GHz 射频物理层。 |
| [-] | 470 | `openthread/ot_sleepy_device/deep_sleep` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。802.15.4 2.4GHz 射频物理层。 |
| [-] | 471 | `openthread/ot_sleepy_device/light_sleep` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。802.15.4 2.4GHz 射频物理层。 |
| [-] | 472 | `openthread/ot_trel` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。802.15.4 2.4GHz 射频物理层。 |

---

<a id="zigbee"></a>
### Zigbee 2.4G 射频网络 (Zigbee)（共 3 项 | 编号 `#473 ~ #475` | 已实证: 0 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 473 | `zigbee/esp_zigbee_gateway` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。Zigbee 2.4GHz 射频物理层。 |
| [-] | 474 | `zigbee/light_sample/HA_on_off_light` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。Zigbee 2.4GHz 射频物理层。 |
| [-] | 475 | `zigbee/light_sample/HA_on_off_switch` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。Zigbee 2.4GHz 射频物理层。 |

---

<a id="ieee802154"></a>
### IEEE 802.15.4 原始射频 (IEEE 802.15.4)（共 1 项 | 编号 `#476 ~ #476` | 已实证: 0 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 476 | `ieee802154/ieee802154_cli` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。IEEE 802.15.4 原始射频物理层。 |

---

<a id="phy"></a>
### 射频物理层与工厂校准 (PHY & Calibration)（共 2 项 | 编号 `#477 ~ #478` | 已实证: 0 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 477 | `phy/antenna` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。芯片工厂射频电气校准与功率表。 |
| [-] | 478 | `phy/cert_test` | ⚙️ Level 4 | P4 | `待适配` | 声明 Out-of-Scope。芯片工厂射频电气校准与功率表。 |

---
