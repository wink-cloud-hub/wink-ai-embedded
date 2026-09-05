# Ⅱa. 核心仿真机制原理（Simulation Mechanisms & Architecture Principles）

本文档阐述 Wink-AI 统一仿真引擎（UniSim）在保证嵌入式行为级高保真仿真时的底层物理与计算模型原理。

---

## 1. 核心机制全景

为了达成与 ESP32 等微控制器真机在时序、中断、外设与故障注入上的高度行为一致性，仿真系统基于以下五大核心计算模型运转：

```text
┌─────────────────────────────────────────────────────────────────────────┐
│                      WinkMicroOS Application (C Code)                   │
├─────────────────────────────────────────────────────────────────────────┤
│                   Platform Abstraction Layer (PAL - wasm)               │
├─────────────────────────────────────────────────────────────────────────┤
│                   C/Wasm Bridge ABI (wasm_bridge.h)                     │
└────────────────────────────────────┬────────────────────────────────────┘
                                     │ (ABI Import/Export)
┌────────────────────────────────────▼────────────────────────────────────┐
│                       UniSim Virtual Runtime Model                      │
│                                                                         │
│  [1. 确定性虚拟时钟]  ──► 64-bit BigInt μs 级无漂移单调时基               │
│  [2. 协作式执行沙箱]  ──► Asyncify 协程挂起与状态快照恢复                 │
│  [3. 四值逻辑引脚仲裁] ──► 0 / 1 / High-Z / Contention 驱动强度仲裁     │
│  [4. 异步外设总线]    ──► Channel 1 (GPIO), 1b (PWM), 2 (I2C/SPI), 3 (ADC)│
│  [5. 物理退化域]      ──► 热敏、欠压、线损与故障注入模型                  │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. 五大支柱机制原理

### 2.1 确定性虚拟时钟（Deterministic Virtual Clock）
- **微秒级整数时基**：仿真时间完全脱离宿主浏览器的真实挂钟时间（Wall-Clock），使用 64 位单调递增微秒计数器推进。
- **离散事件步进**：仿真系统以确定性步长（Time-Slice）分发时钟 Tick。无论宿主机器 CPU 性能高低，给定相同的输入序列，虚拟时钟与状态轨迹完全可重现。

### 2.2 协作式单核并发与协程挂起（Cooperative Sandbox & Suspension）
- **单虚拟核心模型**：严格遵循 MCU 的单核执行特征，保证在无抢占点时 C 代码原子执行。
- **Asyncify 零开销阻塞**：当嵌入式代码调用 `pal_delay_ms` 或等待外设 Ready 时，Wasm 执行栈通过 Asyncify 机制安全挂起，交出控制权给仿真总线，待虚拟时基对齐后再行恢复。

### 2.3 四值逻辑引脚仲裁（4-Value Pin Arbiter）
真实物理世界的引脚状态不仅有高低电平：
* `0`：低电平（Low）
* `1`：高电平（High）
* `Z`：高阻态（High-Impedance，开漏输出未拉低或输入浮空）
* `X`：冲突态（Contention，多个推挽输出强冲突）
引脚仲裁器根据多个驱动源的强度（Strong, Pull-Up, High-Z）计算合成电平，精确模拟浮空、上拉电阻与短路故障。

### 2.4 通道化外设数据面（Channel Routing）
- **Channel 1 (GPIO)**：双向逻辑电平与边沿变化事件。
- **Channel 1b (PWM)**：脉冲宽度调制占空比与基础频率捕获。
- **Channel 2 (Bus)**：I2C（SCL/SDA 时序与从机 ACK/NACK）及 SPI 全双工字节流。
- **Channel 3 (Analog)**：ADC 采样与毫伏电压连续映射。
- **Channel 4 (Block/Buffer)**：高速显示（如 OLED Framebuffer）与块设备传输。

### 2.5 故障注入与物理退化（Fault & Degradation）
仿真引擎支持模拟物理世界的非理想状态：
* **电压漂移与欠压重启（Brownout）**
* **时钟抖动与频偏（Clock Jitter）**
* **传感器接触不良与随机噪点**

---

## 3. ABI 契约规范（SSOT）

C 运行时与仿真宿主的所有交互严格受限于标准 ABI 头文件：
* [`wink-micro-os/targets/wasm/wasm_bridge.h`](../../../wink-micro-os/targets/wasm/wasm_bridge.h)

该头文件定义了所有状态缓冲区、内存布局以及 `js_*` 导入函数与 `pal_wasm_*` 导出函数，构成了高保真仿真的单一事实来源（SSOT）。
