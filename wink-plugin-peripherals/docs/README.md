# UniSim 3.0 外设插件开发者手册 (Peripheral Plugin Developer Guide)

> **版本**：v3.0  
> **适用对象**：外设插件开发者、硬件仿真开发者、前端组件开发者  
> **源码位置**：`peripherals/builtin/{type}/1.0.0/`

欢迎来到 UniSim 3.0 外设插件开发文档中心！本目录为编写、调试与发布自包含外设插件（Pre-bundled Peripheral Plugins）提供集中、权威的操作手册。

---

## 🏛️ 架构设计原则与愿景

UniSim 3.0 的外设系统遵循 **“微内核 + 独立自包含插件 (Microkernel + Self-Contained Plugins)”** 架构原则：

1. **Web-First 跨端一致**：外设包含独立的仿真逻辑 (`simulation.js`) 与前端视图控件 (`index.js`)，在 Web 网页端与 Tauri 桌面端 100% 保持行为与视效一致。
2. **完全解耦与独立维护**：外设源码收影在 `peripherals/` 根目录下，脱离主 `packages/*` workspace 约束。第三方或官方开发者可对单外设独立 Clone、独立编译与版本发布。
3. **分层物理通道与高保真**：基于 UniSim 3.1 物理源通道分类（GPIO 波形、PWM 占空调制、总线协议、模拟量 ADC/DAC、Buffer 载荷），提供微秒级确定性 (`VirtualClock`) 与 C 驱动 SSOT 状态同步。详细规范见 [UniSim 3.1 外设全通道物理仿真与强类型基础设施统一设计规范](../../packages/unisim/docs/design/unified-peripheral-channel-architecture.md)。

---

## 🏛️ 物理通道架构 SSOT 指南 (Core Architecture SSOT)

如需深入理解内核级外设通道抽象、`PluginContext` 强类型 API、Binding Manifest 双层 Hash 校验及降级铁律，请参阅 UniSim 核心规范：

- 📘 **[UniSim 3.1 外设全通道物理仿真与强类型基础设施统一设计规范](../../packages/unisim/docs/design/unified-peripheral-channel-architecture.md)**
- ⏱️ **[UniSim 3.1 高保真波形注入与离散事件仿真 (DES) 架构演进规范](../../packages/unisim/docs/design/high-fidelity-waveform-and-des-evolution.md)**

---

## 📚 文档导航与阅读路线 (Documentation Navigation)

根据您的开发阶段，选择对应的指南文档：

|  顺序  | 文档名称                                                                         | 目标与主要内容                                                                               | 适合谁看                        |
| :----: | :------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------- | :------------------------------ |
| **01** | **[01-PERIPHERAL_QUICKSTART.md](./01-PERIPHERAL_QUICKSTART.md)**                 | **新手 15 分钟快速上手指南**<br>从选模板、建工程、写逻辑到运行自检的全流程。                 | 第一次开发外设的开发者          |
| **02** | **[02-MANIFEST_AND_METADATA_SPEC.md](./02-MANIFEST_AND_METADATA_SPEC.md)**       | **Manifest 元数据与引脚规范**<br>引脚 PinType 全集、属性、状态通道与事件注册契约。           | 需定义硬件引脚/事件的开发者     |
| **03** | **[03-HIGH_FIDELITY_WAVEFORM_GUIDE.md](./03-HIGH_FIDELITY_WAVEFORM_GUIDE.md)**   | **高保真波形与四大通道 API 指南**<br>`injectWaveform` 波形注入、世代令牌、总线 API 与降级。  | 编写高精度/微秒时序外设的开发者 |
| **04** | **[04-BUILD_AND_PACKAGING_TOOLCHAIN.md](./04-BUILD_AND_PACKAGING_TOOLCHAIN.md)** | **Vite 构建工具链与动态加载**<br>双 Vite 编译流 (`vite.config.{sim,ui}.ts`) 与三级扫描路径。 | 需编译打 bundle 包/部署的开发者 |
| **05** | **[05-COMMON_PITFALLS_AND_FAQ.md](./05-COMMON_PITFALLS_AND_FAQ.md)**             | **常见踩坑避坑指南与 FAQ**<br>8 大典型踩坑解析（时间戳、发布 vs 注入、死锁等）。             | 所有外设开发者 (建议必读)       |

---

## 🔌 官方内置外设参考模板 (Builtin Templates)

在开发新外设时，建议优先参考功能相近的官方内置外设源码：

| 设备名称 (Type)    | 通道分类                | 驱动方式 / 交互协议                          | 源码目录                                  |
| :----------------- | :---------------------- | :------------------------------------------- | :---------------------------------------- |
| **`led`**          | 通道 1 (数字 GPIO)      | MCU 输出拉高/拉低点亮                        | `peripherals/builtin/led/1.0.0/`          |
| **`button`**       | 通道 1 (数字 GPIO)      | 手势/UI 刺激输入拉高/拉低                    | `peripherals/builtin/button/1.0.0/`       |
| **`ultrasonic`**   | 通道 1 (波形注入)       | TRIG 触发，高保真 `injectWaveform` ECHO 回波 | `peripherals/builtin/ultrasonic/1.0.0/`   |
| **`rc_servo`**     | 通道 1 (PWM)            | 占空比驱动角度执行器                         | `peripherals/builtin/rc_servo/1.0.0/`     |
| **`mono_oled`**    | 通道 2 (I2C 总线)       | I2C 从设备 (`0x3C`) 帧缓冲显示屏             | `peripherals/builtin/mono_oled/1.0.0/`    |
| **`pcf8574`**      | 通道 2 (I2C 总线)       | I2C GPIO 8位扩展芯片                         | `peripherals/builtin/pcf8574/1.0.0/`      |
| **`analog_knob`**  | 通道 3 (模拟量 ADC)     | 旋钮输出 `[0.0, 1.0]` 归一化模拟电压         | `peripherals/builtin/analog_knob/1.0.0/`  |
| **`ws2812_strip`** | 通道 4 (Buffer Payload) | 单总线 RGB 彩灯串 Zero-copy 帧 Payload       | `peripherals/builtin/ws2812_strip/1.0.0/` |
| **`gps`**          | 通道 2 (UART)           | 串口输出 NMEA 物理地理位置数据帧             | `peripherals/builtin/gps/1.0.0/`          |

---

## 🚀 快速开始

请点击 **[01-PERIPHERAL_QUICKSTART.md](./01-PERIPHERAL_QUICKSTART.md)** 开始您的第一个外设插件开发！
