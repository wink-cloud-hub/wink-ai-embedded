# DAL 开发手册（索引）

面向在 WinkMicroOS 上**使用**或**扩展**器件抽象层（DAL）的开发者。设计真相仍以活规范为准；本目录是动手 Howto + 实践摘要。

| 文档 | 何时读 |
|------|--------|
| [dal-quickstart.md](./dal-quickstart.md) | 第一次写 App / 调用已有 `dal_*` |
| [adding-peripheral.md](./adding-peripheral.md) | 新增一种外设类型 / DAL（`wink.py new-dal`，ADR-0046） |
| [role-interface-codegen.md](./role-interface-codegen.md) | 可选：把驱动包装成 App Role（`{name}_{verb}`，codegen） |
| [dal-best-practices.md](./dal-best-practices.md) | 设计驱动变体、拓扑扩展、裁剪与命名 |
| [dal-api-consistency-spec.md](./dal-api-consistency-spec.md) | **DAL API 设计与一致性规范**（生命周期/领域Trait动词库/功耗与回调/向后兼容） |
| → 其中 [§3.0 字段分层](./dal-best-practices.md#30-wink-appjson-字段分层) | `type` / `role` / `variant` / … 谁通用 |
| → 其中 [type 与 role](./dal-best-practices.md#type-与-role勿混为一谈) | 驱动平面 vs 能力平面；非 BAL；细节见 role 专文 |

## 规范 SSOT（勿在本目录另起一套架构真相）

| 主题 | 活文档 / ADR |
|------|----------------|
| DAL 架构与 API 契约 | [`docs/design/02-wink-micro-os/01-dal-device-abstraction.md`](../../../docs/design/02-wink-micro-os/01-dal-device-abstraction.md) |
| 静态分发 | [ADR-0004](../../../docs/design/decisions/0004-static-dispatch-vs-runtime-ops.md) |
| 驱动 registry | [ADR-0046](../../../docs/design/decisions/0046-dal-driver-registry-ssot.md) |
| 执行器命名 / DC safe_off | [ADR-0048](../../../docs/design/decisions/0048-actuator-control-semantic-naming.md) |
| Role Interface / 扩展根 | [ADR-0051](../../../docs/design/decisions/0051-scannable-codegen-extension-roots.md)（**Accepted**）；[tech-design](../../../docs/design/tech-designs/2026-07-28-scannable-codegen-extension-roots-design.md)；[实施计划](../../../docs/design/implementation-plans/2026-07-29-scannable-codegen-extension-roots-plan.md) |
| 跨 Profile A/B 量纲与定标整数 | [ADR-0056](../../../docs/design/decisions/0056-cross-profile-quantity-ab-class-and-scaled-integers.md)（**Accepted**） |
| Wasm 仿真 3.0 SSOT | [`docs/design/04-wasm-simulation-3.0/`](../../../docs/design/04-wasm-simulation-3.0/00-README.md)（**Active**） |

## 同级相关 Howto

- [`../app-coding-gotchas.md`](../app-coding-gotchas.md)
- [`../system-framework-gotchas.md`](../system-framework-gotchas.md)
