# ESP-IDF v6.1 Vendor Behavior Suite (esp_idfv61)

> **定位**：master plan §7.1.1「三层证据塔」的 **L2 行为深度层**——在 corpus 编译广度之上，
> 为每个已交付门面域提供 1 个官方示例「一行不改」的行为代表，并把上游一致性做成机器可检的哈希链。
>
> **许可**：本目录下源码为 Espressif ESP-IDF 官方示例原文（Apache-2.0 / CC0-1.0 / Public Domain，
> 文件头保留原声明）。`wink-micro-app/vendor/**` 在 `license-map.json` 中为 skip 通道。

## 1. 精选应用矩阵

| App | 门面域 | distinct 风险 | upstream source_dir (v6.1) |
|:---|:---|:---|:---|
| `blink_gpio` | GPIO + FreeRTOS delay | 输出电平/周期调度 | `examples/get-started/blink/main`（`blink_example_main.c`） |
| `ledc_basic` | LEDC PWM | 定时器/通道/占空比/渐变 | `examples/peripherals/ledc/ledc_basic/main`（`ledc_basic_example_main.c`） |
| `i2c_basic` | Modern I2C master | 总线-器件二级句柄事务 | `examples/peripherals/i2c/i2c_basic/main`（`i2c_basic_example_main.c`） |
| `uart_echo` | UART | 阻塞读/写回环、任务栈 | `examples/peripherals/uart/uart_echo/main`（`uart_echo_example_main.c`） |
| `gptimer_alarm` | GPTimer | alarm 回调/自动重载/动态改期 | `examples/peripherals/timer_group/gptimer/main`（`gptimer_example_main.c`） |

**收录规则（master §7.1.1 精选三规则）**：① 有对应门面域；② 行为可观测（编译 + 单测/回放）；
③ 覆盖 distinct 风险。Out-of-scope API 两边都不收。

## 2. 门禁

| 门禁 | 命令 | 说明 |
|:---|:---|:---|
| 上游一致性（本地/CI） | `python wink-micro-os/frameworks/esp_idf/tools/check_vendor_app_upstream.py --root wink-micro-app/vendor/esp_idfv61` | 校验 manifest schema + 每源文件 normalized SHA-256 与 pin 一致 |
| 与 IDF 树逐字 diff（Nightly） | `... --idf-tree <esp-idf>` | v6.1 job 阻断式；v5.1 job `--allow-hash-drift`（仅存在性 + 漂移告警） |
| Host 编译（每 SoC） | `ctest -R esp_idfv61_` | OBJECT 编译，label `esp_idfv61_vendor;tier_v` |
| Wasm 编译 | `ctest -R esp_idfv61_wasm_compile` | emcc compile-only，与 corpus 同源门禁 |
| 行为回放 | `ctest -R esp_idf_headless_replay` | `blink_gpio` 等价路径（corpus blink + runtime）3 次 bit-exact |

## 3. 维护约定

1. **一行不改**：`*.c` 与上游逐字一致；仅允许新增 `wink-app.json` 与 `include/sdkconfig.h`（Kconfig overlay）。
2. **哈希 pin**：更新上游后用 `check_vendor_app_upstream.py --print-hashes` 打印 normalized 哈希并回写 manifest
   （normalized = LF、去行尾空白、单一末尾换行，跨平台稳定）。
3. **不收录**：需要封闭 SLA 白名单扩项的示例暂不收录——当前缺口：SPI（`spi_device_polling_transmit` 为显式 Out-of-scope）、
   NVS（`nvs_entry_*` 迭代器未实现）；待闭源收割规则扩项后补入。
4. **资产/场景**：`unisim-assets/*.wasm` 与 `unisim-scenarios/*.json` 由 UniSim 应用构建流水线生成，不在本目录手工提交。
