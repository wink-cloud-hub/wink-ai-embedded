# Catalog / 外设元数据 SSOT 收敛 Implementation Plan（已归档）

> **状态**：✅ 已完成（2026-07-11）
> **归档说明**：本计划正文含私有仓实现路径与内部文件名，已按文档密级策略整篇移入本地私有通道；
> 本文件仅保留机制概览与公开契约面。
> **全量正文（仅本地）**：`docs/.internals/packages/unisim/docs/internals/embedded-archive/implementation-plans/2026-07-11-catalog-ssot-convergence-plan.md`

## 目标

让「新增/修改一个电路域器件」只需改**一处** `definition.ts`：catalog、绑定校验、画布、资产库全部从该 SSOT 派生；消除引脚双写、stub 散点与 legacy-adapter 间接路径。

## 机制概览（公开面）

- **声明层 SSOT**：`peripherals/` 与 `world-assets/`；`device-catalog.ts` 瘦身为只读聚合 facade；`manifest.bindings` 仍为项目级 binding 实例 SSOT。
- **成熟形态**：凡参与 `connections` 的器件必须是可上画布的 peripheral 插件（`CanvasGlyph` 必选）；`stub` 仅为开发阶段命名，不是永久架构分类。
- **约束**：Manifest `schemaVersion: 2` 不变；不修改 Worker 仿真协议（binding 桥属 W3c）；禁止在 catalog 手写 `category: 'peripheral' | 'stub'` 条目（board 除外）。

## 公开契约与关联

- `wink-app.json` Manifest v2（`devices` / `bindings` / `connections`）
- 相关公开文档：[外设插件注册计划](./2026-07-10-peripheral-plugin-registry-plan.md)、[W2 绑定模型](../../design/05-frontend-workbench/03-dual-viewport-phased-design/02-phase-w2-binding-model.md)、[如何新增外设](../../design/05-frontend-workbench/04-adding-a-peripheral.md)
