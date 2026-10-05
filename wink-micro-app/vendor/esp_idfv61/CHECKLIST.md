<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- ⚠️  此文件由 generate_checklist_v1_1.py 自动生成，严禁人工直接编辑！修改请编辑 checklist.data.json -->
# ESP-IDF v6.1 官方示例全量仿真适配核对清单 (Checklist)

> **数据单一事实源（SSOT）**：[`checklist.data.json`](.governance/data/checklist.data.json)（Spec v2.0.0，多配置实例与五维正交模型）  
> **生成时间**：2026-10-05  
> **分类规范**：[`CLASSIFICATION-SPEC.md`](.governance/specs/CLASSIFICATION-SPEC.md) (v2.0)  
> **能力字典**：[`capability-catalog.yaml`](.governance/catalog/capability-catalog.yaml)  
> **隔离区白名单**：[`gates/quarantine.yaml`](.governance/gates/quarantine.yaml)（10 项存量债务，14 天 TTL 生效中）  
> **执行手册**：[`PLAYBOOK.md`](.governance/specs/PLAYBOOK.md) (v2.0)  

---

## 一、 总体适配进度统计

- **官方独立示例总数**：**478 个**
  - `[x]` **已完成六要素实证 (Verified)**：**34 项**
  - `[?]` **存量隔离待补凭证 (Quarantined Debt)**：**0 项**（14 天 TTL 过期硬阻断，至 `2026-10-13`）
  - `[ ]` **规划中正常排期 (In-Scope Planned)**：**257 项**
  - `[-]` **明确产品排除 / 暂缓投入 (Out-of-Scope / Deferred)**：**187 项**（编译期 `WINK_SLA_ERROR` Fail-Loud 阻断）
  - `?` **待深度审定 (Pending Audit / Unknown Scope)**：**0 项**

### 大类索引导航

| 序号 | 功能大类 | 包含示例数 | 编号跨度 | 已实证 | 隔离待补 |
| :---: | :--- | :---: | :---: | :---: | :---: |
| 01 | [基础快速起步 (Get-Started)](#get-started) | 2 项 | `#001 ~ #002` | 2 项 | 0 项 |
| 02 | [片上与总线外设 (Peripherals)](#peripherals) | 114 项 | `#003 ~ #116` | 13 项 | 0 项 |
| 03 | [操作系统与核心系统调用 (System & OS)](#system) | 68 项 | `#117 ~ #184` | 7 项 | 0 项 |
| 04 | [网络与应用层通信协议 (Protocols)](#protocols) | 35 项 | `#185 ~ #219` | 2 项 | 0 项 |
| 05 | [Wi-Fi 无线局域网 (Wi-Fi)](#wifi) | 24 项 | `#220 ~ #243` | 4 项 | 0 项 |
| 06 | [蓝牙协议栈 (Bluetooth)](#bluetooth) | 147 项 | `#244 ~ #390` | 2 项 | 0 项 |
| 07 | [片上存储与文件系统 (Storage)](#storage) | 27 项 | `#391 ~ #417` | 4 项 | 0 项 |
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

### 并发研发泳道与优先级调度矩阵 (Concurrency Lanes & Execution Matrix)

> **并发编排说明**：为支持后续多个实施计划与 AI Coding Agents **安全并发推进**，478 个官方示例划分为 **6 条正交并发研发泳道**。各泳道在运行时门面（`wink-micro-os/frameworks/esp_idf/`）与外设驱动上物理隔离，支持并行认领开发，杜绝底座代码冲突。

| 泳道代号与技术领域 | 覆盖示例数 (Active) | 核心标杆 (P0) | 通用积木 (P1) | 进阶模式 (P2) | 复杂生态 (P3) | 关键底座依赖 | 并发隔离与协作建议 |
| :--- | :---: | :---: | :---: | :---: | :---: | :--- | :--- |
| **Lane 1: 核心系统与内核调度** | 70 项 | 2 项 | 11 项 | 54 项 | 3 项 | FreeRTOS 纤程调度器、代际令牌 | 底座筑基泳道，优先收敛核心语义；其他泳道的前提 |
| **Lane 2: 通用数字总线与通信** | 46 项 | 2 项 | 12 项 | 24 项 | 8 项 | `pal_i2c`, `pal_spi`, `pal_uart` | 纯外设模型，与 Lane 3~6 物理正交，可独立并行推进 |
| **Lane 3: 脉冲发生与硬件定时器** | 23 项 | 2 项 | 3 项 | 15 项 | 3 项 | 定点 PWM、虚拟微秒因果推进 | 纯波形与硬件定时，与 Lane 2, 4~6 独立并行推进 |
| **Lane 4: 模拟量转换与电学传感** | 12 项 | 1 项 | 4 项 | 7 项 | 0 项 | `pal_adc`, `pal_dac` | 模拟电学采样，与通信总线及网络完全正交，可独立推进 |
| **Lane 5: 本地存储与虚拟文件系统**| 27 项 | 1 项 | 4 项 | 20 项 | 2 项 | 内存虚拟块设备、VFS 句柄抽象 | 纯内存/虚拟块，与外设及网络零耦合，可完全独立推进 |
| **Lane 6: 无线网络与通信协议栈**| 113 项 | 1 项 | 3 项 | 95 项 | 14 项 | 确定性虚拟空口、Netif、NimBLE | 虚拟网络 Broker 闭环，与 Lane 2~5 独立并行推进 |

#### 🎯 各泳道推荐优先并发认领就绪清单 (Ready-to-Claim P0/P1 Backlog)

| 泳道 | 编号 | 官方子示例相对路径 | 优先级 | 对应 App 目录 | 推荐理由与解锁价值 |
| :--- | :---: | :--- | :---: | :--- | :--- |
| **Lane 1: 系统** | `#001` | `get-started/blink` | P0 | [`get-started/blink_gpio`](get-started/blink_gpio) | ✅ 已完成实证。建立最小 FreeRTOS 任务与 GPIO 输出范式 |
| **Lane 1: 系统** | `#002` | `get-started/hello_world` | P1 | `get-started/hello_world` | 解锁系统控制台输出与基础芯片信息获取 |
| **Lane 2: 总线** | `#019` | `peripherals/i2c/i2c_basic` | P0 | `peripherals/i2c_basic` | 解锁对象式 I2C Master 总线与传感器寄存器通信 |
| **Lane 2: 总线** | `#108` | `peripherals/uart/uart_echo` | P0 | `peripherals/uart_echo` | 解锁双任务环形缓冲与串口交互终端 |
| **Lane 2: 总线** | `#088` | `peripherals/spi_master/hd_eeprom` | P1 | `peripherals/spi_eeprom` | 解锁高速 SPI 总线全双工读写支持 |
| **Lane 3: 脉冲** | `#047` | `peripherals/ledc/ledc_basic` | P0 | `peripherals/ledc_basic` | 解锁定点 PWM 占空比无浮点呼吸调光 |
| **Lane 3: 脉冲** | `#096` | `peripherals/timer_group/gptimer` | P0 | `peripherals/gptimer_alarm` | 解锁高精度硬件定时器 Alarm 与中断回调 |
| **Lane 4: 模拟** | `#004` | `peripherals/adc/oneshot_read` | P0 | `peripherals/adc_oneshot` | 解锁电压校准与多通道电位器模拟采样 |
| **Lane 4: 模拟** | `#015` | `peripherals/dac/dac_cosine` | P1 | `peripherals/dac_cosine` | 解锁 DAC 连续余弦波音频发生 |
| **Lane 5: 存储** | `#395` | `storage/nvs/nvs_rw_value` | P0 | `storage/nvs_rw_value` | 解锁键值对非易失性持久化，为 Wi-Fi 凭证打底 |
| **Lane 5: 存储** | `#403` | `storage/spiffs/spiffs` | P1 | `storage/spiffs` | 解锁片上文件系统与文件读写接口 |
| **Lane 6: 网络** | `#220` | `wifi/getting_started/station` | P0 | `wifi/wifi_sta` | 解锁虚拟 AP 状态机与 DHCP 虚拟 IP 分配 |
| **Lane 6: 网络** | `#185` | `protocols/esp_http_client` | P1 | `protocols/http_client` | 解锁 REST GET/POST 网络通信客户端 |
| **Lane 6: 网络** | `#186` | `protocols/mqtt/tcp` | P1 | `protocols/mqtt_tcp` | 解锁轻量 Broker Pub/Sub 实时闭环 |
| **Lane 6: 蓝牙** | `#245` | `bluetooth/nimble/bleprph` | P1 | `bluetooth/bleprph` | 解锁 NimBLE GATT 特征值读写与 Virtual Inspector |

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

### 优先级与泳道编排说明

- **P0（核心筑基标杆）**：已完成验证（`[x]`）或该泳道最核心的基础设施标杆，解锁该泳道后续一切前置依赖。
- **P1（高频通用积木）**：覆盖通用业务场景中约 70% 的核心功能（如 SPI 全双工、GPTimer 报警、NVS 键值存储、HTTP GET/POST）。
- **P2（进阶模式与多通道）**：ADC 连续采样 DMA、LEDC 多通道平滑呼吸、SoftAP + STA 级联、BLE Central 多连接。
- **P3（复杂组合与长尾）**：WebSocket 服务端长连接、mDNS 组播、复杂总线级联传感器、暂缓投入项。
- **P4（硬件约束排除）**：不可逆物理介质（eFuse、外部 PHY 以太网、空间 RF 测试等），编译期显式阻断。

---

## 三、 478 个官方示例逐项核对总账

<a id="get-started"></a>
### 基础快速起步 (Get-Started)（共 2 项 | 编号 `#001 ~ #002` | 已实证: 2 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [x] | 001 | `get-started/blink` | 🎯 Level 1 | P0 | [`get-started/blink_gpio`](get-started/blink_gpio) | [Lane 1: 内核调度] 已完成实证。 |
| [x] | 002 | `get-started/hello_world` | 📜 Level 2 | P0 | [`get-started/hello_world`](get-started/hello_world) | [Lane 1: 内核调度] 已完成实证。 |

---

<a id="peripherals"></a>
### 片上与总线外设 (Peripherals)（共 114 项 | 编号 `#003 ~ #116` | 已实证: 13 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [x] | 003 | `peripherals/adc/continuous_read` | 🎯 Level 1 | P0 | [`peripherals/adc_continuous_read`](peripherals/adc_continuous_read) | [Lane 4: 模拟电学] 已完成实证。 |
| [x] | 004 | `peripherals/adc/oneshot_read` | 🎯 Level 1 | P0 | [`peripherals/adc_oneshot_read`](peripherals/adc_oneshot_read) | [Lane 4: 模拟电学] 已完成实证。 |
| [ ] | 005 | `peripherals/analog_comparator/auto_scan` | 📜 Level 2 | P2 | `peripherals/analog_comparator_auto_scan` | [Lane 4: 模拟电学] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 006 | `peripherals/analog_comparator/etm_periodic_scan` | 📜 Level 2 | P2 | `peripherals/analog_comparator_etm_periodic_scan` | [Lane 4: 模拟电学] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 007 | `peripherals/bitscrambler` | 📜 Level 2 | P3 | `peripherals/bitscrambler` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [-] | 008 | `peripherals/camera/dvp_dsi` | 🎯 Level 1 | P4 | — | 声明 Out-of-Scope。MIPI-CSI/DVP 物理差分摄像头传感器。 |
| [-] | 009 | `peripherals/camera/dvp_isp_dsi` | 🎯 Level 1 | P4 | — | 声明 Out-of-Scope。MIPI-CSI/DVP 物理差分摄像头传感器。 |
| [-] | 010 | `peripherals/camera/mipi_isp_dsi` | 🎯 Level 1 | P4 | — | 声明 Out-of-Scope。MIPI-CSI/DVP 物理差分摄像头传感器。 |
| [ ] | 011 | `peripherals/dac/dac_continuous/dac_audio` | 📜 Level 2 | P2 | `peripherals/dac_dac_continuous_dac_audio` | [Lane 4: 模拟电学] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 012 | `peripherals/dac/dac_continuous/signal_generator` | 📜 Level 2 | P2 | `peripherals/dac_dac_continuous_signal_generator` | [Lane 4: 模拟电学] 待排期。依赖进一步框架门面扩展。 |
| [x] | 013 | `peripherals/dac/dac_cosine_wave` | 📜 Level 2 | P0 | [`peripherals/dac_dac_cosine_wave`](peripherals/dac_dac_cosine_wave) | [Lane 4: 模拟电学] 已完成实证。 |
| [x] | 014 | `peripherals/dac/dac_oneshot` | 📜 Level 2 | P0 | [`peripherals/dac_dac_oneshot`](peripherals/dac_dac_oneshot) | [Lane 4: 模拟电学] 已完成实证。 |
| [ ] | 015 | `peripherals/dedicated_gpio/soft_i2c` | 📜 Level 2 | P1 | `peripherals/dedicated_gpio_soft_i2c` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 016 | `peripherals/dedicated_gpio/soft_spi` | 📜 Level 2 | P1 | `peripherals/dedicated_gpio_soft_spi` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 017 | `peripherals/dedicated_gpio/soft_uart` | 📜 Level 2 | P1 | `peripherals/dedicated_gpio_soft_uart` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 018 | `peripherals/dma/async_color_convert` | 📜 Level 2 | P2 | `peripherals/dma_async_color_convert` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 019 | `peripherals/dma/async_crc` | 📜 Level 2 | P2 | `peripherals/dma_async_crc` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [x] | 020 | `peripherals/gpio/generic_gpio` | 📜 Level 2 | P0 | [`peripherals/gpio_generic_gpio`](peripherals/gpio_generic_gpio) | [Lane 2: 数字总线] 已完成实证。 |
| [ ] | 021 | `peripherals/gpio/matrix_keyboard` | 📜 Level 2 | P1 | `peripherals/gpio_matrix_keyboard` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [-] | 022 | `peripherals/h264` | 🎯 Level 1 | P4 | — | 声明 Out-of-Scope。硬件 H.264 编解码加速器。 |
| [x] | 023 | `peripherals/i2c/i2c_basic` | 📜 Level 2 | P0 | [`peripherals/i2c_basic`](peripherals/i2c_basic) | [Lane 2: 数字总线] 已完成实证。 |
| [x] | 024 | `peripherals/i2c/i2c_eeprom` | 📜 Level 2 | P0 | [`peripherals/i2c_i2c_eeprom`](peripherals/i2c_i2c_eeprom) | [Lane 2: 数字总线] 已完成实证。 |
| [ ] | 025 | `peripherals/i2c/i2c_slave_network_sensor` | 📜 Level 2 | P2 | `peripherals/i2c_i2c_slave_network_sensor` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 026 | `peripherals/i2c/i2c_tools` | 📜 Level 2 | P2 | `peripherals/i2c_i2c_tools` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 027 | `peripherals/i2c/i2c_u8g2` | 📜 Level 2 | P2 | `peripherals/i2c_i2c_u8g2` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [-] | 028 | `peripherals/i2s/i2s_advance/i2s_usb` | 📜 Level 2 | P4 | — | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [ ] | 029 | `peripherals/i2s/i2s_basic/i2s_pdm` | 📜 Level 2 | P1 | `peripherals/i2s_i2s_basic_i2s_pdm` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 030 | `peripherals/i2s/i2s_basic/i2s_std` | 📜 Level 2 | P1 | `peripherals/i2s_i2s_basic_i2s_std` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 031 | `peripherals/i2s/i2s_basic/i2s_tdm` | 📜 Level 2 | P1 | `peripherals/i2s_i2s_basic_i2s_tdm` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 032 | `peripherals/i2s/i2s_codec/i2s_es7210_tdm` | 📜 Level 2 | P2 | `peripherals/i2s_i2s_codec_i2s_es7210_tdm` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 033 | `peripherals/i2s/i2s_codec/i2s_es8311` | 📜 Level 2 | P2 | `peripherals/i2s_i2s_codec_i2s_es8311` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 034 | `peripherals/i2s/i2s_recorder` | 📜 Level 2 | P2 | `peripherals/i2s_i2s_recorder` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 035 | `peripherals/i3c/i3c_i2c_basic` | 📜 Level 2 | P1 | `peripherals/i3c_i3c_i2c_basic` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 036 | `peripherals/i3c/i3c_lsm6dscx` | 📜 Level 2 | P2 | `peripherals/i3c_i3c_lsm6dscx` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [-] | 037 | `peripherals/isp/multi_pipelines` | 🎯 Level 1 | P4 | — | 声明 Out-of-Scope。图像信号处理器 (ISP) 硬件管线。 |
| [-] | 038 | `peripherals/jpeg/jpeg_decode` | 🎯 Level 1 | P4 | — | 声明 Out-of-Scope。硬件 JPEG 编解码加速器。 |
| [-] | 039 | `peripherals/jpeg/jpeg_encode` | 🎯 Level 1 | P4 | — | 声明 Out-of-Scope。硬件 JPEG 编解码加速器。 |
| [ ] | 040 | `peripherals/lcd/i2c_oled` | 📜 Level 2 | P3 | `peripherals/lcd_i2c_oled` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 041 | `peripherals/lcd/i80_controller` | 📜 Level 2 | P3 | `peripherals/lcd_i80_controller` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [-] | 042 | `peripherals/lcd/mipi_dsi` | 📜 Level 2 | P4 | — | 声明 Out-of-Scope。MIPI-DSI 高速差分显示物理接口。 |
| [ ] | 043 | `peripherals/lcd/parlio_simulate` | 📜 Level 2 | P3 | `peripherals/lcd_parlio_simulate` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 044 | `peripherals/lcd/rgb_panel` | 📜 Level 2 | P3 | `peripherals/lcd_rgb_panel` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 045 | `peripherals/lcd/spi_lcd_touch` | 📜 Level 2 | P3 | `peripherals/lcd_spi_lcd_touch` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 046 | `peripherals/lcd/tjpgd` | 📜 Level 2 | P3 | `peripherals/lcd_tjpgd` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [x] | 047 | `peripherals/ledc/ledc_basic` | 🎯 Level 1 | P0 | [`peripherals/ledc_basic`](peripherals/ledc_basic) | [Lane 3: 脉冲定时] 已完成实证。 |
| [ ] | 048 | `peripherals/ledc/ledc_dimmer` | 📜 Level 2 | P2 | `peripherals/ledc_ledc_dimmer` | [Lane 3: 脉冲定时] 待排期。依赖进一步框架门面扩展。 |
| [x] | 049 | `peripherals/ledc/ledc_fade` | 📜 Level 2 | P0 | [`peripherals/ledc_ledc_fade`](peripherals/ledc_ledc_fade) | [Lane 3: 脉冲定时] 已完成实证。 |
| [ ] | 050 | `peripherals/ledc/ledc_gamma_curve_fade` | 📜 Level 2 | P2 | `peripherals/ledc_ledc_gamma_curve_fade` | [Lane 3: 脉冲定时] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 051 | `peripherals/mcpwm/mcpwm_bdc_speed_control` | 📜 Level 2 | P2 | `peripherals/mcpwm_mcpwm_bdc_speed_control` | [Lane 3: 脉冲定时] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 052 | `peripherals/mcpwm/mcpwm_bldc_hall_control` | 📜 Level 2 | P2 | `peripherals/mcpwm_mcpwm_bldc_hall_control` | [Lane 3: 脉冲定时] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 053 | `peripherals/mcpwm/mcpwm_capture_hc_sr04` | 📜 Level 2 | P2 | `peripherals/mcpwm_mcpwm_capture_hc_sr04` | [Lane 3: 脉冲定时] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 054 | `peripherals/mcpwm/mcpwm_foc_svpwm_open_loop` | 📜 Level 2 | P2 | `peripherals/mcpwm_mcpwm_foc_svpwm_open_loop` | [Lane 3: 脉冲定时] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 055 | `peripherals/mcpwm/mcpwm_servo_control` | 📜 Level 2 | P2 | `peripherals/mcpwm_mcpwm_servo_control` | [Lane 3: 脉冲定时] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 056 | `peripherals/mcpwm/mcpwm_sync` | 📜 Level 2 | P2 | `peripherals/mcpwm_mcpwm_sync` | [Lane 3: 脉冲定时] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 057 | `peripherals/parlio/parlio_rx/logic_analyzer` | 📜 Level 2 | P2 | `peripherals/parlio_parlio_rx_logic_analyzer` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 058 | `peripherals/parlio/parlio_tx/advanced_rgb_led_matrix` | 📜 Level 2 | P2 | `peripherals/parlio_parlio_tx_advanced_rgb_led_matrix` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 059 | `peripherals/parlio/parlio_tx/simple_rgb_led_matrix` | 📜 Level 2 | P2 | `peripherals/parlio_parlio_tx_simple_rgb_led_matrix` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 060 | `peripherals/pcnt/rotary_encoder` | 📜 Level 2 | P2 | `peripherals/pcnt_rotary_encoder` | [Lane 3: 脉冲定时] 待排期。依赖进一步框架门面扩展。 |
| [-] | 061 | `peripherals/ppa/ppa_rgb_lcd` | 🎯 Level 1 | P4 | — | 声明 Out-of-Scope。像素处理加速器 (PPA) 硬件管线。 |
| [ ] | 062 | `peripherals/rmt/dshot_esc` | 🎯 Level 1 | P2 | `peripherals/rmt_dshot_esc` | [Lane 3: 脉冲定时] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 063 | `peripherals/rmt/ir_nec_transceiver` | 🎯 Level 1 | P2 | `peripherals/rmt_ir_nec_transceiver` | [Lane 3: 脉冲定时] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 064 | `peripherals/rmt/led_strip` | 🎯 Level 1 | P2 | `peripherals/rmt_led_strip` | [Lane 3: 脉冲定时] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 065 | `peripherals/rmt/led_strip_simple_encoder` | 🎯 Level 1 | P2 | `peripherals/rmt_led_strip_simple_encoder` | [Lane 3: 脉冲定时] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 066 | `peripherals/rmt/musical_buzzer` | 🎯 Level 1 | P2 | `peripherals/rmt_musical_buzzer` | [Lane 3: 脉冲定时] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 067 | `peripherals/rmt/onewire` | 🎯 Level 1 | P2 | `peripherals/rmt_onewire` | [Lane 3: 脉冲定时] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 068 | `peripherals/rmt/stepper_motor` | 🎯 Level 1 | P2 | `peripherals/rmt_stepper_motor` | [Lane 3: 脉冲定时] 待排期。依赖进一步框架门面扩展。 |
| [-] | 069 | `peripherals/sdio/host` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。SDIO 从机高速差分总线。 |
| [-] | 070 | `peripherals/sdio/slave` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。SDIO 从机高速差分总线。 |
| [ ] | 071 | `peripherals/sigma_delta/sdm_dac` | 📜 Level 2 | P2 | `peripherals/sigma_delta_sdm_dac` | [Lane 3: 脉冲定时] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 072 | `peripherals/sigma_delta/sdm_led` | 📜 Level 2 | P2 | `peripherals/sigma_delta_sdm_led` | [Lane 3: 脉冲定时] 待排期。依赖进一步框架门面扩展。 |
| [x] | 073 | `peripherals/spi_master/hd_eeprom` | 📜 Level 2 | P0 | [`peripherals/spi_master_hd_eeprom`](peripherals/spi_master_hd_eeprom) | [Lane 2: 数字总线] 已完成实证。 |
| [ ] | 074 | `peripherals/spi_master/lcd` | 📜 Level 2 | P1 | `peripherals/spi_master_lcd` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 075 | `peripherals/spi_slave/receiver` | 📜 Level 2 | P2 | `peripherals/spi_slave_receiver` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 076 | `peripherals/spi_slave/sender` | 📜 Level 2 | P2 | `peripherals/spi_slave_sender` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 077 | `peripherals/spi_slave_hd/append_mode/master` | 📜 Level 2 | P2 | `peripherals/spi_slave_hd_append_mode_master` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 078 | `peripherals/spi_slave_hd/append_mode/slave` | 📜 Level 2 | P2 | `peripherals/spi_slave_hd_append_mode_slave` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 079 | `peripherals/spi_slave_hd/segment_mode/seg_master` | 📜 Level 2 | P2 | `peripherals/spi_slave_hd_segment_mode_seg_master` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 080 | `peripherals/spi_slave_hd/segment_mode/seg_slave` | 📜 Level 2 | P2 | `peripherals/spi_slave_hd_segment_mode_seg_slave` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 081 | `peripherals/temperature_sensor/temp_sensor` | 📜 Level 2 | P2 | `peripherals/temperature_sensor_temp_sensor` | [Lane 4: 模拟电学] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 082 | `peripherals/temperature_sensor/temp_sensor_monitor` | 📜 Level 2 | P2 | `peripherals/temperature_sensor_temp_sensor_monitor` | [Lane 4: 模拟电学] 待排期。依赖进一步框架门面扩展。 |
| [x] | 083 | `peripherals/timer_group/gptimer` | ⚡ Level 3 | P0 | [`peripherals/gptimer_alarm`](peripherals/gptimer_alarm) | [Lane 3: 脉冲定时] 已完成实证。 |
| [ ] | 084 | `peripherals/timer_group/gptimer_capture_hc_sr04` | 📜 Level 2 | P2 | `peripherals/timer_group_gptimer_capture_hc_sr04` | [Lane 3: 脉冲定时] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 085 | `peripherals/timer_group/wiegand_interface` | 📜 Level 2 | P2 | `peripherals/timer_group_wiegand_interface` | [Lane 3: 脉冲定时] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 086 | `peripherals/touch_sensor/touch_sens_basic` | 📜 Level 2 | P1 | `peripherals/touch_sensor_touch_sens_basic` | [Lane 4: 模拟电学] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 087 | `peripherals/touch_sensor/touch_sens_sleep` | 📜 Level 2 | P2 | `peripherals/touch_sensor_touch_sens_sleep` | [Lane 4: 模拟电学] 待排期。依赖进一步框架门面扩展。 |
| [-] | 088 | `peripherals/twai/cybergear` | 📜 Level 2 | P4 | — | 声明 Out-of-Scope。TWAI/CAN 差分总线物理收发器。 |
| [-] | 089 | `peripherals/twai/twai_error_recovery` | 📜 Level 2 | P4 | — | 声明 Out-of-Scope。TWAI/CAN 差分总线物理收发器。 |
| [-] | 090 | `peripherals/twai/twai_network/twai_listen_only` | 📜 Level 2 | P4 | — | 声明 Out-of-Scope。TWAI/CAN 差分总线物理收发器。 |
| [-] | 091 | `peripherals/twai/twai_network/twai_sender` | 📜 Level 2 | P4 | — | 声明 Out-of-Scope。TWAI/CAN 差分总线物理收发器。 |
| [-] | 092 | `peripherals/twai/twai_utils` | 📜 Level 2 | P4 | — | 声明 Out-of-Scope。TWAI/CAN 差分总线物理收发器。 |
| [ ] | 093 | `peripherals/uart/nmea0183_parser` | 📜 Level 2 | P2 | `peripherals/uart_nmea0183_parser` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 094 | `peripherals/uart/uart_async_rxtxtasks` | 📜 Level 2 | P2 | `peripherals/uart_uart_async_rxtxtasks` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 095 | `peripherals/uart/uart_dma_ota` | 📜 Level 2 | P2 | `peripherals/uart_uart_dma_ota` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [x] | 096 | `peripherals/uart/uart_echo` | 📜 Level 2 | P0 | [`peripherals/uart_echo`](peripherals/uart_echo) | [Lane 2: 数字总线] 已完成实证。 |
| [ ] | 097 | `peripherals/uart/uart_echo_rs485` | 📜 Level 2 | P1 | `peripherals/uart_uart_echo_rs485` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [x] | 098 | `peripherals/uart/uart_events` | 📜 Level 2 | P0 | [`peripherals/uart_uart_events`](peripherals/uart_uart_events) | [Lane 2: 数字总线] 已完成实证。 |
| [ ] | 099 | `peripherals/uart/uart_repl` | 📜 Level 2 | P2 | `peripherals/uart_uart_repl` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 100 | `peripherals/uart/uart_select` | 📜 Level 2 | P2 | `peripherals/uart_uart_select` | [Lane 2: 数字总线] 待排期。依赖进一步框架门面扩展。 |
| [-] | 101 | `peripherals/usb/device/cherryusb_serial_device` | 📜 Level 2 | P4 | — | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [-] | 102 | `peripherals/usb/device/tusb_cdc_acm_wakeup` | 📜 Level 2 | P4 | — | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [-] | 103 | `peripherals/usb/device/tusb_composite_msc_serialdevice` | 📜 Level 2 | P4 | — | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [-] | 104 | `peripherals/usb/device/tusb_console` | 📜 Level 2 | P4 | — | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [-] | 105 | `peripherals/usb/device/tusb_hid` | 📜 Level 2 | P4 | — | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [-] | 106 | `peripherals/usb/device/tusb_midi` | 📜 Level 2 | P4 | — | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [-] | 107 | `peripherals/usb/device/tusb_msc` | 📜 Level 2 | P4 | — | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [-] | 108 | `peripherals/usb/device/tusb_ncm` | 📜 Level 2 | P4 | — | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [-] | 109 | `peripherals/usb/device/tusb_serial_device` | 📜 Level 2 | P4 | — | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [-] | 110 | `peripherals/usb/host/cdc` | 📜 Level 2 | P4 | — | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [-] | 111 | `peripherals/usb/host/cherryusb_host` | 📜 Level 2 | P4 | — | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [-] | 112 | `peripherals/usb/host/hid` | 📜 Level 2 | P4 | — | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [-] | 113 | `peripherals/usb/host/msc` | 📜 Level 2 | P4 | — | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [-] | 114 | `peripherals/usb/host/usb_host_lib` | 📜 Level 2 | P4 | — | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [-] | 115 | `peripherals/usb/host/uvc` | 📜 Level 2 | P4 | — | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |
| [-] | 116 | `peripherals/usb_serial_jtag/usb_serial_jtag_echo` | 📜 Level 2 | P4 | — | 声明 Out-of-Scope。USB-OTG 物理差分 PHY 收发器。 |

---

<a id="system"></a>
### 操作系统与核心系统调用 (System & OS)（共 68 项 | 编号 `#117 ~ #184` | 已实证: 7 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [ ] | 117 | `system/app_trace_basic` | 📜 Level 2 | P1 | `system/app_trace_basic` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 118 | `system/app_trace_to_plot` | 📜 Level 2 | P2 | `system/app_trace_to_plot` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 119 | `system/base_mac_address` | 📜 Level 2 | P2 | `system/base_mac_address` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 120 | `system/console/advanced` | 📜 Level 2 | P2 | `system/console_advanced` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 121 | `system/console/basic` | 📜 Level 2 | P1 | `system/console_basic` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 122 | `system/deep_sleep` | 📜 Level 2 | P2 | `system/deep_sleep` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 123 | `system/deep_sleep_wake_stub` | 📜 Level 2 | P2 | `system/deep_sleep_wake_stub` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [-] | 124 | `system/efuse` | 📜 Level 2 | P4 | — | 声明 Out-of-Scope。调用底层硬件 eFuse 物理熔断驱动，软件仿真无法进行不可逆电气熔断。 |
| [x] | 125 | `system/esp_event/default_event_loop` | 📜 Level 2 | P0 | [`system/esp_event_default_event_loop`](system/esp_event_default_event_loop) | [Lane 1: 内核调度] 已完成实证。 |
| [x] | 126 | `system/esp_event/user_event_loops` | 📜 Level 2 | P0 | [`system/esp_event_user_event_loops`](system/esp_event_user_event_loops) | [Lane 1: 内核调度] 已完成实证。 |
| [x] | 127 | `system/esp_timer` | 📜 Level 2 | P0 | [`system/esp_timer`](system/esp_timer) | [Lane 1: 内核调度] 已完成实证。 |
| [ ] | 128 | `system/esp_trace` | 📜 Level 2 | P2 | `system/esp_trace` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 129 | `system/eventfd` | 📜 Level 2 | P2 | `system/eventfd` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 130 | `system/flash_suspend` | 📜 Level 2 | P2 | `system/flash_suspend` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [x] | 131 | `system/freertos/basic_freertos_smp_usage` | 📜 Level 2 | P0 | [`system/freertos_basic_freertos_smp_usage`](system/freertos_basic_freertos_smp_usage) | [Lane 1: 内核调度] 已完成实证。 |
| [x] | 132 | `system/freertos/real_time_stats` | 📜 Level 2 | P0 | [`system/freertos_real_time_stats`](system/freertos_real_time_stats) | [Lane 1: 内核调度] 已完成实证。 |
| [ ] | 133 | `system/gcov` | 📜 Level 2 | P2 | `system/gcov` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 134 | `system/gdbstub` | 📜 Level 2 | P2 | `system/gdbstub` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 135 | `system/heap_task_tracking/advanced` | 📜 Level 2 | P2 | `system/heap_task_tracking_advanced` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 136 | `system/heap_task_tracking/basic` | 📜 Level 2 | P1 | `system/heap_task_tracking_basic` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 137 | `system/himem` | 📜 Level 2 | P2 | `system/himem` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 138 | `system/ipc/ipc_isr/riscv` | 📜 Level 2 | P2 | `system/ipc_ipc_isr_riscv` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 139 | `system/ipc/ipc_isr/xtensa` | 📜 Level 2 | P2 | `system/ipc_ipc_isr_xtensa` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 140 | `system/light_sleep` | 📜 Level 2 | P2 | `system/light_sleep` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 141 | `system/nmi_isr` | 📜 Level 2 | P2 | `system/nmi_isr` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [-] | 142 | `system/ota/advanced_https_ota` | 📜 Level 2 | P3 | `system/ota_advanced_https_ota` | 暂缓投入。依赖外部模型。 |
| [ ] | 143 | `system/ota/native_ota_example` | 📜 Level 2 | P2 | `system/ota_native_ota_example` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 144 | `system/ota/otatool` | 📜 Level 2 | P2 | `system/ota_otatool` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [-] | 145 | `system/ota/partitions_ota` | 📜 Level 2 | P3 | `system/ota_partitions_ota` | 暂缓投入。依赖外部模型。 |
| [ ] | 146 | `system/ota/simple_ota_example` | 📜 Level 2 | P2 | `system/ota_simple_ota_example` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 147 | `system/perfmon` | 📜 Level 2 | P2 | `system/perfmon` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 148 | `system/pthread` | 📜 Level 2 | P2 | `system/pthread` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 149 | `system/rt_mqueue` | 📜 Level 2 | P2 | `system/rt_mqueue` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 150 | `system/select` | 📜 Level 2 | P2 | `system/select` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [x] | 151 | `system/startup_time` | 📜 Level 2 | P0 | [`system/startup_time`](system/startup_time) | [Lane 1: 内核调度] 已完成实证。 |
| [ ] | 152 | `system/sysview_tracing` | 📜 Level 2 | P2 | `system/sysview_tracing` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 153 | `system/sysview_tracing_heap_log` | 📜 Level 2 | P2 | `system/sysview_tracing_heap_log` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [x] | 154 | `system/task_watchdog` | 📜 Level 2 | P0 | [`system/task_watchdog`](system/task_watchdog) | [Lane 1: 内核调度] 已完成实证。 |
| [ ] | 155 | `system/ulp/lp_core/build_system` | 📜 Level 2 | P3 | `system/ulp_lp_core_build_system` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 156 | `system/ulp/lp_core/debugging` | 📜 Level 2 | P3 | `system/ulp_lp_core_debugging` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 157 | `system/ulp/lp_core/gpio` | 📜 Level 2 | P1 | `system/ulp_lp_core_gpio` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 158 | `system/ulp/lp_core/gpio_intr_pulse_counter` | 📜 Level 2 | P1 | `system/ulp_lp_core_gpio_intr_pulse_counter` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 159 | `system/ulp/lp_core/gpio_wakeup` | 📜 Level 2 | P1 | `system/ulp_lp_core_gpio_wakeup` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 160 | `system/ulp/lp_core/inter_cpu_critical_section` | 📜 Level 2 | P3 | `system/ulp_lp_core_inter_cpu_critical_section` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 161 | `system/ulp/lp_core/interrupt` | 📜 Level 2 | P3 | `system/ulp_lp_core_interrupt` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 162 | `system/ulp/lp_core/lp_adc` | 🎯 Level 1 | P3 | `system/ulp_lp_core_lp_adc` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 163 | `system/ulp/lp_core/lp_i2c` | 📜 Level 2 | P3 | `system/ulp_lp_core_lp_i2c` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 164 | `system/ulp/lp_core/lp_mailbox` | 📜 Level 2 | P3 | `system/ulp_lp_core_lp_mailbox` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 165 | `system/ulp/lp_core/lp_spi` | 📜 Level 2 | P3 | `system/ulp_lp_core_lp_spi` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 166 | `system/ulp/lp_core/lp_timer_interrupt` | 📜 Level 2 | P3 | `system/ulp_lp_core_lp_timer_interrupt` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 167 | `system/ulp/lp_core/lp_touch` | 📜 Level 2 | P3 | `system/ulp_lp_core_lp_touch` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 168 | `system/ulp/lp_core/lp_uart/lp_uart_char_seq_wakeup` | 📜 Level 2 | P3 | `system/ulp_lp_core_lp_uart_lp_uart_char_seq_wakeup` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 169 | `system/ulp/lp_core/lp_uart/lp_uart_echo` | 📜 Level 2 | P1 | `system/ulp_lp_core_lp_uart_lp_uart_echo` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 170 | `system/ulp/lp_core/lp_uart/lp_uart_print` | 📜 Level 2 | P3 | `system/ulp_lp_core_lp_uart_lp_uart_print` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 171 | `system/ulp/ulp_fsm/ulp_adc` | 🎯 Level 1 | P3 | `system/ulp_ulp_fsm_ulp_adc` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 172 | `system/ulp/ulp_fsm_riscv_combined/counter` | 📜 Level 2 | P3 | `system/ulp_ulp_fsm_riscv_combined_counter` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 173 | `system/ulp/ulp_riscv/adc` | 🎯 Level 1 | P3 | `system/ulp_ulp_riscv_adc` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 174 | `system/ulp/ulp_riscv/ds18b20_onewire` | 📜 Level 2 | P3 | `system/ulp_ulp_riscv_ds18b20_onewire` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 175 | `system/ulp/ulp_riscv/gpio` | 📜 Level 2 | P1 | `system/ulp_ulp_riscv_gpio` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 176 | `system/ulp/ulp_riscv/gpio_interrupt` | 📜 Level 2 | P1 | `system/ulp_ulp_riscv_gpio_interrupt` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 177 | `system/ulp/ulp_riscv/gpio_pulse_counter` | 📜 Level 2 | P1 | `system/ulp_ulp_riscv_gpio_pulse_counter` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 178 | `system/ulp/ulp_riscv/i2c` | 📜 Level 2 | P3 | `system/ulp_ulp_riscv_i2c` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 179 | `system/ulp/ulp_riscv/interrupts` | 📜 Level 2 | P3 | `system/ulp_ulp_riscv_interrupts` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 180 | `system/ulp/ulp_riscv/touch` | 📜 Level 2 | P3 | `system/ulp_ulp_riscv_touch` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 181 | `system/ulp/ulp_riscv/uart_print` | 📜 Level 2 | P3 | `system/ulp_ulp_riscv_uart_print` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 182 | `system/unit_test` | 📜 Level 2 | P2 | `system/unit_test` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 183 | `system/unit_test/test` | 📜 Level 2 | P2 | `system/unit_test_test` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 184 | `system/xip_from_psram` | 📜 Level 2 | P2 | `system/xip_from_psram` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |

---

<a id="protocols"></a>
### 网络与应用层通信协议 (Protocols)（共 35 项 | 编号 `#185 ~ #219` | 已实证: 2 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [ ] | 185 | `protocols/dns_over_https` | 📜 Level 2 | P2 | `protocols/dns_over_https` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [x] | 186 | `protocols/esp_http_client` | 📜 Level 2 | P0 | [`protocols/http_client`](protocols/http_client) | [Lane 6: 无线网络] 已完成实证。 |
| [ ] | 187 | `protocols/esp_http_client_mutual_auth` | 📜 Level 2 | P1 | `protocols/esp_http_client_mutual_auth` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 188 | `protocols/esp_local_ctrl` | 📜 Level 2 | P2 | `protocols/esp_local_ctrl` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 189 | `protocols/http_request` | 📜 Level 2 | P2 | `protocols/http_request` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 190 | `protocols/http_server/advanced_tests` | 📜 Level 2 | P3 | `protocols/http_server_advanced_tests` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 191 | `protocols/http_server/async_handlers` | 📜 Level 2 | P3 | `protocols/http_server_async_handlers` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 192 | `protocols/http_server/captive_portal` | 📜 Level 2 | P3 | `protocols/http_server_captive_portal` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 193 | `protocols/http_server/file_serving` | 📜 Level 2 | P3 | `protocols/http_server_file_serving` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 194 | `protocols/http_server/persistent_sockets` | 📜 Level 2 | P3 | `protocols/http_server_persistent_sockets` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 195 | `protocols/http_server/restful_server` | 📜 Level 2 | P3 | `protocols/http_server_restful_server` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 196 | `protocols/http_server/simple` | 📜 Level 2 | P3 | `protocols/http_server_simple` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 197 | `protocols/http_server/ws_echo_server` | 📜 Level 2 | P1 | `protocols/http_server_ws_echo_server` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 198 | `protocols/https_mbedtls` | 📜 Level 2 | P2 | `protocols/https_mbedtls` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 199 | `protocols/https_request` | 📜 Level 2 | P2 | `protocols/https_request` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 200 | `protocols/https_server/simple` | 📜 Level 2 | P3 | `protocols/https_server_simple` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 201 | `protocols/https_server/wss_server` | 📜 Level 2 | P3 | `protocols/https_server_wss_server` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 202 | `protocols/https_x509_bundle` | 📜 Level 2 | P2 | `protocols/https_x509_bundle` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 203 | `protocols/icmp/pmtu_probe` | 📜 Level 2 | P2 | `protocols/icmp_pmtu_probe` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 204 | `protocols/icmp_echo` | 📜 Level 2 | P1 | `protocols/icmp_echo` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 205 | `protocols/l2tap` | 📜 Level 2 | P2 | `protocols/l2tap` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [x] | 206 | `protocols/mqtt` | 📜 Level 2 | P0 | [`protocols/mqtt_tcp`](protocols/mqtt_tcp) | [Lane 6: 无线网络] 已完成实证。 |
| [ ] | 207 | `protocols/mqtt5` | 📜 Level 2 | P1 | `protocols/mqtt5` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 208 | `protocols/smtp_client` | 📜 Level 2 | P2 | `protocols/smtp_client` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 209 | `protocols/sntp` | 📜 Level 2 | P2 | `protocols/sntp` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 210 | `protocols/sockets/icmpv6_ping` | 📜 Level 2 | P2 | `protocols/sockets_icmpv6_ping` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 211 | `protocols/sockets/non_blocking` | 📜 Level 2 | P2 | `protocols/sockets_non_blocking` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 212 | `protocols/sockets/tcp_client` | 📜 Level 2 | P2 | `protocols/sockets_tcp_client` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 213 | `protocols/sockets/tcp_client_multi_net` | 📜 Level 2 | P2 | `protocols/sockets_tcp_client_multi_net` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 214 | `protocols/sockets/tcp_server` | 📜 Level 2 | P3 | `protocols/sockets_tcp_server` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 215 | `protocols/sockets/tcp_transport_client` | 📜 Level 2 | P2 | `protocols/sockets_tcp_transport_client` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 216 | `protocols/sockets/udp_client` | 📜 Level 2 | P2 | `protocols/sockets_udp_client` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 217 | `protocols/sockets/udp_multicast` | 📜 Level 2 | P2 | `protocols/sockets_udp_multicast` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 218 | `protocols/sockets/udp_server` | 📜 Level 2 | P3 | `protocols/sockets_udp_server` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 219 | `protocols/static_ip` | 📜 Level 2 | P2 | `protocols/static_ip` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |

---

<a id="wifi"></a>
### Wi-Fi 无线局域网 (Wi-Fi)（共 24 项 | 编号 `#220 ~ #243` | 已实证: 4 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [ ] | 220 | `wifi/espnow` | 📜 Level 2 | P2 | `wifi/espnow` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [x] | 221 | `wifi/fast_scan` | 📜 Level 2 | P0 | [`wifi/fast_scan`](wifi/fast_scan) | [Lane 6: 无线网络] 已完成实证。 |
| [ ] | 222 | `wifi/ftm` | 📜 Level 2 | P2 | `wifi/ftm` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [x] | 223 | `wifi/getting_started/softAP` | 📜 Level 2 | P0 | [`wifi/getting_started_softAP`](wifi/getting_started_softAP) | [Lane 6: 无线网络] 已完成实证。 |
| [x] | 224 | `wifi/getting_started/station` | ⚡ Level 3 | P0 | [`wifi/wifi_sta`](wifi/wifi_sta) | [Lane 6: 无线网络] 已完成实证。 |
| [ ] | 225 | `wifi/iperf` | 📜 Level 2 | P2 | `wifi/iperf` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 226 | `wifi/itwt` | 📜 Level 2 | P2 | `wifi/itwt` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 227 | `wifi/power_save` | 📜 Level 2 | P2 | `wifi/power_save` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 228 | `wifi/roaming/roaming_11kvr` | 📜 Level 2 | P2 | `wifi/roaming_roaming_11kvr` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 229 | `wifi/roaming/roaming_app` | 📜 Level 2 | P2 | `wifi/roaming_roaming_app` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [x] | 230 | `wifi/scan` | 📜 Level 2 | P0 | [`wifi/scan`](wifi/scan) | [Lane 6: 无线网络] 已完成实证。 |
| [ ] | 231 | `wifi/smart_config` | 📜 Level 2 | P2 | `wifi/smart_config` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 232 | `wifi/softap_sta` | 📜 Level 2 | P2 | `wifi/softap_sta` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 233 | `wifi/wifi_aware/nan_console` | 📜 Level 2 | P2 | `wifi/wifi_aware_nan_console` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 234 | `wifi/wifi_aware/nan_publisher` | 📜 Level 2 | P2 | `wifi/wifi_aware_nan_publisher` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 235 | `wifi/wifi_aware/nan_subscriber` | 📜 Level 2 | P2 | `wifi/wifi_aware_nan_subscriber` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 236 | `wifi/wifi_aware/usd_publisher` | 📜 Level 2 | P2 | `wifi/wifi_aware_usd_publisher` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 237 | `wifi/wifi_aware/usd_subscriber` | 📜 Level 2 | P2 | `wifi/wifi_aware_usd_subscriber` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 238 | `wifi/wifi_eap_fast` | 📜 Level 2 | P2 | `wifi/wifi_eap_fast` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 239 | `wifi/wifi_easy_connect/dpp-enrollee` | 📜 Level 2 | P2 | `wifi/wifi_easy_connect_dpp-enrollee` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 240 | `wifi/wifi_enterprise` | 📜 Level 2 | P2 | `wifi/wifi_enterprise` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 241 | `wifi/wifi_nvs_config` | ⚙️ Level 4 | P2 | `wifi/wifi_nvs_config` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 242 | `wifi/wps` | 📜 Level 2 | P2 | `wifi/wps` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 243 | `wifi/wps_softap_registrar` | 📜 Level 2 | P2 | `wifi/wps_softap_registrar` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |

---

<a id="bluetooth"></a>
### 蓝牙协议栈 (Bluetooth)（共 147 项 | 编号 `#244 ~ #390` | 已实证: 2 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 244 | `bluetooth/ble_get_started/bluedroid/Bluedroid_Beacon` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 245 | `bluetooth/ble_get_started/bluedroid/Bluedroid_Connection` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 246 | `bluetooth/ble_get_started/bluedroid/Bluedroid_GATT_Server` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [x] | 247 | `bluetooth/ble_get_started/nimble/NimBLE_Beacon` | 📜 Level 2 | P0 | [`bluetooth/ble_get_started_nimble_NimBLE_Beacon`](bluetooth/ble_get_started_nimble_NimBLE_Beacon) | [Lane 6: 无线网络] 已完成实证。 |
| [ ] | 248 | `bluetooth/ble_get_started/nimble/NimBLE_Connection` | 📜 Level 2 | P2 | `bluetooth/ble_get_started_nimble_NimBLE_Connection` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 249 | `bluetooth/ble_get_started/nimble/NimBLE_GATT_Server` | 📜 Level 2 | P2 | `bluetooth/ble_get_started_nimble_NimBLE_GATT_Server` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 250 | `bluetooth/ble_get_started/nimble/NimBLE_Security` | 📜 Level 2 | P2 | `bluetooth/ble_get_started_nimble_NimBLE_Security` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 251 | `bluetooth/ble_uart_service` | 📜 Level 2 | P2 | `bluetooth/ble_uart_service` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [-] | 252 | `bluetooth/bluedroid/ble/ble_acl_latency/cent` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 253 | `bluetooth/bluedroid/ble/ble_acl_latency/periph` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 254 | `bluetooth/bluedroid/ble/ble_ancs` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 255 | `bluetooth/bluedroid/ble/ble_compatibility_test` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 256 | `bluetooth/bluedroid/ble/ble_eddystone_receiver` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 257 | `bluetooth/bluedroid/ble/ble_eddystone_sender` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 258 | `bluetooth/bluedroid/ble/ble_enc_adv_data/enc_adv_data_cent` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 259 | `bluetooth/bluedroid/ble/ble_enc_adv_data/enc_adv_data_prph` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 260 | `bluetooth/bluedroid/ble/ble_hid_device_demo` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 261 | `bluetooth/bluedroid/ble/ble_ibeacon` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 262 | `bluetooth/bluedroid/ble/ble_multi_conn/ble_multi_conn_cent` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 263 | `bluetooth/bluedroid/ble/ble_multi_conn/ble_multi_conn_prph` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 264 | `bluetooth/bluedroid/ble/ble_spp_client` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 265 | `bluetooth/bluedroid/ble/ble_spp_server` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 266 | `bluetooth/bluedroid/ble/ble_throughput/throughput_client` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 267 | `bluetooth/bluedroid/ble/ble_throughput/throughput_server` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 268 | `bluetooth/bluedroid/ble/gatt_client` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 269 | `bluetooth/bluedroid/ble/gatt_security_client` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 270 | `bluetooth/bluedroid/ble/gatt_security_server` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 271 | `bluetooth/bluedroid/ble/gatt_server` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 272 | `bluetooth/bluedroid/ble/gatt_server_service_table` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 273 | `bluetooth/bluedroid/ble/gattc_multi_connect` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 274 | `bluetooth/bluedroid/ble_50/ble50_security_client` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 275 | `bluetooth/bluedroid/ble_50/ble50_security_server` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 276 | `bluetooth/bluedroid/ble_50/ble50_throughput/throughput_client` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 277 | `bluetooth/bluedroid/ble_50/ble50_throughput/throughput_server` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 278 | `bluetooth/bluedroid/ble_50/ble_conn_subrating_central` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 279 | `bluetooth/bluedroid/ble_50/ble_conn_subrating_peripheral` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 280 | `bluetooth/bluedroid/ble_50/ble_connection_central_with_cte` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 281 | `bluetooth/bluedroid/ble_50/ble_connection_peripheral_with_cte` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 282 | `bluetooth/bluedroid/ble_50/ble_pawr_advertiser` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 283 | `bluetooth/bluedroid/ble_50/ble_pawr_advertiser_conn` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 284 | `bluetooth/bluedroid/ble_50/ble_pawr_synchronizer` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 285 | `bluetooth/bluedroid/ble_50/ble_periodic_adv_with_cte` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 286 | `bluetooth/bluedroid/ble_50/ble_periodic_sync_with_cte` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 287 | `bluetooth/bluedroid/ble_50/ble_power_control_central` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 288 | `bluetooth/bluedroid/ble_50/ble_power_control_peripheral` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 289 | `bluetooth/bluedroid/ble_50/multi-adv` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 290 | `bluetooth/bluedroid/ble_50/periodic_adv` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 291 | `bluetooth/bluedroid/ble_50/periodic_sync` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 292 | `bluetooth/bluedroid/bluedroid_host_only/bluedroid_host_only_uart` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 293 | `bluetooth/bluedroid/classic_bt/a2dp_sink_stream` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 294 | `bluetooth/bluedroid/classic_bt/a2dp_sink_stream_aac` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 295 | `bluetooth/bluedroid/classic_bt/a2dp_source` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 296 | `bluetooth/bluedroid/classic_bt/a2dp_source_aac` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 297 | `bluetooth/bluedroid/classic_bt/avrcp_absolute_volume` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 298 | `bluetooth/bluedroid/classic_bt/avrcp_ct_cover_art` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 299 | `bluetooth/bluedroid/classic_bt/avrcp_ct_metadata` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 300 | `bluetooth/bluedroid/classic_bt/bt_discovery` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 301 | `bluetooth/bluedroid/classic_bt/bt_hid_mouse_device` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 302 | `bluetooth/bluedroid/classic_bt/bt_l2cap_client` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 303 | `bluetooth/bluedroid/classic_bt/bt_l2cap_server` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 304 | `bluetooth/bluedroid/classic_bt/bt_spp_acceptor` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 305 | `bluetooth/bluedroid/classic_bt/bt_spp_initiator` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 306 | `bluetooth/bluedroid/classic_bt/bt_spp_vfs_acceptor` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 307 | `bluetooth/bluedroid/classic_bt/bt_spp_vfs_initiator` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 308 | `bluetooth/bluedroid/classic_bt/hfp_ag` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 309 | `bluetooth/bluedroid/classic_bt/hfp_hf` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 310 | `bluetooth/bluedroid/coex/a2dp_gatts_coex` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 311 | `bluetooth/bluedroid/coex/gattc_gatts_coex` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [ ] | 312 | `bluetooth/blufi` | 📜 Level 2 | P2 | `bluetooth/blufi` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [-] | 313 | `bluetooth/esp_ble_audio/bap/broadcast_sink` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 314 | `bluetooth/esp_ble_audio/bap/broadcast_source` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 315 | `bluetooth/esp_ble_audio/bap/unicast_client` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 316 | `bluetooth/esp_ble_audio/bap/unicast_server` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 317 | `bluetooth/esp_ble_audio/cap/acceptor` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 318 | `bluetooth/esp_ble_audio/cap/initiator` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 319 | `bluetooth/esp_ble_audio/tmap/bmr` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 320 | `bluetooth/esp_ble_audio/tmap/bms` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 321 | `bluetooth/esp_ble_audio/tmap/central` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 322 | `bluetooth/esp_ble_audio/tmap/peripheral` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 323 | `bluetooth/esp_ble_iso/big_broadcaster` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 324 | `bluetooth/esp_ble_iso/big_receiver` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 325 | `bluetooth/esp_ble_iso/cis_central` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 326 | `bluetooth/esp_ble_iso/cis_peripheral` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 327 | `bluetooth/esp_ble_mesh/aligenie_demo` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 328 | `bluetooth/esp_ble_mesh/directed_forwarding/df_client` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 329 | `bluetooth/esp_ble_mesh/directed_forwarding/df_server` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 330 | `bluetooth/esp_ble_mesh/fast_provisioning/fast_prov_client` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 331 | `bluetooth/esp_ble_mesh/fast_provisioning/fast_prov_server` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 332 | `bluetooth/esp_ble_mesh/onoff_models/onoff_client` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 333 | `bluetooth/esp_ble_mesh/onoff_models/onoff_server` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 334 | `bluetooth/esp_ble_mesh/provisioner` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 335 | `bluetooth/esp_ble_mesh/remote_provisioning/rpr_client` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 336 | `bluetooth/esp_ble_mesh/remote_provisioning/rpr_server` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 337 | `bluetooth/esp_ble_mesh/remote_provisioning/unprov_dev` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 338 | `bluetooth/esp_ble_mesh/sensor_models/sensor_client` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 339 | `bluetooth/esp_ble_mesh/sensor_models/sensor_server` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 340 | `bluetooth/esp_ble_mesh/vendor_models/vendor_client` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 341 | `bluetooth/esp_ble_mesh/vendor_models/vendor_server` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 342 | `bluetooth/esp_ble_mesh/wifi_coexist` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [ ] | 343 | `bluetooth/esp_hid_device` | 📜 Level 2 | P2 | `bluetooth/esp_hid_device` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 344 | `bluetooth/esp_hid_host` | 📜 Level 2 | P2 | `bluetooth/esp_hid_host` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [-] | 345 | `bluetooth/hci/ble_adv_scan_combined` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 346 | `bluetooth/hci/controller_hci_uart_esp32` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 347 | `bluetooth/hci/controller_hci_uart_esp32c3_and_esp32s3` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [-] | 348 | `bluetooth/hci/controller_vhci_ble_adv` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [ ] | 349 | `bluetooth/nimble/ble_ancs` | 📜 Level 2 | P2 | `bluetooth/nimble_ble_ancs` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 350 | `bluetooth/nimble/ble_chan_sound_initiator` | 📜 Level 2 | P2 | `bluetooth/nimble_ble_chan_sound_initiator` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 351 | `bluetooth/nimble/ble_chan_sound_reflector` | 📜 Level 2 | P2 | `bluetooth/nimble_ble_chan_sound_reflector` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 352 | `bluetooth/nimble/ble_cte/ble_periodic_adv_with_cte` | 📜 Level 2 | P2 | `bluetooth/nimble_ble_cte_ble_periodic_adv_with_cte` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 353 | `bluetooth/nimble/ble_cte/ble_periodic_sync_with_cte` | 📜 Level 2 | P2 | `bluetooth/nimble_ble_cte_ble_periodic_sync_with_cte` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 354 | `bluetooth/nimble/ble_cts/cts_cent` | 📜 Level 2 | P2 | `bluetooth/nimble_ble_cts_cts_cent` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 355 | `bluetooth/nimble/ble_cts/cts_prph` | 📜 Level 2 | P2 | `bluetooth/nimble_ble_cts_cts_prph` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 356 | `bluetooth/nimble/ble_dynamic_service` | 📜 Level 2 | P2 | `bluetooth/nimble_ble_dynamic_service` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 357 | `bluetooth/nimble/ble_enc_adv_data/enc_adv_data_cent` | 📜 Level 2 | P2 | `bluetooth/nimble_ble_enc_adv_data_enc_adv_data_cent` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 358 | `bluetooth/nimble/ble_enc_adv_data/enc_adv_data_prph` | 📜 Level 2 | P2 | `bluetooth/nimble_ble_enc_adv_data_enc_adv_data_prph` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 359 | `bluetooth/nimble/ble_gattc_gatts_coex` | 📜 Level 2 | P2 | `bluetooth/nimble_ble_gattc_gatts_coex` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 360 | `bluetooth/nimble/ble_htp/htp_cent` | 📜 Level 2 | P2 | `bluetooth/nimble_ble_htp_htp_cent` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 361 | `bluetooth/nimble/ble_htp/htp_prph` | 📜 Level 2 | P2 | `bluetooth/nimble_ble_htp_htp_prph` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 362 | `bluetooth/nimble/ble_l2cap_coc/coc_blecent` | 📜 Level 2 | P2 | `bluetooth/nimble_ble_l2cap_coc_coc_blecent` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 363 | `bluetooth/nimble/ble_l2cap_coc/coc_bleprph` | 📜 Level 2 | P1 | `bluetooth/nimble_ble_l2cap_coc_coc_bleprph` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 364 | `bluetooth/nimble/ble_multi_adv` | 📜 Level 2 | P2 | `bluetooth/nimble_ble_multi_adv` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 365 | `bluetooth/nimble/ble_multi_conn/ble_multi_conn_cent` | 📜 Level 2 | P2 | `bluetooth/nimble_ble_multi_conn_ble_multi_conn_cent` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 366 | `bluetooth/nimble/ble_multi_conn/ble_multi_conn_prph` | 📜 Level 2 | P2 | `bluetooth/nimble_ble_multi_conn_ble_multi_conn_prph` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 367 | `bluetooth/nimble/ble_pawr_adv/ble_pawr_adv` | 📜 Level 2 | P2 | `bluetooth/nimble_ble_pawr_adv_ble_pawr_adv` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 368 | `bluetooth/nimble/ble_pawr_adv/ble_pawr_sync` | 📜 Level 2 | P2 | `bluetooth/nimble_ble_pawr_adv_ble_pawr_sync` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 369 | `bluetooth/nimble/ble_pawr_adv_conn/ble_pawr_adv_conn` | 📜 Level 2 | P2 | `bluetooth/nimble_ble_pawr_adv_conn_ble_pawr_adv_conn` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 370 | `bluetooth/nimble/ble_pawr_adv_conn/ble_pawr_sync_conn` | 📜 Level 2 | P2 | `bluetooth/nimble_ble_pawr_adv_conn_ble_pawr_sync_conn` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 371 | `bluetooth/nimble/ble_periodic_adv` | 📜 Level 2 | P2 | `bluetooth/nimble_ble_periodic_adv` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 372 | `bluetooth/nimble/ble_periodic_sync` | 📜 Level 2 | P2 | `bluetooth/nimble_ble_periodic_sync` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 373 | `bluetooth/nimble/ble_phy/phy_cent` | 📜 Level 2 | P2 | `bluetooth/nimble_ble_phy_phy_cent` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 374 | `bluetooth/nimble/ble_phy/phy_prph` | 📜 Level 2 | P2 | `bluetooth/nimble_ble_phy_phy_prph` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 375 | `bluetooth/nimble/ble_proximity_sensor/proximity_sensor_cent` | 📜 Level 2 | P2 | `bluetooth/nimble_ble_proximity_sensor_proximity_sensor_cent` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 376 | `bluetooth/nimble/ble_proximity_sensor/proximity_sensor_prph` | 📜 Level 2 | P2 | `bluetooth/nimble_ble_proximity_sensor_proximity_sensor_prph` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 377 | `bluetooth/nimble/ble_spi_slave` | 📜 Level 2 | P2 | `bluetooth/nimble_ble_spi_slave` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 378 | `bluetooth/nimble/ble_spp/spp_client` | 📜 Level 2 | P2 | `bluetooth/nimble_ble_spp_spp_client` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 379 | `bluetooth/nimble/ble_spp/spp_server` | 📜 Level 2 | P3 | `bluetooth/nimble_ble_spp_spp_server` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 380 | `bluetooth/nimble/blecent` | 📜 Level 2 | P2 | `bluetooth/nimble_blecent` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 381 | `bluetooth/nimble/blecsc` | 📜 Level 2 | P2 | `bluetooth/nimble_blecsc` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 382 | `bluetooth/nimble/blehr` | 📜 Level 2 | P2 | `bluetooth/nimble_blehr` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 383 | `bluetooth/nimble/blemesh` | 📜 Level 2 | P2 | `bluetooth/nimble_blemesh` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [x] | 384 | `bluetooth/nimble/bleprph` | 🎯 Level 1 | P0 | [`bluetooth/bleprph`](bluetooth/bleprph) | [Lane 6: 无线网络] 已完成实证。 |
| [ ] | 385 | `bluetooth/nimble/bleprph_host_only` | 📜 Level 2 | P1 | `bluetooth/nimble_bleprph_host_only` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 386 | `bluetooth/nimble/bleprph_wifi_coex` | 📜 Level 2 | P1 | `bluetooth/nimble_bleprph_wifi_coex` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [-] | 387 | `bluetooth/nimble/hci` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。2.4GHz 蓝牙射频基带物理层。 |
| [ ] | 388 | `bluetooth/nimble/power_save` | 📜 Level 2 | P2 | `bluetooth/nimble_power_save` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 389 | `bluetooth/nimble/throughput_app/blecent_throughput` | 📜 Level 2 | P2 | `bluetooth/nimble_throughput_app_blecent_throughput` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 390 | `bluetooth/nimble/throughput_app/bleprph_throughput` | 📜 Level 2 | P1 | `bluetooth/nimble_throughput_app_bleprph_throughput` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |

---

<a id="storage"></a>
### 片上存储与文件系统 (Storage)（共 27 项 | 编号 `#391 ~ #417` | 已实证: 4 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [ ] | 391 | `storage/custom_flash_driver` | 📜 Level 2 | P2 | `storage/custom_flash_driver` | [Lane 5: 本地存储] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 392 | `storage/emmc` | 📜 Level 2 | P2 | `storage/emmc` | [Lane 5: 本地存储] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 393 | `storage/fatfs/bdl_wl` | 📜 Level 2 | P2 | `storage/fatfs_bdl_wl` | [Lane 5: 本地存储] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 394 | `storage/fatfs/ext_flash` | 📜 Level 2 | P2 | `storage/fatfs_ext_flash` | [Lane 5: 本地存储] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 395 | `storage/fatfs/fatfsgen` | 📜 Level 2 | P2 | `storage/fatfs_fatfsgen` | [Lane 5: 本地存储] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 396 | `storage/fatfs/fs_operations` | 📜 Level 2 | P2 | `storage/fatfs_fs_operations` | [Lane 5: 本地存储] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 397 | `storage/fatfs/getting_started` | 📜 Level 2 | P2 | `storage/fatfs_getting_started` | [Lane 5: 本地存储] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 398 | `storage/littlefs` | 📜 Level 2 | P2 | `storage/littlefs` | [Lane 5: 本地存储] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 399 | `storage/nvs/nvs_bootloader` | ⚙️ Level 4 | P2 | `storage/nvs_nvs_bootloader` | [Lane 5: 本地存储] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 400 | `storage/nvs/nvs_console` | ⚙️ Level 4 | P2 | `storage/nvs_nvs_console` | [Lane 5: 本地存储] 待排期。依赖进一步框架门面扩展。 |
| [x] | 401 | `storage/nvs/nvs_iteration` | 📜 Level 2 | P0 | [`storage/nvs_nvs_iteration`](storage/nvs_nvs_iteration) | [Lane 5: 本地存储] 已完成实证。 |
| [x] | 402 | `storage/nvs/nvs_rw_blob` | ⚙️ Level 4 | P0 | [`storage/nvs_nvs_rw_blob`](storage/nvs_nvs_rw_blob) | [Lane 5: 本地存储] 已完成实证。 |
| [x] | 403 | `storage/nvs/nvs_rw_value` | ⚙️ Level 4 | P0 | [`storage/nvs_nvs_rw_value`](storage/nvs_nvs_rw_value) | [Lane 5: 本地存储] 已完成实证。 |
| [ ] | 404 | `storage/nvs/nvs_rw_value_cxx` | ⚙️ Level 4 | P1 | `storage/nvs_nvs_rw_value_cxx` | [Lane 5: 本地存储] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 405 | `storage/nvs/nvs_statistics` | ⚙️ Level 4 | P2 | `storage/nvs_nvs_statistics` | [Lane 5: 本地存储] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 406 | `storage/nvs/nvsgen` | ⚙️ Level 4 | P2 | `storage/nvs_nvsgen` | [Lane 5: 本地存储] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 407 | `storage/partition_api/partition_find` | 📜 Level 2 | P2 | `storage/partition_api_partition_find` | [Lane 5: 本地存储] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 408 | `storage/partition_api/partition_mmap` | 📜 Level 2 | P2 | `storage/partition_api_partition_mmap` | [Lane 5: 本地存储] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 409 | `storage/partition_api/partition_ops` | 📜 Level 2 | P2 | `storage/partition_api_partition_ops` | [Lane 5: 本地存储] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 410 | `storage/parttool` | 📜 Level 2 | P2 | `storage/parttool` | [Lane 5: 本地存储] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 411 | `storage/perf_benchmark` | 📜 Level 2 | P2 | `storage/perf_benchmark` | [Lane 5: 本地存储] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 412 | `storage/sd_card/sdmmc` | 📜 Level 2 | P2 | `storage/sd_card_sdmmc` | [Lane 5: 本地存储] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 413 | `storage/sd_card/sdspi` | 📜 Level 2 | P2 | `storage/sd_card_sdspi` | [Lane 5: 本地存储] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 414 | `storage/semihost_vfs` | 📜 Level 2 | P2 | `storage/semihost_vfs` | [Lane 5: 本地存储] 待排期。依赖进一步框架门面扩展。 |
| [x] | 415 | `storage/spiffs` | 📜 Level 2 | P0 | [`storage/spiffs`](storage/spiffs) | [Lane 5: 本地存储] 已完成实证。 |
| [ ] | 416 | `storage/spiffsgen` | 📜 Level 2 | P2 | `storage/spiffsgen` | [Lane 5: 本地存储] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 417 | `storage/wear_levelling` | 📜 Level 2 | P2 | `storage/wear_levelling` | [Lane 5: 本地存储] 待排期。依赖进一步框架门面扩展。 |

---

<a id="network"></a>
### 底层网络与接口 (Network)（共 5 项 | 编号 `#418 ~ #422` | 已实证: 0 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [ ] | 418 | `network/bridge` | 📜 Level 2 | P2 | `network/bridge` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 419 | `network/eth2ap` | 📜 Level 2 | P2 | `network/eth2ap` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 420 | `network/simple_sniffer` | 📜 Level 2 | P2 | `network/simple_sniffer` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 421 | `network/sta2eth` | 📜 Level 2 | P2 | `network/sta2eth` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 422 | `network/vlan_support` | 📜 Level 2 | P2 | `network/vlan_support` | [Lane 6: 无线网络] 待排期。依赖进一步框架门面扩展。 |

---

<a id="cxx"></a>
### C++ 运行时与语言特性 (C++)（共 3 项 | 编号 `#423 ~ #425` | 已实证: 0 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [ ] | 423 | `cxx/exceptions` | 📜 Level 2 | P2 | `cxx/exceptions` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 424 | `cxx/pthread` | 📜 Level 2 | P2 | `cxx/pthread` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |
| [ ] | 425 | `cxx/rtti` | 📜 Level 2 | P2 | `cxx/rtti` | [Lane 1: 内核调度] 待排期。依赖进一步框架门面扩展。 |

---

<a id="build_system"></a>
### 构建系统与组件组织 (Build System)（共 19 项 | 编号 `#426 ~ #444` | 已实证: 0 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 426 | `build_system/cmake/component_manager` | ⚙️ Level 4 | P3 | `build_system/cmake_component_manager` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 427 | `build_system/cmake/idf_as_lib` | ⚙️ Level 4 | P3 | `build_system/cmake_idf_as_lib` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 428 | `build_system/cmake/import_lib` | ⚙️ Level 4 | P3 | `build_system/cmake_import_lib` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 429 | `build_system/cmake/import_prebuilt` | ⚙️ Level 4 | P3 | `build_system/cmake_import_prebuilt` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 430 | `build_system/cmake/import_prebuilt/prebuilt` | ⚙️ Level 4 | P3 | `build_system/cmake_import_prebuilt_prebuilt` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 431 | `build_system/cmake/multi_config` | ⚙️ Level 4 | P3 | `build_system/cmake_multi_config` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 432 | `build_system/cmake/plugins` | ⚙️ Level 4 | P3 | `build_system/cmake_plugins` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 433 | `build_system/cmakev2/features/component_manager` | ⚙️ Level 4 | P3 | `build_system/cmakev2_features_component_manager` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 434 | `build_system/cmakev2/features/conditional_component` | ⚙️ Level 4 | P3 | `build_system/cmakev2_features_conditional_component` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 435 | `build_system/cmakev2/features/idf_as_lib` | ⚙️ Level 4 | P3 | `build_system/cmakev2_features_idf_as_lib` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 436 | `build_system/cmakev2/features/import_lib` | ⚙️ Level 4 | P3 | `build_system/cmakev2_features_import_lib` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 437 | `build_system/cmakev2/features/import_lib_direct` | ⚙️ Level 4 | P3 | `build_system/cmakev2_features_import_lib_direct` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 438 | `build_system/cmakev2/features/import_prebuilt` | ⚙️ Level 4 | P3 | `build_system/cmakev2_features_import_prebuilt` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 439 | `build_system/cmakev2/features/import_prebuilt/prebuilt` | ⚙️ Level 4 | P3 | `build_system/cmakev2_features_import_prebuilt_prebuilt` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 440 | `build_system/cmakev2/features/multi_binary` | ⚙️ Level 4 | P3 | `build_system/cmakev2_features_multi_binary` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 441 | `build_system/cmakev2/features/multi_config` | ⚙️ Level 4 | P3 | `build_system/cmakev2_features_multi_config` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 442 | `build_system/cmakev2/features/plugins` | ⚙️ Level 4 | P3 | `build_system/cmakev2_features_plugins` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 443 | `build_system/cmakev2/get-started/hello_world` | ⚙️ Level 4 | P3 | `build_system/cmakev2_get-started_hello_world` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |
| [-] | 444 | `build_system/wrappers` | ⚙️ Level 4 | P3 | `build_system/wrappers` | 暂缓投入。依赖重型外部器件模型，当前阶段暂缓投入。 |

---

<a id="security"></a>
### 硬件加密与芯片安全特性 (Security)（共 10 项 | 编号 `#445 ~ #454` | 已实证: 0 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 445 | `security/flash_encryption` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。硬件 eFuse 不可逆熔丝烧写与加密引擎。 |
| [-] | 446 | `security/hmac_soft_jtag` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。硬件 eFuse 不可逆熔丝烧写与加密引擎。 |
| [-] | 447 | `security/key_manager` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。硬件 eFuse 不可逆熔丝烧写与加密引擎。 |
| [-] | 448 | `security/nvs_encryption_hmac` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。硬件 eFuse 不可逆熔丝烧写与加密引擎。 |
| [-] | 449 | `security/psa_its_custom_backend` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。硬件 eFuse 不可逆熔丝烧写与加密引擎。 |
| [-] | 450 | `security/security_features_app` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。硬件 eFuse 不可逆熔丝烧写与加密引擎。 |
| [-] | 451 | `security/tee/tee_attestation` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。硬件 eFuse 不可逆熔丝烧写与加密引擎。 |
| [-] | 452 | `security/tee/tee_basic` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。硬件 eFuse 不可逆熔丝烧写与加密引擎。 |
| [-] | 453 | `security/tee/tee_secure_ota` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。硬件 eFuse 不可逆熔丝烧写与加密引擎。 |
| [-] | 454 | `security/tee/tee_secure_storage` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。硬件 eFuse 不可逆熔丝烧写与加密引擎。 |

---

<a id="custom_bootloader"></a>
### 定制引导加载程序 (Custom Bootloader)（共 4 项 | 编号 `#455 ~ #458` | 已实证: 0 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 455 | `custom_bootloader/bootloader_extra_dir` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。芯片二级引导程序 ROM 硬件引导链。 |
| [-] | 456 | `custom_bootloader/bootloader_hooks` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。芯片二级引导程序 ROM 硬件引导链。 |
| [-] | 457 | `custom_bootloader/bootloader_multiboot` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。芯片二级引导程序 ROM 硬件引导链。 |
| [-] | 458 | `custom_bootloader/bootloader_override` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。芯片二级引导程序 ROM 硬件引导链。 |

---

<a id="ethernet"></a>
### 有线以太网 (Ethernet - 外部 PHY 硬件)（共 3 项 | 编号 `#459 ~ #461` | 已实证: 0 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 459 | `ethernet/basic` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。外部 PHY 变压器与 RMII/SMI 物理总线。 |
| [-] | 460 | `ethernet/iperf` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。外部 PHY 变压器与 RMII/SMI 物理总线。 |
| [-] | 461 | `ethernet/ptp` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。外部 PHY 变压器与 RMII/SMI 物理总线。 |

---

<a id="lowpower"></a>
### 超低功耗与 ULP 协处理器 (Low Power & ULP)（共 2 项 | 编号 `#462 ~ #463` | 已实证: 0 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 462 | `lowpower/power_management` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。ULP 独立硬件微功耗协处理器环境。 |
| [-] | 463 | `lowpower/vbat` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。ULP 独立硬件微功耗协处理器环境。 |

---

<a id="mesh"></a>
### Wi-Fi 空间自组网 (Mesh)（共 3 项 | 编号 `#464 ~ #466` | 已实证: 0 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 464 | `mesh/internal_communication` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。Wi-Fi Mesh 多机空间电磁拓扑。 |
| [-] | 465 | `mesh/ip_internal_network` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。Wi-Fi Mesh 多机空间电磁拓扑。 |
| [-] | 466 | `mesh/manual_networking` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。Wi-Fi Mesh 多机空间电磁拓扑。 |

---

<a id="openthread"></a>
### OpenThread 802.15.4 线程网络 (OpenThread)（共 6 项 | 编号 `#467 ~ #472` | 已实证: 0 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 467 | `openthread/ot_br` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。802.15.4 2.4GHz 射频物理层。 |
| [-] | 468 | `openthread/ot_cli` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。802.15.4 2.4GHz 射频物理层。 |
| [-] | 469 | `openthread/ot_rcp` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。802.15.4 2.4GHz 射频物理层。 |
| [-] | 470 | `openthread/ot_sleepy_device/deep_sleep` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。802.15.4 2.4GHz 射频物理层。 |
| [-] | 471 | `openthread/ot_sleepy_device/light_sleep` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。802.15.4 2.4GHz 射频物理层。 |
| [-] | 472 | `openthread/ot_trel` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。802.15.4 2.4GHz 射频物理层。 |

---

<a id="zigbee"></a>
### Zigbee 2.4G 射频网络 (Zigbee)（共 3 项 | 编号 `#473 ~ #475` | 已实证: 0 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 473 | `zigbee/esp_zigbee_gateway` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。Zigbee 2.4GHz 射频物理层。 |
| [-] | 474 | `zigbee/light_sample/HA_on_off_light` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。Zigbee 2.4GHz 射频物理层。 |
| [-] | 475 | `zigbee/light_sample/HA_on_off_switch` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。Zigbee 2.4GHz 射频物理层。 |

---

<a id="ieee802154"></a>
### IEEE 802.15.4 原始射频 (IEEE 802.15.4)（共 1 项 | 编号 `#476 ~ #476` | 已实证: 0 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 476 | `ieee802154/ieee802154_cli` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。IEEE 802.15.4 原始射频物理层。 |

---

<a id="phy"></a>
### 射频物理层与工厂校准 (PHY & Calibration)（共 2 项 | 编号 `#477 ~ #478` | 已实证: 0 项 | 隔离待补: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 477 | `phy/antenna` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。芯片工厂射频电气校准与功率表。 |
| [-] | 478 | `phy/cert_test` | ⚙️ Level 4 | P4 | — | 声明 Out-of-Scope。芯片工厂射频电气校准与功率表。 |

---
