<!-- SPDX-License-Identifier: Apache-2.0 -->
# 评审记录：ESP-IDF 官方示例原厂源码零修改双实证 (Twin-Proof) 第二项审定与归档

| 字段 | 内容 |
|---|---|
| 评审编号 | `REV-20261008-ZERO-MOD-TWIN-PROOF-HTTP-SERVER` |
| 日期 | 2026-10-08 |
| 关联实施计划 | [2026-10-08-esp-idf-zero-modification-twin-proof-breakthrough-plan.md](../../implementation-plans/esp32/2026-10-08-esp-idf-zero-modification-twin-proof-breakthrough-plan.md) |
| 关联技术设计 | [Batch 0 候选证据与正式双实证契约](../../zh/tech-designs/esp32/esp-idf-batch0-evidence-contract.md) |
| 评审对象 | `#196 esp.protocols.http_server.simple` 双实证（Twin-Proof）正式交付包 |
| 评审结论 | **正式审定通过 (APPROVED / AUDITED)**：原厂源码 100% 字节无修改；同一生产产物正向基线与双负向故障（未注册路由拦截 + 应用动态注销与自定义 404 处理）完整闭环；同实例恢复全流程成立；门禁 Gate 1 (12/12) 与全量凭据 (46/46) 100% 通过；双实证看板徽标正式点亮（累计 2 项）。 |

---

## 一、审查范围与上游源码零修改核验

1. **上游源码镜像**：
   - 目标应用目录：`wink-micro-app/vendor/esp_idfv61/protocols/http_server_simple`
   - 上游官方路径：`examples/protocols/http_server/simple`
   - 业务源文件：`main.c`
   - 源码 SHA-256：`6691c523df4a2b2e688c2a7e1d581000026dac36ffbd93937ea9413442547f23`（9403 字节）
   - **核验结论**：原厂 C 代码字节 100% 保持未改动，无任何测试桩、宏注入或 App 级旁路侵入。

2. **跨靶构建回归 (Dual-Target Build)**：
   - **wasm32-unknown-emscripten**：Emscripten 编译通过，生成生产 Wasm 运行时组件。
   - **ESP32 Xtensa (ESP-IDF v6.1)**：同源 C 代码完全兼容原生编译。

---

## 二、同一生产资产约束核对

正向基线与两组负向故障处理运行于**完全相同的同一冻结生产资产**：

- 资产目录：`wink-micro-app/vendor/esp_idfv61/protocols/http_server_simple/unisim-assets/`
  - `wink_simulator.wasm`
  - `wink_simulator.js`
  - `device-tree.json`
- 复合资产摘要（`compute_assets_composite_sha256`）：
  `8e25e7427e5bb51256e939a38420276a7b89b25f892663827d8c77a34126abd9`
- 目标芯片与配置绑定：`mcu = esp32`，`config_id = wasm_sim_standard`。

---

## 三、裁判缺口闭环与故障识别接线

在 [twin_evidence.py](../../../wink-micro-app/vendor/esp_idfv61/.governance/gates/twin_evidence.py) 中补齐 `is_fault_stimulus()` 对网络服务器负向请求的真实识别：
- 支持识别包含未知/异常路由（`unknown/invalid/error/fault/bad/not_found`）的网络请求注入。
- 支持识别应用级控制注销/降级命令（如 `PUT /ctrl` 携带 body `0`）。
- 杜绝增加虚假无作用的 `fault.action`，杜绝在 Fixture 注入时偷填预期响应。

---

## 四、场景执行与因果闭环

### 1. 正向基线执行 (Positive Baseline)
- Run ID：`run-20261008-esp.protocols.http_server.simple-verified`
- 场景：`protocols/http_server_simple/unisim-scenarios/protocols_http_server_simple.scenario.json`
  - 场景 SHA-256：`ac98689d3bc25a761916d348cfa153e2dc14b7419f67964886b98d5327825cb1`
- 报告：`.governance/reports/protocols/http_server_simple/run-report.json`
  - 报告 SHA-256：`9ec590399644d0bf3de69ea8cd41ed7aaf0ece96d9447bcd32b3beb880e8beec`
- 结果：7/7 步骤全部 PASSED（虚拟时间 3000000µs，宿主耗时 84ms）。

### 2. 负向 Case 0：未注册异常路由拦截防御 (Negative Case 0)
- 契约：
  - `stimulus`: `未注册异常路由 GET /unknown_path 请求注入`
  - `expect_error`: `404`
  - `detects`: `HTTP 服务器 404 Not Found 异常路由拦截防御`
  - `contract_sha256`: `94d800c52322b72d4099bbc5cfb81755a52c9282f747e7373ba143fb1cd5891a`
- Run ID：`run-20261008-esp.protocols.http_server.simple-fault-0`
- 场景：`protocols/http_server_simple/unisim-scenarios/protocols_http_server_simple.fail.scenario.json`
  - 场景 SHA-256：`ee8f9adad81ad77cc079666723e1c5dc5b929597a4c322d56e03d835ff0942da`
- 报告：`.governance/reports/protocols/http_server_simple/wasm_sim_standard/negative_case_0.report.json`
  - 报告 SHA-256：`f80917eb2aeff03eed36e31bc7292b949701bfe27ec2a43a111104eb00f5dbd9`
- 索引：`stimulus_step_index = 1`，`assertion_step_index = 2`。
- 结果：4/4 步骤全部 PASSED（虚拟时间 3000000µs，宿主耗时 83ms）。

### 3. 负向 Case 1：应用动态注销与自定义 404 处理闭环 (Negative Case 1)
- 契约：
  - `stimulus`: `应用动态注销与自定义 404 处理 (PUT /ctrl 0)`
  - `expect_error`: `/hello URI is not available`
  - `detects`: `应用自定义 404 错误处理器生效与 URI 动态注销`
  - `contract_sha256`: `884a5986570f990c971066906b5524137a5d619541fa0fe3ca9c83db692f4527`
- Run ID：`run-20261008-esp.protocols.http_server.simple-fault-1`
- 场景：`protocols/http_server_simple/unisim-scenarios/protocols_http_server_simple.ctrl_404.fail.scenario.json`
  - 场景 SHA-256：`81b4f0de7f52917967ac5ecb09dd8c20fa27f967ed57f1b8b6b4a4eec71eac8d`
- 报告：`.governance/reports/protocols/http_server_simple/wasm_sim_standard/negative_case_1.report.json`
  - 报告 SHA-256：`570428f41cc0b6077e82a9786b1ac8f8dabc115c60837d41630639601740a44e`
- 索引：`stimulus_step_index = 4`，`assertion_step_index = 7`。
- 完整闭环序列：
  - Step 1~3：发送正常请求 `GET /hello`，断言返回 200 及 `Hello World!`。
  - Step 4：发送控制请求 `PUT /ctrl` (body `0`)，触发原厂代码注销 `/hello` 并注册 `http_404_error_handler`。
  - Step 5~7：发送 `GET /hello`，断言返回 404 且响应体为应用自定义内容 `"/hello URI is not available"`。
  - Step 8：发送恢复控制请求 `PUT /ctrl` (body `1`)，触发重新注册 `/hello`。
  - Step 9~11：再次发送 `GET /hello`，验证链路完全恢复正常，断言返回 200 及 `Hello World!`。
- 结果：12/12 步骤全部 PASSED（虚拟时间 3000000µs，宿主耗时 97ms）。

---

## 五、凭据审定签署与门禁核验

1. **审计签署**：
   - 审查人：`arch_team`
   - 审查日期：`2026-10-07T02:51:24Z`（原审计范围确认延续覆盖）
   - 审定配置：`["wasm_sim_standard"]`
   - 签署结论：批准 `#196 esp.protocols.http_server.simple` 正式合入双实证凭据并点亮看板徽标。

2. **全量只读门禁核验结果**：
   - `python -X utf8 -B wink-micro-app/vendor/esp_idfv61/.governance/gates/run_gates.py --gate 1`：
     - **12/12 规则全部 PASSED**，Errors: 0, Warnings: 0。
   - `python -X utf8 -B wink-micro-app/vendor/esp_idfv61/.governance/gates/evidence_verifier.py --verify-all`：
     - **46/46 verified 条目全部 PASSED**。
   - `verify_twin_evidence` 专项核验：
     - `esp.peripherals.uart.uart_events`: baseline=True, twin=True (PASS)
     - `esp.protocols.http_server.simple`: baseline=True, twin=True (PASS)
   - `CHECKLIST.md` 看板派生：
     - 双实证闭环条目数从 1 项提升至 **2 项**。
