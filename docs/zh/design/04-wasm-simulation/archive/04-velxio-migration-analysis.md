# 4.4 Velxio 仿真技术栈迁移与适配分析

为了加速低代码 AI 仿真平台的开发，我们可以深度参考与重用开源仿真平台 `velxio`（以及 Wokwi）的部分设计。然而，由于本平台采用的是**API/组件翻译级 Wasm 仿真**（即 WinkMicroOS 的 DAL+PAL 桥接），而 `velxio` 采用的是**指令级/寄存器级硬件仿真**（运行 CPU 模拟器或 QEMU），我们需要对相关技术进行细致的甄别，分类处理。

---

## 1. 可以直接复用的“零开销”模块

`velxio` 中有关 UI 器件展现、引脚连线求交绘制以及基于 TypeScript 开发的虚拟总线设备，可以在本平台中直接剥离复用：

### 1.1 Wokwi-elements 虚拟器件库集成
*   **Velxio 做法**：直接引用 `@wokwi/elements` 依赖，以 Web Components 形式渲染 LED、按钮、LCD1602、7段数码管、键盘、电位器等器件。
*   **迁移方案**：**100% 继承**。在 Vue 3 前端项目中，我们同样可以通过安装 `@wokwi/elements` 依赖，并在 Vite 编译选项中放行特殊的 Web Components 标签渲染：
    ```typescript
    // vite.config.ts 示例
    import { defineConfig } from 'vite';
    import vue from '@vitejs/plugin-vue';

    export default defineConfig({
      plugins: [
        vue({
          template: {
            compilerOptions: {
              // 告知 Vue 编译器，所有以 'wokwi-' 开头的标签为原生 Web Components
              isCustomElement: (tag) => tag.startsWith('wokwi-')
            }
          }
        })
      ]
    });
    ```

### 1.2 SVG 导线正交路由算法 (Orthogonal Routing)
*   **Velxio 做法**：使用 Union-Find (并查集) 管理电路中的物理节点电性连接关系，并采用正交算法绘制 L 型或多折折线型的 SVG 导线，避免导线无序斜插。
*   **迁移方案**：**直接移植算法**。提取其正交折点求解逻辑（如通过 `v[N]` 和 `h[N]` 控制路径），在 Vue 3 中将其重构为通过 Pinia 进行数据绑定、SVG 进行模板化自适应渲染。

### 1.3 虚拟 I2C / SPI 外设协议状态机
*   **Velxio 做法**：用 TypeScript 实现了大量标准的虚拟外设状态机：
    *   **VirtualSSD1306**：接收到 I2C 字节后，解析显存指令并在 HTML5 Canvas 上刷新像素。
    *   **VirtualDS1307 / DS3231**：模拟 RTC 芯片的寄存器，根据读写指令返回当前的系统时钟。
    *   **VirtualPCF8574**：模拟 8 位 I2C 扩展芯片的输入输出。
*   **迁移方案**：**直接复用**。这些类的逻辑全部基于标准的 TypeScript `I2CDevice` 接口实现，与底层的 CPU 模拟器是解耦的。我们可以原封不动地移植它们，并挂载到我们的 I2CBus 虚拟管理引擎中。

---

## 2. 必须重写与抛弃的模块

由于仿真底层核心由 **CPU 指令级模拟（Emulator）** 演进为 **API层 C源码直接编译执行（Wasm）**，以下高度耦合硬件芯片内部结构的模块**不可使用**，必须自主开发：

### 2.1 抛弃所有的 CPU 模拟器与 QEMU
*   **Velxio 原有做法**：
    *   对于 AVR 单片机，在 JS 中执行寄存器级模拟器 `avr8js`。
    *   对于 RP2040，使用 ARM 寄存器模拟器 `rp2040js`。
    *   对于 ESP32 / STM32，必须在后端云服务器启动重量级的 QEMU 仿真进程，并通过 WebSocket 实时流式传输引脚波形状态。
*   **本平台做法**：**彻底抛弃**。
    *   我们不需要上述任何底层物理芯片的指令级模拟器。
    *   我们的仿真核心（Wasm-Core）就是**将用户生成的 C 代码直接编译成 `wasm32-unknown-emscripten`**。
    *   所有的板子（无论是 ESP32、STM32 还是 RP2040）在网页仿真时都**运行在同一个本地 Web Worker 的 Wasm 沙箱中**。它们调用的 PAL 接口直接静态绑定路由到 JS 桥接函数上。
    *   **核心优势**：免去了部署云端重量级 QEMU 仿真服务集群的巨大开销，真正做到 100% 纯客户端运行高保真行为级仿真，消除了网络延迟，服务器开销降低至零。

### 2.2 状态管理与 UI 库升级
*   **状态管理**：Velxio 使用 React Zustand 管理画布状态。我们将其重构为 Vue 生态原生的 **Pinia**，提供更好的响应式双向绑定和组件内嵌。
*   **UI 布局**：Velxio 采用 TailwindCSS 手写侧边栏配置面板。我们直接利用 **Element Plus** 提供的 `<el-tabs>`、`<el-slider>`、`<el-form>` 等组件进行重新排版布局，提高属性配置的易用性与一致性。

---

## 3. 技术栈迁移对比一览表

| 仿真技术模块 | Velxio 的实现方式 | 本平台的实现方式 | 迁移适配策略 |
| :--- | :--- | :--- | :--- |
| **画布拖拽与渲染** | React + SVG Wires | Vue 3 + SVG Wires | 移植正交坐标计算算法，改用 Vue 模板渲染 |
| **外设图形库** | `@wokwi/elements` (Web Components) | `@wokwi/elements` | **完全照搬**，Vite 配置放行特殊标签 |
| **C/C++ 代码编译** | 云端 `arduino-cli` / `IDF` 生成 hex/bin | 云端 `emcc`（或本地 LLVM Wasm）生成 Wasm | **重新设计**编译管线 |
| **CPU 核心仿真** | `avr8js` / `rp2040js` / 后端 QEMU | **纯网页端 Web Worker Wasm 虚拟机** | **自主开发 Wasm 执行器，丢弃原版模拟器** |
| **外设交互控制** | `PartSimulationRegistry` (引脚钩子) | `WasmPeripheralRegistry` (对接 PAL 桥) | **重新设计**，使之对接 Wasm 导出的 PAL 接口 |
| **导线节点求解** | Union-Find 网表合并 + `ngspice-wasm` | Union-Find 网表合并 | 保留网表合并，暂时**移除/简化 ngspice**（优先保障行为级仿真） |
