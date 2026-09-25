# ADR-0086：收割影子头文件许可契约（公开薄版，实现细节见闭源 ADR-0067）

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-09-25 |
| 触发 | 闭源收割流水线 `PLAN-20260925-SDK-HARVESTER-ENGINE v2.1` 产物需落开源仓 `frameworks/esp_idf/include/`；须在不泄露工具实现的前提下确立许可与消费契约 |
| 影响范围 | `wink-micro-os/frameworks/**/include/**`（LGPL-3.0-only）；`wink-micro-os/NOTICE`；`.github/license-map.json`；`frameworks/esp_idf/include/README.md`；`02-api-coverage-matrix.md` SLA 片段 |
| 决策者 | 项目架构团队 / Owner（法务会签） |
| **关联 ADR** | [ADR-0083](0083-adopt-gpl-3.0-only-license-policy.md)、[ADR-0084](0084-layered-license-map-lgpl-runtime.md)（分层地图 SSOT）；[ADR-0012](0012-contract-honesty-over-silent-degradation.md)（Fail-Loud）；闭源 `ADR-0067 SDK Harvester 许可防火墙`（详细实现，不公开链接，仅具名引用） |
| **关联规范** | `coding-conventions.md §7`；`AGENTS.md`「开源许可」；总纲 `PLAN-20260922-ESP-IDF-SIM-MASTER §3.2 v3.6` |

---

## 1. 背景（Context）

1. 开源仓需消费 ESP-IDF `Apache-2.0` 头树的事实性 C-ABI（名/签名/数值/布局），但 `E-003` 禁止厂商 SDK 入库，整文件复制即违规。
2. 本 ADR 为公开薄契约：只规定输入输出许可与消费流程，不描述收割算法、规则库、解析实现（详见闭源 ADR-0067，恕不公开）。

## 2. 方案比选（Options）

| 方案 | 描述 | 结论 |
|---|---|---|
| A. 手工抄头进仓 | 人肉复制原厂头并换 SPDX | 否决：E-003 违规 + 易腐烂 + 归属缺失 |
| B. 收割事实性声明 + artifact 中介（本 ADR） | 事实四元组 + SPDX/归属双标注 + manifest/sbom + vendoring PR | **采纳** |
| C. 洁净室重写 | 黑盒重写 | 否决：成本过高，仅法务挑战时回退 |

## 3. 决策结论（Decision）

- **D1 输入**：上游为 `Apache-2.0` tag pin（`tag + sha`），CI 以 `espressif/idf` 镜像为准，绝对路径禁入仓。
- **D2 输出**：`frameworks/**/include/**` 为 `LGPL-3.0-only` 事实性声明（无原厂注释/文档/汇编/私头），每文件含 `SPDX + Harvested from <tag>@<sha>` 归属行 + `manifest.hash`。
- **D3 消费**：闭源产 versioned artifact（`tar + manifest.json + sbom + NOTICE.inc`），开源侧以 vendoring PR 合入（含 `NOTICE` 更新 + `license-map` 校验），禁止直写；保留 `N-1` 回滚。
- **D4 门禁**：`check_license_map.py` + 生成头自校验（`manifest.hash` 篡改检测 + 汇编/私头零容忍）为 CI 硬门；`test/tools` 仍 `GPL-3.0-only`（ADR-0084 D3）不受影响。
- **D5豁免**：`sdkconfig*.h`、`esp_macros.h`（宏劫持）、`soc/gpio_struct.h`（影子结构体）为手写豁免，不走收割（见总纲 §3.2）。

## 4. 后果与约束（Consequences & Constraints）

| 正面效益 | 约束与代价 |
|---|---|
| 影子头版权干净、可审计、可回滚；`LGPL` 静态链接叙事自洽（ADR-0084） | 每次 vendoring 须同步 `NOTICE.inc` + `coverage-matrix` 片段，否则 CI 阻断 |
| E-003 合规（厂商源码永不入库，仅事实性声明） | `GPL-2.0-only` 组件永久禁入（ADR-0084 D7），收割输入须 SBOM 白名单 |
| 合约诚实（超 SLA 编译期 Fail-Loud，ADR-0012） | 本 ADR 不含实现细节；实现争议以闭源 ADR-0067 为准，公开侧只验产物 + 门禁 |

## 5. 遵循与后续（Compliance & Follow-up）

- [x] `NOTICE` + `license-map.json` 更新（含收割产物规则，置于 `wink-micro-os/**` 兜底之前）
- [x] `frameworks/esp_idf/include/README.md` 入口统一为 `wink internal harvest-sdk` artifact 流程
- [x] CI `license-gate` + `manifest.hash` 校验脚本落盘（闭源 `ci_gate` / 开源 `.github/scripts/check_harvested_headers.py`）；已完成一次正式 vendoring（P1-A，官方构建/ctest 28/28）
- [x] Owner 批准转 Accepted（2026-09-25）；回写 `coding-conventions §7`；回滚演练保留为独立跟踪项（N-1 artifact 机制已具备）

---

*该 ADR 状态变更记录：*
- 2026-09-25：Proposed（架构组，随收割计划 v2.1 起草，待法务会签）
- 2026-09-25：**Accepted**（Owner 批准；证据：P1-A vendoring 落库，manifest `84db04923752c2f8`，`check_harvested_headers --rules` + `check_license_map.py` 双绿）
