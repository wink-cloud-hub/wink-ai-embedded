# 🤖 Wink-AI 嵌入式平台 AI Agent 检索与开发指南 (AGENTS.md)

> 📌 **语言版本 / Language**：[中文架构 SSOT (默认)](zh/design/) ｜ [English SSOT](en/design/)

本文件是专门为 AI 智能体 (Antigravity, Cursor, Copilot 等) 编写的快速上下文指南。在分析、修改或解答本仓库代码与架构时，**必须严格遵守以下 SSOT 导引与黑盒隔离原则**。

---

## 1. 跨仓黑盒隔离与代码映射铁律 (Insulation & Mapping Rules)

> [!IMPORTANT]
> 1. **`wink-micro-os` (本仓 C SDK 内核)**：允许且必须通过 `Code-Mapping` 精确定位至内仓 C 头文件与源文件（如 `/src/core/dal/`, `/src/core/pal/include/pal_gpio.h`）。
> 2. **外仓组件 (`unisim`, `embedded-frontend`, `wink-tools`)**：**严格禁止猜测或泄露其内部 TypeScript / Vue 源码路径或实现细节**。仅允许通过公开 **`Contract-Mapping`（契约接口）** 进行交互：
>    - `unisim`: 参照 `wasm_bridge.h` C-ABI 与 `SimTraceSpecV2` JSON Schema。
>    - `embedded-frontend`: 参照 `wink-app.json` (v2 Manifest Schema) 与视窗 DTO。
>    - `wink-tools`: 参照 `wink` CLI 命令行参数规范。

---

## 2. 核心架构与代码映射快速查阅表

| 领域 (Domain) | 设计 SSOT (Design Spec) | 源码 / 契约映射 (Mapping Target) | 性质 (Type) |
| :--- | :--- | :--- | :--- |
| **系统总体与跨仓** | [01-system-overall/](zh/design/01-system-overall/01-system-overview.md) | [跨仓边界契约](zh/design/01-system-overall/04-cross-repo-boundary-contract.md) | 架构规范 |
| **WinkMicroOS 内核** | [02-wink-micro-os/](zh/design/02-wink-micro-os/README.md) | `Code-Mapping: /src/core/` (`dal/`, `pal/`, `bal/`) | **内仓源码** |
| **应用生成 & Manifest** | [03-app-codegen/](zh/design/03-app-codegen/01-app-business-logic.md) | `Contract-Mapping: wink-app.json (v2 Schema)` | 契约/Schema |
| **UniSim Wasm 仿真** | [04-wasm-simulation/](zh/design/04-wasm-simulation/00-README.md) | `Contract-Mapping: wasm_bridge.h` & `SimTraceSpecV2` | 外仓黑盒 ABI |
| **前端工作台** | [05-frontend-workbench/](zh/design/05-frontend-workbench/README.md) | `Contract-Mapping: Dual-Viewport State DTO` | 外仓黑盒 DTO |
| **云编译工具链** | [06-build-toolchain/](zh/design/06-build-toolchain/README.md) | `Contract-Mapping: Dual-Target CMake Build` | 编译 Protocol |
| **平台治理与 Registry** | [07-platform-governance/](zh/design/07-platform-governance/01-device-model-registry.md) | `Contract-Mapping: Device Model Registry Schema` | 治理 Schema |

---

## 3. 防幻觉规则 (Anti-Hallucination Guardrails)

当响应用户请求时，AI 智能体必须遵守：
1. **禁止假设 VM 解释器**：`wink-micro-os` 在 MCU 上是 100% 静态编译的 C 代码，不使用 MicroPython、JS VM 或 Wasm-on-MCU（如 WAMR）。
2. **禁止编写动态内存分配**：`wink-micro-os` 严格限制使用 `malloc`/`free`，应用层采用静态 BSS 分配与 Protothreads 无栈协程。
3. **不可破坏黑盒**：不要试图在 `docs/{zh,en}/design/` 中添加外仓 TypeScript 的实现逻辑细节。若存在功能修改诉求，提出契约层变更提案（ADR）。

---

## 4. 本地 AI 深度研发通道 (Local AI Deep Development Channel)

在本地全栈开发环境下，为了确保 AI 编码助手具备完整的跨模块推导能力，本地工作区已挂载专属私有通道（该目录已被 `.gitignore` 自动免疫，绝不会泄露至开源端）：
* **Unisim 仿真内核机制/审查/决策**：`docs/.internals/packages/unisim/docs/internals/`
* **前端工作台架构/算法/审查**：`docs/.internals/packages/embedded-frontend/docs/internals/`
* **工具链与 CLI 内部实现/决策**：`docs/.internals/packages/wink-tools/docs/internals/`

AI 助手在本地排查 Wasm 联调与复杂边界问题时，可直接读取上述私有通道文档获取 100% 完整的深层上下文。
