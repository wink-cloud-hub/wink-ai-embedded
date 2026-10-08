<!-- SPDX-License-Identifier: Apache-2.0 -->
# 评审记录：ESP-IDF 官方示例原厂源码零修改双实证 (Twin-Proof) 第四项审定与归档

| 字段 | 内容 |
|---|---|
| 评审编号 | `REV-20261008-ZERO-MOD-TWIN-PROOF-TASK-WATCHDOG` |
| 日期 | 2026-10-08 |
| 关联实施计划 | [2026-10-08-esp-idf-zero-modification-twin-proof-breakthrough-plan.md](../../implementation-plans/esp32/2026-10-08-esp-idf-zero-modification-twin-proof-breakthrough-plan.md) |
| 关联技术设计 | [Batch 0 候选证据与正式双实证契约](../../zh/tech-designs/esp32/esp-idf-batch0-evidence-contract.md) |
| 评审对象 | `#154 esp.system.task_watchdog` 双实证（Twin-Proof）正式交付包 |
| 评审结论 | **正式审定通过 (APPROVED / AUDITED)**：原厂源码 100% 字节无修改；同一生产产物正向基线与负向受监控任务挂起/看门狗饥饿超时故障处理完整闭环；底层平台级任务挂起与恢复跨 C 运行时内核与 TS 驱动完全打通；变异自检与无故障对照组严格成立（因果性 100% 闭环）；门禁 Gate 1 (12/12) 与全量凭据 (46/46) 全部通过；双实证看板徽标正式点亮（首批 4 项标杆全部完成，达 4/4 项）。 |

---

## 一、审查范围与上游源码零修改核验

1. **上游源码镜像**：
   - 目标应用目录：[system/task_watchdog](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/system/task_watchdog)
   - 上游官方路径：`examples/system/task_watchdog`
   - 业务源文件：[task_watchdog_example_main.c](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/system/task_watchdog/task_watchdog_example_main.c)
   - **核验结论**：原厂 C 代码字节 100% 保持未改动（0 差异），无任何测试桩注入、延迟插入或 App 级旁路侵入。

2. **跨靶构建回归 (Dual-Target Build)**：
   - **wasm32-unknown-emscripten**：Emscripten 编译通过，生成生产 Wasm 运行时组件。
   - **ESP32 Xtensa (ESP-IDF v6.1)**：同源 C 代码完全兼容原生编译。

---

## 二、同一生产资产约束核对

正向基线与负向故障处理运行于**完全相同的同一冻结生产资产**：

- 资产目录：[unisim-assets](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/system/task_watchdog/unisim-assets)
  - `wink_simulator.wasm`
  - `wink_simulator.js`
  - `device-tree.json`
- 复合资产摘要（`compute_assets_composite_sha256`）：
  `40d7fb60a22fbc4cdf9c8cca52be8c5a07623f3485b3d849e7891d9928b5a975`
- 目标芯片与配置绑定：`mcu = esp32`，`config_id = wasm_sim_standard`。

---

## 三、底层故障机制突破：外部受控任务饥饿与看门狗超时

1. **外部任务挂起与恢复抽象**：
   - 官方示例设计为工作任务 `task` 在循环中持续调用 `esp_task_wdt_reset()`、`func_a()`、`func_b()`。为了在**不修改任何 C 源码**的前提下触发看门狗警报，必须从外部环境注入故障使该任务暂停喂狗。
   - 在 C 运行时内核 [esp_sim_fault.h](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/core/esp_sim_fault.h) 与 [freertos_task.c](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/freertos/freertos_task.c) 中实现：
     - `sim_esp_task_suspend(const char *task_name)`：通过任务名查找对应 `esp_tcb_t`，调用 `vTaskSuspend()`。纤程调度器将其置为 `SIM_TASK_STATE_BLOCKED` 并清除唤醒时间，使该任务不再获得运行时间片。
     - `sim_esp_task_resume(const char *task_name)`：调用 `vTaskResume()`，将纤程重新置为 `SIM_TASK_STATE_READY`，使任务恢复调度并恢复喂狗。
   - 跨导出符号表 [exported_runtime_functions.json](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/targets/wasm/exported_runtime_functions.json) 导出 `_sim_esp_task_suspend` 与 `_sim_esp_task_resume`。
2. **TS 仿真运行时故障驱动对接**：
   - 在 [headless-sim-runner.ts](file:///d:/workspaces/ai-coding/wink-ai/wink-ai/packages/unisim/src/simulation-runner/headless/headless-sim-runner.ts) 中增加 `domain === 'task' || domain === 'freertos'` 故障分发，支持由场景指令直接调用挂起与恢复：
     - `INJECT_PLATFORM_FAULT` `{ "domain": "task", "fault": { "action": "suspend", "task": "task" } }`
     - `INJECT_PLATFORM_FAULT` `{ "domain": "task", "fault": { "action": "resume", "task": "task" } }`
3. **FreeRTOS 定时器守护进程与看门狗判定闭环**：
   - 当任务 `task` 被挂起后，TWDT 定时器超时（3000ms）。FreeRTOS 定时器守护进程 `timer_daemon` 准时在虚拟时间 5001ms 触发，调用 `twdt_timeout()` -> `esp_task_wdt_print_triggered_tasks()`，准确打印超时警告及未重置的任务与用户句柄列表。
   - 在 7500ms 注入恢复故障后，`task` 被唤醒并恢复执行，最终在 10000ms 主任务完成等待后安全退订并输出 `Example complete`。

---

## 四、场景执行与因果闭环

### 1. 正向基线执行 (Positive Baseline)
- Run ID：`run-20261008-esp.system.task_watchdog-verified`
- 场景：[system_task_watchdog.scenario.json](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/system/task_watchdog/unisim-scenarios/system_task_watchdog.scenario.json)
  - 场景 SHA-256：`2cfd3749ad08e212901176d8896a6d8f5719d67b5fb23bfc4fecfaadace4fee1`
- 报告：[run-report.json](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/.governance/reports/system/task_watchdog/run-report.json)
  - 报告 SHA-256：`819442796bba7de654e12cc5c18c01768bf34108a9757ee3eedca31862e81769`
- 结果：7/7 步骤全部 PASSED（虚拟时间 15000000µs，宿主耗时 147ms）。包含 TWDT 初始化、订阅、延时监控、退订、反初始化及成功完成。

### 2. 负向 Case 0：受监控任务挂起导致看门狗饥饿超时处理 (Negative Case 0)
- 契约：
  - `stimulus`: `外部挂起受监控任务导致看门狗饥饿超时`
  - `expect_error`: `Task watchdog got triggered`
  - `detects`: `受监控任务未及时喂狗时看门狗报警`
  - `contract_sha256`: `d47f98fe11c9bb0d8bbec3049581ec01c778401340026e63dc4e9fa8cf998344`
- Run ID：`run-20261008-esp.system.task_watchdog-fault-0`
- 场景：[system_task_watchdog.fail.scenario.json](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/system/task_watchdog/unisim-scenarios/system_task_watchdog.fail.scenario.json)
  - 场景 SHA-256：`34942aa2fad1e59d26e86c94323f2ab3057e9542d0dc2588bb1fca76fc79e096`
- 报告：[negative_case_0.report.json](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/.governance/reports/system/task_watchdog/wasm_sim_standard/negative_case_0.report.json)
  - 报告 SHA-256：`b101ab71f8cd714c30de4ac21e48b84969132fc6e85c613f26169a1a587e644e`
- 索引：`stimulus_step_index = 3`，`assertion_step_index = 4`。
- 结果：11/11 步骤全部 PASSED（虚拟时间 15000000µs，宿主耗时 154ms）。完整捕获看门狗超时报警、未重置 task 及 func_a/func_b 用户句柄，并在恢复后安全完成全流程。

### 3. 因果敏感性与变异核验 (Causality & Mutation Proof)
- **变异 1（断言器自检）**：将负向断言修改为不存在的字符串 `Task watchdog NEVER triggered xyz_impossible_string`，执行快速失败（Exit Code 1，Matched 0 packets），证明断言具备活性，未发生空跑假绿。
- **变异 2（无故障对照组）**：移除故障注入直接运行原厂固件并断言看门狗报警，执行因任务持续正常喂狗而在断言窗口内未收到任何报警包（Matched 0 packets）而失败，证明看门狗报警完全由任务暂停故障因果诱发。

---

## 五、凭据审定签署与门禁核验

1. **登记与审计信息**：
   - 条目 ID：`esp.system.task_watchdog`（Display ID: 154）
   - 审计责任人：`arch_team`
   - 审计覆盖配置：`["wasm_sim_standard"]`
   - 审定结论：`audited`
   - 独立审计覆盖规则校验通过，移除了历史占位 `loop_sop_daemon`。

2. **自动化门禁复核**：
   - `python run_gates.py --gate 1`：**12/12 PASS**。
   - `python evidence_verifier.py --verify-all`：**46/46 PASS**（含 #154）。
   - `twin_evidence.verify_twin_evidence()`：**PASS**（`Same configuration has complete positive and fault-handling evidence`）。
   - `python generate_checklist_v1_1.py`：派生看板生成成功，全库 Twin-Proof 徽标数正式达成目标：**4 项**（4/4 标杆全部交付）。
   - `python check_license_map.py`：开源许可证合规校验完全通过。
   - `winkcli lint --pack layering --pack api`：零分层违规发现。

---

## 六、突破计划总结与后续工作

本项评审标志着 [2026-10-08-esp-idf-zero-modification-twin-proof-breakthrough-plan.md](../../implementation-plans/esp32/2026-10-08-esp-idf-zero-modification-twin-proof-breakthrough-plan.md) 中规划的**全部 4 个关键业务领域代表标杆示例**零修改双实证全部圆满攻克：
1. **外设总线事件领域**：`#098 esp.peripherals.uart.uart_events`（硬件错误注入与事件队列溢出闭环）
2. **应用层网络协议领域**：`#196 esp.protocols.http_server.simple`（虚拟 HTTP 客户端错误方法探测与 404 处置闭环）
3. **非易失存储领域**：`#403 esp.storage.nvs.nvs_rw_value`（底层 NVS 块介质损坏故障与开机预注入闭环）
4. **系统核心内核与看门狗领域**：`#154 esp.system.task_watchdog`（平台级任务挂起饥饿、看门狗超时报警与恢复闭环）

所有 4 项均严格满足原厂 C 源码 100% 字节无修改、同一生产产物双向实证、因果突变 100% 敏感、Gate 1 与全量凭据自动校验通过的最高工程治理标准。
