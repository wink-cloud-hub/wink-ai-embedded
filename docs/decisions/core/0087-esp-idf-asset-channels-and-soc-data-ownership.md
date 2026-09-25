# ADR-0087：ESP-IDF 门面资产通道登记与 SoC 能力数据归属

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳，2026-09-25）** |
| 日期 | 2026-09-25 |
| 触发 | 目录层级评审（P0-1/P0-2）：① `include/soc/{soc_caps,gpio_num}.h` 生成数据与 `chips/` `#include_next` 转发半迁移，M3 计划将手改生成文件并破坏 manifest 门禁；② 开源侧无手写资产登记，21 个手写头中 4 个（`driver/i2c.h`、`driver/i2c_types_legacy.h`、`esp_pm.h`、`esp_timer.h`）未在任何清单中登记 |
| 影响范围 | `wink-micro-os/frameworks/esp_idf/{chips,include,channels.json,esp_idf_target.cmake,CMakeLists.txt,esp_idf_sources.cmake}`；`.github/scripts/check_harvested_headers.py`；闭源 harvester 规则（另出改动清单） |
| 决策者 | 项目架构团队 / Owner |
| **关联 ADR** | [ADR-0085](0085-esp-idf-facade-soc-caps-vs-pal-caps-dual-ssot.md)（本 ADR 修订其 D3 数据落位）、[ADR-0086](0086-harvested-headers-license-contract.md)（收割许可契约）、[ADR-0012](0012-contract-honesty-over-silent-degradation.md)（Fail-Loud）、[ADR-0004](0004-static-dispatch-vs-runtime-ops.md)（静态分发） |
| **关联计划** | `docs/implementation-plans/esp32/2026-09-25-esp-idf-asset-channels-refactor-plan.md`；`2026-09-26-esp-idf-sim-m3-soc-ci-plan.md`（Step 4 已按本 ADR 修订） |

---

## 1. 背景（Context）

1. 现行 vendoring 把 esp32 的 `soc_caps.h`/`gpio_num.h` 生成到共享 `include/soc/`，`chips/esp32` 仅以 `#include_next` 转发——这只是"入口语义"迁移，数据归属未定；总纲 §3.2.4（v3.2）又明文"严禁 `#include_next`（MSVC 不支持）"，文档与现实矛盾。
2. M3 计划 Step 4 拟把生成文件改写为手写 `#if CONFIG_IDF_TARGET_*` 分发体，会破坏 `manifest.file_hashes`/Banner 门禁与"include/ 100% 机器产出"铁律。
3. 手写资产（豁免/门面扩展）只在闭源 `rules/esp_idf.yaml` 中登记，开源仓无法自证所有权；一次 re-vendor 可能无声覆盖手写头。

## 2. 方案比选（Options）

| 方案 | 描述 | 结论 |
|---|---|---|
| A. 维持 `#include_next` + 手改生成分发体 | 修 M3 计划原文 | 否决：破坏 vendoring 门禁；`#include_next` 不可移植；数据仍非按 SoC 生成 |
| B. `chips/<soc>/` 收数据，`include/` 去同名 | 选片靠 CMake include 顺序；数据由收割器按 SoC 发射（过渡期字节迁移） | **采纳** |
| C. `include/soc/<soc>/` 收数据，`chips/` 手写分发 | 与 A 同源问题：开源树内新增手写分发层，数据虽按 SoC 但入口仍手写 | 否决 |

## 3. 决策结论（Decision）

- **D1 数据归属**：`soc/soc_caps.h`、`soc/gpio_num.h` 的 per-SoC 数据物理归属 `chips/<soc>/include/soc/`；共享 `include/soc/` 不得再出现同名文件（禁止双份真相）。选片由 CMake include 顺序完成（`chips/${WINK_ESP_TARGET}/include` 在 `include/` 之前），**禁止 `#include_next`**。
- **D2 过渡期字节迁移**：首个落地（esp32）以 vendored 产物按字节迁移，sha256 与 `manifest.file_hashes` 同值，Banner/Manifest 保持可验；后续 SoC 由收割器按配置发射（<soc> 目录），规则改动清单见实施计划。未登记的手工落位由门禁拒绝。
- **D3 开源侧资产登记 `channels.json`**：登记 `handwritten`（共享树手写头）、`chips_handwritten`（芯片目录手写头）、`relocated`（迁移映射）与 `default_soc`；由 `check_harvested_headers.py` 强制：
  - 未登记的手写头 / 陈旧登记 → fail；
  - `relocated` 条目仍出现在共享树 → fail；目标缺失或 sha256 漂移 → fail；
  - 迁移目标 Banner 哈希与 manifest 不一致 → fail。
- **D4 目标宏 SSOT**：`CONFIG_IDF_TARGET_*` / `CONFIG_IDF_TARGET` 由 `esp_idf_target.cmake` 从 `WINK_ESP_TARGET` 派生并经 `wink_framework_esp_idf`（PUBLIC）注入；`sdkconfig_base.h` 禁止硬编码目标宏。未提供数据的目标在 configure 期 `FATAL_ERROR`（Fail-Loud，不静默回退 esp32）。

## 4. 影响（Consequences）

| 正面收益 | 约束与代价 |
|---|---|
| 数据归属单一、无转发、MSVC 安全；ADR-0085 语义真正落地 | 闭源 harvester 需支持按 (soc) 输出路径并更新 manifest/rules（改动清单另行跟踪） |
| 手写头不可再"隐身"；re-vendor 覆盖风险被门禁前移阻断 | 新增手写头必须同步登记 `channels.json`，否则 CI fail |
| `WINK_ESP_TARGET` 单一入口，测试/语料/门面共享，消除宏遗漏 | 未提供数据的 SoC 在 configure 期直接失败（预期行为，M3 补数据后解除） |
| 生成元数据（manifest/片段）保持原位零改动，N-1 回滚不受影响 | `relocated` 语义由开源侧门禁解释，闭源 artifact 消费者需知晓 |

## 5. 遵循与后续（Compliance & Follow-up）

- [x] `channels.json` 落盘并接入门禁（负向用例：篡改芯片数据 / 重复发布 / 陈旧登记均 fail）
- [x] `chips/esp32/include/soc/{soc_caps,gpio_num}.h` 数据迁移（sha256 与 manifest 一致）；`include/soc` 同名删除
- [x] `esp_idf_target.cmake` 单源 + 目标宏 PUBLIC 注入 + 缺数据 FATAL_ERROR；`sdkconfig_base.h`/legacy_i2c 语料去硬编码
- [x] host 构建 + `ctest -L esp_idf` 28/28 通过；`check_harvested_headers.py [--rules]` 双绿
- [x] CI 硬门 `.github/workflows/harvest-gate.yml` 落盘；`check_license_map.py` 绿
- [ ] 闭源 harvester：按 SoC 发射 + `verify.macro_headers` 路径/清单更新（见实施计划 §闭源改动清单）
- [ ] M3 新增 SoC 时登记 `channels.json` 并复用本机制（M3 计划已修订）

---

*该 ADR 状态变更记录：*
- 2026-09-25：Proposed → **Accepted**（Owner 会话批准，方案 B；证据：门禁负向用例 + host ctest 28/28 + manifest 哈希同值迁移）
