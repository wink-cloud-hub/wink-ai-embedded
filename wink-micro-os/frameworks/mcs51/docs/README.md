# MCS-51 仿真拦截层文档索引 (Documentation Index)

本目录为 WinkMicroOS `frameworks/mcs51` 模块的专属开发与维护文档库。

## 核心文档目录

| 文档 | 说明 | 适用对象 |
| :--- | :--- | :--- |
| **[01-architecture-and-governance-guide.md](./01-architecture-and-governance-guide.md)** | **MCS-51 架构治理与开发维护规范（SSOT）**<br>涵盖分层单向依赖、内存池化、AD-13 Trap 四红线、新芯片接入 SOP、自动化门禁体系与自查清单。 | 框架内核开发者、芯片自治包扩展者、AI Coding Agent |

## 外部关键设计与决策链接

* **活设计规范**：[`docs/zh/design/02-wink-micro-os/07-mcs51-simulation-interception.md`](../../../../docs/zh/design/02-wink-micro-os/07-mcs51-simulation-interception.md)
* **解耦系列实施总纲**：[`docs/implementation-plans/mcs51/2026-09-11-mcs51-decoupling/00-README.md`](../../../../docs/implementation-plans/mcs51/2026-09-11-mcs51-decoupling/00-README.md)
* **核心架构决策**：
  * [ADR-0004 编译期静态分发与无虚表原则](../../../../docs/design/decisions/0004-static-dispatch-vs-runtime-ops.md)
  * [ADR-0070 MCS-51 零代码仿真拦截总纲](../../../../docs/decisions/core/0070-mcs51-zero-code-simulation-interception-layer.md)
  * [ADR-0071 SFR 代理、RMW 与边缘数据面](../../../../docs/decisions/core/0071-sfr-proxy-rmw-edge-data-plane.md)
  * [ADR-0074 Read-Pin 外部电平仲裁与双读路径](../../../../docs/decisions/core/0074-mcs51-channel1-external-read-pin.md)
  * [ADR-0078 中断两阶段挂起与在服务屏蔽](../../../../docs/decisions/core/0078-mcs51-two-phase-irq-and-in-service-masking.md)
