# ESP-IDF v6.1 Vendor Behavior Suite (esp_idfv61)

> **定位**：master plan §7.1.1「三层证据塔」的 **L2 行为深度层**——在 corpus 编译广度之上，
> 为每个已交付门面域提供 1 个官方示例「一行不改」的行为代表，并把上游一致性做成机器可检的哈希链。
>
> **许可**：本目录下源码为 Espressif ESP-IDF 官方示例原文（Apache-2.0 / CC0-1.0 / Public Domain，
> 文件头保留原声明）。`wink-micro-app/vendor/**` 在 `license-map.json` 中为 skip 通道。
>
> 📖 **官方规范与进度总账**：
> - 📋 **[CHECKLIST.md](CHECKLIST.md)**：ESP-IDF v6.1 478 个官方示例全量普查总账与核对大表
> - 🛠️ **[PLAYBOOK.md](.governance/specs/PLAYBOOK.md)**：官方示例端到端仿真适配与无头实证标准执行手册 (SOP)
> - 📜 **[CLASSIFICATION-SPEC.md](.governance/specs/CLASSIFICATION-SPEC.md)**：分级分类管治法典与四道门禁规范
> - 🏛️ **[架构治理资产库](.governance/)**：能力字典、数据 SSOT、门禁系统（Gate 1~4）与自动化脚本集

## 1. 精选应用矩阵与实证现状

| App (落盘相对路径) | 门面域 | `unisim-assets/`<br>(仿真资产) | `unisim-scenarios/`<br>(无头场景) | 当前真实测试阶段 | CHECKLIST 状态 | upstream source_dir (v6.1) |
|:---|:---|:---:|:---:|:---|:---:|:---|
| `get-started/blink_gpio` | GPIO + FreeRTOS delay | ✅ 存在 | ✅ 存在 | 端到端无头仿真实证通过 | `[x] Verified` | `examples/get-started/blink/main`（`blink_example_main.c`） |
| `wifi/wifi_sta` | Wi-Fi STA | ✅ 存在 | ✅ 存在 | 场景已编写、资产已就绪，审计收尾中 | `[ ] Planned` | `examples/wifi/getting_started/station/main`（`station_example_main.c`） |
| `protocols/http_client` | HTTP Client | ✅ 存在 | ✅ 存在 | 场景已编写、资产已就绪，待归档证据 | `[ ] Planned` | `examples/protocols/esp_http_client/main`（`esp_http_client_example.c`） |
| `peripherals/ledc_basic` | LEDC PWM | ❌ 无 | ❌ 无 | **Phase 1: 仅 C 源码镜像与编译验证，未测仿真** | `[ ] Planned` | `examples/peripherals/ledc/ledc_basic/main`（`ledc_basic_example_main.c`） |
| `peripherals/i2c_basic` | Modern I2C master | ❌ 无 | ❌ 无 | **Phase 1: 仅 C 源码镜像与编译验证，未测仿真** | `[ ] Planned` | `examples/peripherals/i2c/i2c_basic/main`（`i2c_basic_example_main.c`） |
| `peripherals/uart_echo` | UART | ❌ 无 | ❌ 无 | **Phase 1: 仅 C 源码镜像与编译验证，未测仿真** | `[ ] Planned` | `examples/peripherals/uart/uart_echo/main`（`uart_echo_example_main.c`） |
| `peripherals/gptimer_alarm` | GPTimer | ❌ 无 | ❌ 无 | **Phase 1: 仅 C 源码镜像与编译验证，未测仿真** | `[ ] Planned` | `examples/peripherals/timer_group/gptimer/main`（`gptimer_example_main.c`） |
| `protocols/mqtt_tcp` | MQTT Client | ❌ 无 | ❌ 无 | **Phase 1: 仅 C 源码镜像与编译验证，未测仿真** | `[ ] Planned` | `examples/protocols/mqtt/main`（`app_main.c`） |
| `bluetooth/bleprph` | NimBLE GATT | ❌ 无 | ❌ 无 | **Phase 1: 仅 C 源码镜像与编译验证，未测仿真** | `[ ] Planned` | `examples/bluetooth/nimble/bleprph/main`（`main.c`, `gatt_svr.c`） |

> 📌 **阶段说明**：
> - **Phase 1（代码镜像）**：原厂源码 `*.c` 一行不改镜像，通过 Host/Wasm 编译门禁（仅证明编译符号与门面存在，**不代表行为已验证**）；
> - **Phase 2（仿真实证）**：由 `wink.py build sim` 编译生成 `unisim-assets/` 三件套，并编写 `unisim-scenarios/` 确定性时序场景，通过无头仿真 100% 绿灯并归档 `run-report.json` 后方可在 [`CHECKLIST.md`](CHECKLIST.md) 打勾。

**收录规则（master §7.1.1 精选三规则）**：① 有对应门面域；② 行为可观测（编译 + 单测/回放）；
③ 覆盖 distinct 风险。Out-of-scope API 两边都不收。

## 2. 门禁

| 门禁 | 命令 | 说明 |
|:---|:---|:---|
| 上游一致性（本地/CI） | `python wink-micro-os/frameworks/esp_idf/tools/check_vendor_app_upstream.py --root wink-micro-app/vendor/esp_idfv61` | 校验 manifest schema + 每源文件 normalized SHA-256 与 pin 一致 |
| 与 IDF 树逐字 diff（Nightly） | `... --idf-tree <esp-idf>` | v6.1 job 阻断式；v5.1 job `--allow-hash-drift`（仅存在性 + 漂移告警） |
| Host 编译（每 SoC） | `ctest -R esp_idfv61_` | OBJECT 编译，label `esp_idfv61_vendor;tier_v` |
| Wasm 编译 | `ctest -R esp_idfv61_wasm_compile` | emcc compile-only，与 corpus 同源门禁 |
| 行为回放与实证 | `ctest -R esp_idf_headless_replay` 或 `run_esp32_headless_evidence.ps1` | `unisim-scenarios/` 场景微秒断言 100% 绿灯 |

## 3. 维护约定

1. **一行不改**：`*.c` 与上游逐字一致；仅允许新增 `wink-app.json` 与 `include/sdkconfig.h`（Kconfig overlay）。
2. **哈希 pin**：更新上游后用 `check_vendor_app_upstream.py --print-hashes` 打印 normalized 哈希并回写 manifest
   （normalized = LF、去行尾空白、单一末尾换行，跨平台稳定）。
3. **不收录**：需要封闭 SLA 白名单扩项的示例暂不收录——当前缺口：SPI（`spi_device_polling_transmit` 为显式 Out-of-scope）、
   NVS（`nvs_entry_*` 迭代器未实现）；待闭源收割规则扩项后补入。
4. **资产与场景硬性规范**：
   - `unisim-assets/`（含 `device-tree.json`, `wink_simulator.js`, `wink_simulator.wasm` 三件套）：由 `wink.py build sim --app <app>` 真实编译产出，用于浏览器/无头仿真运行时加载，并在 Gate 1 中进行复合 SHA-256 强校验；
   - `unisim-scenarios/*.scenario.json`：由开发者依据官方示例电气时序**手动编写的自动化场景断言脚本**（绝非流水线自动生成），供 `run_esp32_headless_evidence.ps1` 驱动执行；
   - **门禁铁律**：任何 Wasm 仿真配置要想打勾交付（`delivery_state: "verified"`），**必须同时具备上述两目录**并附带全绿的 `run-report.json`。未完成前必须诚实保留为 `planned`，严禁在看板中标记为 `[x]`。
