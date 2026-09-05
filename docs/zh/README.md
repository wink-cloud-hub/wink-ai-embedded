# Wink-AI 嵌入式开发平台中文文档中心 (Documentation Hub)

> 🌐 **语言 / Language**: [📖 全局 Hub](../README.md) | **🇨🇳 简体中文** | [🇺🇸 English](../en/README.md)

欢迎来到 **Wink-AI 嵌入式开发与仿真系统 (WinkMicroOS)** 中文文档中心。

---

## 🏛️ 系统设计规范 (System Design SSOT)

| 模块 | 职责与内容 | 对应规范 |
| :--- | :--- | :--- |
| **00. 快速入门** | 5 分钟上手与开发指引 | [00-quick-start/](./design/00-quick-start/01-5min-getting-started.md) |
| **01. 系统总体架构** | 平台总体架构、跨仓边界契约与 MVP 路线 | [01-system-overall/](./design/01-system-overall/01-system-overview.md) |
| **02. WinkMicroOS 内核** | C SDK 分层架构 (PAL/DAL/BAL) 与运行期规范 | [02-wink-micro-os/](./design/02-wink-micro-os/README.md) |
| **03. 应用层与代码生成** | 应用业务模型、Manifest Schema 与安全生成 | [03-app-codegen/](./design/03-app-codegen/01-app-business-logic.md) |
| **04. Wasm 仿真体系** | UniSim 仿真内核、Wasm-JS Bridge 与物理仿真 | [04-wasm-simulation/](./design/04-wasm-simulation/00-README.md) |
| **05. 前端工作台** | 双视窗交互、画布与 3D 渲染规范 | [05-frontend-workbench/](./design/05-frontend-workbench/01-frontend-workbench-architecture.md) |
| **06. 云编译工具链** | 云端隔离编译服务与 Job Protocol | [06-build-toolchain/](./design/06-build-toolchain/01-toolchain-deployment.md) |
| **07. 平台治理与 Registry** | 器件注册表、故障模型与编码规范 | [07-platform-governance/](./design/07-platform-governance/01-device-model-registry.md) |

---

## 💡 核心技术方案 (Tech-Designs RFC)
- [C 内核技术方案](./tech-designs/core/README.md)
- [MCS-51 仿真拦截层技术方案](./tech-designs/mcs51/README.md)
- [工具链技术方案](./tech-designs/tools/README.md)
- [前端工作台方案](./tech-designs/frontend/README.md)
- [仿真引擎技术方案](./tech-designs/unisim/README.md)

---

## 📊 产品与市场
- [UniSim 嵌入式在线仿真平台商业与市场分析报告](./product/market-analysis.md)
