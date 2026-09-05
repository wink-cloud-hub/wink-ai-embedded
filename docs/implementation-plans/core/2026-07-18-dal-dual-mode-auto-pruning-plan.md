# DAL 双模式自动裁剪 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `subagent-driven-development` (recommended) or `executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.  
> Domain skill: `embedded-best-practice`（静态分发 / 负数错误码 / 双 target；本计划以 CMake + codegen 为主）。

**Goal:** 按 [ADR-0039](../../decisions/core/0039-dal-dual-mode-auto-pruning.md) 与 [tech-design](../../tech-designs/core/2026-07-18-dal-dual-mode-auto-pruning.md)，一次打通 ESP32 / Host / Binary SDK / wasm：有 JSON 按声明裁剪，无 JSON 九驱动全 ON。

**Architecture:** Codegen 写满九宏 ON/OFF → 共享 `cmake/wink_dal_drivers.cmake` 应用 CACHE 并按开关加源 → 各入口删除硬编码基线。

**Tech Stack:** CMake、Python codegen、ESP-IDF、Unity host tests、`python wink-tools/wink.py test`。

## Global Constraints

- SSOT：ADR-0039；活规范 [06-bal-layer.md §4.4](../../design/02-wink-micro-os/06-bal-layer.md) 已回写。
- 九宏全集：LED BUTTON SERVO SSD1306 ULTRASONIC GPS EEPROM MOTOR ENCODER。
- 有 JSON 且 `devices` 为空 → 九宏全 OFF（与「无 JSON → 全 ON」不同）。
- 不改 DAL/PAL 运行时语义；不改 BAL 算法；仅裁剪与 stub 路径。
- Commit message 英文、原子化；用户未要求则不 push。
- 验收：codegen golden 绿；`python wink-tools/wink.py test` 绿；ESP32 `devkitc_smoke` 构建冒烟；无 JSON WARNING 可复现。

---

## 1. 元数据

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260718-DAL-AUTO-PRUNE` |
| **创建日期** | 2026-07-18 |
| **计划状态** | ✅ 已完成（2026-07-18） |
| **优先级** | P1 |
| **关联 ADR** | [ADR-0039](../../decisions/core/0039-dal-dual-mode-auto-pruning.md) |
| **关联设计** | [tech-designs/2026-07-18-dal-dual-mode-auto-pruning.md](../../tech-designs/core/2026-07-18-dal-dual-mode-auto-pruning.md) |

---

## 2. 文件清单（先锁定）

| 文件 | 动作 |
|------|------|
| `wink-micro-os/cmake/wink_dal_drivers.cmake` | 新建 |
| `wink-micro-os/tools/codegen/app_codegen.py` | 改：全表 ON/OFF |
| `wink-micro-os/tools/codegen/templates/app_options.cmake.j2` | 改 |
| `wink-micro-os/tools/codegen/drivers/motor.py` | 新建 |
| `wink-micro-os/tools/codegen/drivers/encoder.py` | 新建 |
| `wink-micro-os/tools/codegen/tests/golden_*/app_options.cmake` | 改 |
| `wink-micro-os/core_sources.cmake` | 改：去掉无条件 DAL 基线列表 |
| `wink-micro-os/targets/esp32/CMakeLists.txt` | 改：用共享模块 |
| `wink-micro-os/dal/CMakeLists.txt` | 小改：可在文档注释对齐；option 仍默认 ON |
| `wink-micro-os/CMakeLists.txt` | 改：可选 `WINK_APP_JSON` 时先 apply |
| `wink-micro-os/tools/binary_sdk_cmake/CMakeLists.txt` | 改：foreach 九项 |
| `docs/decisions/core/0039-dal-dual-mode-auto-pruning.md` | 勾选回写完成 |

---

### Task 0: Pre-flight

- [ ] **Step 1:** 跑 `python wink-tools/wink.py test`，记录 PASS 基线。
- [ ] **Step 2:** 确认当前 `targets/esp32` 仍硬编码 7 宏 + motor 特例（对照起点）。

---

### Task 1: Codegen — 全表 ON/OFF + golden

**Files:**
- Modify: `wink-micro-os/tools/codegen/app_codegen.py`
- Modify: `wink-micro-os/tools/codegen/templates/app_options.cmake.j2`
- Modify: `wink-micro-os/tools/codegen/tests/golden_expected/app_options.cmake`
- Modify: `wink-micro-os/tools/codegen/tests/golden_multi_expected/app_options.cmake`

**Interfaces:**
- Produces: `cmake_option_states: List[Tuple[str, bool]]` 或等价（名 → ON/OFF）供模板渲染。

- [ ] **Step 1:** 在 `app_codegen.py`（或 `drivers` 旁常量模块）定义：

```python
ALL_WINK_USE_OPTIONS = [
    "WINK_USE_LED",
    "WINK_USE_BUTTON",
    "WINK_USE_SERVO",
    "WINK_USE_SSD1306",
    "WINK_USE_ULTRASONIC",
    "WINK_USE_GPS",
    "WINK_USE_EEPROM",
    "WINK_USE_MOTOR",
    "WINK_USE_ENCODER",
]
```

- [ ] **Step 2:** 构建上下文时：从已用 driver 收集 ON 集合；对 `ALL_WINK_USE_OPTIONS` 每项输出 `(name, name in on_set)`。

- [ ] **Step 3:** 改模板为：

```jinja2
{% for opt, enabled in cmake_option_states %}
set({{ opt }} {% if enabled %}ON{% else %}OFF{% endif %} CACHE BOOL "" FORCE)
{% endfor %}
set(WINK_MAX_PERIODIC {{ instance_counts.values() | sum + 4 }} CACHE STRING "" FORCE)
```

- [ ] **Step 4:** 更新 golden：`golden_expected`（led/button/ultrasonic ON，其余 OFF）；`golden_multi` 按实际设备矩阵更新。

- [ ] **Step 5:** 跑 `python -m pytest wink-micro-os/tools/codegen/tests -q`（或项目既有 golden 命令），期望 PASS。

- [ ] **Step 6:** Commit：`feat(codegen): emit full WINK_USE ON/OFF matrix in app_options`

---

### Task 2: motor / encoder codegen 插件

**Files:**
- Create: `wink-micro-os/tools/codegen/drivers/motor.py`
- Create: `wink-micro-os/tools/codegen/drivers/encoder.py`
- Test: 新增最小 JSON fixture 或扩展现有 golden（至少一个含 motor）

**Interfaces:**
- Consumes: `DriverBase`
- Produces: `type="motor"|"encoder"`，`cmake_options()` 默认即可

- [ ] **Step 1:** 实现 `motor.py`（对齐 `dal_motor_config_t`）：

```python
class MotorDriver(DriverBase):
    type = "motor"
    is_actuator = True
    required_fields = ["pwm_channel", "dir_pin_a"]

    def get_headers(self):
        return ["dal_motor.h"]  # 或 "actuator/dal_motor.h" 若模板用域前缀

    def get_device_type(self):
        return "dal_motor_t"

    def get_safe_off_fn(self):
        return "dal_motor_safe_off"

    def render_deinit(self, dev_name: str) -> str:
        return "dal_motor_deinit"

    def render_config_init(self, dev_name: str, spec: dict) -> str:
        # dir_pin_b 默认 -1；pwm_freq_hz 默认 20000
        ...
```

- [ ] **Step 2:** 实现 `encoder.py`（`pin_a` 必填；`pin_b` 可选 -1；`pull` 可选）。

- [ ] **Step 3:** 手跑 `app_codegen.py` 对含 `"type":"motor"` 的临时 JSON，确认 `WINK_USE_MOTOR ON` 且其它 OFF 正确。

- [ ] **Step 4:** Commit：`feat(codegen): add motor and encoder driver plugins`

---

### Task 3: 共享 `wink_dal_drivers.cmake`

**Files:**
- Create: `wink-micro-os/cmake/wink_dal_drivers.cmake`

- [ ] **Step 1:** 写入驱动表 + 两个函数（见 tech-design §3）。无 JSON 分支：

```cmake
message(WARNING
  "wink_dal: no wink-app.json — enabling ALL DAL drivers (ADR-0039). "
  "Add wink-app.json to prune unused drivers.")
foreach(_opt IN ITEMS LED BUTTON SERVO SSD1306 ULTRASONIC GPS EEPROM MOTOR ENCODER)
  set(WINK_USE_${_opt} ON CACHE BOOL "" FORCE)
endforeach()
```

有 JSON：`execute_process(... app_codegen.py ...)` → `include(${out}/app_options.cmake)`。

- [ ] **Step 2:** `wink_dal_add_enabled_sources(target)`：对每个 ON 的宏 `target_sources` + `target_compile_definitions(... PUBLIC WINK_USE_X=1)`；SSD1306 附带 font TU（复用现有 `WINK_SSD1306_FONT` 逻辑）。

- [ ] **Step 3:** Commit：`feat(cmake): add shared wink_dal_drivers pruning module`

---

### Task 4: ESP32 + core_sources 接线

**Files:**
- Modify: `wink-micro-os/core_sources.cmake`
- Modify: `wink-micro-os/targets/esp32/CMakeLists.txt`

- [ ] **Step 1:** `core_sources.cmake`：将 `WINK_DAL_SOURCES` 改为空列表或删除其加入 `WINK_CORE_SOURCES` 的路径；在注释中说明 DAL `.c` 由 `wink_dal_add_enabled_sources` 注入。确保 `WINK_CORE_SOURCES` 仍含 runtime/trace/selftest/BAL。

- [ ] **Step 2:** `targets/esp32/CMakeLists.txt`：
  - `include(${WINK_MICRO_OS_ROOT}/cmake/wink_dal_drivers.cmake)`
  - 删除硬编码七宏 `target_compile_definitions`
  - 删除仅 motor/encoder 的特例 `if(DEFINED WINK_USE_MOTOR...)`
  - 调用 `wink_dal_apply_pruning`（传入 `WINK_APP_JSON` 或空）+ `wink_dal_add_enabled_sources(${COMPONENT_LIB})`
  - 保留 font 选择若未并入 helper，则移入 helper

- [ ] **Step 3:** 用 `devkitc_smoke` 构建冒烟（`wink.py esp32` 或项目既有命令）。期望：未在 JSON 的驱动无对应 `.c` 编译单元。

- [ ] **Step 4:** Commit：`refactor(esp32): wire dual-mode DAL pruning via shared cmake`

---

### Task 5: Host + Binary SDK

**Files:**
- Modify: `wink-micro-os/CMakeLists.txt`（或 dal 之前的 include 点）
- Modify: `wink-micro-os/dal/CMakeLists.txt`（注释 / 可选对接）
- Modify: `wink-micro-os/tools/binary_sdk_cmake/CMakeLists.txt`

- [ ] **Step 1:** Host：若定义 `WINK_APP_JSON`，在 `add_subdirectory(dal)` **之前** `wink_dal_apply_pruning`；否则不调用（保留 option 默认 ON = 全开）。

- [ ] **Step 2:** Binary SDK：将

```cmake
foreach(_drv IN ITEMS LED BUTTON SERVO SSD1306 ULTRASONIC GPS EEPROM)
```

扩为含 `MOTOR` `ENCODER`；确保 `include(app_options)` 后 defs 与 OFF 驱动一致（未 ON 则不 define，头走 stub）。

- [ ] **Step 3:** `python wink-tools/wink.py test` — 期望不低于 Task 0 基线。

- [ ] **Step 4:** Commit：`feat(build): apply ADR-0039 pruning on host and binary SDK`

---

### Task 6: 验收与文档收尾

- [ ] **Step 1:** Codegen pytest / golden 再跑一遍。
- [ ] **Step 2:** 无 JSON 路径 configure 日志含 ADR-0039 WARNING（可用最小 Arduino/空 JSON 路径复现）。
- [ ] **Step 3:** 勾选 ADR-0039「回写要求」；确认 `06-bal-layer.md §4.4` 已与实现对齐。
- [ ] **Step 4:** 可选：`nm`/`llvm-nm` 检查 `devkitc_smoke` 产物无 `dal_gps_` 等未声明符号。
- [ ] **Step 5:** Commit：`docs: mark ADR-0039 pruning plan verified`（若仅文档勾选）

---

## 3. 执行方式（完成后请用户选）

计划完成后可选：

1. **Subagent-Driven** — 每 Task 新 subagent + 审查  
2. **Inline Execution** — 本会话按 Task 推进  

---

## 4. Self-review（作者）

| 检查 | 结果 |
|------|------|
| ADR 双模式 / 九宏 / 共享 cmake / codegen OFF | 均有 Task |
| 空 devices ≠ 无 JSON | tech-design §4.1 + Global Constraints 已写明 |
| Host 误绑 JSON 风险 | tech-design §7 + Task 5 注释 |
| 无 TBD 占位 | 已扫 |

