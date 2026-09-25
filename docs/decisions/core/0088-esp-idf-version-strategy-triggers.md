# ADR-0088：ESP-IDF 多版本支持策略与"单树 → 多树"切换触发器

| 项 | 内容 |
|---|---|
| 状态 | **Proposed（待 Owner 会签）** |
| 日期 | 2026-09-25 |
| 触发 | 长期可维护性评审（P1）：现行"单一版本树 + version_overlays"策略未定义何时升级为多版本树；ADR-0030 已确认开发机多版本 IDF 并存；M3 兼容基线同时声明 v5.1.3 与 v6.1+ |
| 影响范围 | `wink-micro-os/frameworks/esp_idf/include/`（版本树布局）、闭源 harvester 规则与发射路径、CI 版本矩阵、`check_harvested_headers.py` |
| 决策者 | 项目架构团队 / Owner |
| **关联 ADR** | [ADR-0030](0030-esp-idf-never-auto-installed.md)（IDF 多版本由 EIM 管理）、[ADR-0006](0006-esp-idf-v6-i2c-compatibility.md)（v6 I2C 兼容）、[ADR-0086](0086-harvested-headers-license-contract.md)（artifact 中介）、[ADR-0087](0087-esp-idf-asset-channels-and-soc-data-ownership.md)（资产归属） |
| **关联计划** | `2026-09-22-esp-idf-simulation-interception-master-plan.md` §3.4；`2026-09-26-esp-idf-sim-m3-soc-ci-plan.md`（T-012 双版本矩阵） |

---

## 1. 背景（Context）

1. `include/README.md §3` 冻结策略：同一时刻 `include/` 只承载**一个 IDF 版本的产物**，跨版本差异由闭源 `rules/esp_idf.yaml: version_overlays` 数据条目收敛；切版本 = 重新 vendoring。
2. 该策略当前成立（v5.1.3 overlay 骨架 + v6.1 vendored 树），但未定义退出条件：
   - v7.0 计划移除 Legacy I2C，单树超集将无法同时"保留 legacy 头 + 对齐 v7 语义"；
   - ADR-0030 确认用户机器并存多个 IDF（EIM profile），若产品需按用户工程锁定版本，单树将无法同时服务两个版本的链接期 ABI 差异。
3. 若拖到冲突爆发再决策，改造成本将覆盖：harvester 发射路径、manifest/门禁、CMake include 选择、语料矩阵、全部文档。

## 2. 方案比选（Options）

| 方案 | 描述 | 结论 |
|---|---|---|
| A. 永久单树 + overlay 超集 | 所有版本差异靠数据化 overlay 收敛 | 暂用：成本最低；但超集不可持续时必然返工 |
| B. 立即多树 `include/<idf_tag>/` | 每版本一份完整树，CMake 按版本选择 | 否决（现阶段）：无实际双版本并发需求，属过度工程 |
| C. 单树先行 + 预置切换触发器（本 ADR） | 维持 A，但写死退出条件与迁移路径 | **采纳** |

## 3. 决策结论（Decision）

- **D1 现行策略不变**：`include/` 单版本树 + `version_overlays` 数据化差异；开源树内**禁止**用 `#if ESP_IDF_VERSION` 在头文件层做版本分叉（差异必须收敛在收割规则中）。
- **D2 切换触发器（满足任一即启动迁移 RFC/ADR）**：
  - **T1 超集失效**：任一代表性语料在 v5/v6（或未来 vN/vN+1）双版本矩阵下，无法用同一棵树通过编译；
  - **T2 移除型破坏**：官方在目标基线版本中彻底移除本平台 SLA 内的 API/头（如 v7 移除 Legacy I2C），导致超集必须保留已删除符号；
  - **T3 产品需求**：出现"用户工程锁定 IDF 版本并与平台内置版本不同且需同时在线编译"的明确需求。
- **D3 迁移路径（触发器命中后执行，预置决策避免返工）**：
  1. 目录切换为 `include/<idf_tag>/`（artifact 按版本分树，manifest 增加 `tree_idf_tag`）；
  2. `include/current` 符号链接或 CMake 变量 `WINK_IDF_TAG` 选择默认树；`esp_idf_target.cmake` 扩展为 (soc, idf) 双维选择；
  3. 门禁 `check_harvested_headers.py` 增加 `--idf-tag`，`channels.json.relocated` 语义不变；
  4. `version_overlays` 从"单树补丁"降级为"树内差异说明"，不再承担跨版本超集职责。
- **D4 CI 证据**：M3 交付时必须包含 v5.1.3 与 v6.1 至少各一组"语料 compile-only"双版本矩阵（当前为 T-012 遗留项）；未接入 Nightly 前不得宣布多版本兼容完成。

## 4. 影响（Consequences）

| 正面收益 | 约束与代价 |
|---|---|
| 现在零成本，未来按触发器执行，避免临时大重构与争论 | 触发器的监控依赖双版本 CI；若 CI 缺位，T1 可能被延迟发现 |
| 版本差异位置被限定在闭源规则（数据），开源树保持纯净 | 多版本并发需求若突然出现，迁移仍需 3~5 天专项（路径/门禁/CMake/文档） |
| 与 ADR-0030 的 EIM 多版本现实解耦：平台只锁"默认版本" | v7 移除类事件会提前触发 T2，需在官方 EOL 公告后 1 周内评估 |

## 5. 遵循与后续（Compliance & Follow-up）

- [ ] Owner 会签转 Accepted
- [ ] 回写 `docs/zh/design/06-build-toolchain/`（版本选择与 artifact 流程）与 `docs/zh/design/04-wasm-simulation/`（编译矩阵）
- [ ] M3：T-012 双版本矩阵接入 CI；每季度复核 T1/T2 状态（官方 release notes）
- [ ] `include/README.md §3` 增补本 ADR 链接与触发器摘要

---

*该 ADR 状态变更记录：*
- 2026-09-25：Proposed（架构组，随目录层级评审 P1 起草）
