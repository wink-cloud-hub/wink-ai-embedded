<!-- SPDX-License-Identifier: GPL-3.0-only -->
# ADR-0091：ESP-IDF 示例分类多配置实例与五维正交 Schema 架构决策

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-09-29（提议）/ 2026-09-29（采纳） |
| 决策编号 | ADR-0091 |
| 影响范围 | `wink-micro-app/vendor/esp_idfv61/checklist.data.json`（Schema v2.0 升级）；`capability-catalog.yaml`（`depends_on` 图谱）；`.gates/quarantine.yaml`（存量隔离白名单）；`CLASSIFICATION-SPEC.md`（v2.0 规范回写）；CI Gate 1~4 |
| 决策者 | 架构委员会 & 用户 |
| 关联技术设计 | [`docs/zh/tech-designs/esp32/esp-idf-classification-schema-spec.md`](../../zh/tech-designs/esp32/esp-idf-classification-schema-spec.md)（阶段 A 终版规格）、[`docs/zh/tech-designs/esp32/esp-idf-classification-gate-system.md`](../../zh/tech-designs/esp32/esp-idf-classification-gate-system.md) |
| 关联实施计划 | [`2026-09-29-esp-idf-classification-baseline-remediation-plan.md`](../../implementation-plans/esp32/2026-09-29-esp-idf-classification-baseline-remediation-plan.md) |
| 关联规范 | [`CLASSIFICATION-SPEC.md`](../../../wink-micro-app/vendor/esp_idfv61/CLASSIFICATION-SPEC.md) |
| 前序 ADR | [ADR-0012](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)、[ADR-0002](0002-dual-target-compilation.md)、[ADR-0003](0003-simulation-fidelity-boundary.md)、[ADR-0090](0090-centralized-pluggable-gate-system.md) |

---

## 背景（Context）

在对 ESP-IDF v6.1 官方 478 个示例的仿真治理演进中，CLASSIFICATION-SPEC v1.0 及 v1.1 暴露出严重的结构性数据语义冲突与架构漏洞（见 [2026-09-29 架构评审](../../reviews/esp32/2026-09-29-esp-idf-classification-spec-review-1.2.md)）：

1. **状态混用与自相矛盾**：在 v1.1 的数据源中，条目 1 `esp.get_started.blink` 同时标记为 `delivery.state = "verified"` 与 `scope_and_maturity.status = "in_scope_deficit"`（已完成交付却仍是规划缺口），范围、排期、审计、能力满足度与交付态五维混杂在单值枚举中。
2. **缺乏实体级配置定位**：原 Schema 将 `target_soc` 和 `execution_backend` 平铺在根部或填报为 `"all"`，导致一个后端的测试通过（如 Node.js Headless 仿真）被虚假放大为真实浏览器、甚至物理 ESP32 真机的支持宣称，违反同源双靶诚信原则。
3. **机器凭证悬空**：当前数据源宣称已验证的 10 个条目，其 `assets_sha256`、`scenario_sha256` 和执行记录全部为 `null`，机器门禁形同虚设。
4. **依赖闭包与防线缺失**：示例手工维护平铺能力数组，Catalog 缺乏 `depends_on` 依赖图；预留条件依赖时缺乏防御性解析规则，存在静默跳过而误判满足的风险。
5. **门禁与治理法典脱节**：规范开篇宣称 `.gates/run_gates.py` 已经生效，但实际仓内目录尚未落地；且原方案设想“门禁在 CI 运行时自动修改 JSON 状态”，直接违反了门禁插件纯函数只读约束。

为支撑 478 个示例的大规模、长期稳定治理，必须对数据层执行一次规范化的**破损性架构升级（Schema v2.0）**。

---

## 决策（Decision）

架构委员会决定全面采纳 **阶段 A 技术设计规格（[esp-idf-classification-schema-spec.md](../../zh/tech-designs/esp32/esp-idf-classification-schema-spec.md)）**，确立以下六大不可妥协的架构支柱：

### 1. 将“执行配置实例（`executions: [...]`）”立为一等公民实体
- 每个示例档案根部仅持有稳定标识、原厂路径、直接能力需求和五维范围/审计；
- **交付态与证据完全下沉至具体的配置实例中**；
- 显式区分运行后端与宿主环境：`backend: wasm_browser | wasm_node | esp32_hardware | host_native`；
- 根部 `audit` 强制绑定审定配置列表（`audited_configs`），新增配置不自动继承审定结论。

### 2. 建立“五维正交状态机”并保留显式 unknown
- **产品范围 (Scope)**：`unknown | in_scope | out_of_scope`（彻底保留 unknown，禁止未审先排）；
- **投入安排 (Schedule)**：`active | deferred`；
- **需求审计 (Audit)**：`pending | audited | needs_review`；
- **能力满足度 (Maturity)**：由 Catalog 依赖图与配置动态派生 `unknown | satisfied | blocked`，禁止示例中手工维护；
- **交付证据 (Delivery)**：配置级持有 `planned | building | verified | stale | regressed`。

### 3. 确立门禁与看板唯一的“六位一体打勾公式”
门禁系统与看板渲染脚本必须强制遵循完全同一套充要条件判定，只有同时满足：
1. `scope.inclusion == "in_scope"`；
2. `audit.verdict == "audited"` 且覆盖当前配置案例；
3. 必需依赖闭包动态计算全部为 `satisfied`（无 unknown 或 blocked）；
4. 该配置显式声明为 `delivery_state == "verified"`；
5. `assets_sha256`、`scenario_sha256` 及运行报告哈希非空且匹配当前工作区；
6. 绑定的最新执行报告证实**真实执行成功**（退出码 0、断言全过、无未捕获异常）。

**任何一项不满足（如即使哈希存在但测试断言失败处于 `regressed`），门禁坚决拦截，看板严禁渲染绿色勾选框 `[x]`。**

### 4. 依赖闭包解析器设立三条防御红线
在 Catalog 中引入 `depends_on: { mandatory: [...], conditional: [...] }` 结构，工具解析依赖闭包时强制执行：
- **红线 1**：解析器不支持的高阶条件语法强制阻断并报错，绝不生成假满足；
- **红线 2**：条件引用的配置字段缺失时按 `unknown` 阻断，严禁默认 false；
- **红线 3**：求值必须使用已解析的稳定配置上下文，并将指纹纳入证据链。

### 5. 存量债务隔离区（Quarantine）与门禁只读纯函数约束
- 针对当前 10 个哈希为 null 的存量条目，在 `.gates/quarantine.yaml` 建立隔离区白名单，标记为 `provisional_unverified`，看板渲染为 **`[?] 待补凭证`**；
- 设立 14 天硬性 TTL，逾期未补凭据门禁立即报告阻断 Error；
- **门禁纯函数原则**：门禁在 CI 运行时严禁修改任何数据文件。状态回退走显式数据迁移 PR，严禁门禁静默篡改。

### 6. 落实基于 `WINK_SLA_ERROR` 的编译期 Fail-Loud 规范
- 对不可逆物理介质（Efuse 物理烧写、DVP 物理摄像头差分对等）归类为 `out_of_scope` 的示例，必须在 C 头文件中提供 `__attribute__((error(...)))` 编译期硬断言；
- 门禁必须真实启动编译并精确匹配断言错误文本，严禁任何假空桩（Fake Stubs）。

---

## 影响与后果（Consequences）

### 积极影响 (Positive)
1. **数据与逻辑彻底闭环**：消灭了 `in_scope_deficit` 与 `verified` 共存的荒谬现象，看板与数据源逐维严格一致；
2. **防范跨环境虚假承诺**：区分了 Node.js 与浏览器宿主，使得 Web 仿真与物理真机的证据独立表达；
3. **零证据通过彻底清零**：10 个存量空哈希条目进入隔离区，新声明必须提供真实运行报告与防伪哈希，CI 守住真正的质量底线；
4. **同源双靶边界清晰**：差分测试承诺收窄至指定观测项与容差，坚决对齐现行 SSOT [03-production-contract.md](../../design/04-wasm-simulation/01-overview/03-production-contract.md)。

### 破坏性变更与迁移成本 (Negative / Breaking)
1. **Schema 破损性升级**：从 v1.1 到 v2.0，原有的 `scope_and_maturity`、`delivery` 结构被重组为 `scope`、`audit` 和 `executions: [...]`；
2. **存量数据必须全量迁移**：`checklist.data.json` 中的 478 个条目必须通过确定性的迁移脚本批量升级格式；
3. **CI 门禁与看板渲染器重写**：Gate 1 校验器与 `generate_checklist` 脚本必须同步适配新 Schema。

---

## 实施要求与时间表

1. **版本标识**：目标数据格式版本定为 **`spec_version: "2.0.0"`**；
2. **回写规范**：立即将 CLASSIFICATION-SPEC.md 修订至 v2.0-Draft；
3. **编写迁移转换工具**：在阶段 D 开发 `migrate_checklist_v1_to_v2.py`，保证 478 条示例平滑转换并不制造人工假审定。
