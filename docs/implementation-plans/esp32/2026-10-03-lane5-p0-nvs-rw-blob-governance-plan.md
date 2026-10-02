<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：Lane 5 标杆 `storage/nvs_rw_blob` 官方示例仿真治理闭环

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20261003-LANE5-NVS-RW-BLOB-v1.0 |
| 状态 | 🟢 **Complete (Fully Verified & Signed · 15/15 Baseline)** |
| 日期 | 2026-10-03 |
| 目标平台/SoC | `wasm32-unknown-emscripten` / `esp32 xtensa` 双 Target |
| 工具链/SDK版本| `ESP-IDF v6.1@fff9895c` / `Emscripten 3.1.x` / Node.js 24+ / Python 3.11+ |
| 优先级 | **P0（Lane 5 存储系统核心支柱 · NVS 复杂结构体与动态数组 Blob 存取）** |
| 治理依据 | [governance-sop-esp](../../../.agents/skills/governance-sop-esp/SKILL.md)<br>[ADR-0001：负数错误码标准](../../decisions/core/0001-error-code-sign-convention.md)<br>[ADR-0002：双 Target 同源编译](../../decisions/core/0002-dual-target-compilation.md)<br>[ADR-0004：编译期静态分发](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)<br>[ADR-0012：Fail-Loud 原则](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)<br>[ADR-0083 / ADR-0084：开源许可分层地图](../../decisions/core/0083-open-source-license-boundary.md) |
| 管辖数据源 | `checklist.data.json`（Display ID: 402, `esp.storage.nvs.nvs_rw_blob`） |
| 实施目标文件 | `wink-micro-os/frameworks/esp_idf/src/core/esp_nvs.c`（核对并保障 Blob 存取与长度探测规范）<br>`wink-micro-app/vendor/esp_idfv61/storage/nvs_nvs_rw_blob/`（新建示例应用脚手架）<br>`wink-micro-app/vendor/esp_idfv61/storage/nvs_nvs_rw_blob/nvs_blob_example_main.c`（原厂零修改镜像，固化 SHA-256）<br>`wink-micro-app/vendor/esp_idfv61/storage/nvs_nvs_rw_blob/unisim-scenarios/nvs_nvs_rw_blob.scenario.json`（业务因果断言场景）<br>`wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1`（登记 carrier） |
| 验收门禁 | `python -X utf8 -B wink-micro-app/vendor/esp_idfv61/.governance/gates/run_gates.py --gate 1`<br>`python .github/scripts/check_license_map.py`<br>Headless 正向基线通过 + Canary 变异 100% 击杀<br>`evidence_verifier.py --verify-all` 凭据验证全绿（达成 15/15 黄金基准） |

---

## 一、 战略目标与现状分析

### 1.1 背景与战略价值
在 ESP-IDF 官方示例治理清单中：
- 前序任务我们已成功交付 **Lane 5 P0 标杆 `storage/nvs_rw_value`（Display ID: 403）**，打通了 NVS 存储引擎、CRC 校验、沙箱二进制读写和迭代器机制，实现 Lane 5 破零；
- **`storage/nvs_rw_blob`（Display ID: 402）** 是 Lane 5 存储体系中与 `nvs_rw_value` 并列的孪生核心支柱；
- 它验证了 NVS 对复杂二进制数据块（Blob）的存取能力，具体包括：
  1. **复杂聚合结构体存取**：`test_data_t`（混合 `uint8_t`, `char[]`, `float[]`, `uint32_t`, `int16_t[]`, `bool` 等多类型紧凑/对齐字段）；
  2. **动态数组追加存取**：先通过 `nvs_get_blob(..., NULL, &required_size)` 探测已有数据长度，动态 `malloc` 分配后读出并追加时间戳，再写回 NVS；
  3. **沙箱持久化验证**：检验多条目、多尺寸 Blob 在沙箱 `.bin` 中的 CRC 完整性与断电恢复。
- 攻克该标杆将实现 **NVS 基础核心两大形态（Value 单值 + Blob 结构体数据块）的 100% 双重闭环**，使 Lane 5 彻底稳固。

### 1.2 原厂代码与底座缺口分析
权威原厂路径：`D:\software\embedded-tools\esp-idf\.espressif\v6.1\esp-idf\examples\storage\nvs\nvs_rw_blob\main\nvs_blob_example_main.c`
固化 SHA-256：`5a2d37cd89b7d12934a1bd2abb512097e1be315b6dbfbbcfa6e39b9dbe7223ba`

1. **Layer A 框架就绪项**：
   - `nvs_flash_init()`、`nvs_flash_erase()` 已实现；
   - `nvs_open()`、`nvs_close()` 句柄池与代际令牌机制已就绪；
   - `nvs_set_blob()`、`nvs_get_blob()`、`nvs_commit()` 已实现；
   - 支持 `out_value == NULL` 时仅获取长度 `*length = entry.len`；
   - 沙箱 `.sim_sandbox/nvs_storage.bin` 文件已支持二进制持久化。
2. **Layer A 框架检查与微调**：
   - 检查 `nvs_get_blob` 在缓冲区长度不足时的错误码是否严格遵循 ESP-IDF 标准返回 `ESP_ERR_NVS_INVALID_LENGTH`；
   - 检查 `NVS_VAL_BUF_SIZE`（当前为 128 字节）是否能容纳 `test_data_t`（约 56 字节）与多轮追加的 `array_data`。
3. **Layer C 应用载体落地**：
   - 创建 `wink-micro-app/vendor/esp_idfv61/storage/nvs_nvs_rw_blob/`；
   - 镜像 `nvs_blob_example_main.c`（零篡改，固化 SHA-256）；
   - 配置 `CMakeLists.txt`、`sdkconfig.h`（集成 UART0 硬件控制台日志桥接）、`wink-app.json`；
   - 注册 carrier 到 `run_esp32_headless_evidence.ps1`。
4. **Layer S 场景断言编撰**：
   - 编撰 `unisim-scenarios/nvs_nvs_rw_blob.scenario.json`；
   - 采用因果断言，匹配控制台关键日志序列：
     - `"Saving test data blob..."`
     - `"Reading test data blob:"`
     - `"ID: 123"`
     - `"Name: Test Sample"`
     - `"Flags: 0xABCD1234"`
     - `"Active: true"`
     - `"Reading array data blob:"`
     - `"Array[0] = "`
     - `"Blob operations completed."`。

---

## 二、 架构规格与接口设计

### 2.1 C-ABI 头文件契约
全部接口均在已导出的 `include/nvs.h` 与 `include/nvs_flash.h` 中，无需改动对外接口契约（满足零破坏性变更铁律）。

### 2.2 存储容量与对齐保障
在 `esp_nvs.c` 中，每个条目的数据缓冲区为 `NVS_VAL_BUF_SIZE = 128` 字节：
- `sizeof(test_data_t) = 1 + 32 + 8 + 4 + 4 + 1 + padding ≈ 52~56` 字节，完全在 128 字节之内；
- `array_data` 初始追加一个 `uint32_t`（4 字节），完全在 128 字节之内；
- 存储记录满足物理与内存对齐约束。

---

## 三、 执行计划与任务拆解

### Phase 1：Layer A 底座框架对齐核验
- [x] **Task 1.1**：核验 `nvs_get_blob` 规范，确保长度探测（`out_value == NULL`）与缓冲区不足时返回 `ESP_ERR_NVS_INVALID_LENGTH`。
- [x] **Task 1.2**：运行现行 NVS 单元测试，确保回归测试全绿通过。

### Phase 2：Layer C 应用载体与原厂镜像落盘
- [x] **Task 2.1**：创建目录 `wink-micro-app/vendor/esp_idfv61/storage/nvs_nvs_rw_blob/`。
- [x] **Task 2.2**：镜像 `nvs_blob_example_main.c`，固化 SHA-256（`5a2d37cd89b7d12934a1bd2abb512097e1be315b6dbfbbcfa6e39b9dbe7223ba`）。
- [x] **Task 2.3**：创建 `CMakeLists.txt`、`include/sdkconfig.h` 与 `wink-app.json`。
- [x] **Task 2.4**：在 `run_esp32_headless_evidence.ps1` 登记 `nvs_nvs_rw_blob`。

### Phase 3：Layer S 高保真因果场景编撰
- [x] **Task 3.1**：编撰 `unisim-scenarios/nvs_nvs_rw_blob.scenario.json` 业务因果断言：
  - 断言 `"Saving test data blob..."`；
  - 断言 `"Reading test data blob:"`；
  - 断言 `"ID: 123"`；
  - 断言 `"Name: Test Sample"`；
  - 断言 `"Flags: 0xABCD1234"`；
  - 断言 `"Active: true"`；
  - 断言 `"Reading array data blob:"`；
  - 断言 `"Array[0] = "`；
  - 断言 `"Blob operations completed."`。
- [x] **Task 3.2**：更新 `checklist.data.json` 中该项的 `scenario_path`。

### Phase 4：Layer G 闭环验证与 Canary 变异击杀
- [x] **Task 4.1**：运行 Gate 1 静态语义门禁 `run_gates.py --gate 1`。
- [x] **Task 4.2**：驱动 LoopRunner 运行正向 Headless 仿真，确认基线通过并生成 `run-report.json`。
- [x] **Task 4.3**：执行 Canary 变异击杀验证，确保缺陷敏感性 100% 达标。
- [x] **Task 4.4**：运行开源许可分层门禁 `check_license_map.py`。

### Phase 5：Layer G 凭据归档与 15/15 黄金基准交付
- [x] **Task 5.1**：更新 `checklist.data.json` 晋升为 `verified` 并生成机器审计签署。
- [x] **Task 5.2**：运行 `generate_checklist_v1_1.py` 刷新 `CHECKLIST.md`。
- [x] **Task 5.3**：运行 `evidence_verifier.py --verify-all` 进行全量一致性复核（达成 15/15 黄金基准）。
- [x] **Task 5.4**：执行规范 Git 原子提交并更新计划文档。
