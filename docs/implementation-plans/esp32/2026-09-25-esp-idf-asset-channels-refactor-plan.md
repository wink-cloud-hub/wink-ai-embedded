# ESP-IDF 门面资产通道与 SoC 数据归属重构实施计划

> **计划编号**：`PLAN-20260925-ESP-IDF-ASSET-CHANNELS`
> **状态**：✅ 开源侧已完成（2026-09-25）；⏳ 闭源 harvester 侧待跟进（见 §4）
> **触发**：目录层级与长期可维护性评审（P0-1/P0-2/P1/P2）
> **关联 ADR**：[ADR-0087](../../decisions/core/0087-esp-idf-asset-channels-and-soc-data-ownership.md)（本计划主决策）、
> [ADR-0085 修订](../../decisions/core/0085-esp-idf-facade-soc-caps-vs-pal-caps-dual-ssot.md)、
> [ADR-0088](../../decisions/core/0088-esp-idf-version-strategy-triggers.md)（版本触发器，Proposed）
> **关联计划**：[`2026-09-26-esp-idf-sim-m3-soc-ci-plan.md`](./2026-09-26-esp-idf-sim-m3-soc-ci-plan.md)（Step 4/4.5 已修订）

---

## 1. 背景与目标

评审结论：`include/` 平铺是 ABI 镜像的硬约束（保留不动），但存在三项结构性债：
1. `include/soc/{soc_caps,gpio_num}.h` 生成数据与 `chips/` `#include_next` 转发半迁移，M3 原计划将手改生成文件破坏 vendoring 门禁；
2. 手写资产无开源侧登记（21 个手写头中 4 个未登记），re-vendor 有覆盖风险；
3. 目标宏（`CONFIG_IDF_TARGET*`）硬编码在 `sdkconfig_base.h`，多 SoC 构建宏污染；CMake 逻辑分散三处。

目标：数据归属单一化（chips/<soc>）、手写/生成通道门禁化（channels.json）、目标选择单源化（esp_idf_target.cmake）。

---

## 2. 开源侧已完成改动（wink-ai-embedded）

| 文件 | 变更 |
|---|---|
| `frameworks/esp_idf/chips/esp32/include/soc/{soc_caps,gpio_num}.h` | 🆕 数据点：由 vendored 生成文件按字节迁移（sha256 与 `manifest.file_hashes` 同值），替换原 `#include_next` 转发壳 |
| `frameworks/esp_idf/include/soc/{soc_caps,gpio_num}.h` | 🗑 删除（共享树禁止同名，防双份真相） |
| `frameworks/esp_idf/channels.json` | 🆕 资产通道登记：`handwritten`(21) / `chips_handwritten`(0) / `relocated`(2) / `default_soc` |
| `.github/scripts/check_harvested_headers.py` | ✏️ 新增 `--channels`；relocation 校验（共享树禁现、目标 sha256 对齐 manifest、Banner 同哈希）、手写登记完备性（共享树 + chips）、`verify.*` 路径 relocation-aware |
| `frameworks/esp_idf/esp_idf_target.cmake` | 🆕 `WINK_ESP_TARGET` 单源 → `WINK_ESP_TARGET_INCLUDE_DIR` / `WINK_IDF_TARGET_DEFINE` / `WINK_IDF_TARGET_STRING_DEFINE` + 缺数据 `FATAL_ERROR` |
| `frameworks/esp_idf/esp_idf_sources.cmake` / `CMakeLists.txt` | ✏️ include 路径改用单源；目标宏 `PUBLIC` 注入 |
| `frameworks/esp_idf/include/sdkconfig_base.h` | ✏️ 删除硬编码 `CONFIG_IDF_TARGET_ESP32` / `CONFIG_IDF_TARGET` |
| `frameworks/esp_idf/test/corpus/legacy_i2c/include/sdkconfig.h` | ✏️ 删除 `CONFIG_IDF_TARGET_ESP32` / `SOC_HP_I2C_NUM` 硬编码（能力宏由 chips 数据提供） |
| `frameworks/esp_idf/test/wasm/esp_idf_wasm_compile.cmake` | ✏️ 复用 target 单源（include 目录 + `CONFIG_IDF_TARGET_*`），不再硬编码 `chips/esp32` |
| `frameworks/esp_idf/{README.md,include/README.md,docs/01,docs/03}` | ✏️ 数据归属、通道登记、门禁命令同步 |
| `docs/decisions/core/{0085,0087,0088}` | 🆕/✏️ ADR 与修订记录 |
| `.github/workflows/harvest-gate.yml` | 🆕 CI 硬门：每次 push/PR 运行通道/迁移/manifest 自锚校验（公开 CI 无闭源 rules，`--rules` 保留本地/发布流水线） |

### 2.1 验收证据（2026-09-25）

- `python .github/scripts/check_harvested_headers.py`：`errors=0`（278 generated + 21 handwritten，2 relocated）
- `... --rules <esp_idf.yaml>`：`errors=0`（`verify.macro_headers` 经 relocation 解析通过）
- 负向用例：篡改 chips 数据 / 共享树重复发布 / 陈旧手写登记 → 均 fail
- `cmake -DTARGET_PLATFORM=host` + 全量构建 + `ctest -L esp_idf`：**28/28 通过**（含 13 个 wasm compile-only）
- `WINK_ESP_TARGET=esp32c3` 在数据缺失时 configure 期 `FATAL_ERROR`（预期；M3 补数据后解除）

### 2.2 测试布局评审结论（P2，延后执行）

`test/core/` 现混放 core 与 drivers 测试。结论：**不在本次拆分**，由 M3 一并处理（M3 将新增
`test_esp_soc_matrix.c`；若现在拆目录会与其文件清单产生双重 churn）。M3 落地时按 `src/` 镜像为
`test/{core,drivers,freertos,run,corpus,wasm,headless}`，并同步 `wink-micro-os/test/CMakeLists.txt` 路径。

---

## 3. 兼容性与回滚

- **manifest 零改动**：删除的 2 个条目保留在 `file_hashes`，由门禁按 relocated 目标校验哈希；`hash` 自锚不含 `file_hashes`，Banner/片段不受影响。
- **构建语义**：`WINK_ESP_TARGET` 默认 `esp32`，行为与改造前一致；目标宏由 CMake 注入替代 `sdkconfig_base.h` 硬编码（同值）。
- **回滚**：恢复 `include/soc/{soc_caps,gpio_num}.h`（manifest 原文件同哈希）并移除 `channels.json.relocated`，即可回到旧态；chips 壳恢复转发（不推荐，`#include_next` 不可移植）。

---

## 4. 闭源 harvester 侧改动清单（待跟进，wink-tools）

> 本仓约定：闭源仓不在本仓库改；以下为规则/实现规格，供 harvester 下一次迭代执行。

1. **规则（`rules/esp_idf.yaml`）**
   - 新增 per-SoC 发射映射（示意，字段名以 harvester schema 为准）：
     ```yaml
     per_soc_emit:
       soc/soc_caps.h: chips/{soc}/include/soc/soc_caps.h
       soc/gpio_num.h: chips/{soc}/include/soc/gpio_num.h
     ```
     共享 `include/` 发射路径跳过这两项；`v5.1.3` overlay 同映射。
   - `verify.macro_headers` 保留 `soc/{soc_caps,gpio_num}.h` 逻辑路径（开源门禁已 relocation-aware 解析），
     或在规则中改为直接列发射后路径，二者取其一并同步文档。
   - `configs` 矩阵：每个 `(soc, idf)` 组合按 `per_soc_emit` 输出对应芯片目录；禁止跨 SoC 互相覆盖。
2. **emitter / manifest**
   - artifact 目录树包含 `chips/<soc>/include/soc/...`；`manifest.file_hashes` 的键保持"逻辑路径"
     （`soc/soc_caps.h`）或新增 `relocated` 段，二选一后与开源门禁保持一致（开源侧当前按 channels.json 解析逻辑路径）。
   - `manifest` 顶层增加 `default_soc`（或由 `configs[0].soc` 固定约定），与 `channels.json.default_soc` 一致。
   - 生成文件 Banner 保持 `Manifest: <hash>` 不变，确保 chips 目标可验。
3. **vendoring SOP**
   - artifact 叠加时**必须保留** `channels.json` 与 `esp_idf_target.cmake`；新 SoC 的 chips 头若为手写版本，
     保持 `channels.json.chips_handwritten` 登记，待收割转正后移出。
   - 门禁命令更新为：`check_harvested_headers.py --rules ... --channels ...`（已写入 `include/README.md §4`）。
4. **harvester ci_gate**
   - 读取 `channels.json.relocated`，在 `--include-mode block` 下不得把 relocated 路径重复发射回共享树，
     否则开源 CI 将 fail（预期拦截）。

---

## 5. 后续任务

- [ ] M3：新增 S3/C3/C6 chips 数据 + `channels.json` 登记 + SoC 矩阵测试（Step 4/4.5 已按 ADR-0087 修订）
- [ ] 闭源 harvester：按 §4 支持 per-SoC 发射；完成后将 `chips_handwritten` 清空（全部转正）
- [x] 门禁接入 CI：`.github/workflows/harvest-gate.yml`（2026-09-25 落盘）
- [ ] ADR-0088 Owner 会签 + 双版本语料矩阵（T-012）
- [ ] 测试目录拆分（并入 M3）
