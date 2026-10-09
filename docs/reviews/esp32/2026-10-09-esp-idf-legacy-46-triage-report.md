# ESP-IDF 存量 46 项已验证条目 AFG v1.1 凭据降级与重验裁决报告

> 评估时间: 2026-10-09T10:44:12Z | 裁决标准: `AFG-Engine v1.1 四态决策树`

## 1. 总体裁决汇总

| 裁决状态 | 条目数量 | 处理措施与跟进路径 |
|---|---|---|
| **ELIGIBLE** (完全合规) | **4** | 直接签发 v1.1 凭据，维持验证有效状态 |
| **needs_driver_fix** (驱动缺陷阻断) | **1** | 归入 Loop 整改计划跟踪（S-03），等待底层驱动修复后重验 |
| **needs_proofplan_update** (待补变异场景) | **41** | 待 Tier 2/3 Archetype 接入后补充负向变异见证，暂停晋升 |
| **deferred** (硬件独占) | **0** | 芯片独占外设诚实标注暂缓，严禁伪造通用通过 |
| **合计** | **46** | 100% 显式裁决，零静默忽略项 |

## 2. 明细条目裁决表

| # | 条目 ID | 上游路径 | 裁决状态 | 裁决原因 |
|---|---|---|---|---|
| 1 | `esp.get_started.blink` | `examples/get-started/blink` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 2 | `esp.get_started.hello_world` | `examples/get-started/hello_world` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 3 | `esp.peripherals.adc.continuous_read` | `examples/peripherals/adc/continuous_read` | 🔴 `needs_driver_fix` | Driver defect S-03: ADC continuous read virtual clock backpressure / overrun not reported |
| 4 | `esp.peripherals.adc.oneshot_read` | `examples/peripherals/adc/oneshot_read` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 5 | `esp.peripherals.dac.dac_cosine_wave` | `examples/peripherals/dac/dac_cosine_wave` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 6 | `esp.peripherals.dac.dac_oneshot` | `examples/peripherals/dac/dac_oneshot` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 7 | `esp.peripherals.dedicated_gpio.soft_uart` | `examples/peripherals/dedicated_gpio/soft_uart` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 8 | `esp.peripherals.gpio.generic_gpio` | `examples/peripherals/gpio/generic_gpio` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 9 | `esp.peripherals.i2c.i2c_basic` | `examples/peripherals/i2c/i2c_basic` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 10 | `esp.peripherals.i2c.i2c_eeprom` | `examples/peripherals/i2c/i2c_eeprom` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 11 | `esp.peripherals.ledc.ledc_basic` | `examples/peripherals/ledc/ledc_basic` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 12 | `esp.peripherals.ledc.ledc_fade` | `examples/peripherals/ledc/ledc_fade` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 13 | `esp.peripherals.spi_master.hd_eeprom` | `examples/peripherals/spi_master/hd_eeprom` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 14 | `esp.peripherals.timer_group.gptimer` | `examples/peripherals/timer_group/gptimer` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 15 | `esp.peripherals.uart.uart_async_rxtxtasks` | `examples/peripherals/uart/uart_async_rxtxtasks` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 16 | `esp.peripherals.uart.uart_echo` | `examples/peripherals/uart/uart_echo` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 17 | `esp.peripherals.uart.uart_echo_rs485` | `examples/peripherals/uart/uart_echo_rs485` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 18 | `esp.peripherals.uart.uart_events` | `examples/peripherals/uart/uart_events` | 🟢 `ELIGIBLE` | Verified under AFG v1.1: baseline PASS + twin/canary mutation kill |
| 19 | `esp.system.deep_sleep` | `examples/system/deep_sleep` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 20 | `esp.system.esp_event.default_event_loop` | `examples/system/esp_event/default_event_loop` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 21 | `esp.system.esp_event.user_event_loops` | `examples/system/esp_event/user_event_loops` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 22 | `esp.system.esp_timer` | `examples/system/esp_timer` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 23 | `esp.system.freertos.basic_freertos_smp_usage` | `examples/system/freertos/basic_freertos_smp_usage` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 24 | `esp.system.freertos.real_time_stats` | `examples/system/freertos/real_time_stats` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 25 | `esp.system.ota.simple_ota_example` | `examples/system/ota/simple_ota_example` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 26 | `esp.system.startup_time` | `examples/system/startup_time` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 27 | `esp.system.task_watchdog` | `examples/system/task_watchdog` | 🟢 `ELIGIBLE` | Verified under AFG v1.1: baseline PASS + twin/canary mutation kill |
| 28 | `esp.protocols.esp_http_client` | `examples/protocols/esp_http_client` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 29 | `esp.protocols.http_server.restful_server` | `examples/protocols/http_server/restful_server` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 30 | `esp.protocols.http_server.simple` | `examples/protocols/http_server/simple` | 🟢 `ELIGIBLE` | Verified under AFG v1.1: baseline PASS + twin/canary mutation kill |
| 31 | `esp.protocols.http_server.ws_echo_server` | `examples/protocols/http_server/ws_echo_server` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 32 | `esp.protocols.mqtt` | `examples/protocols/mqtt` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 33 | `esp.protocols.sntp` | `examples/protocols/sntp` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 34 | `esp.protocols.sockets.tcp_client` | `examples/protocols/sockets/tcp_client` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 35 | `esp.protocols.sockets.tcp_server` | `examples/protocols/sockets/tcp_server` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 36 | `esp.wifi.fast_scan` | `examples/wifi/fast_scan` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 37 | `esp.wifi.getting_started.softap` | `examples/wifi/getting_started/softAP` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 38 | `esp.wifi.getting_started.station` | `examples/wifi/getting_started/station` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 39 | `esp.wifi.scan` | `examples/wifi/scan` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 40 | `esp.wifi.softap_sta` | `examples/wifi/softap_sta` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 41 | `esp.bluetooth.ble_get_started.nimble.nimble_beacon` | `examples/bluetooth/ble_get_started/nimble/NimBLE_Beacon` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 42 | `esp.bluetooth.nimble.bleprph` | `examples/bluetooth/nimble/bleprph` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 43 | `esp.storage.nvs.nvs_iteration` | `examples/storage/nvs/nvs_iteration` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 44 | `esp.storage.nvs.nvs_rw_blob` | `examples/storage/nvs/nvs_rw_blob` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
| 45 | `esp.storage.nvs.nvs_rw_value` | `examples/storage/nvs/nvs_rw_value` | 🟢 `ELIGIBLE` | Verified under AFG v1.1: baseline PASS + twin/canary mutation kill |
| 46 | `esp.storage.spiffs` | `examples/storage/spiffs` | 🟡 `needs_proofplan_update` | Baseline PASS; pending Tier 2/3 archetype L1/L2 negative mutation scenario integration |
