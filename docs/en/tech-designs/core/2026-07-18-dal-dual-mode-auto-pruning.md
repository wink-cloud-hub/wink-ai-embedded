# DAL 双模式自动裁剪 — 技术设计

| 项 | 内容 |
|----|------|
| 创建日期 | 2026-07-18 |
| 状态 | 已实施（2026-07-18） |
| 关联 ADR | [ADR-0039](../../decisions/core/0039-dal-dual-mode-auto-pruning.md) |
| 关联计划 | [2026-07-18-dal-dual-mode-auto-pruning-plan.md](../../implementation-plans/core/2026-07-18-dal-dual-mode-auto-pruning-plan.md) |
| 范围 | ESP32 + wasm 单 App + Host + Binary SDK（方案 C） |

---

## 1. 目标与非目标

**目标**

- 有 `wink-app.json` → 仅声明驱动编入；`app_options.cmake` 写满九宏 ON/OFF。
- 无 JSON → 九驱动全 ON + configure `WARNING`。
- 一处驱动表，多入口消费；删除 ESP32 硬编码基线与 MOTOR 特例路径。

**非目标**

- 强制 JSON；BAL source selection；按镜像字节设 CI 硬门禁。

---

## 2. 架构

```text
wink-app.json (optional)
        │
        ▼
app_codegen.py ──► app_options.cmake   # 九宏全部 ON|OFF + WINK_MAX_PERIODIC
        │
        ▼
cmake/wink_dal_drivers.cmake
  ├── wink_dal_apply_pruning(json|empty)
  └── wink_dal_add_enabled_sources(target)
        │
        ├── targets/esp32 (IDF component)
        ├── dal/CMakeLists.txt (Host shared lib)
        ├── Binary SDK consumer
        └── wasm/source single-app (via same helpers)
```

### 2.1 驱动表（SSOT in CMake；codegen 镜像同名宏）

| type (JSON) | 宏 | 源 |
|-------------|-----|-----|
| led | WINK_USE_LED | dal/src/output/dal_led.c |
| button | WINK_USE_BUTTON | dal/src/input/dal_button.c |
| servo | WINK_USE_SERVO | dal/src/actuator/dal_servo.c |
| ssd1306 | WINK_USE_SSD1306 | dal/src/display/dal_ssd1306.c (+ font TU) |
| ultrasonic | WINK_USE_ULTRASONIC | dal/src/sensor/dal_ultrasonic.c |
| gps | WINK_USE_GPS | dal/src/communication/dal_gps.c |
| eeprom | WINK_USE_EEPROM | dal/src/storage/dal_eeprom.c |
| motor | WINK_USE_MOTOR | dal/src/actuator/dal_motor.c |
| encoder | WINK_USE_ENCODER | dal/src/sensor/dal_encoder.c |

新增驱动：表 + codegen plugin + 头 stub + 本设计表，四处同步。

---

## 3. 共享模块 API（约定）

文件：`wink-micro-os/cmake/wink_dal_drivers.cmake`

```cmake
# 无 JSON / 空路径：九宏 CACHE ON + WARNING
# 有 JSON：execute_process(app_codegen) → include(app_options.cmake)
function(wink_dal_apply_pruning)
  # args: JSON_PATH (may be "")  OUT_DIR (for generated app_options)
endfunction()

# 按 WINK_USE_* 向 target 添加 .c，并 PUBLIC 定义 WINK_USE_*=1
function(wink_dal_add_enabled_sources target)
endfunction()
```

`core_sources.cmake`：`WINK_DAL_SOURCES` 不再无条件列出基线 7 个；ESP32 改为在 apply 之后由 `wink_dal_add_enabled_sources` 注入。Host `dal` target 继续用现有 `_wink_dal_enable`，但其 `option()` 值必须在 `add_subdirectory(dal)` **之前**被 `apply_pruning` 写好（有 JSON 时）。

---

## 4. Codegen

### 4.1 `app_options.cmake.j2`

```cmake
# 对 ALL_WINK_USE_OPTIONS 中每一项：
set(WINK_USE_LED ON CACHE BOOL "" FORCE)   # 或 OFF
...
set(WINK_MAX_PERIODIC <n> CACHE STRING "" FORCE)
```

Python 侧维护与上表一致的 `ALL_WINK_USE_OPTIONS`；JSON 用到的类型 → ON，其余 OFF。空 devices → 九个全 OFF（有 JSON 但无设备时的严格裁剪）。

> 注意：与「无 JSON → 全 ON」不同。**有 JSON 且 devices 为空** → 全 OFF。

### 4.2 motor / encoder 插件

最小 `DriverBase` 实现，字段对齐 `dal_motor_config_t` / `dal_encoder_config_t`：

- motor：`pwm_channel`, `dir_pin_a`, optional `dir_pin_b`/`pwm_freq_hz`；`is_actuator=True`；`safe_off=dal_motor_safe_off`
- encoder：`pin_a`, optional `pin_b`/`pull`

### 4.3 Golden

更新 `golden_expected` / `golden_multi_expected` 的 `app_options.cmake`：断言完整 ON/OFF 矩阵。

---

## 5. 各入口行为矩阵

| 入口 | JSON | 结果 |
|------|------|------|
| ESP32 App 有 JSON | 有 | 按声明裁剪 |
| ESP32 Arduino 无 JSON | 无 | 九全开 + WARNING |
| Host 默认（多 sample） | 无绑定 | 九全开 |
| Host 传入 `WINK_APP_JSON` | 有 | 按声明裁剪共享 `dal` |
| Binary SDK | 通常有 | 九宏 defs 全覆盖（含 MOTOR/ENCODER） |
| wasm 单 App 源码构建 | 有 | 同固件路径 |

---

## 6. 验收

| 项 | 标准 |
|----|------|
| Codegen golden | `app_options` 含九宏且 OFF 正确 |
| ESP32 `devkitc_smoke` | 镜像/`nm` 不见未声明驱动的 `dal_*` 强符号（如 `dal_gps_*`，以 JSON 为准） |
| 无 JSON 冒烟 | configure 有 WARNING；可编过 |
| Host | `python wink-tools/wink.py test` 全绿（默认全 ON） |
| Binary SDK | `foreach` 含 MOTOR/ENCODER；头文件 defs 与 options 一致 |
| 回归 | 调用未启用驱动 → `WINK_UNAVAILABLE` 消息仍含 wink-app.json 指引 |

---

## 7. 风险

| 风险 | 缓解 |
|------|------|
| Host 误传某 App JSON → 其他 sample 链裁剪后的 dal 失败 | 文档：仅单 App 工程设置 `WINK_APP_JSON`；CI 默认不设 |
| 空 devices JSON 全 OFF 导致 BAL stub | 符合契约；App 需声明 motor/encoder |
| 无 JSON 全开含 motor 改变 Arduino 体积 | ADR 已接受；正式固件应带 JSON |

