# UniSim 4.0 外设插件开发者手册 (Peripheral Plugin Developer Guide)

> **版本**：v4.0 (Aligned with UniSim 4.0 & @wink-ai/unisim-ui)  
> **适用对象**：外设插件开发者、硬件仿真开发者、前端组件开发者  
> **源码位置**：`builtin/{type}/{version}/` (当前仓 `wink-plugin-peripherals`)

欢迎来到 UniSim 4.0 外设插件开发文档中心！本目录为编写、调试与发布自包含外设插件（Pre-bundled Peripheral Plugins）提供集中、权威的操作手册。

---

## 🏛️ 架构设计原则与愿景

UniSim 4.0 的外设系统遵循 **“微内核 + 独立自包含插件 (Microkernel + Self-Contained Plugins)”** 架构原则：

1. **Web-First 跨端一致**：外设包含独立的仿真逻辑 (`simulation.js`) 与前端视图控件 (`frontend.js` + `wink-ai.css`)，在 Web 网页端与 Tauri 桌面端 100% 保持行为与视效一致。
2. **完全解耦与独立仓库维护**：外设源码独立托管在 `wink-plugin-peripherals` 仓库，具备自包含的依赖环境（基于 `@wink-ai/unisim` 与 `@wink-ai/unisim-ui`），脱离主框架依赖包袱，支持单外设独立开发、编译与发布。
3. **分层物理通道与高保真 (Axis A~F)**：基于 UniSim 4.0 物理通道分类（GPIO 波形、PWM 定时调制、I2C/SPI/UART 总线、模拟量 ADC/DAC、Buffer/DMA 帧载荷），提供微秒级确定性 (`VirtualClock`)、四态电气仲裁 (`PinArbiter`) 与 C 固件 WASM ABI 紧密对齐。
4. **统一 UI 契约与双视图模型**：前端定义基于 `@wink-ai/unisim-ui` 的 `definePeripheral`，提供电路画布视图 (`CanvasGlyph`) 与虚拟现实视图 (`WorldWidget`) 双呈现模式，并通过 `pinsOverlay` 精确支持前端布线引擎的自动吸附。

---

## 🏛️ 平台核心架构指南导航 (Core Architecture References)

如需深入理解 UniSim 4.0 内核级架构、WASM C ABI 桥接、时钟调度与动态加载，请参阅平台核心文档（位于 `wink-ai/packages/unisim/docs/architecture/`）：

- 📐 **[UniSim 4.0 完整架构文档](../../wink-ai/packages/unisim/docs/architecture/overview.md)**：分层规范、SSOT 代码生成流与设计决策全景。
- 🎛️ **[外设通道与 C ABI 双向对接指南](../../wink-ai/packages/unisim/docs/architecture/hardware-channel-c-abi-guide.md)**：五大硬件通道与 `wink-micro-os` WASM C ABI 映射。
- ⏱️ **[外设生命周期与事件系统架构指南](../../wink-ai/packages/unisim/docs/architecture/plugin-lifecycle-and-events-guide.md)**：12 大生命周期 Hook、微秒级时钟调度与世代令牌防抖。
- 🎨 **[前端 UI 渲染与画布数据绑定指南](../../wink-ai/packages/unisim/docs/architecture/frontend-ui-and-canvas-guide.md)**：2D 画布外观、Web Component 集成与 `ui.canvasProps` 映射。
- 🎚️ **[外设事件与控制面板映射规范](../../wink-ai/packages/unisim/docs/architecture/peripheral-events-and-control-mapping-guide.md)**：语义事件 SSOT、控制面板推导与 `mapEventToMethod` 映射规则。
- 📦 **[外置插件动态加载架构](../../wink-ai/packages/unisim/docs/architecture/external-plugin-loading.md)**：Hono 后端多版本分发、前端动态 import 与 Hydration 机制。

---

## 📚 外设开发分册导航 (Documentation Navigation)

根据您的开发阶段，选择对应的指南文档：

| 顺序 | 文档名称 | 目标与主要内容 | 适合谁看 |
| :---: | :--- | :--- | :--- |
| **01** | **[01-PERIPHERAL_QUICKSTART.md](./01-PERIPHERAL_QUICKSTART.md)** | **新手 15 分钟快速上手指南**<br>从脚手架一键生成、Wokwi 元件选型、写仿真/UI 到本地自检。 | 第一次开发外设的开发者 |
| **02** | **[02-MANIFEST_AND_METADATA_SPEC.md](./02-MANIFEST_AND_METADATA_SPEC.md)** | **Manifest 元数据与引脚规范**<br>PinType 快捷基类、pinsOverlay 坐标规范与 `mapEventToMethod` 契约。 | 需定义硬件引脚/事件的开发者 |
| **03** | **[03-HIGH_FIDELITY_WAVEFORM_GUIDE.md](./03-HIGH_FIDELITY_WAVEFORM_GUIDE.md)** | **高保真波形与全通道 API 指南**<br>`injectWaveform` 波形注入、世代令牌、五大通道 API 与能力降级。 | 编写高精度/微秒时序外设的开发者 |
| **04** | **[04-BUILD_AND_PACKAGING_TOOLCHAIN.md](./04-BUILD_AND_PACKAGING_TOOLCHAIN.md)** | **Vite 构建工具链与动态加载**<br>`@wink-ai/unisim-ui/vite` 预设、构建产物规范 (`frontend.js`) 与三级扫描路径。 | 需编译打 bundle 包/部署的开发者 |
| **05** | **[05-COMMON_PITFALLS_AND_FAQ.md](./05-COMMON_PITFALLS_AND_FAQ.md)** | **常见踩坑避坑指南与 FAQ**<br>方法命名陷阱、时间戳规范、事件映射、样式隔离等 9 大避坑点。 | 所有外设开发者 (建议必读) |
| **06** | **[06-PHYSICS_AND_HEADLESS_TESTING.md](./06-PHYSICS_AND_HEADLESS_TESTING.md)** | **物理算法与无头单测验证指南**<br>`bun test` 纯函数算法单测、仿真插件状态机隔离测试与边缘用例。 | 需保证物理精度与自动回归的开发者 |

---

## 🔌 官方内置外设参考模板 (Builtin Templates)

在开发新外设时，建议优先参考功能相近的官方内置外设源码：

| 设备名称 (Type) | 通道分类 | 驱动方式 / 交互协议 | 源码目录 |
| :--- | :--- | :--- | :--- |
| **`led`** | 通道 1 (数字 GPIO) | MCU 输出拉高/拉低点亮 | `builtin/led/1.0.0/` |
| **`button`** | 通道 1 (数字 GPIO) | 手势/UI 刺激输入拉高/拉低 | `builtin/button/1.0.0/` |
| **`ultrasonic`** | 通道 1 (高保真波形) | TRIG 触发，`injectWaveform` / `pal_wasm_push_pin_event` ECHO 回波 | `builtin/ultrasonic/1.0.0/` |
| **`rc_servo`** | 通道 1b (PWM 定时调制) | 占空比驱动角度执行器 | `builtin/rc_servo/1.0.0/` |
| **`mono_oled`** | 通道 2 (I2C 总线) + 通道 4 | I2C 从设备 (`0x3C`) 帧缓冲显示屏 | `builtin/mono_oled/1.0.0/` |
| **`pcf8574`** | 通道 2 (I2C 总线) | I2C GPIO 8位扩展芯片 | `builtin/pcf8574/1.0.0/` |
| **`analog_knob`** | 通道 3 (模拟量 ADC) | 旋钮输出 `[0.0, 1.0]` 归一化模拟电压 | `builtin/analog_knob/1.0.0/` |
| **`ws2812_strip`** | 通道 4 (Buffer Payload) | 单总线 RGB 彩灯串 Zero-copy 帧 Payload | `builtin/ws2812_strip/1.0.0/` |
| **`gps`** | 通道 2 (UART 串口) | 串口输出 NMEA 物理地理位置数据帧 | `builtin/gps/1.0.0/` |

---

## 🚀 快速开始

请点击 **[01-PERIPHERAL_QUICKSTART.md](./01-PERIPHERAL_QUICKSTART.md)** 开始您的第一个外设插件开发！
