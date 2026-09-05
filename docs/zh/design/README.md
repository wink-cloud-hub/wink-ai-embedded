# Wink-AI 嵌入式设计文档库 (Design Documentation Hub)

欢迎来到 **Wink-AI 嵌入式运行时及仿真系统 (WinkMicroOS)** 设计文档库。本系统是一个面向 AI 生成嵌入式应用的低代码开发平台，支持用户通过可视化画布或 AI 自动生成业务逻辑，在 Web 端基于 WebAssembly 进行行为级高保真仿真、异常注入验证和一致性追踪，然后通过云端交叉编译与浏览器 WebSerial/WebUSB 完成真机部署。

本设计文档库按专业产品架构拆分为 7 大模块/系统层级，便于不同团队（前端、内核、仿真桥、App代码生成/编译器、运维）并行开展开发：

---

## ⚡ 快速入口 & 全局契约与 ABI 权威导航矩阵

* **快速上手 (Getting Started)**：请参考 [00-quick-start/01-5min-getting-started.md](./00-quick-start/01-5min-getting-started.md)
* **AI 开发者指南**：请阅读 [docs/AGENTS.md](../AGENTS.md) 了解黑盒隔离与检索规则

### 🔗 跨模块/跨仓契约与 ABI 矩阵 (Global Contracts & ABI Matrix)

| 契约 / ABI 名称 | 涉及系统层级 (Subject) | 权威设计文档 (SSOT) | 源码/契约映射 (Mapping Target) | 映射类型 |
| :--- | :--- | :--- | :--- | :--- |
| **Wasm Bridge C-ABI** | UniSim Wasm ↔ Web Worker | [04-wasm-simulation/02-mechanisms/](./04-wasm-simulation/02-mechanisms/) | `wasm_bridge.h` C 导出接口声明 | `Contract-Mapping` |
| **SimTrace Event Spec** | 仿真/真机事件可观测性 | [07-platform-governance/04-simulation-consistency.md](./07-platform-governance/04-simulation-consistency.md) | `SimTraceSpecV2` JSON Schema | `Contract-Mapping` |
| **C-DAL Device API** | WinkMicroOS 器件抽象层 | [02-wink-micro-os/01-dal-device-abstraction.md](./02-wink-micro-os/01-dal-device-abstraction.md) | `/src/core/dal/include/dal_gpio.h` | `Code-Mapping` (内仓) |
| **C-PAL Platform HAL** | 平台适配接口 (HAL/OSAL) | [02-wink-micro-os/02-pal-platform-abstraction.md](./02-wink-micro-os/02-pal-platform-abstraction.md) | `/src/core/pal/include/pal_gpio.h` | `Code-Mapping` (内仓) |
| **Project Manifest** | 应用配置与器件树定义 | [03-app-codegen/02-project-manifest-schema.md](./03-app-codegen/02-project-manifest-schema.md) | `wink-app.json` (v2 Schema) | `Contract-Mapping` |
| **Cloud Build Job API** | 云端隔离交叉编译管线 | [06-build-toolchain/02-build-service-job-protocol.md](./06-build-toolchain/02-build-service-job-protocol.md) | `Build Job Protocol (job.proto)` | `Contract-Mapping` |
| **Device Model Registry** | 统一器件拓扑与属性注册 | [07-platform-governance/01-device-model-registry.md](./07-platform-governance/01-device-model-registry.md) | `Device Model Registry Schema` | `Contract-Mapping` |

---

## 架构设计文档导航

### 1. 总体设计与产品规划 (System Level Design & Product Roadmap)

* **[01-system-overall/01-system-overview.md](./01-system-overall/01-system-overview.md) - 平台系统级总体架构设计与跨仓五大模块指南**
  * 阐述平台愿景、分层架构、解耦执行流、**跨仓 5 大核心模块全景卡片 (embedded-frontend, unisim, wink-tools, wink-micro-os, wink-micro-app)** 及其黑盒接口指南。
* **[01-system-overall/02-mvp-roadmap.md](./01-system-overall/02-mvp-roadmap.md) - 产品路线、硬件矩阵与阶段性交付规划**
  * 收敛硬件矩阵到 ESP32/STM32、Wasm 仿真、故障测试、云编译和 WebSerial/WebUSB 烧录闭环。
* **[01-system-overall/03-product-user-journey.md](./01-system-overall/03-product-user-journey.md) - 产品用户旅程、双视窗 (2D/3D) 体验与 IDE 信息架构**
  * 定义核心用户路径、双视窗分屏与联动交互、工作模式、AI 介入点和烧录向导体验。
* **[01-system-overall/04-cross-repo-boundary-contract.md](./01-system-overall/04-cross-repo-boundary-contract.md) - 跨仓组件依赖、契约协议与商业机密隔离规范**
  * 定义与 Wink-AI 主仓的物理包结构、商业机密黑盒隔离原则、`wink-app.json` Manifest / Wasm Bridge ABI / SimTrace 跨仓机读契约协议。

### 2. 运行时内核设计 (WinkMicroOS Kernel)

* **[02-wink-micro-os/README.md](./02-wink-micro-os/README.md) - WinkMicroOS 运行时总览**
  * 子目录导引，描述 DAL/PAL 的运行时关系与 C 语言内核职责。
* **[02-wink-micro-os/01-dal-device-abstraction.md](./02-wink-micro-os/01-dal-device-abstraction.md) - 器件抽象层 (DAL) 设计规范与静态设备树**
  * 定义器件语义 API、双模运行实现、SIMULATION 分支、设备树生成模板。
* **[02-wink-micro-os/02-pal-platform-abstraction.md](./02-wink-micro-os/02-pal-platform-abstraction.md) - 平台抽象层 (PAL) API 规范**
  * 定义平台无关 OSAL/HAL API，包括 GPIO、PWM、I2C、SPI、ADC、线程、互斥锁和系统 Tick。

### 3. 应用层业务逻辑与代码生成 (App & Codegen)

* **[03-app-codegen/01-app-business-logic.md](./03-app-codegen/01-app-business-logic.md) - 应用层 (App) 运行时规范**
  * 定义低代码/AI 生成的应用层业务逻辑规范、生命周期契约、状态机代码生成规则和硬件解耦约束。
* **[03-app-codegen/02-project-manifest-schema.md](./03-app-codegen/02-project-manifest-schema.md) - Embedded Project Manifest 与 Registry Lock 规范**
  * 定义项目单一事实源、设备锁文件、可复现构建、迁移和主项目摘要 metadata。
* **[03-app-codegen/03-ai-dsl-and-codegen-pipeline.md](./03-app-codegen/03-ai-dsl-and-codegen-pipeline.md) - AI DSL、状态机 AST 与 App Safe Codegen 管线规范**
  * 定义自然语言/低代码到受限 DSL，再到确定性 App C 代码生成的安全链路。

### 4. Web 仿真与运行时 (Wasm & Web Simulation)

> **现行 SSOT 为 04-wasm-simulation（UniSim 四层文档）**：宏观 overview / 机制 mechanisms / 保真轴 axes / 验收 assurance。1.0 与 2.0 均已归档至 `04-wasm-simulation/archive/` 供只读历史对照。

* **[04-wasm-simulation/00-README.md](./04-wasm-simulation/00-README.md) — 【现行 Active】UniSim 3.0 SSOT 总入口**
  * 四层结构 + SSOT 铁律 + 文档/成熟度词表；A~F 保真轴与 C1~C25 一致性场景的权威索引。
* **[04-wasm-simulation/01-overview/](./04-wasm-simulation/01-overview/) — Ⅰ 宏观**：架构、A~F 定义、生产口径、方法论、术语表
* **[04-wasm-simulation/02-mechanisms/](./04-wasm-simulation/02-mechanisms/) — Ⅱa 机制（实现 SSOT）**：沙箱/Asyncify、虚拟时钟、调度器、中断、故障、物理退化、外设注册、通道路由、定时器/PWM、Wasm↔JS ABI、精度观测
* **[04-wasm-simulation/03-axes/](./04-wasm-simulation/03-axes/) — Ⅱb 保真轴 A~F 薄索引**
* **[04-wasm-simulation/04-assurance/](./04-wasm-simulation/04-assurance/) — Ⅲ 验收治理**：一致性 spec（C1~C25）、状态矩阵清单、路线图与治理
* **[04-wasm-simulation/archive/](./04-wasm-simulation/archive/)**：历史版本（只读归档 1.0 / 2.0）

### 5. 前端工作台设计 (Frontend Workbench)

* **[05-frontend-workbench/01-frontend-workbench-architecture.md](./05-frontend-workbench/01-frontend-workbench-architecture.md) - 嵌入式前端工作台架构与体验设计**
  * 定义三栏工作台、Manifest 驱动状态、Wasm Worker 客户端、Trace Console 和主项目路由集成。
* **[05-frontend-workbench/02-dual-viewport-product-world-layout.md](./05-frontend-workbench/02-dual-viewport-product-world-layout.md) - 双视窗产品世界布局与 3D 机械仿真界面规范**
  * 定义电路 2D + 产品世界 3D 分屏布局、工作模式状态机、Manifest v2 机械/环境/绑定扩展、双域数据流与因果链控制台。

### 6. 云编译与物理烧录工具链 (Cloud Build & Toolchain)

* **[06-build-toolchain/01-toolchain-deployment.md](./06-build-toolchain/01-toolchain-deployment.md) - 编译与物理烧录管线**
  * 设计云端 Docker 交叉编译流水线和浏览器 WebSerial/WebUSB 真机烧录闭环。
* **[06-build-toolchain/02-build-service-job-protocol.md](./06-build-toolchain/02-build-service-job-protocol.md) - 编译服务、构建任务与 Artifact 协议规范**
  * 定义异步 Build Job、隔离编译 worker、Build Manifest、artifact hash、日志归一化和烧录前置校验。

### 7. 专业化平台治理与安全规范 (Platform Governance)

* **[07-platform-governance/01-device-model-registry.md](./07-platform-governance/01-device-model-registry.md) - Device Model Registry 统一器件模型规范**
  * 将外设引脚、属性、DAL API、仿真行为、真机约束、故障模型和代码生成规则统一为单一事实源。
* **[07-platform-governance/02-error-fault-model.md](./07-platform-governance/02-error-fault-model.md) - 错误模型、故障注入与安全降级规范**
  * 定义 `wink_status_t`、DAL/PAL 错误返回、故障注入、fail-safe 姿态和错误可观测性。
* **[07-platform-governance/03-security-sandbox.md](./07-platform-governance/03-security-sandbox.md) - AI 生成代码安全沙箱与云端编译安全规范**
  * 定义 App Safe Codegen、Wasm Worker watchdog、编译容器隔离、固件 manifest 和 WebSerial 安全策略。
* **[07-platform-governance/04-simulation-consistency.md](./07-platform-governance/04-simulation-consistency.md) - 虚实一致性验证与 Golden Trace 规范**
  * 定义仿真/真机 trace 事件、输入回放、差异对比、一致性等级和 CI 回归验证。

### 8. 横向过程产物管理 (Decisions, Plans, Reviews & Tech Designs)

本设计文档库采取 **“SSOT 保持唯一 + 过程产物按生命周期/领域归档”** 的双层架构：

* **[decisions/](../README.md) — 架构决策记录 (ADR)**
  * 存放重大技术决议（`0001-` ~ `0064-`，含 [ADR-0064 异构芯片仿真四层架构模型](../decisions/unisim/0064-chip-simulation-four-tier-taxonomy.md)），决策终态留痕，结论必须回写 `01~07` SSOT。
* **[tech-designs/](../README.md) — 技术方案与 RFC**
  * 按领域拆分为 `core/`, `unisim/`, `frontend/`, `tools/` 子目录，存放功能开发前的详细 How-To 方案。
* **[implementation-plans/](../README.md) — 实施计划 (Plan)**
  * 拆分为 `active/`（包含正执行/待确认计划）与 `archived/`（已完成结项计划）。可运行 `python implementation-plans/scripts/list_plans.py` 查验状态。
* **[reviews/](../README.md) — 架构评审与测试验证**
  * 拆分为 `active/`（含待整改项）与 `archived/`（历史验收/Hardware Smoke Test 报告）。

---

## 📋 文档组织与治理规范

| 分类 | 存放目录 | 性质 | 可变性 | 治理准则 |
|---|---|---|---|---|
| 架构规范 (SSOT) | `01~07` 各模块 | 系统"当前真相" | 持续原地更新 | 改架构直接改 `01~07` 文件 |
| 决策记录 (ADR) | `decisions/` | 架构决策纪要 | 终态只读 | 选型开 ADR，结论回写 `01~07` |
| 技术方案 (RFC) | `tech-designs/{domain}/` | 详细设计方案 | 实现后沉淀 | 按 core/unisim/frontend/tools 分组 |
| 实施计划 (Plan) | `implementation-plans/` | 阶段任务计划 | 生命极隔离 | 存放于 `active/`，结项移入 `archived/` |
| 评审报告 | `reviews/` | 评审体检报告 | 快照归档 | 包含未决项存 `active/`，结项移入 `archived/` |

> 📌 **文档治理十六字口诀**：
> **架构改动改 01~07 (SSOT)  │  重大选型开 decisions (ADR)**
> **方案分类进 tech-designs   │  计划完结入 archived (Plan)**

---

## 核心理念：安全的虚实融合 (Safe Dual-Mode Harmony)

```text
[ AI / Low-Code 输入 ]
        │
        ▼
[ App DSL / 状态机 AST + Project Manifest ]
        │
        ▼
[ App Safe Codegen + 静态检查 ]
        │
        ▼
[ Device Model Registry + Registry Lock ] ──► [ device_tree / SchemaForm / DAL / Sim ]
        │
        ▼
[ Wasm Worker 仿真 + Fault Injection + Golden Trace ]
        │
        ▼
[ 云端隔离编译 + Firmware Manifest ]
        │
        ▼
[ WebSerial/WebUSB 用户授权烧录 ]
        │
        ▼
[ 真机 Runtime + Trace Compare ]
```

平台不追求替代 SPICE、示波器或真实硬件验证，而是聚焦于 **AI 生成嵌入式业务逻辑的安全生成、行为级仿真、异常路径验证和真机部署闭环**。


## 项目代码规范
### 嵌入式代码规范
🎯 一句话总结

| 场景 | 用哪个 |
|---|---|
| 新人入职第一天给他看 | `c-code.md`（10 条铁律，先记住不能碰什么） |
| 写完代码提交前自检 | `code-quality-checklist.md`（逐项打勾） |
| 某条拿不准为什么要这么做 | 点开对应专项规范文件（理解设计原理） |
| Code Review 给人提意见 | 先指 `code-quality-checklist.md` 条目，再甩专项规范链接 |

