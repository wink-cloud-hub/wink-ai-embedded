# 02. MVP 产品路线、能力边界与阶段性交付规划

Wink-AI 的长期愿景是低代码 AI 嵌入式开发、浏览器高性能仿真与真机一键部署的一体化平台。为了避免架构过宽导致落地困难，MVP 阶段必须聚焦最短闭环：用户能在浏览器中创建一个真实可用的 ESP32 项目，完成仿真、异常验证、云编译和 WebSerial 烧录。

---

## 1. 产品定位

Wink-AI MVP 的核心定位：

> 面向 AI 生成嵌入式应用的“先仿真、再烧录”的安全开发平台。

核心用户：

| 用户 | 需求 | MVP 价值 |
|---|---|---|
| 嵌入式初学者 | 不会配置工具链 | 浏览器完成开发和烧录 |
| 创客/教育用户 | 快速做硬件 demo | 拖拽外设、仿真、真机验证 |
| AI 代码生成用户 | 担心代码直接伤害硬件 | Wasm 沙箱和故障测试先行 |
| 产品原型团队 | 快速验证控制逻辑 | 行为级仿真降低试错成本 |

---

## 2. MVP 北极星指标

1. 用户从新建项目到看到 Web 仿真运行：小于 3 分钟。
2. 用户从仿真通过到 ESP32 烧录成功：小于 5 分钟。
3. 官方示例项目仿真成功率：大于 95%。
4. WebSerial 烧录成功率：大于 90%。
5. AI 生成 App 静态检查拦截高危代码：100%。

---

## 3. MVP 范围

### 3.1 必须支持（MVP 聚焦 Tier 1: AI-Native 统一 OS 架构）

| 模块 | 范围 | 仿真模式归属 |
|---|---|---|
| 目标板 | ESP32 DevKit V1 / STM32F4 / Host Wasm | **Tier 1 (AI-Native 统一 OS，Role-Action 语义)** |
| 外设 | LED、Button、RC/Industrial Servo、HC-SR04、SSD1306 OLED、FOC 电机、闭环直流电机、自锁继电器、ADC 模拟量 | 虚实同源 DAL / PAL 物理源旁路 |
| 总线与抽象 | GPIO、PWM (Router)、I2C (v6 兼容)、ADC 子系统、RMT | PAL 统一抽象 |
| 仿真 | Wasm Worker (@wink-ai/unisim)、Asyncify、PAL 物理源旁路、Protocol Bypass | UniSim 3.0 Wasm-Core |
| 代码生成 | App 模板、device_tree 生成 (`wink gen`) | Safe Codegen |
| 安全 | App 静态检查、Worker watchdog、错误状态码 (`wink_status_t`) | 沙箱隔离 |
| 编译 | 云端/本地 Docker 与 `wink build` CLI | 交叉编译 |
| 烧录 | Chrome/Edge WebSerial/WebUSB 烧录 (`wink flash`) | 浏览器直连 |
| 验证 | Golden Trace 基础事件与对比 | 虚实一致性验证 |

### 3.2 演进与暂缓支持（Post-MVP / Tier 2/3/4 演进路线）

| 能力 / 芯片生态 | 规划层级 | 推进阶段与原因 |
|---|---|---|
| **C51 / Arduino 零侵入代码导入** | **Tier 2 (API/HAL 拦截代理)** | **Phase 2 (生态兼容期)**：提供 C++ Proxy 与 Arduino HAL 拦截桩 |
| **应广 (PDK) / 辉芒微 (FMD) 等 8位芯片** | **Tier 3 (1:1 指令级解释器)** | **Phase 3 (工业级下沉期)**：开发专有 ISA 虚拟机，赋能义乌/余慈低成本玩具家电产线 |
| **异构多核混合仿真引擎** | **Tier 4 (混合协同架构)** | **Phase 4 (混合仿真演进)**：Core 指令级 + 外设 C++ Proxy 高速通道 |
| RP2040 原生烧录 | Tier 1 扩展 | MVP 暂缓，需要独立 toolchain 和 UF2 流程 |
| 多板通信 | 平台系统扩展 | 状态同步和时钟模型复杂 |
| ngspice 电路仿真 | 电气级仿真 | 与行为级 MVP 价值不一致，非平台目标 |
| 复杂 3D 机械臂 | 可视化扩展 | 美术资源和物理建模成本高 |
| Wi-Fi/BLE 云连接 | 通信扩展 | 安全和网络配置复杂 |
| 用户自定义 C 库 | 安全隔离 | 安全审计成本高 |

---

## 4. 阶段路线

### Phase 0：架构骨架

目标：跑通最小运行时链路。

交付：

1. App/BAL/DAL/PAL 基础目录。
2. `wink_status_t`。
3. LED/Button/Servo DAL。
4. Wasm Worker 加载和 heartbeat。
5. 简单 project JSON。
6. device_tree 生成最小版本。

验收：

```text
用户拖拽 LED + Button，点击仿真，按按钮后 LED 状态变化。
```

### Phase 1：行为级仿真闭环

目标：证明 PAL 物理源旁路路线成立。

交付：

1. HC-SR04 ECHO 边沿脉冲注入与 PAL 旁路。
2. Servo 输出仿真。
3. 障碍物距离输入面板。
4. app_loop watchdog。
5. fault injection：timeout/disconnect。
6. Golden Trace 基础记录。

验收：

```text
避障小车示例在仿真中根据距离变化触发舵机动作，传感器 timeout 后进入 fault 状态。
```

### Phase 2：协议级旁路

目标：验证 I2C/OLED 事务级仿真。

交付：

1. ✅ `pal_i2c_transfer` Wasm 拦截（`js_pal_i2c_transfer` 已存在，SSD1306 验证通路）。
2. ✅ SSD1306 虚拟屏幕（`dal_ssd1306`：帧缓冲 + 6×8 字体 + 分页 flush，Protocol Bypass 第 2 级）。
3. ✅ I2C 地址冲突检测（`PAL_RESOURCE_I2C_ADDR`，`(port, 7位地址)` 粒度，device-owner claim）。
4. ✅ Trace 记录 `pal.transfer` 摘要（归属 JS Worker 侧，`js_pal_i2c_transfer` 字段已含 bus/port/size/status；规范已明确）。

验收：

```text
App 调用 OLED 显示 API，Web Canvas 显示文本或图案。
OLED Dashboard 示例 host e2e 通过（按钮 → LED + I2C flush + 帧缓冲非空 + 无 fault）。
```

### Phase 3：云编译与 WebSerial 烧录

目标：形成真机闭环。

交付：

1. ESP-IDF Docker 编译镜像。
2. build manifest。
3. firmware sha256。
4. WebSerial ESP32 bootloader 握手。
5. 烧录进度条与失败恢复指引。

验收：

```text
同一避障示例从仿真通过后编译为 ESP32 固件，并通过浏览器烧录运行。
```

### Phase 4：一致性验证

目标：证明虚实行为一致。

交付：

1. 真机 UART trace。
2. Trace compare 工具。
3. C1/C2/C3 一致性等级。
4. 官方示例 golden trace。

验收：

```text
避障示例仿真 trace 与真机 trace 的状态迁移和执行器命令一致。
```

---

## 5. 官方示例项目

MVP 只维护三个高质量示例：

| 示例 | 覆盖能力 |
|---|---|
| Button LED | GPIO、Pin-level、基础代码生成 |
| Servo Radar | PAL 物理源旁路、故障注入、状态机 |
| OLED Dashboard | I2C Protocol Bypass、屏幕渲染 |

每个示例必须包含：

1. 项目拓扑。
2. App 源码。
3. Device Model manifest。
4. 仿真 trace。
5. 截图或预期行为说明。
6. 真机部署说明。

---

## 6. 用户旅程

> 💡 **完整 UI/UX 与信息架构规范**：详见 [03-product-user-journey.md](./03-product-user-journey.md)。

### 6.1 新手用户

```text
选择模板 -> 进入画布 -> 修改参数 -> 点击仿真 -> 查看结果 -> 点击一键烧录 -> 浏览器选择串口 -> 烧录成功
```

关键体验：

1. 不要求用户安装 ESP-IDF。
2. 不要求理解 GPIO 复用细节。
3. 错误提示必须可操作。

### 6.2 AI 生成用户

```text
输入自然语言需求 -> AI 生成 App/拓扑建议 -> 静态安全检查 -> 仿真运行 -> 故障测试 -> 编译烧录
```

关键体验：

1. AI 生成失败时显示规则化诊断。
2. AI 不直接生成 PAL 调用。
3. 平台建议补充故障处理逻辑。

### 6.3 专业开发者

```text
导入项目 -> 查看生成代码 -> 调整 Device Model 属性 -> 运行 trace 对比 -> 下载固件或在线烧录
```

关键体验：

1. 可查看完整生成 C 代码。
2. 可导出 trace。
3. 可下载 firmware 和 manifest。

---

## 7. 非目标声明

MVP 不承诺：

1. 电气级电路仿真准确性。
2. 替代示波器、逻辑分析仪或真实硬件测试。
3. 支持所有 MCU 和所有开发板。
4. AI 生成代码一次性完全正确。
5. 复杂机械系统动力学仿真。
6. 浏览器外的所有烧录方式。

产品文案建议使用“行为级高保真仿真”，避免笼统宣称“100% 真实硬件仿真”。

---

## 8. 风险与对策

| 风险 | 对策 |
|---|---|
| WebSerial 兼容性不足 | 提供固件下载和桌面助手路线 |
| AI 生成 C 代码不安全 | App Safe Codegen + 静态检查 + Wasm 沙箱 |
| 仿真与真机偏差 | Golden Trace + 一致性等级 |
| 设备库维护成本高 | Device Model Registry 统一模型 |
| 云编译成本高 | 预编译 runtime、增量链接、缓存隔离 |
| 用户连线错误 | 画布静态校验和电压/引脚提示 |

---

## 9. 成功标准

MVP 达标定义：

1. 三个官方示例均可完整跑通仿真。
2. 至少一个示例可成功烧录 ESP32 真机。
3. 传感器 timeout/disconnect 能触发 fault。
4. Worker 死循环能被 watchdog 终止。
5. 编译产物包含 manifest 和 sha256。
6. README 中清晰说明能力边界和浏览器兼容性。
