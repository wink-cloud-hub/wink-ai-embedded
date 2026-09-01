# Wink-AI Embedded Platform (WinkMicroOS)

> 🌐 **Documentation Center / 文档中心**: 
> [📖 Global Hub](./docs/README.md) | [🇨🇳 简体中文文档](./docs/zh/README.md) | [🇺🇸 English Docs](./docs/en/README.md)

Wink-AI 嵌入式运行时及仿真系统（**WinkMicroOS**）：面向 AI 生成嵌入式应用的低代码开发平台。实现可视化/AI 生成的业务逻辑在浏览器 Wasm 仿真（行为级高保真）与 ESP32 等物理硬件上的同源编译与执行。

---

## 📚 快速文档导航 (Documentation Index)

| 入口 | 说明 | 快速链接 |
| :--- | :--- | :--- |
| **🌐 全局文档中心** | 全局文档拓扑图、i18n 多语言架构与工具链 | [docs/README.md](./docs/README.md) |
| **🇨🇳 中文文档中心** | 7 大系统设计规范 (SSOT)、RFC 技术方案、市场分析 | [docs/zh/README.md](./docs/zh/README.md) |
| **🇺🇸 English Docs Hub** | System Design SSOT Specifications & Technical RFCs | [docs/en/README.md](./docs/en/README.md) |
| **⚡ 5分钟快速上手** | 浏览器免安装在线仿真与极速上手教程 | [5min Getting Started](./docs/zh/design/00-quick-start/01-5min-getting-started.md) |

---

## 🏛️ 项目核心模块

- `wink-micro-os/`: C 语言嵌入式运行时内核（PAL 硬件抽象层 / DAL 器件抽象层 / BAL 业务抽象层）
- `simulator/`: Wasm 仿真器及运行时支撑
- `embedded-frontend/`: 嵌入式可视化工作台与仿真视窗
- `docs/`: 完整双语 SSOT 规范、架构决策记录 (ADR) 与实施计划
