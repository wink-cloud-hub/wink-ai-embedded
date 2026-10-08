<!-- SPDX-License-Identifier: Apache-2.0 -->
# 评审记录：ESP-IDF 官方示例原厂源码零修改双实证 (Twin-Proof) 第三项审定与归档

| 字段 | 内容 |
|---|---|
| 评审编号 | `REV-20261008-ZERO-MOD-TWIN-PROOF-NVS-RW-VALUE` |
| 日期 | 2026-10-08 |
| 关联实施计划 | [2026-10-08-esp-idf-zero-modification-twin-proof-breakthrough-plan.md](../../implementation-plans/esp32/2026-10-08-esp-idf-zero-modification-twin-proof-breakthrough-plan.md) |
| 关联技术设计 | [Batch 0 候选证据与正式双实证契约](../../zh/tech-designs/esp32/esp-idf-batch0-evidence-contract.md) |
| 评审对象 | `#403 esp.storage.nvs.nvs_rw_value` 双实证（Twin-Proof）正式交付包 |
| 评审结论 | **正式审定通过 (APPROVED / AUDITED)**：原厂源码 100% 字节无修改；同一生产产物正向基线与负向介质损坏故障处理完整闭环；冷启动阶段平台故障注入时机与真实底层错误名映射完全打通；变异自检与无故障对照组严格成立（因果性 100% 闭环）；门禁 Gate 1 (12/12) 与全量凭据 (46/46) 全部通过；双实证看板徽标正式点亮（累计 3 项）。 |

---

## 一、审查范围与上游源码零修改核验

1. **上游源码镜像**：
   - 目标应用目录：[storage/nvs_nvs_rw_value](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/storage/nvs_nvs_rw_value)
   - 上游官方路径：`examples/storage/nvs/nvs_rw_value`
   - 业务源文件：[nvs_value_example_main.c](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/storage/nvs_nvs_rw_value/nvs_value_example_main.c)
   - **核验结论**：原厂 C 代码字节 100% 保持未改动（0 差异），无任何测试桩注入、延迟插入或 App 级旁路侵入。

2. **跨靶构建回归 (Dual-Target Build)**：
   - **wasm32-unknown-emscripten**：Emscripten 编译通过，生成生产 Wasm 运行时组件。
   - **ESP32 Xtensa (ESP-IDF v6.1)**：同源 C 代码完全兼容原生编译。

---

## 二、同一生产资产约束核对

正向基线与负向故障处理运行于**完全相同的同一冻结生产资产**：

- 资产目录：[unisim-assets](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/storage/nvs_nvs_rw_value/unisim-assets)
  - `wink_simulator.wasm`
  - `wink_simulator.js`
  - `device-tree.json`
- 复合资产摘要（`compute_assets_composite_sha256`）：
  `87a25322192a5ee04331ca549f0be99a478e892d4758fcffd1380dba0e482b6e`
- 目标芯片与配置绑定：`mcu = esp32`，`config_id = wasm_sim_standard`。

---

## 三、底层故障机制突破与错误名映射

1. **冷启动阶段平台故障注入时机闭环**：
   - 问题根因：ESP-IDF 示例在 `simEngine.init()` 期间即启动执行 `app_main()`。常规场景步骤在 `session.dispatchDue()` 派发，晚于虚拟时间 0 的初始读写操作。
   - 架构解法：在 [headless-sim-runner.ts](file:///d:/workspaces/ai-coding/wink-ai/wink-ai/packages/unisim/src/simulation-runner/headless/headless-sim-runner.ts) 中增加启动前预注入机制（Pre-boot fault injection），对于 `timeUs === 0` 的平台介质故障，在 `simEngine.init()` 启动前预先装载至活跃故障表（`s_active_faults`），确保固件开机首轮读写立即生效。
2. **底层 NVS 错误名称标准映射**：
   - 在 [esp_err.c](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/core/esp_err.c) 中完整补齐 `ESP_ERR_NVS_*` 错误码在 `esp_err_to_name()` 中的映射表，使得原厂代码打印真实的 `ESP_ERR_NVS_CORRUPT_KEY_PART`，取代原有的 `UNKNOWN ERROR`。

---

## 四、场景执行与因果闭环

### 1. 正向基线执行 (Positive Baseline)
- Run ID：`run-20261008-esp.storage.nvs.nvs_rw_value-verified`
- 场景：[nvs_nvs_rw_value.scenario.json](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/storage/nvs_nvs_rw_value/unisim-scenarios/nvs_nvs_rw_value.scenario.json)
  - 场景 SHA-256：`be09ec0daf23db0803b4d8c0f55435d384c695dc19acf055fc0865202094f363`
- 报告：[run-report.json](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/.governance/reports/storage/nvs_nvs_rw_value/run-report.json)
  - 报告 SHA-256：`76505e186934b1f54f437d216f246a8df745da7fc7c6a50b3c0f29bde5605629`
- 结果：7/7 步骤全部 PASSED（虚拟时间 3000000µs，宿主耗时 116ms）。成功断言供电轨、写入读出 counter (42)、字符串读出、迭代器遍历。

### 2. 负向 Case 0：NVS 介质校验异常损坏故障处理 (Negative Case 0)
- 契约：
  - `stimulus`: `注入底层 NVS 页损坏故障 (FAULT_NVS_READ_CORRUPT)`
  - `expect_error`: `ESP_ERR_NVS_CORRUPT_KEY_PART`
  - `detects`: `读页损坏时误报成功`
  - `contract_sha256`: `4b50ba711e9a9866b65143bcebab4ab3273619d59ab2fe98a282187102a87cc5`
- Run ID：`run-20261008-esp.storage.nvs.nvs_rw_value-fault-0`
- 场景：[nvs_nvs_rw_value.fail.scenario.json](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/storage/nvs_nvs_rw_value/unisim-scenarios/nvs_nvs_rw_value.fail.scenario.json)
  - 场景 SHA-256：`9695ab81afdf74b095053f42be7cf8dd381074ad509692a96683810f951febb2`
- 报告：[negative_case_0.report.json](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/.governance/reports/storage/nvs_nvs_rw_value/wasm_sim_standard/negative_case_0.report.json)
  - 报告 SHA-256：`0f48e549d3fe922dbe20e1507f449f2899aed4a1c8405f74a26320b0ae793739`
- 索引：`stimulus_step_index = 1`，`assertion_step_index = 2`。
- 结果：4/4 步骤全部 PASSED（虚拟时间 3000000µs，宿主耗时 122ms）。原厂代码准确进入错误分支打印 `Error (ESP_ERR_NVS_CORRUPT_KEY_PART) reading!`。

### 3. 因果敏感性与变异核验 (Causality & Mutation Proof)
- **变异 1（断言器自检）**：将负向断言改为不存在的错误 `ESP_ERR_NVS_IMPOSSIBLE_MUTANT`，执行快速失败（Exit Code 1，Matched 0 packets），证明断言具备活性，未发生空跑假绿。
- **变异 2（无故障对照组）**：移除故障注入直接运行原厂固件并断言故障错误，执行因实际输出为 `Read counter = 42` 而失败（Matched 0 packets），证明错误输出完全由故障注入因果诱发，非默认行为或缓存输出。

---

## 五、凭据审定签署与门禁核验

1. **登记与审计信息**：
   - 条目 ID：`esp.storage.nvs.nvs_rw_value`（Display ID: 403）
   - 审计责任人：`arch_team`
   - 审计覆盖配置：`["wasm_sim_standard"]`
   - 审定结论：`audited`
   - 独立审计覆盖规则校验通过，移除了历史占位 `loop_sop_daemon`。

2. **自动化门禁复核**：
   - `python run_gates.py --gate 1`：**12/12 PASS**。
   - `python evidence_verifier.py --verify-all`：**46/46 PASS**（含 #403）。
   - `twin_evidence.verify_twin_evidence()`：**PASS**（`Same configuration has complete positive and fault-handling evidence`）。
   - `python generate_checklist_v1_1.py`：派生看板生成成功，全库 Twin-Proof 徽标数由 2 项正式递增至 **3 项**。
   - `python check_license_map.py`：开源许可证合规校验完全通过。
   - `winkcli lint --pack layering --pack api`：零分层违规发现。

---

## 六、归档与后续计划

本评审记录即日起归档生效。至此，实施计划中规定的前三项试点（#098 UART Events、#196 HTTP Server Simple、#403 NVS Read/Write Value）已全部以 100% 源码零修改姿态圆满交付。第四项试点（#154 Task Watchdog）将按计划预研外部漏喂通路后推进。
