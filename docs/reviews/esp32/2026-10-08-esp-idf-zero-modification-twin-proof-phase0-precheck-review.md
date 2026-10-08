<!-- SPDX-License-Identifier: Apache-2.0 -->
# 评审记录：ESP-IDF 零修改双实证破零攻坚 Phase 0 能力预检与基线冻结

| 字段 | 内容 |
|---|---|
| 评审编号 | `REV-20261008-ZERO-MOD-TWIN-PROOF-PHASE0-PRECHECK` |
| 日期 | 2026-10-08 |
| 关联实施计划 | [2026-10-08-esp-idf-zero-modification-twin-proof-breakthrough-plan.md](../../implementation-plans/esp32/2026-10-08-esp-idf-zero-modification-twin-proof-breakthrough-plan.md) |
| 评审对象 | 4 项试点条目能力闭包、工具链就绪度、冻结资产入口及双靶编译器验证 |
| 评审结论 | **Phase 0 出口达成 (PASS)**：四项身份与源码锁定，双 target 工具链（Emscripten wasm32 + ESP-IDF v6.1 Xtensa）完备就绪，UART 原生故障接线缺口精准定位，具备进入 Phase 1/M1 条件 |

---

## 一、T0.1 四项试点身份、上游来源与源文件哈希锁定

所有源文件均位于 `wink-micro-app/vendor/esp_idfv61/`，经实测提取其物理源文件哈希（排除 `unisim-assets/` 与 `unisim-scenarios/`）：

| 应用编号 / 稳定 ID | 真实应用目录 | 上游路径 | 关键源文件 | SHA-256 (64 hex) | 字节数 |
|---|---|---|---|---|---|
| **#098** `esp.peripherals.uart.uart_events` | `peripherals/uart_uart_events` | `examples/peripherals/uart/uart_events` | `uart_events_example_main.c`<br>`CMakeLists.txt`<br>`wink-app.json`<br>`include/sdkconfig.h` | `8d173ac3cd0e1a9fec6dc1bf6de48a865172a43cc5f910635c63e00dbef770de`<br>`c4d255bdd7de83162fda24749f170cbcd8d03ffc3007db4f8b27405c249ba506`<br>`62916eca42c80b37dc9ba932a241163bbe82539588e5983fe7875b03b4240bd5`<br>`91a98ad682c2acca3fc2d0f57c2c04b28e5c2304d00dea2b26da5f471137cbd8` | 6111<br>625<br>438<br>1096 |
| **#196** `esp.protocols.http_server.simple` | `protocols/http_server_simple` | `examples/protocols/http_server/simple` | `main.c`<br>`CMakeLists.txt`<br>`wink-app.json`<br>`include/sdkconfig.h` | `6691c523df4a2b2e688c2a7e1d581000026dac36ffbd93937ea9413442547f23`<br>`edf63ea7824566ae9e131a0f41d3b7f032bc3085c0402ea170157937c4009f2b`<br>`02169132d6626529c8955574dbcd5a0f4853e6b179b87f346ec13accd03b7d3e`<br>`ef30b80417a6d5012fa6688ee38c0be0269e267da29eaa9bddb27deacdda46bc` | 18732<br>530<br>427<br>197 |
| **#403** `esp.storage.nvs.nvs_rw_value` | `storage/nvs_nvs_rw_value` | `examples/storage/nvs/nvs_rw_value` | `nvs_value_example_main.c`<br>`CMakeLists.txt`<br>`wink-app.json`<br>`include/sdkconfig.h` | `ecaa14df2632d48fe570db6314201e6c3320bcdd359c66862f30b54102f43fee`<br>`e3fa213797570e59fc17fd453d8235064cef8392fccb5e636d4724e7fe6221a6`<br>`5baf60e961726e818269305eac71e223d5cbcd7e276d5e893826b8d661e5dc00`<br>`1a1049872caf9c38b99792e326be9b8537b60dde29c61cd69a60f7cd16a7c555` | 4888<br>619<br>467<br>1063 |
| **#154** `esp.system.task_watchdog` | `system/task_watchdog` | `examples/system/task_watchdog` | `task_watchdog_example_main.c`<br>`CMakeLists.txt`<br>`wink-app.json`<br>`include/sdkconfig.h` | `5855b893bf480e818fb8421b23bcbdd50480c9d21a013725de9df14e4e56dc96`<br>`f81a1da2bb1e844bfc8609e7ddeb519157b653862d531f515a848c89167d1251`<br>`8b4dc8c6f8f4264d448f4f8e766b27b6b3928e4b2ac0416c02a9b6f97c76dcef`<br>`de97382fbf30beb2d2ef88bfd28d32801534758fb65e9893dc179f079b46e2f1` | 3124<br>663<br>508<br>1710 |

---

## 二、T0.2 & T0.3 逐层能力矩阵与后端映射

| 层级 | #098 UART Events | #196 HTTP Server | #403 NVS rw_value | #154 TWDT |
|---|---|---|---|---|
| **登记配置 ID** | `wasm_sim_standard` | `wasm_sim_standard` | `wasm_sim_standard` | `wasm_sim_standard` |
| **登记 Backend / SoC / Profile** | `wasm_browser` / `esp32` / `standard` | `wasm_browser` / `esp32` / `standard` | `wasm_browser` / `esp32` / `standard` | `wasm_browser` / `esp32` / `standard` |
| **运行执行模式** | `headless` (UniSim Node/Worker) | `headless` (UniSim Node/Worker) | `headless` (UniSim Node/Worker) | `headless` (UniSim Node/Worker) |
| **底层 C / PAL ABI** | `pal_wasm_push_uart_rx_error(port, flags)` [Landed] | `sim_http_responder/server_inject_json` [Landed] | `sim_nvs_inject_fault` [Landed] | TWDT work token & tick [Landed] |
| **中间件 / 门面分发** | `esp_uart.c` on_pal_uart_event -> FreeRTOS queue [Landed] | `esp_http_server.c` 路由分发与 404 handler [Landed] | `esp_nvs.c` 错误映射 [Landed] | `task_watchdog.c` 周期计时 [Landed] |
| **UniSim TS 总线 / 适配器** | `UARTBus.injectError(port, flags)` [Landed] | `injectNetFixture` [Landed] | NVS snapshot pre-load [Landed] | 外部漏喂暂停机制 [Gap] |
| **ScenarioSession 场景解析** | **缺口**：未分发 `INJECT_PLATFORM_FAULT` 到 `UARTBus.injectError` | 支持 `INJECT_NET_FIXTURE` | 支持 snapshot，需启动前错误注入 | **缺口**：无外部任务暂停指令 |
| **裁判与门禁支持** | `twin_evidence.py` 支持 `INJECT_PLATFORM_FAULT`；但当前 `checklist.data.json` 错误声明为 `ESP_ERR_INVALID_STATE` (需修正为原厂真实输出) | `twin_evidence.py` 要求 4xx/5xx 路由识别闭环 | 需介质损坏错误断言 | 需外部漏喂因果识别 |

---

## 三、T0.4 冻结入口与资产哈希纪律核实

1. **构建覆写现象**：实测运行 `winkcli sim run --app <dir>` 时，若不指定 `--out`，内部 `BuildSimCommand` 会自动将新构建产物提取并覆写至 `<dir>/unisim-assets/`。
2. **冻结入口规约**：
   - 为确保正常基线、无故障对照、故障处理及恢复使用**完全相同的同一生产资产**，必须在 Phase 2 执行前固化生产资产。
   - 使用隔离目录机制运行多轮测试，禁止在每轮 scenario 执行前重建 Wasm 资产。
3. **Windows CRLF 换行敏感性**：
   - 实测证明 `compute_assets_composite_sha256()` 对 `wink_simulator.js` 与 `device-tree.json` 的换行符直接敏感。
   - Windows 平台下 git 检出需保持 CRLF 状态以匹配已登记的历史 `assets_sha256: 37436b2da6c7a11a9e931a0de306e8c39109be73758d362c7680107b55806c51`。

---

## 四、T0.5 依赖闭包与 Wave 1 修复状态核查

- **前序依赖验证**：Wave 1 网络治理整改（SNTP, WebSocket typed frames, async work queue, broker 路由）已全部合入且提交（最新提交 `34e8b2ec`）。
- **门禁与基线核验**：
  - `run_gates.py --gate 1`：**12/12 全部 PASS**。
  - `evidence_verifier.py --verify-all`：**46/46 项现有 Verified 凭据 100% PASS**。
  - 工作区处于干净、无污染状态。

---

## 五、T0.6 双 Target 编译工具链验证

- **Wasm 编译器 (wasm32-unknown-emscripten)**：
  - 命令：`emcc -v`
  - 版本：Emscripten 6.0.9 (clang 24.0.0git)
  - 路径：`D:\software\embedded-tools\emsdk\upstream\bin\emcc.exe`
  - 状态：**可用 (PASS)**
- **ESP32 物理硬件工具链 (Xtensa)**：
  - 命令：`idf.py --version`（加载 `C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1` 后）
  - 版本：`ESP-IDF v6.1`
  - 路径：`D:\software\embedded-tools\esp-idf\.espressif\v6.1\esp-idf`
  - 状态：**可用 (PASS)**

---

## 六、预检结论与 Phase 1 准入决议

1. **M0 准入条件完全满足**：四项试点应用来源锁定，双 target 编译器就绪，基线 100% 绿灯。
2. **M1 首项破零推进路径**：
   - 聚焦 `#098 esp.peripherals.uart.uart_events`。
   - 核心接线任务：在 `wink-ai/packages/unisim` 的 `ScenarioSession` / `headless-sim-runner` 中打通 `INJECT_PLATFORM_FAULT`（`domain: "uart"`，`fault: { "action": "framing_error"|"parity_error"|"fifo_overflow", "flags": 1|2|4 }`）到 `UARTBus.injectError()`，使原生 `.fail.scenario.json` 能够驱动并被 `twin_evidence.py` 的裁判逻辑接受。
   - 契约校准任务：在 `checklist.data.json` 中将 `#098` 的 `negative_cases` 预期符号从错误的 `ESP_ERR_INVALID_STATE` 校准为原厂真实输出符号（如 `uart frame error`、`hw fifo overflow`），并保留完整覆盖。
