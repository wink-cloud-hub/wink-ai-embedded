# Wink 板级硬件定义规范 (Board Definition)

本目录收录 Wink 平台支持的开发板硬件描述文件（`*.json`）。

开发板描述文件（`board.json`）是 Wink Micro OS 与 Wink Tools 代码生成链中的**硬件单源真理（SSOT）**，用于在编译期将用户应用配置（`wink-app.json`）映射到底层物理引脚、总线拓扑、内存配额及外设约束。

---

## 1. 目录结构与收录板卡 (Phase 2 分组架构)

板级文件按 MCU 芯片家族（Family）进行目录分级归类：

```text
boards/
├── README.md
├── avr/
│   └── arduino_uno_r3.json            # 经典 Arduino UNO R3 官方物理板 (ATmega328P, 5V/10-bit ADC)
├── esp32/
│   └── esp32_devkitc_v4.json          # 乐鑫 ESP32-DevKitC V4 商业物理板
├── mcs51/
│   ├── stc89c52_devboard.json         # 宏晶 STC89C52 教学开发板 (标准 P0-P3 排针 + ADC0832 扩展)
│   └── cms8s78xx_devboard.json        # 中微 CMS8S78xx 工业开发板 (1T 8051, 12-bit ADC, 26路 GPIO, LED/LCD 硬件驱动)
└── pdk/
    └── padauk_pfs154_devboard.json    # 应广 PFS154 参考开发板 (PA/PB 双端口)
```

| 规范路径 | 目标 MCU | 架构 / 内核 | 典型硬件特性 |
| :--- | :--- | :--- | :--- |
| [`avr/arduino_uno_r3.json`](./avr/arduino_uno_r3.json) | ATmega328P | AVR8 | 经典 Arduino 排针 (D0-D13, A0-A5)、5V/10 位 ADC、板载 D13 LED |
| [`esp32/esp32_devkitc_v4.json`](./esp32/esp32_devkitc_v4.json) | ESP32 | Xtensa-LX6 | Wi-Fi/BLE 射频、ADC1/ADC2 双单元分区、I2C/SPI 总线 |
| [`mcs51/stc89c52_devboard.json`](./mcs51/stc89c52_devboard.json) | STC89C52 | 8051 / 8052 | P0-P3 线性化 32 引脚映射、ADC0832 外部采样支持 |
| [`mcs51/cms8s78xx_devboard.json`](./mcs51/cms8s78xx_devboard.json) | CMS8S78xx | 1T 8051 | 26路 GPIO、内置 12 位 ADC (AN0-AN25)、COM/SEG 硬件驱动 |
| [`pdk/padauk_pfs154_devboard.json`](./pdk/padauk_pfs154_devboard.json) | PFS154 | PDK14 | 超低成本应广单片机、PA/PB 双端口排针 |

---

## 2. 字段字典与核心架构规范

开发板描述文件分为 5 大顶级模块：`metadata`、`onboard_devices`、`buses`、`adc`、`headers`。

### 2.1 顶级字段全景字典

| 字段路径 | 类型 | 必填 | 嵌入式工程含义与消费方 |
| :--- | :--- | :---: | :--- |
| **`metadata`** | `object` | **是** | 描述板卡标识、MCU 架构与内存配额基线。 |
| `metadata.board_name` | `string` | **是** | 板卡唯一 ID，与 `wink-app.json` 中的 `"board"` 字段精确匹配。 |
| `metadata.family` | `string` | **是** | 芯片家族（如 `esp32`, `mcs51`, `pdk`），决定底层 PAL/HAL 平台实现分支。 |
| `metadata.core` | `string` | **是** | 处理器内核指令集（如 `xtensa-lx6`, `8051`, `pdk14`），供工具链确定编译器与汇编约束。 |
| `metadata.mcu` | `string` | **是** | **具体 MCU 芯片型号**（如 `esp32`, `cms8s78xx`, `at89c52`, `pfs154`），贯穿 CMake 宏注入、固件寄存器分发与前端仿真推导，详见 [§3.4](#34-metadatamcu-芯片型号的系统级联动与消费链路)。 |
| `metadata.vendor` | `string` | 否 | 芯片原厂供应商（如 `espressif`, `generic-8051`, `padauk`）。 |
| `metadata.memory.sim_heap_quota_kb` | `integer` | **是** | 仿真与 WASM 运行期的堆内存配额基线（KB），作为内存配置的兜底参考。 |
| **`onboard_devices`** | `object` | 否 | 板载出厂硬连线外设清单（如焊死在板上的 LED、按键、蜂鸣器）。 |
| `onboard_devices.<name>.type` | `string` | **是** | 外设类型，必须匹配 `tools/codegen/drivers/` 中的已知驱动类型（如 `led`, `button`）。 |
| `onboard_devices.<name>.gpio_pin` | `integer` | **是** | 外设硬连线对应的物理 GPIO 引脚编号。 |
| `onboard_devices.<name>.active_high` | `boolean` | 否 | **有效电平极性**：`true` 表示高电平点亮/有效（拉电流驱动）；默认通常为 `true`。 |
| `onboard_devices.<name>.active_low` | `boolean` | 否 | **有效电平极性**：`true` 表示低电平点亮/触发（灌电流驱动或内部上拉按键）。 |
| **`buses`** | `object` | 否 | 硬件通信总线默认引脚拓扑（I2C、SPI、UART 等）。 |
| `buses.i2c<N>.sda` | `integer` | 否 | 硬件 I2C 控制器编号 $N$ 的默认数据线物理 GPIO（如 `i2c0` 的 SDA）。 |
| `buses.i2c<N>.scl` | `integer` | 否 | 硬件 I2C 控制器编号 $N$ 的默认时钟线物理 GPIO。 |
| `buses.spi<N>.*` | `integer` | 否 | 硬件 SPI 控制器的引脚映射（`mosi`, `miso`, `sclk`, `cs`）。 |
| **`adc`** | `object` | 否 | 模拟前端通道拓扑与硬件物理冲突门禁规则（ESP32 特有核心段）。 |
| `adc.pins.<gpio>.channel` | `integer` | **是** | 该物理 GPIO 对应的片上 ADC 模拟采样通道编号（如通道 0~9）。 |
| `adc.pins.<gpio>.unit` | `integer` | **是** | 该引脚所属的片上 ADC 控制器单元编号（如 ADC1 或 ADC2）。 |
| `adc.pins.<gpio>.wifi_conflict` | `boolean` | 否 | **射频硬件冲突标记**：`true` 表示该引脚归属 ADC2，与片上 Wi-Fi/BLE 射频前端互斥。 |
| `adc.default_full_scale_mv` | `integer` | 否 | 默认满量程参考电压（毫伏），如衰减 11dB 典型为 3100mV，供前端将原始采样值折算为电压。 |
| `adc.default_resolution_bits` | `integer` | 否 | 模数转换精度（位），如 12bit (0~4095) 或 10bit (0~1023)。 |
| **`headers`** | `object` | **是** | 外部排针丝印/别名到物理引脚或线性端口编号的映射字典。服务三条消费链路：codegen 期 `$board.headers.<KEY>` 符号解析、前端画布丝印与走线端点派生、仿真 PinArbiter 通道号约定。**注意：headers 标签本身不会在固件中生成具名 C 常量**（Arduino 引脚常量由专门的 `wink_board_pins.h` 生成契约承担），详见 [§3.3](#33-headers-排针映射与三方消费链路) 与 [§3.5](#35-wink_board_pinsh-板级引脚常量生成契约arduino)。 |
| `headers.<label>` | `integer` | **是** | 排针引脚名映射到的物理引脚或线性端口编号。 |

---

## 3. 重点字段的设计原理解析

### 3.1 `onboard_devices` 中的极性控制 (`active_high` vs `active_low`)

在嵌入式硬件设计中，相同外设在不同板卡上的接法经常不同：
* **LED 驱动**：
  * **拉电流接法 (Active-High)**：GPIO 输出高电平（3.3V）$\to$ 限流电阻 $\to$ LED $\to$ GND。此时必须配置 `"active_high": true`。
  * **灌电流接法 (Active-Low)**：3.3V $\to$ 限流电阻 $\to$ LED $\to$ GPIO。低电平导通点亮。此时必须配置 `"active_low": true`。
* **按键电路 (Button)**：
  * **上拉接地 (Active-Low)**：按键一端接 GPIO，另一端接 GND，内部配置上拉电阻，按下时拉低输入。
* **工程价值**：应用层只需声明 `"use_onboard": "status_led"`，底层代码生成器自动匹配极性反转逻辑，上层业务只需调用统一的 `activate()` / `deactivate()`，无需人工判断高低电平。

---

### 3.2 `adc` 模拟前端与 Wi-Fi 射频冲突门禁 (`wifi_conflict`)

#### 硬件物理背景：
在 ESP32 芯片中，包含两个 SAR ADC 模块（ADC1 与 ADC2）：
* **ADC1**：GPIO 32 ~ 39，由独立控制器驱动；
* **ADC2**：GPIO 0, 2, 4, 12 ~ 15, 25 ~ 27。**ADC2 的控制器与 Wi-Fi / BLE 射频基带共享逻辑前端**。
一旦底层激活了 Wi-Fi 栈（调用 `esp_wifi_start()`），Wi-Fi 驱动将周期性抢占 SAR ADC2 采样模块用于发射功率校准。此时应用程序若读取 ADC2 引脚，驱动会返回 `ESP_ERR_TIMEOUT`，严重时引发系统死锁。

#### Codegen 静态门禁实现：
在 `app_codegen.py` 的 `_validate_adc_gate()` 中：
1. 校验使用 `analog_input` 角色的 GPIO 是否在 `adc.pins` 列表中；
2. 当 `wink-app.json` 中配置了 `system.connectivity.wifi: true` 或 `ble: true` 时，若引脚标记了 `wifi_conflict: true`，**在编译期直接报错终止**：
   ```text
   error: device 'temperature' gpio_pin 25 belongs to ADC2 (wifi_conflict=true),
   which conflicts with system.connectivity.wifi/ble=true on board 'esp32_devkitc_v4'.
   Use an ADC1 pin or disable the SoC radio.
   ```
3. 检测重复通道：即使两个引脚编号不同，若对应同一个 `(unit, channel)`，同样拦截，防止底层采样前端冲突。

---

### 3.3 `headers` 排针映射与三方消费链路

`headers` 是板卡排针丝印名（`D2`、`A0`、`P3.2`、`GPIO2` 等）到统一线性引脚编号的符号查表字典。需要首先明确：**headers 标签本身不会在微应用固件中生成具名 C 常量**——标签在 codegen 期被解析为整数后即完成使命（Arduino 式引脚常量 `D2`/`A0`/`LED_BUILTIN` 由 §3.5 的 `wink_board_pins.h` 契约单独承担）。headers 通过以下三条链路被系统消费：

```text
          boards/<board>.json  "headers": { "P3.2": 26, "D2": 2, ... }
                                    │
      ┌─────────────────────────────┼─────────────────────────────┐
      ▼                             ▼                             ▼
① codegen 符号解析（构建期）   ② 前端画布与走线（UI 运行期）  ③ 仿真通道号约定（固件↔仿真器）
wink-app.json 中               embedded-frontend:            线性编号 port*8+bit 是
"$board.headers.P3.2" → 26     board-json-loader.ts:         PinArbiter 的电平投递地址；
写入 generated/device_tree.c   丝印规范名去重、排针几何、    mcs51_board_config.h 的
与 device-tree.json，仅存整数  走线 LEFT/RIGHT 吸附端点      MCS51_PIN_IDX_PORT/BIT 宏
                               均由 headers 派生             与此约定 MUST match
```

#### 1. 链路一：codegen 期符号解析（标签不进入固件）

为屏蔽不同 MCU 复杂的物理 GPIO 编号，应用配置支持通过 `$board.headers.<KEY>` 间接寻址：

```json
"headers": {
  "D2": 2,
  "GPIO2": 2,
  "A0": 36,
  "GPIO36": 36
}
```

* **多习惯友好**：习惯 Arduino 的开发者配置 `"pin": "D2"`，看芯片手册的工程师配置 `"pin": "GPIO2"`，两者均能通过 `headers` 准确解析到底层物理 GPIO 2；
* **换板零修改**：在 `wink-app.json` 中写 `"gpio_pin": "$board.headers.D18"`，换板时只需替换 `board.json`，无需修改 App 引脚配置；
* **转义规则**：若应用需要输出字面量 `"$board.headers.D18"`，使用 `"$$board.headers.D18"`，生成器会自动剥离第一个 `$` 符号；
* **解析产物是整数**：标签在 codegen 期完成查表替换，`generated/device_tree.c` 中只保留数值（如 `.trig_pin = 4`），符号名不进入固件。

#### 2. 链路二：前端画布丝印与走线端点

前端仿真器（`embedded-frontend/boards/board-json-loader.ts`）直接加载板级 JSON：

* `Object.values(headers)` 去重排序 → 板上可布线引脚集合（`gpioPins` / `routablePins`）；
* **规范丝印名**：多个别名指向同一 GPIO 时（如 `D2` 与 `GPIO2` 同为 2），按芯片家族优先级只保留一个画布标签——esp32 优先 `IO#/GPIO#`（避免与板载 SPI Flash 的 D0~D3 丝印混淆），avr 优先 `D#/A#`，mcs51 为 `P#.#`，pdk 为 `PA.#/PB.#`；
* **排针几何**：由引脚数派生双列排针的本地坐标与 LEFT/RIGHT 边，作为画布导线物理吸附的端点。

#### 3. 链路三：8051 端口位线性化与仿真通道号

8051 架构以 `P0` ~ `P3` 端口寻址，每个端口 8 位。`headers` 采用以下数学投影将其展平为 `0 ~ 31` 的线性连续索引：

$$\text{linear\_index} = (\text{port} \times 8) + \text{bit}$$

```json
"headers": {
  "P0.0": 0,  "P0.1": 1,  ..., "P0.7": 7,
  "P1.0": 8,  "P1.1": 9,  ..., "P1.7": 15,
  "P2.0": 16, "P2.1": 17, ..., "P2.7": 23,
  "P3.0": 24, "P3.1": 25, ..., "P3.7": 31
}
```

该投影同时是**固件侧与仿真侧的共同编号约定**：固件生成头 `mcs51_board_config.h` 提供 `MCS51_PIN_IDX_PORT(idx) = (idx >> 3) & 0x3` 与 `MCS51_PIN_IDX_BIT(idx) = idx & 0x7` 两个通用宏，其头注释明确要求 *"Pin index convention (MUST match boards/\<board\>.json headers)"*——board.json 是 SSOT，固件宏与 PinArbiter 通道号都是消费方，codegen 不为每个排针标签生成常量。

**实战闭环链路（以按键点灯 `mcs51_button_led` 为例）**：

1. 用户原生 C 源码：`sbit KEY = P3^2;`（端口 3，位 2）；
2. `headers` 数学投影：`P3.2` 对应通道 $(3 \times 8) + 2 = \mathbf{26}$；
3. `wink-app.json` / 设备树：按钮外设配置 `"gpio_pin": 26`（或 `"$board.headers.P3.2"`）；
4. UniSim 虚拟外设：当用户在界面按下按钮，虚拟引脚仲裁器（PinArbiter）将电平注入通道 26；
5. 仿真桥解码：mcs51 框架用 `idx >> 3` / `idx & 0x7` 将通道 26 分解回 (P3, 位 2)，写入虚拟 SFR 位；原生固件中 `sbit KEY = P3^2` 读取的正是该 SFR 位。

#### 4. 原生源码“零修改”的真实机制

* **Keil C51 `sbit` 源码**：`sbit KEY = P3^2;` 由真实 SFR 寄存器头在编译期解析——`wink_mcu.h` 按 MCU 宏路由到 `REGX52.H`（标准 8051）或 `REG_CMS8S78XX.H`（中微），`P3` 本身就是 SFR。**这条链路不依赖 headers 字典生成任何 C 常量**；headers 的职责是保证“配置/仿真侧的通道 26”与“固件侧的 `P3^2`”指向同一个物理点。
* **Arduino 引脚常量（`D2`、`A0`、`LED_BUILTIN`）**：由 codegen 从 `headers` 与 `onboard_devices` 生成板级常量头 `wink_board_pins.h`，Arduino 框架经 `Arduino.h` 中的 `__has_include` 守卫自动引入，生成规则见 §3.5。

---

### 3.4 `metadata.mcu` 芯片型号的系统级联动与消费链路

`mcu` 是整个平台连接构建脚本、底层硬件寄存器与前端仿真器的纽带。其继承规则为：**`wink-app.json` 显式声明优先；若未声明，则回退继承 `board.json` 的 `metadata.mcu`**。

它在四大子系统中发挥关键作用：

#### 1. CMake 构建系统：编译宏自动注入
在 `wink-micro-os/CMakeLists.txt` 中，构建系统解析到 `mcu` 后，会自动将其**转换为大写并将 `-` 替换为 `_`**，通过命令行注入预编译宏：
```cmake
string(TOUPPER "${_found_mcu}" _wink_app_mcu_upper)
string(REPLACE "-" "_" _wink_app_mcu_upper "${_wink_app_mcu_upper}")
add_compile_definitions(WINK_MCU_${_wink_app_mcu_upper}=1)
```
* 例如：`"mcu": "cms8s78xx"` $\to$ 注入 `-DWINK_MCU_CMS8S78XX=1`。

#### 2. `config_h.py`：头文件级宏保护
在生成 `wink_config.h` 时，也会生成带防重定义的宏开关：
```c
#ifndef WINK_MCU_ESP32
#define WINK_MCU_ESP32              (1)
#endif
```

#### 3. C/C++ 固件层：原厂寄存器统一门面 (`wink_mcu.h`)
在轻量 MCU（如 8051、Padauk）开发中，寄存器头文件千差万别（标准 8051 用 `REGX52.H`，中微用 `REG_CMS8S78XX.H`，应广用 `pfs154.h`）。
Wink Micro OS 设计了统一的门面头文件 `wink_mcu.h`（`wink-micro-os/runtime/include/wink_mcu.h`）：门面按此宏转发到各平台路由头，51 家族由 `frameworks/mcs51/include/mcs51_family_route.h` 逐家族路由（Stage7 收尾后 Keil 寄存器包含只存在于沙箱路由头，不再进入可移植门面）：
```c
// wink-micro-os/runtime/include/wink_mcu.h（可移植门面，节选）
#if defined(WINK_MCU_CMS8S78XX) || defined(WINK_MCU_AT89C52) || ...
    #include "mcs51_family_route.h"   // 由 frameworks/mcs51/ 提供

// wink-micro-os/frameworks/mcs51/include/mcs51_family_route.h（沙箱路由，节选）
#if defined(WINK_MCU_CMS8S78XX)
    #include "REG_CMS8S78XX.H"       // 中微特有 SFR 寄存器与 ADC 扩展定义
#elif defined(WINK_MCU_AT89C52) || defined(WINK_MCU_MCS51)
    #include "REGX52.H"              // 标准 8051 经典 SFR
#elif ...
```
**开发者编写应用时只需统一 `#include "wink_mcu.h"`**，换芯片时只需在配置中修改 `mcu`，无需更改业务代码中的头文件包含。

#### 4. 前端与 Web 仿真器：仿真架构等级推导 (ADR-0064)
`runtime_device_tree.py` 将 `mcu` 发射到 `device_tree.json` 供前端推导（`chip-deducer.ts`），决定仿真器架构及画布外观：
* **Tier 1 (AI-Native OS, 如 `esp32`, `stm32`)**：全功能 WASM 运行时，模拟调度器与多任务；
* **Tier 2 (C51 Proxy, 如 `at89c52`, `cms8s78xx`)**：8051 固件指令拦截与 SFR 虚拟仿真网关；
* **Tier 3 (1:1 ISA VM, 如 `pfs154`, `pms150c`)**：周期精确级硬件指令集虚拟机。

---

### 3.5 `wink_board_pins.h` 板级引脚常量生成契约（Arduino）

未修改的 Arduino Sketch 会直接使用 SDK 内置引脚常量（`D2`、`A0`、`LED_BUILTIN` 等）。为让这类源码在 Wink 仿真环境中零修改编译，codegen 在每个 app 的构建树 `generated/` 下发射 `wink_board_pins.h`，由 `frameworks/arduino/include/Arduino.h` 通过 `__has_include` 守卫自动引入（头文件不存在时静默跳过，框架保持自包含）。

**生成规则**：

1. **仅发射标识符安全且符合引脚命名范式的标签**：白名单正则 `^(D|A|GPIO|IO)\d+$`（PDK 家族为 `^P[AB]\d+$`）。带点号的 8051/PDK 端口位标签（`P3.2`、`PA.0`）一律跳过——它们由原生 SFR 头（`REGX52.H`、`pfs154.h`）服务；`EN`、`VP`、`3V3` 等特殊功能/电源名同样跳过，避免与上游核心或业务代码的符号冲突；
2. **每个宏带 `#ifndef` 守卫**：用户 Sketch 内自定义的同名常量优先（如 `arduino_blink_demo` 中手写的 `#define LED_BUILTIN 2` 不被覆盖，同值重定义亦合法）；
3. **`LED_BUILTIN` 从 `onboard_devices` 推导，而非 headers 标签**：取名为 `status_led` / `led_builtin` 的 `type: "led"` 板载设备，否则取第一个板载 LED 设备的 `gpio_pin`。例如 `esp32_devkitc_v4` 推导出 GPIO 2、`arduino_uno_r3` 推导出 D13，与上游 Arduino 核心约定一致；
4. **职责边界**：该头只承载“手写原生源码”使用的引脚常量宏；设备树外设引脚仍走链路一（`$board.headers.<KEY>` 在 codegen 期解析为整数写入 `device_tree.c`），两条链路不重叠。

```c
/* AUTO-GENERATED by wink-tools codegen from boards/<board>.json. DO NOT EDIT. */
#ifndef WINK_BOARD_PINS_H
#define WINK_BOARD_PINS_H

#ifndef D2
#define D2              (2)
#endif
#ifndef A0
#define A0              (36)
#endif
#ifndef LED_BUILTIN
#define LED_BUILTIN     (2)   /* onboard_devices.status_led.gpio_pin */
#endif

#endif /* WINK_BOARD_PINS_H */
```

---

## 4. 工具链统一加载机制 (`loader.py`)

在 Phase 2 重构后，工具链彻底弃用了硬编码平铺路径，全部收敛至集中式模块 `tools.codegen.boards.loader`（[`loader.py`](./loader.py)）。

### 4.1 检索支持的形式
当 `wink-app.json` 声明 `"board"` 时，支持以下 2 种规范形式：
1. **全局唯一板卡名（推荐）**：如 `"board": "stc89c52_devboard"` 或 `"board": "esp32_devkitc_v4"`；
2. **带家族的命名空间名**：如 `"board": "mcs51/stc89c52_devboard"` 或 `"board": "esp32/esp32_devkitc_v4"`。

### 4.2 目录搜索优先级
加载器在内存建立高速哈希索引，按以下顺序检索候选根目录下的 `boards/<family>/<board_name>.json`：
1. **App 局部优先**：`<app_dir>/boards/*/<board_name>.json` 或 `<app_dir>/<board_name>.json`；
2. **SDK / 外部源码树**：环境变量 `$WINK_AI_EMBEDDED_DIR/.../boards/*/<board_name>.json`；
3. **工具链内置默认**：`packages/wink-tools/tools/codegen/boards/*/<board_name>.json`。

---

## 5. 新增开发板步骤

1. 在对应的家族目录下（如 `boards/esp32/`）新建 `<board_name>.json`；若是全新芯片架构，先创建对应的 `<family>/` 目录；
2. 参照已有板卡定义填入 `metadata`（特别是确切的 `board_name`、`mcu` 与 `family`）、`onboard_devices` 与 `headers`；
3. 若芯片带模数转换（ADC），完整定义 `adc.pins` 映射及 `wifi_conflict` 标志；
4. 若芯片带硬件总线（I2C/SPI），定义 `buses` 的默认引脚；
5. 在任意 App 的 `wink-app.json` 中配置 `"board": "<board_name>"`，运行生成验证：
   ```bash
   winkcli gen app --app <your_app_name>
   ```
