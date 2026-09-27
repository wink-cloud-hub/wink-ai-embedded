# ESP-IDF v6.1 官方示例全量仿真适配核对清单 (Checklist)

> **权威上游路径**：`D:\software\embedded-tools\esp-idf\.espressif\v6.1\esp-idf\examples`  
> **参考标准规范**：[`PLAN-20260922-ESP-IDF-SIM-MASTER`](../../implementation-plans/esp32/2026-09-22-esp-idf-simulation-interception-master-plan.md) §7.1.1（三层证据塔与精选规则）  
> **对标基线**：[`docs/vendors/Cmsemicon/CMS8S78XX_EXAMPLE_CHECKLIST.md`](../Cmsemicon/CMS8S78XX_EXAMPLE_CHECKLIST.md)（业界最高保真审计基线）  
> **生成日期**：2026-09-27 | **版本**：v1.0 (全量普查版)  

---

## 一、 总体适配进度与全量普查统计

- **官方分类总数**：19 个功能大类
- **官方独立子示例总数**：**476 个**（地毯式 100% 全量建档，无任何遗漏）
- **全景状态分布统计**：
  - `[x]` **已完成适配并实证 (Passed)**：**9 项**（M0~M4 核心黄金外设与网络连接代表）
  - `[ ]` **待适配排期中 (Pending In-Scope)**：**287 项**（ADC、RMT/RGB、NVS、WebSocket 等纯软件可模拟用例）
  - `[-]` **纯物理硬件专用 / 声明 Out-of-Scope**：**180 项**（Zigbee、Thread、以太网变压器、eFuse 硬件不可逆特性等诚实排除项）
  - `🚫` **存在前置阻断 (Blocked)**：**0 项**

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

## 三、 476 个官方示例逐项核对总账

### 基础快速起步 (Get-Started)（共 2 项 | 已实证: 1 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [x] | 174 | `get-started/blink` | 🎯 Level 1 | P0 | `esp_idfv61_blink_gpio` | M0 标杆已落地。GPIO 输出 + FreeRTOS 延时驱动板载 LED 呼吸闪烁。 |
| [ ] | 175 | `get-started/hello_world` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |

---

### 片上与总线外设 (Peripherals)（共 114 项 | 已实证: 4 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [ ] | 193 | `peripherals/adc/continuous_read` | 🎯 Level 1 | P1 | `待适配` | 排期中。拟对接 PAL pal_adc，支持 ADC 模拟量转换与虚拟电位器/光敏电阻控件。 |
| [ ] | 194 | `peripherals/adc/oneshot_read` | 🎯 Level 1 | P1 | `待适配` | 排期中。拟对接 PAL pal_adc，支持 ADC 模拟量转换与虚拟电位器/光敏电阻控件。 |
| [ ] | 195 | `peripherals/analog_comparator/auto_scan` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 196 | `peripherals/analog_comparator/etm_periodic_scan` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 197 | `peripherals/bitscrambler` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [-] | 198 | `peripherals/camera/dvp_dsi` | 🎯 Level 1 | P3 | `-` | 声明 Out-of-Scope。MIPI-CSI/DVP 摄像头图像传感器与硬件编解码加速器。 |
| [-] | 199 | `peripherals/camera/dvp_isp_dsi` | 🎯 Level 1 | P3 | `-` | 声明 Out-of-Scope。MIPI-CSI/DVP 摄像头图像传感器与硬件编解码加速器。 |
| [-] | 200 | `peripherals/camera/mipi_isp_dsi` | 🎯 Level 1 | P3 | `-` | 声明 Out-of-Scope。MIPI-CSI/DVP 摄像头图像传感器与硬件编解码加速器。 |
| [ ] | 201 | `peripherals/dac/dac_continuous/dac_audio` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 202 | `peripherals/dac/dac_continuous/signal_generator` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 203 | `peripherals/dac/dac_cosine_wave` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 204 | `peripherals/dac/dac_oneshot` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 205 | `peripherals/dedicated_gpio/soft_i2c` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 206 | `peripherals/dedicated_gpio/soft_spi` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 207 | `peripherals/dedicated_gpio/soft_uart` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 208 | `peripherals/dma/async_color_convert` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 209 | `peripherals/dma/async_crc` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 210 | `peripherals/gpio/generic_gpio` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 211 | `peripherals/gpio/matrix_keyboard` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [-] | 212 | `peripherals/h264` | 🎯 Level 1 | P3 | `-` | 声明 Out-of-Scope。MIPI-CSI/DVP 摄像头图像传感器与硬件编解码加速器。 |
| [x] | 213 | `peripherals/i2c/i2c_basic` | 📜 Level 2 | P0 | `esp_idfv61_i2c_basic` | M2 已落地。现代对象式 I2C Master 驱动，总线与器件二级句柄，读写传感器寄存器。 |
| [ ] | 214 | `peripherals/i2c/i2c_eeprom` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 215 | `peripherals/i2c/i2c_slave_network_sensor` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 216 | `peripherals/i2c/i2c_tools` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 217 | `peripherals/i2c/i2c_u8g2` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [-] | 218 | `peripherals/i2s/i2s_advance/i2s_usb` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [ ] | 219 | `peripherals/i2s/i2s_basic/i2s_pdm` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 220 | `peripherals/i2s/i2s_basic/i2s_std` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 221 | `peripherals/i2s/i2s_basic/i2s_tdm` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 222 | `peripherals/i2s/i2s_codec/i2s_es7210_tdm` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 223 | `peripherals/i2s/i2s_codec/i2s_es8311` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 224 | `peripherals/i2s/i2s_recorder` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 225 | `peripherals/i3c/i3c_i2c_basic` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 226 | `peripherals/i3c/i3c_lsm6dscx` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [-] | 227 | `peripherals/isp/multi_pipelines` | 🎯 Level 1 | P3 | `-` | 声明 Out-of-Scope。MIPI-CSI/DVP 摄像头图像传感器与硬件编解码加速器。 |
| [-] | 228 | `peripherals/jpeg/jpeg_decode` | 🎯 Level 1 | P3 | `-` | 声明 Out-of-Scope。MIPI-CSI/DVP 摄像头图像传感器与硬件编解码加速器。 |
| [-] | 229 | `peripherals/jpeg/jpeg_encode` | 🎯 Level 1 | P3 | `-` | 声明 Out-of-Scope。MIPI-CSI/DVP 摄像头图像传感器与硬件编解码加速器。 |
| [ ] | 230 | `peripherals/lcd/i2c_oled` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 231 | `peripherals/lcd/i80_controller` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 232 | `peripherals/lcd/mipi_dsi` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 233 | `peripherals/lcd/parlio_simulate` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 234 | `peripherals/lcd/rgb_panel` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 235 | `peripherals/lcd/spi_lcd_touch` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 236 | `peripherals/lcd/tjpgd` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [x] | 237 | `peripherals/ledc/ledc_basic` | 🎯 Level 1 | P0 | `esp_idfv61_ledc_basic` | M2 已落地。LEDC 定时器与通道配置，定点 PWM 占空比平滑渐变，无浮点。 |
| [ ] | 238 | `peripherals/ledc/ledc_dimmer` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 239 | `peripherals/ledc/ledc_fade` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 240 | `peripherals/ledc/ledc_gamma_curve_fade` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 241 | `peripherals/mcpwm/mcpwm_bdc_speed_control` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 242 | `peripherals/mcpwm/mcpwm_bldc_hall_control` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 243 | `peripherals/mcpwm/mcpwm_capture_hc_sr04` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 244 | `peripherals/mcpwm/mcpwm_foc_svpwm_open_loop` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 245 | `peripherals/mcpwm/mcpwm_servo_control` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 246 | `peripherals/mcpwm/mcpwm_sync` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 247 | `peripherals/parlio/parlio_rx/logic_analyzer` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 248 | `peripherals/parlio/parlio_tx/advanced_rgb_led_matrix` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 249 | `peripherals/parlio/parlio_tx/simple_rgb_led_matrix` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 250 | `peripherals/pcnt/rotary_encoder` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [-] | 251 | `peripherals/ppa/ppa_rgb_lcd` | 🎯 Level 1 | P3 | `-` | 声明 Out-of-Scope。MIPI-CSI/DVP 摄像头图像传感器与硬件编解码加速器。 |
| [ ] | 252 | `peripherals/rmt/dshot_esc` | 🎯 Level 1 | P1 | `待适配` | 排期中。拟实现 RMT 脉冲引擎与 led_strip 门面，驱动虚拟 WS2812 RGB 彩灯。 |
| [ ] | 253 | `peripherals/rmt/ir_nec_transceiver` | 🎯 Level 1 | P1 | `待适配` | 排期中。拟实现 RMT 脉冲引擎与 led_strip 门面，驱动虚拟 WS2812 RGB 彩灯。 |
| [ ] | 254 | `peripherals/rmt/led_strip` | 🎯 Level 1 | P1 | `待适配` | 排期中。拟实现 RMT 脉冲引擎与 led_strip 门面，驱动虚拟 WS2812 RGB 彩灯。 |
| [ ] | 255 | `peripherals/rmt/led_strip_simple_encoder` | 🎯 Level 1 | P1 | `待适配` | 排期中。拟实现 RMT 脉冲引擎与 led_strip 门面，驱动虚拟 WS2812 RGB 彩灯。 |
| [ ] | 256 | `peripherals/rmt/musical_buzzer` | 🎯 Level 1 | P1 | `待适配` | 排期中。拟实现 RMT 脉冲引擎与 led_strip 门面，驱动虚拟 WS2812 RGB 彩灯。 |
| [ ] | 257 | `peripherals/rmt/onewire` | 🎯 Level 1 | P1 | `待适配` | 排期中。拟实现 RMT 脉冲引擎与 led_strip 门面，驱动虚拟 WS2812 RGB 彩灯。 |
| [ ] | 258 | `peripherals/rmt/stepper_motor` | 🎯 Level 1 | P1 | `待适配` | 排期中。拟实现 RMT 脉冲引擎与 led_strip 门面，驱动虚拟 WS2812 RGB 彩灯。 |
| [-] | 259 | `peripherals/sdio/host` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。SDIO 从机高速硬件总线。 |
| [-] | 260 | `peripherals/sdio/slave` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。SDIO 从机高速硬件总线。 |
| [ ] | 261 | `peripherals/sigma_delta/sdm_dac` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 262 | `peripherals/sigma_delta/sdm_led` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 263 | `peripherals/spi_master/hd_eeprom` | 📜 Level 2 | P1 | `待适配` | 排期中。拟对接 SPI Master 轮询与中断事务传输。 |
| [ ] | 264 | `peripherals/spi_master/lcd` | 📜 Level 2 | P1 | `待适配` | 排期中。拟对接 SPI Master 轮询与中断事务传输。 |
| [ ] | 265 | `peripherals/spi_slave/receiver` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 266 | `peripherals/spi_slave/sender` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 267 | `peripherals/spi_slave_hd/append_mode/master` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 268 | `peripherals/spi_slave_hd/append_mode/slave` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 269 | `peripherals/spi_slave_hd/segment_mode/seg_master` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 270 | `peripherals/spi_slave_hd/segment_mode/seg_slave` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 271 | `peripherals/temperature_sensor/temp_sensor` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 272 | `peripherals/temperature_sensor/temp_sensor_monitor` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [x] | 273 | `peripherals/timer_group/gptimer` | ⚡ Level 3 | P0 | `esp_idfv61_gptimer_alarm` | M2 已落地。高精度通用硬件定时器 Alarm 回调与自动重载机制。 |
| [ ] | 274 | `peripherals/timer_group/gptimer_capture_hc_sr04` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 275 | `peripherals/timer_group/wiegand_interface` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 276 | `peripherals/touch_sensor/touch_sens_basic` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 277 | `peripherals/touch_sensor/touch_sens_sleep` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [-] | 278 | `peripherals/twai/cybergear` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。TWAI/CAN 工业汽车差分总线物理收发器。 |
| [-] | 279 | `peripherals/twai/twai_error_recovery` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。TWAI/CAN 工业汽车差分总线物理收发器。 |
| [-] | 280 | `peripherals/twai/twai_network/twai_listen_only` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。TWAI/CAN 工业汽车差分总线物理收发器。 |
| [-] | 281 | `peripherals/twai/twai_network/twai_sender` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。TWAI/CAN 工业汽车差分总线物理收发器。 |
| [-] | 282 | `peripherals/twai/twai_utils` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。TWAI/CAN 工业汽车差分总线物理收发器。 |
| [ ] | 283 | `peripherals/uart/nmea0183_parser` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 284 | `peripherals/uart/uart_async_rxtxtasks` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 285 | `peripherals/uart/uart_dma_ota` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [x] | 286 | `peripherals/uart/uart_echo` | 📜 Level 2 | P0 | `esp_idfv61_uart_echo` | M2 已落地。UART 阻塞读/写回环，环形缓冲区与多任务调度。 |
| [ ] | 287 | `peripherals/uart/uart_echo_rs485` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 288 | `peripherals/uart/uart_events` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 289 | `peripherals/uart/uart_repl` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 290 | `peripherals/uart/uart_select` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [-] | 291 | `peripherals/usb/device/cherryusb_serial_device` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [-] | 292 | `peripherals/usb/device/tusb_cdc_acm_wakeup` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [-] | 293 | `peripherals/usb/device/tusb_composite_msc_serialdevice` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [-] | 294 | `peripherals/usb/device/tusb_console` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [-] | 295 | `peripherals/usb/device/tusb_hid` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [-] | 296 | `peripherals/usb/device/tusb_midi` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [-] | 297 | `peripherals/usb/device/tusb_msc` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [-] | 298 | `peripherals/usb/device/tusb_ncm` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [-] | 299 | `peripherals/usb/device/tusb_serial_device` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [-] | 300 | `peripherals/usb/host/cdc` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [-] | 301 | `peripherals/usb/host/cherryusb_host` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [-] | 302 | `peripherals/usb/host/hid` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [-] | 303 | `peripherals/usb/host/msc` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [-] | 304 | `peripherals/usb/host/usb_host_lib` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [-] | 305 | `peripherals/usb/host/uvc` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |
| [-] | 306 | `peripherals/usb_serial_jtag/usb_serial_jtag_echo` | 📜 Level 2 | P3 | `-` | 声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。 |

---

### 操作系统与核心系统调用 (System & OS)（共 69 项 | 已实证: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [ ] | 381 | `system/app_trace_basic` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 382 | `system/app_trace_to_plot` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 383 | `system/base_mac_address` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 384 | `system/console/advanced` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 385 | `system/console/basic` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 386 | `system/deep_sleep` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 387 | `system/deep_sleep_wake_stub` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 388 | `system/efuse` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 389 | `system/esp_event/default_event_loop` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 390 | `system/esp_event/user_event_loops` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 391 | `system/esp_timer` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 392 | `system/esp_trace` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 393 | `system/eventfd` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 394 | `system/flash_suspend` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 395 | `system/freertos/basic_freertos_smp_usage` | 📜 Level 2 | P2 | `待适配` | 排期中。FreeRTOS 更多高级并发用例（任务通知、软件定时器等）。 |
| [ ] | 396 | `system/freertos/real_time_stats` | 📜 Level 2 | P2 | `待适配` | 排期中。FreeRTOS 更多高级并发用例（任务通知、软件定时器等）。 |
| [ ] | 397 | `system/gcov` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 398 | `system/gdbstub` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 399 | `system/heap_task_tracking/advanced` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 400 | `system/heap_task_tracking/basic` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 401 | `system/himem` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 402 | `system/ipc/ipc_isr/riscv` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 403 | `system/ipc/ipc_isr/xtensa` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 404 | `system/light_sleep` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 405 | `system/nmi_isr` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 406 | `system/ota/advanced_https_ota` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 407 | `system/ota/native_ota_example` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 408 | `system/ota/otatool` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 409 | `system/ota/partitions_ota` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 410 | `system/ota/simple_ota_example` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 411 | `system/perfmon` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 412 | `system/pthread` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 413 | `system/rt_mqueue` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 414 | `system/select` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 415 | `system/startup_time` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 416 | `system/sysview_tracing` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 417 | `system/sysview_tracing_heap_log` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 418 | `system/task_watchdog` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 419 | `system/ulp/lp_core/build_system` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 420 | `system/ulp/lp_core/debugging` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 421 | `system/ulp/lp_core/gpio` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 422 | `system/ulp/lp_core/gpio_intr_pulse_counter` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 423 | `system/ulp/lp_core/gpio_wakeup` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 424 | `system/ulp/lp_core/inter_cpu_critical_section` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 425 | `system/ulp/lp_core/interrupt` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 426 | `system/ulp/lp_core/lp_adc` | 🎯 Level 1 | P1 | `待适配` | 排期中。拟对接 PAL pal_adc，支持 ADC 模拟量转换与虚拟电位器/光敏电阻控件。 |
| [ ] | 427 | `system/ulp/lp_core/lp_i2c` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 428 | `system/ulp/lp_core/lp_mailbox` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 429 | `system/ulp/lp_core/lp_spi` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 430 | `system/ulp/lp_core/lp_timer_interrupt` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 431 | `system/ulp/lp_core/lp_touch` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 432 | `system/ulp/lp_core/lp_uart/lp_uart_char_seq_wakeup` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 433 | `system/ulp/lp_core/lp_uart/lp_uart_echo` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 434 | `system/ulp/lp_core/lp_uart/lp_uart_print` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 435 | `system/ulp/ulp_fsm/ulp` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 436 | `system/ulp/ulp_fsm/ulp_adc` | 🎯 Level 1 | P1 | `待适配` | 排期中。拟对接 PAL pal_adc，支持 ADC 模拟量转换与虚拟电位器/光敏电阻控件。 |
| [ ] | 437 | `system/ulp/ulp_fsm_riscv_combined/counter` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 438 | `system/ulp/ulp_riscv/adc` | 🎯 Level 1 | P1 | `待适配` | 排期中。拟对接 PAL pal_adc，支持 ADC 模拟量转换与虚拟电位器/光敏电阻控件。 |
| [ ] | 439 | `system/ulp/ulp_riscv/ds18b20_onewire` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 440 | `system/ulp/ulp_riscv/gpio` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 441 | `system/ulp/ulp_riscv/gpio_interrupt` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 442 | `system/ulp/ulp_riscv/gpio_pulse_counter` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 443 | `system/ulp/ulp_riscv/i2c` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 444 | `system/ulp/ulp_riscv/interrupts` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 445 | `system/ulp/ulp_riscv/touch` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 446 | `system/ulp/ulp_riscv/uart_print` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 447 | `system/unit_test` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 448 | `system/unit_test/test` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 449 | `system/xip_from_psram` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |

---

### 网络与应用层通信协议 (Protocols)（共 35 项 | 已实证: 2 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [ ] | 309 | `protocols/dns_over_https` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [x] | 310 | `protocols/esp_http_client` | 📜 Level 2 | P0 | `esp_idfv61_http_client` | M4-2 已落地。HTTP 客户端请求隧道桥接，支持 GET/POST 响应流式解析。 |
| [ ] | 311 | `protocols/esp_http_client_mutual_auth` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 312 | `protocols/esp_local_ctrl` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 313 | `protocols/http_request` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 314 | `protocols/http_server/advanced_tests` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 315 | `protocols/http_server/async_handlers` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 316 | `protocols/http_server/captive_portal` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 317 | `protocols/http_server/file_serving` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 318 | `protocols/http_server/persistent_sockets` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 319 | `protocols/http_server/restful_server` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 320 | `protocols/http_server/simple` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 321 | `protocols/http_server/ws_echo_server` | 📜 Level 2 | P2 | `待适配` | 排期中。WebSocket 长连接协议客户端。 |
| [ ] | 322 | `protocols/https_mbedtls` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 323 | `protocols/https_request` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 324 | `protocols/https_server/simple` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 325 | `protocols/https_server/wss_server` | 📜 Level 2 | P2 | `待适配` | 排期中。WebSocket 长连接协议客户端。 |
| [ ] | 326 | `protocols/https_x509_bundle` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 327 | `protocols/icmp/pmtu_probe` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 328 | `protocols/icmp_echo` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 329 | `protocols/l2tap` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [x] | 330 | `protocols/mqtt` | 📜 Level 2 | P0 | `esp_idfv61_mqtt_tcp` | M4-2 已落地。ESP-MQTT 客户端连接内存虚拟轻量 Broker，Pub/Sub 实时闭环。 |
| [ ] | 331 | `protocols/mqtt5` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 332 | `protocols/smtp_client` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 333 | `protocols/sntp` | 📜 Level 2 | P2 | `待适配` | 排期中。SNTP 网络时间协议，校准虚拟 RTC 时钟。 |
| [ ] | 334 | `protocols/sockets/icmpv6_ping` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 335 | `protocols/sockets/non_blocking` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 336 | `protocols/sockets/tcp_client` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 337 | `protocols/sockets/tcp_client_multi_net` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 338 | `protocols/sockets/tcp_server` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 339 | `protocols/sockets/tcp_transport_client` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 340 | `protocols/sockets/udp_client` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 341 | `protocols/sockets/udp_multicast` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 342 | `protocols/sockets/udp_server` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 343 | `protocols/static_ip` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |

---

### Wi-Fi 无线局域网 (Wi-Fi)（共 24 项 | 已实证: 1 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [ ] | 450 | `wifi/espnow` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 451 | `wifi/fast_scan` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 452 | `wifi/ftm` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 453 | `wifi/getting_started/softAP` | 📜 Level 2 | P2 | `待适配` | 排期中。扩展 Wi-Fi 接入点热点 (SoftAP) 广播与虚拟 DHCP Server 租约。 |
| [x] | 454 | `wifi/getting_started/station` | 📜 Level 2 | P0 | `esp_idfv61_wifi_sta` | M4-1 已落地。虚拟 AP 状态机与 DHCP 虚拟 IP 分配，esp_event 事件循环派发。 |
| [ ] | 455 | `wifi/iperf` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 456 | `wifi/itwt` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 457 | `wifi/power_save` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 458 | `wifi/roaming/roaming_11kvr` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 459 | `wifi/roaming/roaming_app` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 460 | `wifi/scan` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 461 | `wifi/smart_config` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 462 | `wifi/softap_sta` | 📜 Level 2 | P2 | `待适配` | 排期中。扩展 Wi-Fi 接入点热点 (SoftAP) 广播与虚拟 DHCP Server 租约。 |
| [ ] | 463 | `wifi/wifi_aware/nan_console` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 464 | `wifi/wifi_aware/nan_publisher` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 465 | `wifi/wifi_aware/nan_subscriber` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 466 | `wifi/wifi_aware/usd_publisher` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 467 | `wifi/wifi_aware/usd_subscriber` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 468 | `wifi/wifi_eap_fast` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 469 | `wifi/wifi_easy_connect/dpp-enrollee` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 470 | `wifi/wifi_enterprise` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 471 | `wifi/wifi_nvs_config` | ⚙️ Level 4 | P1 | `待适配` | 排期中。拟对接 UniSim 虚拟键值存储，支持 Flash 掉电保存。 |
| [ ] | 472 | `wifi/wps` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 473 | `wifi/wps_softap_registrar` | 📜 Level 2 | P2 | `待适配` | 排期中。扩展 Wi-Fi 接入点热点 (SoftAP) 广播与虚拟 DHCP Server 租约。 |

---

### 蓝牙协议栈 (Bluetooth)（共 147 项 | 已实证: 1 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 001 | `bluetooth/ble_get_started/bluedroid/Bluedroid_Beacon` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 002 | `bluetooth/ble_get_started/bluedroid/Bluedroid_Connection` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 003 | `bluetooth/ble_get_started/bluedroid/Bluedroid_GATT_Server` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [ ] | 004 | `bluetooth/ble_get_started/nimble/NimBLE_Beacon` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 005 | `bluetooth/ble_get_started/nimble/NimBLE_Connection` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 006 | `bluetooth/ble_get_started/nimble/NimBLE_GATT_Server` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 007 | `bluetooth/ble_get_started/nimble/NimBLE_Security` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 008 | `bluetooth/ble_uart_service` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [-] | 009 | `bluetooth/bluedroid/ble/ble_acl_latency/cent` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 010 | `bluetooth/bluedroid/ble/ble_acl_latency/periph` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 011 | `bluetooth/bluedroid/ble/ble_ancs` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 012 | `bluetooth/bluedroid/ble/ble_compatibility_test` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 013 | `bluetooth/bluedroid/ble/ble_eddystone_receiver` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 014 | `bluetooth/bluedroid/ble/ble_eddystone_sender` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 015 | `bluetooth/bluedroid/ble/ble_enc_adv_data/enc_adv_data_cent` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 016 | `bluetooth/bluedroid/ble/ble_enc_adv_data/enc_adv_data_prph` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 017 | `bluetooth/bluedroid/ble/ble_hid_device_demo` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 018 | `bluetooth/bluedroid/ble/ble_ibeacon` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 019 | `bluetooth/bluedroid/ble/ble_multi_conn/ble_multi_conn_cent` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 020 | `bluetooth/bluedroid/ble/ble_multi_conn/ble_multi_conn_prph` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 021 | `bluetooth/bluedroid/ble/ble_spp_client` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 022 | `bluetooth/bluedroid/ble/ble_spp_server` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 023 | `bluetooth/bluedroid/ble/ble_throughput/throughput_client` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 024 | `bluetooth/bluedroid/ble/ble_throughput/throughput_server` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 025 | `bluetooth/bluedroid/ble/gatt_client` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 026 | `bluetooth/bluedroid/ble/gatt_security_client` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 027 | `bluetooth/bluedroid/ble/gatt_security_server` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 028 | `bluetooth/bluedroid/ble/gatt_server` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 029 | `bluetooth/bluedroid/ble/gatt_server_service_table` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 030 | `bluetooth/bluedroid/ble/gattc_multi_connect` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 031 | `bluetooth/bluedroid/ble_50/ble50_security_client` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 032 | `bluetooth/bluedroid/ble_50/ble50_security_server` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 033 | `bluetooth/bluedroid/ble_50/ble50_throughput/throughput_client` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 034 | `bluetooth/bluedroid/ble_50/ble50_throughput/throughput_server` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 035 | `bluetooth/bluedroid/ble_50/ble_conn_subrating_central` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 036 | `bluetooth/bluedroid/ble_50/ble_conn_subrating_peripheral` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 037 | `bluetooth/bluedroid/ble_50/ble_connection_central_with_cte` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 038 | `bluetooth/bluedroid/ble_50/ble_connection_peripheral_with_cte` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 039 | `bluetooth/bluedroid/ble_50/ble_pawr_advertiser` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 040 | `bluetooth/bluedroid/ble_50/ble_pawr_advertiser_conn` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 041 | `bluetooth/bluedroid/ble_50/ble_pawr_synchronizer` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 042 | `bluetooth/bluedroid/ble_50/ble_periodic_adv_with_cte` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 043 | `bluetooth/bluedroid/ble_50/ble_periodic_sync_with_cte` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 044 | `bluetooth/bluedroid/ble_50/ble_power_control_central` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 045 | `bluetooth/bluedroid/ble_50/ble_power_control_peripheral` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 046 | `bluetooth/bluedroid/ble_50/multi-adv` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 047 | `bluetooth/bluedroid/ble_50/periodic_adv` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 048 | `bluetooth/bluedroid/ble_50/periodic_sync` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 049 | `bluetooth/bluedroid/bluedroid_host_only/bluedroid_host_only_uart` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 050 | `bluetooth/bluedroid/classic_bt/a2dp_sink_stream` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 051 | `bluetooth/bluedroid/classic_bt/a2dp_sink_stream_aac` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 052 | `bluetooth/bluedroid/classic_bt/a2dp_source` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 053 | `bluetooth/bluedroid/classic_bt/a2dp_source_aac` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 054 | `bluetooth/bluedroid/classic_bt/avrcp_absolute_volume` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 055 | `bluetooth/bluedroid/classic_bt/avrcp_ct_cover_art` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 056 | `bluetooth/bluedroid/classic_bt/avrcp_ct_metadata` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 057 | `bluetooth/bluedroid/classic_bt/bt_discovery` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 058 | `bluetooth/bluedroid/classic_bt/bt_hid_mouse_device` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 059 | `bluetooth/bluedroid/classic_bt/bt_l2cap_client` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 060 | `bluetooth/bluedroid/classic_bt/bt_l2cap_server` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 061 | `bluetooth/bluedroid/classic_bt/bt_spp_acceptor` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 062 | `bluetooth/bluedroid/classic_bt/bt_spp_initiator` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 063 | `bluetooth/bluedroid/classic_bt/bt_spp_vfs_acceptor` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 064 | `bluetooth/bluedroid/classic_bt/bt_spp_vfs_initiator` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 065 | `bluetooth/bluedroid/classic_bt/hfp_ag` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 066 | `bluetooth/bluedroid/classic_bt/hfp_hf` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 067 | `bluetooth/bluedroid/coex/a2dp_gatts_coex` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [-] | 068 | `bluetooth/bluedroid/coex/gattc_gatts_coex` | ⚙️ Level 4 | P3 | `-` | 架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。 |
| [ ] | 069 | `bluetooth/blufi` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [-] | 070 | `bluetooth/esp_ble_audio/bap/broadcast_sink` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。LE Audio 等时信道与 LC3 专用音频硬件编解码。 |
| [-] | 071 | `bluetooth/esp_ble_audio/bap/broadcast_source` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。LE Audio 等时信道与 LC3 专用音频硬件编解码。 |
| [-] | 072 | `bluetooth/esp_ble_audio/bap/unicast_client` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。LE Audio 等时信道与 LC3 专用音频硬件编解码。 |
| [-] | 073 | `bluetooth/esp_ble_audio/bap/unicast_server` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。LE Audio 等时信道与 LC3 专用音频硬件编解码。 |
| [-] | 074 | `bluetooth/esp_ble_audio/cap/acceptor` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。LE Audio 等时信道与 LC3 专用音频硬件编解码。 |
| [-] | 075 | `bluetooth/esp_ble_audio/cap/initiator` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。LE Audio 等时信道与 LC3 专用音频硬件编解码。 |
| [-] | 076 | `bluetooth/esp_ble_audio/tmap/bmr` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。LE Audio 等时信道与 LC3 专用音频硬件编解码。 |
| [-] | 077 | `bluetooth/esp_ble_audio/tmap/bms` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。LE Audio 等时信道与 LC3 专用音频硬件编解码。 |
| [-] | 078 | `bluetooth/esp_ble_audio/tmap/central` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。LE Audio 等时信道与 LC3 专用音频硬件编解码。 |
| [-] | 079 | `bluetooth/esp_ble_audio/tmap/peripheral` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。LE Audio 等时信道与 LC3 专用音频硬件编解码。 |
| [-] | 080 | `bluetooth/esp_ble_iso/big_broadcaster` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。LE Audio 等时信道与 LC3 专用音频硬件编解码。 |
| [-] | 081 | `bluetooth/esp_ble_iso/big_receiver` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。LE Audio 等时信道与 LC3 专用音频硬件编解码。 |
| [-] | 082 | `bluetooth/esp_ble_iso/cis_central` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。LE Audio 等时信道与 LC3 专用音频硬件编解码。 |
| [-] | 083 | `bluetooth/esp_ble_iso/cis_peripheral` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。LE Audio 等时信道与 LC3 专用音频硬件编解码。 |
| [-] | 084 | `bluetooth/esp_ble_mesh/aligenie_demo` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [-] | 085 | `bluetooth/esp_ble_mesh/directed_forwarding/df_client` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [-] | 086 | `bluetooth/esp_ble_mesh/directed_forwarding/df_server` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [-] | 087 | `bluetooth/esp_ble_mesh/fast_provisioning/fast_prov_client` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [-] | 088 | `bluetooth/esp_ble_mesh/fast_provisioning/fast_prov_server` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [-] | 089 | `bluetooth/esp_ble_mesh/onoff_models/onoff_client` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [-] | 090 | `bluetooth/esp_ble_mesh/onoff_models/onoff_server` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [-] | 091 | `bluetooth/esp_ble_mesh/provisioner` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [-] | 092 | `bluetooth/esp_ble_mesh/remote_provisioning/rpr_client` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [-] | 093 | `bluetooth/esp_ble_mesh/remote_provisioning/rpr_server` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [-] | 094 | `bluetooth/esp_ble_mesh/remote_provisioning/unprov_dev` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [-] | 095 | `bluetooth/esp_ble_mesh/sensor_models/sensor_client` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [-] | 096 | `bluetooth/esp_ble_mesh/sensor_models/sensor_server` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [-] | 097 | `bluetooth/esp_ble_mesh/vendor_models/vendor_client` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [-] | 098 | `bluetooth/esp_ble_mesh/vendor_models/vendor_server` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [-] | 099 | `bluetooth/esp_ble_mesh/wifi_coexist` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。 |
| [ ] | 100 | `bluetooth/esp_hid_device` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 101 | `bluetooth/esp_hid_host` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [-] | 102 | `bluetooth/hci/ble_adv_scan_combined` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。裸控制器 HCI 物理 UART 传输层协议。 |
| [-] | 103 | `bluetooth/hci/controller_hci_uart_esp32` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。裸控制器 HCI 物理 UART 传输层协议。 |
| [-] | 104 | `bluetooth/hci/controller_hci_uart_esp32c3_and_esp32s3` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。裸控制器 HCI 物理 UART 传输层协议。 |
| [-] | 105 | `bluetooth/hci/controller_vhci_ble_adv` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。裸控制器 HCI 物理 UART 传输层协议。 |
| [ ] | 106 | `bluetooth/nimble/ble_ancs` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 107 | `bluetooth/nimble/ble_chan_sound_initiator` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 108 | `bluetooth/nimble/ble_chan_sound_reflector` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 109 | `bluetooth/nimble/ble_cte/ble_periodic_adv_with_cte` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 110 | `bluetooth/nimble/ble_cte/ble_periodic_sync_with_cte` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 111 | `bluetooth/nimble/ble_cts/cts_cent` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 112 | `bluetooth/nimble/ble_cts/cts_prph` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 113 | `bluetooth/nimble/ble_dynamic_service` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 114 | `bluetooth/nimble/ble_enc_adv_data/enc_adv_data_cent` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 115 | `bluetooth/nimble/ble_enc_adv_data/enc_adv_data_prph` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 116 | `bluetooth/nimble/ble_gattc_gatts_coex` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 117 | `bluetooth/nimble/ble_htp/htp_cent` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 118 | `bluetooth/nimble/ble_htp/htp_prph` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 119 | `bluetooth/nimble/ble_l2cap_coc/coc_blecent` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 120 | `bluetooth/nimble/ble_l2cap_coc/coc_bleprph` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 121 | `bluetooth/nimble/ble_multi_adv` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 122 | `bluetooth/nimble/ble_multi_conn/ble_multi_conn_cent` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 123 | `bluetooth/nimble/ble_multi_conn/ble_multi_conn_prph` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 124 | `bluetooth/nimble/ble_pawr_adv/ble_pawr_adv` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 125 | `bluetooth/nimble/ble_pawr_adv/ble_pawr_sync` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 126 | `bluetooth/nimble/ble_pawr_adv_conn/ble_pawr_adv_conn` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 127 | `bluetooth/nimble/ble_pawr_adv_conn/ble_pawr_sync_conn` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 128 | `bluetooth/nimble/ble_periodic_adv` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 129 | `bluetooth/nimble/ble_periodic_sync` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 130 | `bluetooth/nimble/ble_phy/phy_cent` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 131 | `bluetooth/nimble/ble_phy/phy_prph` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 132 | `bluetooth/nimble/ble_proximity_sensor/proximity_sensor_cent` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 133 | `bluetooth/nimble/ble_proximity_sensor/proximity_sensor_prph` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 134 | `bluetooth/nimble/ble_spi_slave` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 135 | `bluetooth/nimble/ble_spp/spp_client` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 136 | `bluetooth/nimble/ble_spp/spp_server` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 137 | `bluetooth/nimble/blecent` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 138 | `bluetooth/nimble/blecsc` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 139 | `bluetooth/nimble/blehr` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 140 | `bluetooth/nimble/blemesh` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [x] | 141 | `bluetooth/nimble/bleprph` | 🎯 Level 1 | P0 | `esp_idfv61_bleprph` | M4-3/M4-4 已落地。NimBLE GATT 静态属性池、特征值读写回调与 Virtual BLE Inspector 交互面板。 |
| [ ] | 142 | `bluetooth/nimble/bleprph_host_only` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 143 | `bluetooth/nimble/bleprph_wifi_coex` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [-] | 144 | `bluetooth/nimble/hci` | ⚙️ Level 4 | P3 | `-` | 声明 Out-of-Scope。裸控制器 HCI 物理 UART 传输层协议。 |
| [ ] | 145 | `bluetooth/nimble/power_save` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 146 | `bluetooth/nimble/throughput_app/blecent_throughput` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 147 | `bluetooth/nimble/throughput_app/bleprph_throughput` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |

---

### 片上存储与文件系统 (Storage)（共 27 项 | 已实证: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [ ] | 354 | `storage/custom_flash_driver` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 355 | `storage/emmc` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 356 | `storage/fatfs/bdl_wl` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 357 | `storage/fatfs/ext_flash` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 358 | `storage/fatfs/fatfsgen` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 359 | `storage/fatfs/fs_operations` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 360 | `storage/fatfs/getting_started` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 361 | `storage/littlefs` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 362 | `storage/nvs/nvs_bootloader` | ⚙️ Level 4 | P1 | `待适配` | 排期中。拟对接 UniSim 虚拟键值存储，支持 Flash 掉电保存。 |
| [ ] | 363 | `storage/nvs/nvs_console` | ⚙️ Level 4 | P1 | `待适配` | 排期中。拟对接 UniSim 虚拟键值存储，支持 Flash 掉电保存。 |
| [ ] | 364 | `storage/nvs/nvs_iteration` | ⚙️ Level 4 | P1 | `待适配` | 排期中。拟对接 UniSim 虚拟键值存储，支持 Flash 掉电保存。 |
| [ ] | 365 | `storage/nvs/nvs_rw_blob` | ⚙️ Level 4 | P1 | `待适配` | 排期中。拟对接 UniSim 虚拟键值存储，支持 Flash 掉电保存。 |
| [ ] | 366 | `storage/nvs/nvs_rw_value` | ⚙️ Level 4 | P1 | `待适配` | 排期中。拟对接 UniSim 虚拟键值存储，支持 Flash 掉电保存。 |
| [ ] | 367 | `storage/nvs/nvs_rw_value_cxx` | ⚙️ Level 4 | P1 | `待适配` | 排期中。拟对接 UniSim 虚拟键值存储，支持 Flash 掉电保存。 |
| [ ] | 368 | `storage/nvs/nvs_statistics` | ⚙️ Level 4 | P1 | `待适配` | 排期中。拟对接 UniSim 虚拟键值存储，支持 Flash 掉电保存。 |
| [ ] | 369 | `storage/nvs/nvsgen` | ⚙️ Level 4 | P1 | `待适配` | 排期中。拟对接 UniSim 虚拟键值存储，支持 Flash 掉电保存。 |
| [ ] | 370 | `storage/partition_api/partition_find` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 371 | `storage/partition_api/partition_mmap` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 372 | `storage/partition_api/partition_ops` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 373 | `storage/parttool` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 374 | `storage/perf_benchmark` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 375 | `storage/sd_card/sdmmc` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 376 | `storage/sd_card/sdspi` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 377 | `storage/semihost_vfs` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 378 | `storage/spiffs` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 379 | `storage/spiffsgen` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 380 | `storage/wear_levelling` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |

---

### 底层网络与接口 (Network)（共 5 项 | 已实证: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [ ] | 182 | `network/bridge` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 183 | `network/eth2ap` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 184 | `network/simple_sniffer` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 185 | `network/sta2eth` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 186 | `network/vlan_support` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |

---

### C++ 运行时与语言特性 (C++)（共 3 项 | 已实证: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [ ] | 168 | `cxx/exceptions` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 169 | `cxx/pthread` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |
| [ ] | 170 | `cxx/rtti` | 📜 Level 2 | P2 | `待适配` | 待排期。依赖进一步框架门面扩展。 |

---

### 构建系统与组件组织 (Build System)（共 16 项 | 已实证: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 148 | `build_system/cmake/component_manager` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 149 | `build_system/cmake/import_lib` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 150 | `build_system/cmake/import_prebuilt` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 151 | `build_system/cmake/import_prebuilt/prebuilt` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 152 | `build_system/cmake/multi_config` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 153 | `build_system/cmake/plugins` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 154 | `build_system/cmakev2/features/component_manager` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 155 | `build_system/cmakev2/features/conditional_component` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 156 | `build_system/cmakev2/features/import_lib` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 157 | `build_system/cmakev2/features/import_lib_direct` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 158 | `build_system/cmakev2/features/import_prebuilt` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 159 | `build_system/cmakev2/features/import_prebuilt/prebuilt` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 160 | `build_system/cmakev2/features/multi_config` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 161 | `build_system/cmakev2/features/plugins` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 162 | `build_system/cmakev2/get-started/hello_world` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |
| [-] | 163 | `build_system/wrappers` | ⚙️ Level 4 | P4 | `-` | 编译构建工具链自身测试，非嵌入式运行时业务代码。 |

---

### 硬件加密与芯片安全特性 (Security - 硬件物理特性)（共 10 项 | 已实证: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 344 | `security/flash_encryption` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。涉及硬件 eFuse 烧死熔断与物理 Flash 硬件解密引擎。 |
| [-] | 345 | `security/hmac_soft_jtag` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。涉及硬件 eFuse 烧死熔断与物理 Flash 硬件解密引擎。 |
| [-] | 346 | `security/key_manager` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。涉及硬件 eFuse 烧死熔断与物理 Flash 硬件解密引擎。 |
| [-] | 347 | `security/nvs_encryption_hmac` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。涉及硬件 eFuse 烧死熔断与物理 Flash 硬件解密引擎。 |
| [-] | 348 | `security/psa_its_custom_backend` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。涉及硬件 eFuse 烧死熔断与物理 Flash 硬件解密引擎。 |
| [-] | 349 | `security/security_features_app` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。涉及硬件 eFuse 烧死熔断与物理 Flash 硬件解密引擎。 |
| [-] | 350 | `security/tee/tee_attestation` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。涉及硬件 eFuse 烧死熔断与物理 Flash 硬件解密引擎。 |
| [-] | 351 | `security/tee/tee_basic` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。涉及硬件 eFuse 烧死熔断与物理 Flash 硬件解密引擎。 |
| [-] | 352 | `security/tee/tee_secure_ota` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。涉及硬件 eFuse 烧死熔断与物理 Flash 硬件解密引擎。 |
| [-] | 353 | `security/tee/tee_secure_storage` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。涉及硬件 eFuse 烧死熔断与物理 Flash 硬件解密引擎。 |

---

### 定制引导加载程序 (Custom Bootloader)（共 4 项 | 已实证: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 164 | `custom_bootloader/bootloader_extra_dir` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。芯片二级引导程序，纯软件仿真直接进入 app_main。 |
| [-] | 165 | `custom_bootloader/bootloader_hooks` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。芯片二级引导程序，纯软件仿真直接进入 app_main。 |
| [-] | 166 | `custom_bootloader/bootloader_multiboot` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。芯片二级引导程序，纯软件仿真直接进入 app_main。 |
| [-] | 167 | `custom_bootloader/bootloader_override` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。芯片二级引导程序，纯软件仿真直接进入 app_main。 |

---

### 有线以太网 (Ethernet - 外部 PHY 硬件)（共 3 项 | 已实证: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 171 | `ethernet/basic` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。依赖板外物理变压器及外部 PHY 芯片（RMII/SMI 总线）。 |
| [-] | 172 | `ethernet/iperf` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。依赖板外物理变压器及外部 PHY 芯片（RMII/SMI 总线）。 |
| [-] | 173 | `ethernet/ptp` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。依赖板外物理变压器及外部 PHY 芯片（RMII/SMI 总线）。 |

---

### 超低功耗与 ULP 协处理器 (Low Power & ULP)（共 2 项 | 已实证: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 177 | `lowpower/power_management` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。ULP 独立微功耗 FSM/RISC-V 协处理器硬件运行环境。 |
| [-] | 178 | `lowpower/vbat` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。ULP 独立微功耗 FSM/RISC-V 协处理器硬件运行环境。 |

---

### Wi-Fi 空间自组网 (Mesh)（共 3 项 | 已实证: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 179 | `mesh/internal_communication` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。多机空间电磁跳步网络，需分布式拓扑模拟。 |
| [-] | 180 | `mesh/ip_internal_network` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。多机空间电磁跳步网络，需分布式拓扑模拟。 |
| [-] | 181 | `mesh/manual_networking` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。多机空间电磁跳步网络，需分布式拓扑模拟。 |

---

### OpenThread 802.15.4 线程网络 (OpenThread)（共 6 项 | 已实证: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 187 | `openthread/ot_br` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。依赖 2.4GHz 空间物理射频网卡与硬件基带解调。 |
| [-] | 188 | `openthread/ot_cli` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。依赖 2.4GHz 空间物理射频网卡与硬件基带解调。 |
| [-] | 189 | `openthread/ot_rcp` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。依赖 2.4GHz 空间物理射频网卡与硬件基带解调。 |
| [-] | 190 | `openthread/ot_sleepy_device/deep_sleep` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。依赖 2.4GHz 空间物理射频网卡与硬件基带解调。 |
| [-] | 191 | `openthread/ot_sleepy_device/light_sleep` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。依赖 2.4GHz 空间物理射频网卡与硬件基带解调。 |
| [-] | 192 | `openthread/ot_trel` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。依赖 2.4GHz 空间物理射频网卡与硬件基带解调。 |

---

### Zigbee 2.4G 射频网络 (Zigbee)（共 3 项 | 已实证: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 474 | `zigbee/esp_zigbee_gateway` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。依赖 2.4GHz 空间物理射频网卡与硬件基带解调。 |
| [-] | 475 | `zigbee/light_sample/HA_on_off_light` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。依赖 2.4GHz 空间物理射频网卡与硬件基带解调。 |
| [-] | 476 | `zigbee/light_sample/HA_on_off_switch` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。依赖 2.4GHz 空间物理射频网卡与硬件基带解调。 |

---

### IEEE 802.15.4 原始射频 (IEEE 802.15.4)（共 1 项 | 已实证: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 176 | `ieee802154/ieee802154_cli` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。依赖 2.4GHz 空间物理射频网卡与硬件基带解调。 |

---

### 射频物理层与工厂校准 (PHY & Calibration)（共 2 项 | 已实证: 0 项）

| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |
| :---: | :---: | :--- | :---: | :---: | :--- | :--- |
| [-] | 307 | `phy/antenna` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。芯片工厂模拟射频电气校准与功率表。 |
| [-] | 308 | `phy/cert_test` | ⚙️ Level 4 | P4 | `-` | 声明 Out-of-Scope。芯片工厂模拟射频电气校准与功率表。 |

---
