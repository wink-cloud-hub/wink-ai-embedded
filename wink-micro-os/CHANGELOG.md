# Changelog — wink-micro-os

本文件记录运行时内核的重要变更。格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.0.0/)。

## [Unreleased]

### Changed

- **BAL naming hard cut ([ADR-0038](../docs/design/decisions/0038-bal-naming-hard-cut-and-layer-ssot.md))**: removed `*_helper.h` / `*_controller.h` public headers; renamed APIs (`wink_ultrasonic_poll_*`, `wink_servo_sweep_*`, etc.); `wink_bal_opts_t` replaces `wink_helper_opts_t`; `src/` mirrors `include/<domain>/`; PUBLIC include is only `bal/include` root. **Breaking:** deleted `wink-micro-app/common/include/` BAL shims — use domain-prefixed includes (e.g. `output/wink_led_blink.h`, `comm/wink_telemetry_default.h`).

### Added

- **BAL 正式层**（`bal/`，[ADR-0023](../docs/design/decisions/0023-bal-business-abstraction-layer.md)）：LED blink、button events、ultrasonic poll、servo sweep、telemetry 等业务服务，强类型双轨 API（`_start` / `_start_ex`），codegen 驱动 `WINK_APP_MAX_*_INSTANCES` 槽容量。
- **Runtime**：`WINK_PERIODIC_INVALID`、`WINK_ERR_CANCELED`、`wink_periodic_change_period()`、`wink_periodic_active_count()`；`wink_blocking_region.h`（`WINK_INIT_BLOCKING_REGION` / `WINK_INTERNAL_BLOCKING_REGION`）。
- **PAL**：`pal_i2c_bus_init()` / `pal_i2c_bus_deinit()`（极简 I2C bus 生命周期，服务 codegen bus-owner 节点）。
- **DAL**：7 个驱动补齐 `deinit`（含 ESP32 `gpio_reset_pin`）；ssd1306/eeprom/gps 等新增 deinit。
- **Codegen**：servo/ssd1306/eeprom/gps driver 插件；bus-owner 拓扑；删除 `app_support.c`；`wink-app.json` 仅描述静态设备树。
- **Sim**：wasm 默认 `WINK_STRICT_NONBLOCKING=1`（仅 app 源文件）；`WINK_PT_DEBUG` 下 LIGHT 上下文阻塞断言升级为 `assert()`/hard fault。

### Changed

- **App 回调签名**：`init` → `init_status()`，`on_fault` → `on_fault_status()`（返回 `wink_status_t`）；业务回调零 file-scope `#pragma`。
- **Samples 迁移**：`devkitc_smoke`、`oled_dashboard`、`dual_task_demo`、`resource_conflict`、`unisim_smoke` 适配新模式；`oled_dashboard` 使用 codegen 生成 `device_tree`。
- **`wink-micro-app/common/`**：不再提供 BAL 转发 shim；仅 INTERFACE（selftest include + link `wink_bal`）。
- **ESP32 真机**：devkitc_smoke **S1–S11** 全 PASS（含 deinit 循环 5 轮无 GPIO 占用）。

### Removed

- `*_helper.h` / `*_controller.h` 公开头、`wink_helper_opts_t`、公开符号中的 `sonar`（ADR-0038 hard cut）。

---

## BAL/DCST 迁移指南（2026-07-06 重构）

> 实施计划：[PLAN-20260706-BAL-DCST](../docs/design/implementation-plans/2026-07-06-bal-dcst-refactor-plan.md)

### 1. Include 路径

| 旧 | 新（ADR-0038） |
|---|---|
| `wink_blink_helper.h` / `output/wink_blink_helper.h` | `output/wink_led_blink.h`（link `wink_bal`） |
| `wink_button_helper.h` | `input/wink_button_events.h`（`wink_button_enable_events` / `disable`） |
| `wink_sonar_helper.h` | `sensor/wink_ultrasonic_poll.h` |
| `wink_servo_helper.h` | `actuator/wink_servo_sweep.h` |
| `wink_telemetry_helper.h` / `wink_default_telemetry.h` | `comm/wink_telemetry_default.h` |
| `wink_helper_opts.h` / `wink_helper_opts_t` | `wink_bal_opts.h` / `wink_bal_opts_t` |
| `wink_chassis_controller.h` | `control/wink_chassis.h` |
| `wink_sim_ultrasonic_echo.h`（app common shim） | `runtime/selftest/src/wink_sim_ultrasonic_echo.h`（仅 bringup） |

CMake：`target_link_libraries(your_app PRIVATE wink_bal)`；BAL 通过 PUBLIC include 导出 `bal/include/**`。

### 2. App 回调

```c
static wink_status_t app_init_status(void)
{
    WINK_TRY(wink_device_tree_init());
    static const wink_button_event_config_t cfg = {
        .drive           = WINK_BUTTON_DRIVE_SOFT_POLL,
        .auto_poll_ms    = USER_BUTTON_AUTO_POLL_MS,
        .debounce_ms     = USER_BUTTON_DEBOUNCE_MS,
        .wake_from_sleep = false,
    };
    WINK_TRY(wink_button_enable_events(&btn, &cfg));
    return WINK_OK;
}

static wink_status_t app_on_fault_status(uint32_t code)
{
    wink_trace_fault(code);
    return WINK_OK;  /* 或 WINK_ERR_* 触发 boot lockout */
}
```

- **禁止**在 `app_callbacks.c` 顶部使用 file-scope `#pragma GCC diagnostic ignored`。
- init 阶段小块阻塞（如 OLED flush、selftest）用 `WINK_INIT_BLOCKING_REGION_BEGIN/END` 包裹。
- 业务 `app_loop` 内**不得**调用 `WINK_BLOCKING` API；周期任务用 BAL 服务或 `wink_periodic_start_ex(WINK_PERIODIC_MAY_BLOCK, ...)`。

### 3. 设备树与 codegen

- 新建/维护 `wink-app.json`，运行 `wink-micro-os/tools/codegen/app_codegen.py`（或 `python wink-micro-os/tools/wink.py gen`）生成 `device_tree.[hc]` + `app_options.cmake`。
- Sample `CMakeLists.txt` 参考 `samples/oled_dashboard/CMakeLists.txt`（host e2e + wasm `PARENT_SCOPE` 导出）。
- **ADR-0008 例外**：`avoidance_car` / `dual_task_demo` 保留手写 `device_tree.c`（Flash 覆写表）；`wink-app.json` 仅作文档。

### 4. CMake / 链接

```cmake
add_subdirectory(bal)   # 顶层 wink-micro-os/CMakeLists.txt 已包含
target_link_libraries(your_target PRIVATE wink_bal dal wink_runtime)
```

### 5. Wasm 严格模式

- 默认：`WINK_STRICT_NONBLOCKING=1` 仅施加于 **app 源文件**（`wink_simulator`）。
- `unisim_smoke` 专用构建：`-DWINK_STRICT_NONBLOCKING=0`（需 exercise blocking PAL imports）。
- 逃生口：`-DWINK_STRICT_NONBLOCKING=0` 仅用于 bringup 调试。

### 6. 验证清单

```powershell
# Host（52 tests + wasm_node_smoke）
.\run-tests.ps1

# Wasm（换 App）
emcmake cmake -B build-wasm -DTARGET_PLATFORM=wasm -DWINK_APP_DIR=samples/oled_dashboard .
cmake --build build-wasm

# ESP32
idf.py -C esp32_firmware build flash monitor   # devkitc_smoke S1–S11
```
