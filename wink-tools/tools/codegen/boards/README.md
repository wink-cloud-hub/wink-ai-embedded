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
│   └── stc89c52_devboard.json         # 宏晶 STC89C52 教学开发板 (标准 P0-P3 排针 + ADC0832 扩展)
└── pdk/
    └── padauk_pfs154_devboard.json    # 应广 PFS154 参考开发板 (PA/PB 双端口)
```

| 规范路径 | 目标 MCU | 架构 / 内核 | 典型硬件特性 |
| :--- | :--- | :--- | :--- |
| [`avr/arduino_uno_r3.json`](./avr/arduino_uno_r3.json) | ATmega328P | AVR8 | 经典 Arduino 排针 (D0-D13, A0-A5)、5V/10 位 ADC、板载 D13 LED |
| [`esp32/esp32_devkitc_v4.json`](./esp32/esp32_devkitc_v4.json) | ESP32 | Xtensa-LX6 | Wi-Fi/BLE 射频、ADC1/ADC2 双单元分区、I2C/SPI 总线 |
| [`mcs51/stc89c52_devboard.json`](./mcs51/stc89c52_devboard.json) | STC89C52 | 8051 / 8052 | P0-P3 线性化 32 引脚映射、ADC0832 外部采样支持 |
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
| **`headers`** | `object` | **是** | 外部排针丝印/别名到物理引脚或线性端口编号的映射字典。**核心价值在于让存量 C/C++ 业务代码（如 Keil C51 原生语法、Arduino 内置常量）一字不改即可在 Wink 仿真环境中直接运行**，详见 [§3.3](#33-headers-排针映射与-8051-端口位线性化)。 |
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

### 3.3 `headers` 排针映射与 8051 端口位线性化

`headers` 是 Wink 板级架构中最具特色的桥梁设计，其核心理念在于实现**“业务源码与仿真平台两端皆无需迁就”的双重价值闭环**：

```text
┌────────────────────────────────────────────────────────────────────────┐
│ 1. 业务源码层 (Zero-Modification Code)                                   │
│    • Keil C51 原生源码:  sbit KEY = P3^2;  sbit LED = P1^0; (一字不改)   │
│    • Arduino 原生 Sketch: pinMode(LED_BUILTIN, OUTPUT); 或 D2, A0 内置常量│
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ headers 桥接映射
┌───────────────────────────────────▼────────────────────────────────────┐
│ 2. 仿真与配置层 (Unified Bridge)                                         │
│    • wink-app.json / 设备树: "gpio_pin": 26 (或 "$board.headers.P3.2")  │
│    • UniSim WASM 仿真器: PinArbiter 高速投递电平到 8051 虚拟 SFR 寄存器位    │
│    • 前端可视化电路: 画布根据引脚通道生成端点，实现导线精准物理吸附       │
└────────────────────────────────────────────────────────────────────────┘
```

#### 1. 核心价值一：存量 C/C++ 业务代码“一字不改”（Zero-Modification Code）
在高校单片机教学、教科书经典实验与开源社区中，存在大量存量代码：
* **标准 8051 代码**：深度依赖 Keil C51 的特有语法 `sbit` 进行端口位操作（如 `sbit KEY = P3^2;`）；
* **Arduino 生态代码**：直接使用 SDK 内置的引脚常量（如 `A0`、`D2` 或 `LED_BUILTIN`）。

**Wink 的设计原则是：绝不要求开发者为了适配仿真平台而修改原生业务逻辑。**  
通过将板卡的排针标号（如 `P3.2`、`D2`、`A0`）收录在 `headers` 字典中，微应用工程可以直接无缝编译原生单片机源码（如 `wink-micro-app/mcs51_button_led/button_led.c`），底层自动完成对接。

#### 2. 核心价值二：跨架构统一映射桥梁（Unified Simulation Bridge）
不同单片机体系的引脚寻址模式天然割裂：
* 8051 采用二维“端口+位”寻址（`P0.0 ~ P3.7`）；
* ESP32 采用一维数字编号（`GPIO 0 ~ 39`）；
* Arduino 采用功能复合标号（`D0 ~ D13`，`A0 ~ A5`）。

`headers` 充当了异构硬件在 Wink 系统中的**统一标尺与符号查表器**：

##### A. 逻辑别名解耦与多习惯兼容（Arduino / ESP-IDF 通吃）
为屏蔽不同 MCU 复杂的物理 GPIO 编号，支持在应用配置中通过 `$board.headers.<KEY>` 间接寻址：
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
* **转义规则**：若应用需要输出字面量 `"$board.headers.D18"`，使用 `"$$board.headers.D18"`，生成器会自动剥离第一个 `$` 符号。

##### B. 8051 端口位的线性索引投影（以 `stc89c52_devboard.json` 为例）
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
* **实战闭环链路（以按键点灯 `mcs51_button_led` 为例）**：
  1. 用户原生 C 源码：`sbit KEY = P3^2;`（端口 3，位 2）；
  2. `headers` 数学投影：`P3.2` 对应通道 $(3 \times 8) + 2 = \mathbf{26}$；
  3. `wink-app.json` / 设备树：按钮外设直接配置 `"gpio_pin": 26`（或 `"$board.headers.P3.2"`）；
  4. UniSim 虚拟外设：当用户在界面按下按钮，虚拟引脚仲裁器（PinArbiter）将电平精准注入通道 26；
  5. 固件生成期解码：`mcs51_board_config.py` 通过极低开销的位运算直接生成操作 SFR 寄存器的底层指令：
     * $\text{port} = (\text{linear\_index} \gg 3) \ \& \ 0\text{x}3 \implies 3$（对应 `P3`）
     * $\text{bit} = \text{linear\_index} \ \& \ 0\text{x}7 \implies 2$（对应位 2）
  从而达成从界面交互、设备树配置到原生 C 语言执行的全链路零摩擦直通！

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
Wink Micro OS 设计了统一的门面头文件 `wink_mcu.h`（`frameworks/mcs51/include/wink_mcu.h`），依据此宏进行安全路由：
```c
// frameworks/mcs51/include/wink_mcu.h
#if defined(WINK_MCU_CMS8S78XX) || defined(CMS8S78XX)
    #include "REG_CMS8S78XX.H"   // 中微特有 SFR 寄存器与 ADC 扩展定义
#elif defined(WINK_MCU_AT89C52) || defined(WINK_MCU_MCS51)
    #include "REGX52.H"          // 标准 8051 经典 SFR
#elif defined(WINK_MCU_PFS154) || defined(PFS154)
    #include <pfs154.h>          // 应广 PDK14 寄存器
#elif defined(WINK_MCU_PMS150C) || defined(PMS150C)
    #include <pms150c.h>         // 应广 PDK13 寄存器
#else
    #error "[wink_mcu.h] No valid MCU target defined! Please specify 'mcu' in wink-app.json or board.json"
#endif
```
**开发者编写应用时只需统一 `#include "wink_mcu.h"`**，换芯片时只需在配置中修改 `mcu`，无需更改业务代码中的头文件包含。

#### 4. 前端与 Web 仿真器：仿真架构等级推导 (ADR-0064)
`runtime_device_tree.py` 将 `mcu` 发射到 `device_tree.json` 供前端推导（`chip-deducer.ts`），决定仿真器架构及画布外观：
* **Tier 1 (AI-Native OS, 如 `esp32`, `stm32`)**：全功能 WASM 运行时，模拟调度器与多任务；
* **Tier 2 (C51 Proxy, 如 `at89c52`, `cms8s78xx`)**：8051 固件指令拦截与 SFR 虚拟仿真网关；
* **Tier 3 (1:1 ISA VM, 如 `pfs154`, `pms150c`)**：周期精确级硬件指令集虚拟机。

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
