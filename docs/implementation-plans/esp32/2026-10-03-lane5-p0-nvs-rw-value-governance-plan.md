<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：Lane 5 P0 标杆 `storage/nvs_rw_value` 官方示例仿真治理闭环

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20261003-LANE5-P0-NVS-RW-VALUE-v1.0 |
| 状态 | 🟢 **Complete (Fully Verified & Signed · 14/14 Baseline)** |
| 日期 | 2026-10-03 |
| 目标平台/SoC | `wasm32-unknown-emscripten` / `esp32 xtensa` 双 Target |
| 工具链/SDK版本| `ESP-IDF v6.1@fff9895c` / `Emscripten 3.1.x` / Node.js 24+ / Python 3.11+ |
| 优先级 | **P0（Lane 5 存储系统与持久化核心基石标杆 · 6 大泳道 100% 破零）** |
| 治理依据 | [governance-sop-esp](../../../.agents/skills/governance-sop-esp/SKILL.md)<br>[ADR-0001：负数错误码标准](../../decisions/core/0001-error-code-sign-convention.md)<br>[ADR-0002：双 Target 同源编译](../../decisions/core/0002-dual-target-compilation.md)<br>[ADR-0004：编译期静态分发](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)<br>[ADR-0012：Fail-Loud 原则](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)<br>[ADR-0083 / ADR-0084：开源许可分层地图](../../decisions/core/0083-open-source-license-boundary.md) |
| 管辖数据源 | `checklist.data.json`（Display ID: 403, `esp.storage.nvs.nvs_rw_value`） |
| 实施目标文件 | `wink-micro-os/frameworks/esp_idf/src/core/esp_nvs.c`（补齐 NVS 迭代器 C-ABI 与类型记账）<br>`wink-micro-app/vendor/esp_idfv61/storage/nvs_nvs_rw_value/`（新建示例应用脚手架）<br>`wink-micro-app/vendor/esp_idfv61/storage/nvs_nvs_rw_value/nvs_value_example_main.c`（原厂零修改镜像，固化 SHA-256）<br>`wink-micro-app/vendor/esp_idfv61/storage/nvs_nvs_rw_value/unisim-scenarios/nvs_nvs_rw_value.scenario.json`（业务因果断言场景）<br>`wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1`（登记 carrier） |
| 验收门禁 | `python -X utf8 -B wink-micro-app/vendor/esp_idfv61/.governance/gates/run_gates.py --gate 1`<br>`python .github/scripts/check_license_map.py`<br>Headless 正向基线通过 + Canary 变异 100% 击杀<br>`evidence_verifier.py --verify-all` 凭据验证全绿（达成 14/14 黄金基准） |

---

## 一、 战略目标与现状分析

### 1.1 背景与破局意义
在 ESP-IDF 官方示例治理清单中：
- **Lane 5: 存储系统（Storage）** 目前实证完成率为 **0% (0/28)**，是 6 大并发泳道中唯一未破零的空白泳道；
- **`storage/nvs/nvs_rw_value`（Display ID: 403）** 是 Lane 5 中唯一的 **P0 核心筑基标杆**；
- 它验证了 NVS 非易失性存储的分区初始化、整型/字符串读写、键值迭代器遍历、键删除与原子提交；是后续 Wi-Fi 凭证持久化、系统配置存取、BLE 绑定信息维护的底层先决条件；
- 攻克该标杆将一举打开 Lane 5 的治理通道，使 6 大并发泳道实现 **100% 破零**。

### 1.2 原厂代码与底座缺口分析
权威原厂路径：`D:\software\embedded-tools\esp-idf\.espressif\v6.1\esp-idf\examples\storage\nvs\nvs_rw_value\main\nvs_value_example_main.c`
固化 SHA-256：`ecaa14df2632d48fe570db6314201e6c3320bcdd359c66862f30b54102f43fee`

1. **Layer A 框架就绪项**：
   - `nvs_flash_init()`、`nvs_flash_erase()` 已在 `esp_nvs.c` 中实现；
   - `nvs_open()`、`nvs_close()` 句柄池与代际令牌机制已就绪；
   - `nvs_set_i32()`、`nvs_get_i32()`、`nvs_set_str()`、`nvs_get_str()`、`nvs_commit()`、`nvs_erase_key()` 已实现；
   - `.sim_sandbox/nvs_storage.bin` 文件沙箱与原子写入机制已具备。
2. **Layer A 框架缺口项**：
   - 现行 `nvs_entry_t` 仅记录键名与原始二进制数据，未记账条目类型 `nvs_type_t type`；
   - 缺少 NVS 迭代器 C-ABI 实现：
     - `nvs_entry_find(const char *part_name, const char *namespace_name, nvs_type_t type, nvs_iterator_t *output_iterator)`；
     - `nvs_entry_info(const nvs_iterator_t iterator, nvs_entry_info_t *out_info)`；
     - `nvs_entry_next(nvs_iterator_t *iterator)`；
     - `nvs_release_iterator(nvs_iterator_t iterator)`。
3. **Layer C 应用载体缺口**：
   - `wink-micro-app/vendor/esp_idfv61/storage/nvs_nvs_rw_value` 尚未落盘；
   - 需零篡改镜像 `nvs_value_example_main.c`，配置 `CMakeLists.txt`、`sdkconfig.h`、`wink-app.json`；
   - 在 `run_esp32_headless_evidence.ps1` 中登记 `nvs_nvs_rw_value` carrier。
4. **Layer S 场景断言编撰**：
   - 编撰 `unisim-scenarios/nvs_nvs_rw_value.scenario.json`；
   - 采用因果断言，匹配控制台关键日志序列：
     - `"Read counter = 42"`
     - `"Read string: Hello from NVS!"`
     - `"Key: 'counter', Type: i32"`
     - `"Key: 'message', Type: str"`
     - `"Returned to app_main"`。

---

## 二、 架构规格与接口设计

### 2.1 C-ABI 头文件契约
全部函数原型已在已固化的 `include/nvs.h` 与 `include/nvs_flash.h` 中导出，无需扩充公共头文件（满足零破坏性变更铁律）。

### 2.2 `esp_nvs.c` 内部增强实现规格
1. 在内部 `nvs_entry_t` 结构体扩充 `nvs_type_t type` 字段；
2. 升级 `nvs_set_typed_blob`，在存入数据时同步记账其真实类型（如 `NVS_TYPE_I32`、`NVS_TYPE_STR`）；
3. 实现 `struct nvs_opaque_iterator_t` 及其遍历步进逻辑，支持 `NVS_TYPE_ANY` 泛型过滤与命名空间约束；
4. 实现 `nvs_release_iterator` 安全释放堆内存。

---

## 三、 执行计划与任务拆解

### Phase 1：Layer A 底座框架 C-ABI 补齐
- [x] **Task 1.1**：在 `wink-micro-os/frameworks/esp_idf/src/core/esp_nvs.c` 中增加 `nvs_type_t type` 记账。
- [x] **Task 1.2**：实现 `nvs_entry_find()`、`nvs_entry_info()`、`nvs_entry_next()`、`nvs_release_iterator()` 完整迭代器生命周期。

### Phase 2：Layer C 应用载体与原厂镜像落盘
- [x] **Task 2.1**：创建目录 `wink-micro-app/vendor/esp_idfv61/storage/nvs_nvs_rw_value/`。
- [x] **Task 2.2**：镜像 `nvs_value_example_main.c`，固化 SHA-256（`ecaa14df2632d48fe570db6314201e6c3320bcdd359c66862f30b54102f43fee`）。
- [x] **Task 2.3**：创建 `CMakeLists.txt`、`include/sdkconfig.h` 与 `wink-app.json`。
- [x] **Task 2.4**：在 `run_esp32_headless_evidence.ps1` 的 `$allCarriers` 数组登记 `nvs_nvs_rw_value`。

### Phase 3：Layer S 高保真因果场景编撰
- [x] **Task 3.1**：编撰 `unisim-scenarios/nvs_nvs_rw_value.scenario.json` 业务因果断言：
  - 断言控制台 `"Opening Non-Volatile Storage"`；
  - 断言 `"Read counter = 42"`；
  - 断言 `"Read string: Hello from NVS!"`；
  - 断言 `"Key: 'counter', Type: i32"` 与 `"Key: 'message', Type: str"`；
  - 断言 `"Returned to app_main"` 正常结束。

### Phase 4：Layer G 闭环验证与 Canary 变异击杀
- [x] **Task 4.1**：运行 Gate 1 静态语义门禁 `run_gates.py --gate 1`。
- [x] **Task 4.2**：运行正向 Headless 仿真，确认基线通过并生成 `run-report.json`。
- [x] **Task 4.3**：执行 Canary 变异击杀验证（如篡改写入值为 99），确保缺陷敏感性 100% 达标。
- [x] **Task 4.4**：运行开源许可分层门禁 `check_license_map.py`。

### Phase 5：Layer G 凭据归档与 14/14 黄金基准交付
- [x] **Task 5.1**：带 `-WriteEvidence` 运行生成官方凭据，更新 `checklist.data.json` 晋升为 `verified` 并生成机器审计签署。
- [x] **Task 5.2**：运行 `generate_checklist_v1_1.py` 重新生成 `CHECKLIST.md`。
- [x] **Task 5.3**：运行 `evidence_verifier.py --verify-all` 进行全量一致性复核（达成 14/14 黄金基准）。
- [x] **Task 5.4**：执行规范 Git 原子提交并汇报成果。
