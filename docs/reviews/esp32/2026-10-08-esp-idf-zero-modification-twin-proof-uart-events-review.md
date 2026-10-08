<!-- SPDX-License-Identifier: Apache-2.0 -->
# 评审记录：ESP-IDF 官方示例原厂源码零修改双实证 (Twin-Proof) 首项破零审定与归档

| 字段 | 内容 |
|---|---|
| 评审编号 | `REV-20261008-ZERO-MOD-TWIN-PROOF-UART-EVENTS` |
| 日期 | 2026-10-08 |
| 关联实施计划 | [2026-10-08-esp-idf-zero-modification-twin-proof-breakthrough-plan.md](../../implementation-plans/esp32/2026-10-08-esp-idf-zero-modification-twin-proof-breakthrough-plan.md) |
| 关联技术设计 | [Batch 0 候选证据与正式双实证契约](../../zh/tech-designs/esp32/esp-idf-batch0-evidence-contract.md) |
| 评审对象 | `#098 esp.peripherals.uart.uart_events` 双实证（Twin-Proof）首项正式交付包 |
| 评审结论 | **正式破零审定通过 (APPROVED / AUDITED)**：原厂源码 100% 字节无修改；同一生产产物正向、原生故障与恢复全闭环；断言器自检与故障敏感性检验成立；双 target 构建（wasm32 + ESP32 Xtensa）完整回归通过；正式 Twin-Proof 凭据核验通过。 |

---

## 一、审查范围与上游源码零修改核验

1. **上游源码镜像**：
   - 目标应用目录：`wink-micro-app/vendor/esp_idfv61/peripherals/uart_uart_events`
   - 上游官方路径：`examples/peripherals/uart/uart_events`
   - 业务源文件：`uart_events_example_main.c`
   - 源码 SHA-256：`8d173ac3cd0e1a9fec6dc1bf6de48a865172a43cc5f910635c63e00dbef770de`（6111 字节）
   - **核验结论**：原厂 C 代码字节 100% 保持未改动，无任何测试桩、宏注入或 App 级旁路侵入。

2. **跨靶构建回归 (Dual-Target Build)**：
   - **wasm32-unknown-emscripten**：Emscripten 6.0.9 编译通过，生成生产 Wasm 运行时组件。
   - **ESP32 Xtensa (ESP-IDF v6.1)**：通过 `wink.py esp32` 完整执行 ESP-IDF 原生编译，生成 `bootloader.bin`、`partition-table.bin` 与 `wink_esp32_firmware.bin`（大小 `0x2a0e0` 字节，通过检查分区大小），退出码 0，编译无告警。

---

## 二、同一生产资产约束核对

正向基线、原生故障处理与因果敏感性变异运行于**完全相同的同一冻结生产资产**：

- 资产目录：`wink-micro-app/vendor/esp_idfv61/peripherals/uart_uart_events/unisim-assets/`
  - `wink_simulator.wasm`
  - `wink_simulator.js`
  - `device-tree.json`
- 复合资产摘要（`compute_assets_composite_sha256`）：
  `46addf71b805c72a5e079715d2f363d9aac90d7342d169f1430b25d7779821f5`
- 目标芯片与配置绑定：`mcu = esp32`，`config_id = wasm_sim_standard`。

---

## 三、原生故障接线与场景执行

UniSim 底座已完成 `INJECT_PLATFORM_FAULT` 原生故障注入支持（`packages/unisim/src/scenario/runner/step-table.ts` 与 `simulation-runner/headless/headless-sim-runner.ts`），直接驱动底层 Wasm ABI `pal_wasm_push_uart_rx_error(0, flags=1)`。

### 1. 正向基线执行
- Run ID：`run-20261008-esp.peripherals.uart.uart_events-verified`
- 场景：`peripherals/uart_uart_events/unisim-scenarios/peripherals_uart_uart_events.scenario.json`
  - 场景 SHA-256：`fbbeaf7233cb07e5b3b8eae2ed986f59ab45567abcd8b9e83ae66640c9f1c214`
- 报告：`.governance/reports/peripherals/uart_uart_events/run-report.json`
  - 报告 SHA-256：`e3a1935bd4d93520b443f2bf6e5e3e6f2ddb5c4b5882c8da91b935ede155fb02`
- 结果：7/7 步骤全部 PASSED（虚拟时间 3000000µs，宿主耗时 121ms）。

### 2. 负向故障处理与同实例恢复执行
- Run ID：`run-20261008-esp.peripherals.uart.uart_events-fault-0`
- 场景：`peripherals/uart_uart_events/unisim-scenarios/peripherals_uart_uart_events.fail.scenario.json`
  - 场景 SHA-256：`48144cc4afeeefdbe943694930b96733c3564a9ca2043aeaba4f470fcfc521d8`
- 报告：`.governance/reports/peripherals/uart_uart_events/wasm_sim_standard/negative_case_0.report.json`
  - 报告 SHA-256：`fec64cedcdc0a0f04f89afacf7defe334f024c8fd78f57fc7c1412fd0713e2f5`
- 序列：
  - Step 1~3：前置载荷 `PRE_FRAME_WINK` 发送与 `[DATA EVT]:` 回显通过。
  - Step 4 (800ms)：`INJECT_PLATFORM_FAULT` 注入硬件帧错误（flags=1）。
  - Step 5 (800~1400ms)：`ASSERT_BUS_PAYLOAD` 观测到原厂事件任务打印 `uart frame error`。
  - Step 6~7 (1500~2800ms)：后置载荷 `POST_FRAME_WINK` 发送，验证链路无损恢复，回显正常。
- 结果：7/7 步骤全部 PASSED（虚拟时间 3000000µs，宿主耗时 89ms）。

---

## 四、因果关系与敏感性检验

1. **断言器自检 (Assertion Self-Check)**：
   - 构造断言失败场景（将 Step 5 的期望日志篡改为不存在的字符串 `IMPOSSIBLE_ERROR_STRING_NEVER_LOGGED`）。
   - 运行结果：Step 1~4 正常通过，Step 5 精准报错 `Matched 0 packets`；后续步骤维持 PENDING。
   - 校验器：`validate_scenario_report(..., failure_index=4)` 返回 `(True, 'Selected scenario failed with matching evaluated steps')`，证明 Matcher 具有完全活性。

2. **故障注入敏感性 / 固件因果检验 (Firmware Dependency / Fault Sensitivity)**：
   - 构造无故障对照场景（将 Step 4 故障注入的 `flags` 设为 `0`，即不注入异常）。
   - 运行结果：原厂固件未收到异常事件中断，Step 5 在 `[800ms, 1400ms]` 窗口内未观测到 `uart frame error`，断言失败。
   - 校验器：`validate_scenario_report(..., failure_index=4)` 返回 `True`，证明错误日志严格由异常注入触发，杜绝假绿。

---

## 五、契约修正与凭据审定签署

1. **契约校准说明**：
   - 原 `checklist.data.json` 错误登记为 `expect_error: "ESP_ERR_INVALID_STATE"`。上游零修改固件在遇到 `UART_FRAME_ERR` 事件时实际打出的日志为 `"uart frame error"`。
   - 现校准为：`stimulus: "UART 硬件帧错误事件 (UART_FRAME_ERR)"`，`expect_error: "uart frame error"`，`detects: "硬件帧错误未向应用事件队列上报导致通信异常"`，`sla_error_symbol: "uart frame error"`。契约哈希（SHA-256）为 `942a128bbacb3c7d6d865c3d873208809e61634ddccfd8463d24f64553fc827c`。
2. **审计签署**：
   - 审查人：`arch_team`
   - 审查日期：`2026-10-08T03:40:00Z`
   - 审定配置：`["wasm_sim_standard"]`
   - 签署结论：批准 `#098 esp.peripherals.uart.uart_events` 正式合入双实证凭据并点亮看板徽标。
