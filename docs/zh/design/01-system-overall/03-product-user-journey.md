# 03. 产品用户旅程、信息架构与关键体验设计

本文件从产品经理视角定义 Wink-AI 的核心用户路径、页面信息架构、关键决策点和失败恢复体验，确保底层架构能力能够转化为清晰、可信、低门槛的用户体验。

---

## 1. 产品一句话

> Wink-AI 是一个让用户通过 AI/低代码生成嵌入式业务逻辑，并在浏览器中先仿真验证、再一键烧录到真实开发板的安全开发平台。

---

## 2. 核心价值主张

排序如下：

1. **安全**：AI 生成代码先在 Wasm 沙箱和故障测试中验证，再允许烧录。
2. **低门槛**：用户不需要安装交叉编译工具链。
3. **快闭环**：拖拽、仿真、编译、烧录在同一浏览器流程中完成。
4. **同源逻辑**：同一份 App 业务逻辑运行于仿真与真机。
5. **专业验证**：通过 Golden Trace 和故障注入验证异常路径。

---

## 3. 用户角色

| 角色 | 主要任务 | 关键障碍 | 平台应提供 |
|---|---|---|---|
| 教育用户 | 学习传感器和控制逻辑 | 不懂工具链和底层驱动 | 模板、图形化、即时反馈 |
| 创客用户 | 快速做原型 | 连线和烧录麻烦 | 一键仿真与烧录 |
| AI 生成用户 | 从需求生成硬件逻辑 | 担心代码不安全 | 静态检查、沙箱、异常测试 |
| 专业开发者 | 验证业务控制逻辑 | 担心平台过度封装 | 可查看代码、trace、manifest |

---

## 4. 信息架构

```text
Embedded Workbench (双视窗工作台 - design / simulate / diagnose 工作模式)
├── Top Bar
│   ├── Target Board Selector (ESP32 / STM32)
│   ├── Workbench Mode Switcher (design | simulate | diagnose)
│   ├── Safety Level & Consistency Badge (S0 - S4)
│   └── Build & Flash Trigger
├─ Center Workspace (Dual-Viewport 分屏与联动)
│   ├── Viewport A: Visual 2D Circuit Canvas (HCTR 布线、引脚交互、端口校验)
│   └── Viewport B: Product World 3D (Three.js/WebGL 机械、物理量与环境模型)
├── Right Panel (上下文感知属性面板)
│   ├── Property Inspector (SchemaForm 自动渲染)
│   ├── Bindings Panel (电路引脚与 3D 传感器/执行器绑定)
│   └── Fault Injection Matrix (故障注入配置)
├── Left Drawer (外设与工具库)
│   ├── Device Catalog (Board / Peripherals / Mechanical Assets)
│   └── AI Assistant / State Machine DSL Editor
└── Bottom Console
    ├── Trace Console (Golden Trace Spec v2 实时时间线)
    ├── Static Check & Compiler Diagnostics (Monaco 红色波浪线)
    └── Build & WebSerial / WebUSB Flash Wizard
```

---

## 5. 核心路径：从模板到真机

```text
选择模板
  ↓
打开画布
  ↓
调整外设属性
  ↓
生成/编辑业务逻辑
  ↓
静态安全检查
  ↓
运行 Wasm 仿真
  ↓
执行故障测试
  ↓
云端编译
  ↓
查看固件 manifest
  ↓
授权 WebSerial
  ↓
烧录真机
  ↓
采集真机 trace
  ↓
显示一致性等级
```

---

## 6. 页面关键状态

### 6.1 静态检查状态

| 状态 | 用户看到 | 可操作 |
|---|---|---|
| 未检查 | “请先运行安全检查” | 运行检查 |
| 通过 | “安全检查通过，可仿真” | 运行仿真 |
| 警告 | “存在潜在风险，但可仿真” | 查看建议、继续仿真 |
| 错误 | “存在阻断问题” | 定位代码、AI 修复 |

### 6.2 仿真状态

| 状态 | 用户看到 | 可操作 |
|---|---|---|
| Ready | 等待运行 | Run |
| Running | 实时画布、trace 流 | Pause/Stop/Fault |
| Paused | 当前状态冻结 | Resume/Stop |
| Faulted | 错误原因和安全状态 | 查看 trace、AI 修复 |
| Watchdog Terminated | 代码可能死循环 | 查看诊断、禁止烧录 |

### 6.3 编译状态

| 状态 | 用户看到 | 可操作 |
|---|---|---|
| Pending | 等待构建 | 开始编译 |
| Building | 进度和日志 | 取消 |
| Success | 固件 manifest | 烧录/下载 |
| Failed | 编译错误定位 | AI 修复/查看日志 |

---

## 7. 错误提示原则

错误提示必须包含：

1. 发生了什么。
2. 为什么会发生。
3. 用户下一步能做什么。
4. 平台是否可以自动修复。

示例：

```text
错误：front_radar 的 ECHO 引脚输出为 5V，但 ESP32 GPIO5 仅支持 3.3V 输入。
原因：直接连接可能损坏开发板。
建议：添加电平转换器或使用分压电阻模块。
操作：[自动添加电平转换器] [更换引脚] [查看说明]
```

---

## 8. AI 助手介入点

AI 不应只生成代码，还应参与诊断与修复。

| 场景 | AI 行为 |
|---|---|
| 新建项目 | 根据自然语言推荐模板和外设 |
| 连线错误 | 解释错误并建议正确连线 |
| 静态检查失败 | 修改 App 代码以满足安全规则 |
| 仿真 fault | 分析 trace，定位状态机问题 |
| 编译失败 | 根据 gcc 日志给出修复补丁 |
| 烧录失败 | 根据串口日志指导进入 bootloader |

AI 生成代码必须经过 App Safe Codegen 和静态检查，不允许直接进入烧录流程。

---

## 9. Build & Flash 向导

烧录向导步骤：

1. 选择目标板卡。
2. 显示浏览器兼容性。
3. 显示固件 manifest。
4. 请求串口授权。
5. 自动进入 bootloader。
6. 擦除与写入。
7. 校验 hash。
8. 重启运行。
9. 可选采集真机 trace。

失败恢复提示：

| 失败 | 指引 |
|---|---|
| 找不到串口 | 检查数据线、驱动、浏览器权限 |
| 进入 bootloader 失败 | 按住 BOOT，点击 EN，再重试 |
| 写入中断 | 降低波特率重试 |
| 校验失败 | 重新编译或重新烧录 |
| 浏览器不支持 | 下载固件并使用命令行烧录 |

---

## 10. 专业可信感设计

为体现架构师级专业度，界面应显式展示：

1. Device Model 版本。
2. Firmware sha256。
3. Runtime 版本。
4. 一致性等级。
5. Trace 导出入口。
6. Fault injection 结果。
7. 目标板能力校验。

示例状态卡：

```text
Project Health
├── Static Check: Passed
├── Simulation: Passed
├── Fault Test: Sensor timeout handled
├── Consistency: C2
├── Target: ESP32 DevKit V1
├── Runtime: WinkMicroOS 0.1.0
└── Firmware: sha256:xxxx
```

---

## 11. Onboarding 模板

MVP 推荐三个入口模板：

1. **点亮 LED**：最短路径，验证 GPIO 和烧录。
2. **雷达舵机避障**：展示 PAL 物理源旁路、状态机、fault。
3. **OLED 仪表盘**：展示 I2C 协议旁路和屏幕仿真。

每个模板都提供：

1. 目标说明。
2. 画布拓扑。
3. 关键代码片段。
4. 可操作仿真控件。
5. 一键故障测试。
6. 真机烧录指引。

---

## 12. 产品边界表达

文案建议：

```text
Wink-AI 提供行为级高保真仿真，帮助你在真实烧录前验证业务逻辑、外设交互和异常处理。它不替代专业电气仿真、示波器或最终硬件验证。
```

避免：

```text
100% 模拟真实硬件。
完全不需要硬件测试。
支持所有单片机。
```

---

## 13. MVP 体验验收

完成 MVP 时，用户应能独立完成：

1. 从模板创建 ESP32 项目。
2. 修改传感器阈值。
3. 运行仿真并观察执行器变化。
4. 注入传感器 timeout 并看到 fault。
5. 通过云端编译生成固件。
6. 使用 Chrome/Edge WebSerial 烧录 ESP32。
7. 查看固件 manifest 和 trace。
