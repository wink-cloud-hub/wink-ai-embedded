# BAL 命名硬切割 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `subagent-driven-development` (recommended) or `executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.  
> Domain skill: `embedded-best-practice`（静态分发 / 分层红线；本计划以 rename 为主，不改控制算法语义）。  
> **计划版本：** v1.1（2026-07-18）— 合并架构评审意见：显式宏/doxygen、test CMake 闭环、Task 1 文件清单、原子合入、pre-flight、`sonar` 标识符必清等。

**Goal:** 按 [ADR-0038](../../decisions/core/0038-bal-naming-hard-cut-and-layer-ssot.md) 与 [06-bal-layer.md](../../design/02-wink-micro-os/06-bal-layer.md)，一次全仓去掉 BAL 公开方言（`_helper` / `_controller` / `sonar` / 扁平 `src` / 多路径 PUBLIC include），合入后无 deprecated 双名残留。

**Architecture:** Pre-flight 确认基线绿 → 改共享 opts → 按组件 rename（头/符号/测试/App；`.c` 路径延后）→ Task 7 统一镜像 `src/<domain>/` + 收窄 PUBLIC include + 清理 test 冗余 `-I` → 启用 §6 CI 门禁 → residual grep + 全量测试。本计划是 **机械硬切割**，不改变 slot / A-B-C 动词语义 / 闭环算法行为。

**Tech Stack:** C11、CMake、Unity host tests、PowerShell `python wink-tools/wink.py test`。

## Global Constraints

- SSOT：[06-bal-layer.md](../../design/02-wink-micro-os/06-bal-layer.md)；决策：[ADR-0038](../../decisions/core/0038-bal-naming-hard-cut-and-layer-ssot.md)。
- **禁止**合入后保留 `WINK_DEPRECATED` 旧公开符号或 `*_helper.h` / `*_controller.h`。
- **禁止** `bal/include/**` 新增 `pal_*.h`（`pal_log.h` 除外）。
- PUBLIC include **仅** `bal/include`；一律 `#include "domain/wink_….h"`。
- `src/` 必须与 `include/` 子目录镜像。
- 词表：代码树（`bal/` / `test/` / `wink-micro-app/` 的 `.c/.h`）禁用标识符 `\bsonar\b`（含局部变量、类型名、注释中的旧 API 名）；统一 `ultrasonic`。
- 不改 DAL/PAL/Arduino；不改闭环 PID 数学语义。
- Commit message 英文、原子化；每 Task 末提交一次（或按 §7 commit 建议）。
- 验收：`python wink-tools/wink.py test` 全绿，且 Task 9 residual grep 零命中。
- **原子合入（强制）：** Task 0–9 必须作为**同一 PR / 同一合入批次**落地。**禁止**单独 cherry-pick Task 2–6 而不含 Task 7–8（中间态头文件名与 `src/` 路径不一致；若提前启用 BAL-SRC-1 会 FATAL）。
- **`.c` 路径策略：** Task 2–6 **只改**头文件名、公开符号、`.c` 内部符号与 `#include`；**不改** `.c` 文件路径/文件名。路径与文件名重命名**统一在 Task 7** 一次完成（避免 CMake `target_sources` 改两次）。

### 确认记录（非阻塞）：DAL 域前缀 include

BAL 头中已有 `#include "actuator/dal_motor.h"` 等形式。收窄 `wink_bal` 的 PUBLIC include **不影响** DAL 解析：`dal` target 的 PUBLIC 已包含根目录 `include`（见 `dal/CMakeLists.txt`），故 `"actuator/dal_motor.h"` 经 `dal` 传播即可找到。未来若 DAL 也做类似收窄，另开计划协调——**不在本次范围**。

---

## 1. 元数据

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260718-BAL-HARD-CUT` |
| **创建日期** | 2026-07-18 |
| **修订** | v1.1 — 吸收计划评审（显式宏、test `-I` 清理、文件清单、原子合入、pre-flight 等） |
| **目标平台** | host（强制）/ wasm / ESP32（构建冒烟，可选同 PR） |
| **计划状态** | ✅ 已完成（2026-07-18） |
| **优先级** | 🔴 P0 |
| **关联 ADR** | [ADR-0038](../../decisions/core/0038-bal-naming-hard-cut-and-layer-ssot.md)、[ADR-0037](../../decisions/core/0037-bal-domain-partition-and-closed-loop-motor.md)、[ADR-0023](../../decisions/core/0023-bal-business-abstraction-layer.md)、[ADR-0032](../../decisions/core/0032-bal-role-operation-naming-classes.md) |
| **关联设计规范** | [06-bal-layer.md](../../design/02-wink-micro-os/06-bal-layer.md)、[coding-conventions.md §4](../../design/07-platform-governance/coding-conventions.md) |
| **前置依赖** | ADR-0038 / 06-bal-layer 已 Accepted 并入库 |
| **所需技能** | `embedded-best-practice` + `subagent-driven-development` / `executing-plans` |

---

## 2. 背景与验收

### 2.1 问题

三域已落地，但公开命名仍混用 `_helper` / `_controller` / `sonar`，`src` 扁平、CMake 多挂 include，导致 AI/人工无法机械推导符号，与 SSOT 冲突。

### 2.2 成功指标

| 指标 | 通过标准 | 验证方法 |
|------|----------|----------|
| Pre-flight 基线 | Task 0 记录的 PASS 数作为对照 | `python wink-tools/wink.py test` + 日志文件 |
| Host 单测 | 100% PASS（不低于基线） | `python wink-tools/wink.py test` |
| 旧公开名 residual | 0 命中（Task 9 grep 清单） | ripgrep |
| `\bsonar\b`（代码树） | 0 命中于 `bal`/`test`/`wink-micro-app` 的 `.c/.h` | ripgrep |
| test CMake | 无 `bal/include/<subdir>` 的 PRIVATE `-I` | 人工审 `test/CMakeLists.txt` |
| CI 门禁 | configure 期 FATAL 旧名/违规 include | 故意造失败样例本地验证后删除 |
| App | `devkitc_smoke` 等无旧 shim/旧 API | grep + 可选构建 |

---

## 3. 符号映射表（执行时唯一对照）

### 3.1 文件与类型

| 旧 | 新 |
|----|----|
| `bal/include/wink_helper_opts.h` | `bal/include/wink_bal_opts.h` |
| `wink_helper_opts_t` | `wink_bal_opts_t` |
| `WINK_HELPER_OPTS_DEFAULT` | `WINK_BAL_OPTS_DEFAULT` |
| `WINK_HELPER_OPTS(...)` | `WINK_BAL_OPTS(...)` |
| `output/wink_blink_helper.h` | `output/wink_led_blink.h` |
| `WINK_BLINK_HELPER_MAX` | `WINK_LED_BLINK_MAX` |
| `sensor/wink_sonar_helper.h` | `sensor/wink_ultrasonic_poll.h` |
| `WINK_SONAR_HELPER_MAX` | `WINK_ULTRASONIC_POLL_MAX` |
| `actuator/wink_servo_helper.h` | `actuator/wink_servo_sweep.h` |
| `WINK_SERVO_HELPER_MAX` | `WINK_SERVO_SWEEP_MAX` |
| `comm/wink_telemetry_helper.h` | `comm/wink_telemetry_default.h` |
| `WINK_TELEMETRY_HELPER_MAX` | `WINK_TELEMETRY_DEFAULT_MAX` |
| `control/wink_chassis_controller.h` | `control/wink_chassis.h` |
| `WINK_CHASSIS_HELPER_MAX` | `WINK_CHASSIS_MAX`（含 `.c` 内 `#ifndef` 定义与全部使用处 + 头文件 doxygen） |

### 3.2 API（blink 已基本正确，仅文件/宏）

| 旧 API | 新 API |
|--------|--------|
| `wink_led_blink_start/_ex/_stop` | **保持**（opts 参数类型改为 `wink_bal_opts_t`） |
| `wink_sonar_helper_start` | `wink_ultrasonic_poll_start` |
| `wink_sonar_helper_start_ex` | `wink_ultrasonic_poll_start_ex` |
| `wink_sonar_helper_stop` | `wink_ultrasonic_poll_stop` |
| `wink_sonar_helper_set_period` | `wink_ultrasonic_poll_set_period` |
| `wink_sonar_helper_is_running` | `wink_ultrasonic_poll_is_running` |
| `wink_sonar_helper_reset` | `wink_ultrasonic_poll_reset` |
| `wink_servo_sweep_start/_ex` | **保持** |
| `wink_servo_helper_stop` | `wink_servo_sweep_stop` |
| `wink_servo_helper_set_period` | `wink_servo_sweep_set_period` |
| `wink_servo_helper_is_running` | `wink_servo_sweep_is_running` |
| `wink_servo_helper_reset` | `wink_servo_sweep_reset` |
| `wink_servo_set_angle` | **保持** |
| `wink_telemetry_default_*` | **保持**（仅头文件/宏/opts 类型） |
| `wink_chassis_*` | **保持**（仅头文件/宏/opts 类型） |
| `wink_closed_loop_motor_*` | **保持**（仅 opts 类型） |
| `wink_button_*` / `wink_ultrasonic_enable_distance_events` | **保持** |

### 3.3 `src/` 镜像目标（Task 7 一次完成）

| 旧 `bal/src/` | 新 |
|---------------|-----|
| `wink_bal_opts`（无独立 .c） | — |
| `wink_blink_helper.c` | `src/output/wink_led_blink.c` |
| `wink_button_events.c` | `src/input/wink_button_events.c` |
| `wink_button_events_irq.c` | `src/input/wink_button_events_irq.c` |
| `wink_button_events_internal.h` | `src/input/wink_button_events_internal.h`（**必搬**；两 `.c` 的 `#include "wink_button_events_internal.h"` 同目录可保持不变） |
| `wink_sonar_helper.c` | `src/sensor/wink_ultrasonic_poll.c` |
| `wink_ultrasonic_distance_events.c` | `src/sensor/wink_ultrasonic_distance_events.c` |
| `wink_servo_helper.c` | `src/actuator/wink_servo_sweep.c` |
| `wink_telemetry_helper.c` | `src/comm/wink_telemetry_default.c` |
| `wink_pid.c` | `src/math/wink_pid.c` |
| `wink_diff_drive_kinematics.c` | `src/math/wink_diff_drive_kinematics.c` |
| `wink_closed_loop_motor.c` | `src/control/wink_closed_loop_motor.c` |
| `wink_chassis_controller.c` | `src/control/wink_chassis.c` |
| `wink_bal_stub.c` | `src/wink_bal_stub.c`（无域，留 `src/` 根） |

### 3.4 删除 / 清理（硬切割：不留 deprecated shim）

| 路径 | 动作 |
|------|------|
| `wink-micro-app/common/include/wink_blink_helper.h` | 🗑️ 删除 |
| `wink-micro-app/common/include/wink_telemetry_helper.h` | 🗑️ 删除 |
| `wink-micro-app/common/include/wink_default_telemetry.h` | 🗑️ 删除；调用方改 `#include "comm/wink_telemetry_default.h"` |
| `wink-micro-app/common/include/wink_sim_ultrasonic_echo.h` | ✏️ 至少更新注释中对 `wink_blink_helper.h shim` 的引用；若仅为兼容转发且无调用方依赖，评估 🗑️ 删除并让 App 直接 include 真源头 |

---

## 4. 文件变更总览

| 路径 | 类型 |
|------|------|
| `wink-micro-os/bal/include/**` | rename + 符号替换 |
| `wink-micro-os/bal/src/**` | 迁入子目录 + rename |
| `wink-micro-os/bal/CMakeLists.txt` | sources 路径、PUBLIC include 收窄、注释去 helper/sonar、§6 门禁 |
| `wink-micro-os/test/test_*.c` + `test/CMakeLists.txt` | 符号/文件名/目标名；**删除**各测试对 `bal/include/<subdir>` 的 PRIVATE `-I` |
| `wink-micro-app/devkitc_smoke/app_callbacks.c` | include + API + 变量名去 `sonar` |
| `wink-micro-app/common/include/*` shim | 删除 / 注释清理 |
| `docs/design/**` 活规范示例 | ✏️ 改到新名（历史 ADR 正文可保留旧名 + 顶注） |
| codegen golden | 若含旧 BAL 符号则更新（当前无命中则跳过并记日志） |

---

## Tasks

### Task 0: Pre-flight 基线（执行前必做）

**目的：** 确认硬切割前仓库已绿，避免把既有失败算进 rename 验收。

- [ ] **Step 1:** 运行并保存基线：

```powershell
python wink-tools/wink.py test | Tee-Object -FilePath .\wink-micro-os\baseline_test_results_bal_hardcut.txt
```

- [ ] **Step 2:** 记录 PASS/FAIL 摘要到本计划「执行日志」或 PR 描述。若已有失败：**先修基线或显式豁免**，再开始 Task 1。
- [ ] **Step 3:** 不强制单独 commit（本地日志可不入库；若入库则 `.gitignore` 或放 `docs/` 外临时路径）。

---

### Task 1: `wink_bal_opts` 硬切割（全仓类型先换）

**Files（显式清单，约 32 处引用，防遗漏）：**

| 文件 | 操作 |
|------|------|
| `bal/include/wink_bal_opts.h` | 🆕 由旧头改名创建 |
| `bal/include/wink_helper_opts.h` | 🗑️ |
| `bal/include/output/wink_blink_helper.h` | ✏️ opts 类型（本 Task；文件 rename 在 Task 2） |
| `bal/include/sensor/wink_sonar_helper.h` | ✏️ |
| `bal/include/actuator/wink_servo_helper.h` | ✏️ |
| `bal/include/comm/wink_telemetry_helper.h` | ✏️ |
| `bal/include/control/wink_chassis_controller.h` | ✏️ |
| `bal/include/control/wink_closed_loop_motor.h` | ✏️（约 2 处） |
| `bal/src/wink_blink_helper.c` | ✏️ |
| `bal/src/wink_sonar_helper.c` | ✏️ |
| `bal/src/wink_servo_helper.c` | ✏️ |
| `bal/src/wink_telemetry_helper.c` | ✏️ |
| `bal/src/wink_chassis_controller.c` | ✏️ |
| `bal/src/wink_closed_loop_motor.c` | ✏️（约 3 处） |
| `test/test_bal_telemetry.c` | ✏️ |
| `test/test_bal_sonar.c` | ✏️ |
| `test/test_bal_servo.c` | ✏️ |
| 其它 rg 命中文件 | ✏️ 一并改 |

**Produces:** 全仓只存在 `wink_bal_opts_t` / `WINK_BAL_OPTS_*`

- [ ] **Step 1:** 复制 `wink_helper_opts.h` → `wink_bal_opts.h`，替换：
  - include guard → `WINK_BAL_OPTS_H`
  - `wink_helper_opts_t` → `wink_bal_opts_t`
  - `WINK_HELPER_OPTS_DEFAULT` → `WINK_BAL_OPTS_DEFAULT`
  - `WINK_HELPER_OPTS(` → `WINK_BAL_OPTS(`
  - 注释：`helper options` → `BAL options`；示例改为 `wink_ultrasonic_poll_start_ex`
- [ ] **Step 2:** 按上表 + rg 补扫，全量替换 `#include` / 类型 / 宏：

```powershell
rg -n "wink_helper_opts|WINK_HELPER_OPTS" wink-micro-os wink-micro-app --glob '*.{c,h}'
```

- [ ] **Step 3:** 删除 `wink_helper_opts.h`；再次 rg，Expected: **无匹配**（CHANGELOG 历史句若命中则改掉或移出代码 glob）。
- [ ] **Step 4: Commit**

```bash
git add wink-micro-os/bal/include/wink_bal_opts.h
git add -u wink-micro-os wink-micro-app
git commit -m "$(cat <<'EOF'
refactor(bal): rename wink_helper_opts to wink_bal_opts (ADR-0038)

EOF
)"
```

---

### Task 2: LED blink — 文件/宏对齐（API 名已正确）

> **Note:** `.c` 文件路径与文件名在 **Task 7** 统一处理；本 Task 仅改头文件、内部符号/宏，以及仍位于 `src/wink_blink_helper.c` 的 include/宏。

**Files:**
- Create: `bal/include/output/wink_led_blink.h`
- Delete: `bal/include/output/wink_blink_helper.h`
- Modify: `bal/src/wink_blink_helper.c`（宏/include；路径仍旧）
- Modify: `test/test_blink_helper.c` → `test_led_blink.c`；`test/CMakeLists.txt` 目标名（子目录 `-I` 的清理留 Task 7）
- Modify: `wink-micro-app/devkitc_smoke/app_callbacks.c`
- Delete: `wink-micro-app/common/include/wink_blink_helper.h`
- Modify: `wink-micro-app/common/include/wink_sim_ultrasonic_echo.h`（更新/删除对 blink shim 的注释引用）

**Produces:** `#include "output/wink_led_blink.h"`；`WINK_LED_BLINK_MAX`

- [ ] **Step 1:** 新头基于旧头：
  - guard `WINK_LED_BLINK_H`
  - `WINK_BLINK_HELPER_MAX` → `WINK_LED_BLINK_MAX`
  - `#include "wink_bal_opts.h"`；`const wink_bal_opts_t *opts`
  - API 保持 `wink_led_blink_start` / `_start_ex` / `_stop`
- [ ] **Step 2:** `.c` 同步宏与 include；删除旧 `.h`（`.c` 暂名仍为 `wink_blink_helper.c`）
- [ ] **Step 3:** 测试：

```c
#include "output/wink_led_blink.h"
```

CMake: `add_wink_host_test(test_led_blink test_led_blink.c)`（替换 `test_blink_helper`）。

- [ ] **Step 4:** 删 App blink shim；`devkitc_smoke` → `#include "output/wink_led_blink.h"`；处理 `wink_sim_ultrasonic_echo.h` 注释/去留
- [ ] **Step 5: Commit** `refactor(bal): rename blink helper headers to wink_led_blink`

---

### Task 3: Ultrasonic poll（原 sonar helper）全符号替换

> **Note:** `.c` 路径 rename 在 Task 7；本 Task 改符号与头文件。

**Files:**
- Create: `bal/include/sensor/wink_ultrasonic_poll.h`
- Delete: `bal/include/sensor/wink_sonar_helper.h`
- Modify: `bal/src/wink_sonar_helper.c`
- Modify: `bal/src/wink_ultrasonic_distance_events.c`
- Modify: `test/test_bal_sonar.c` → `test_bal_ultrasonic_poll.c`；`test_ultrasonic_distance_events.c`
- Modify: `wink-micro-app/devkitc_smoke/app_callbacks.c`（API **与** 变量/注释中的 `sonar`）

**Produces:** 代码树无 `\bsonar\b` / `wink_sonar_*`

- [ ] **Step 1:** 新头：§3.2 全部 6 个 API + `WINK_ULTRASONIC_POLL_MAX`
- [ ] **Step 2:** `.c` 全局替换函数名与宏；`sonar_ctx_t` → `ultrasonic_poll_ctx_t`（**必改**，避免 `\bsonar\b`）
- [ ] **Step 3:** `distance_events.c` 互斥：

```c
if (wink_ultrasonic_poll_is_running(dev)) {
    return WINK_ERR_INVALID_STATE;
}
```

- [ ] **Step 4:** 测试重命名 + 符号替换；CMake 目标 `test_bal_ultrasonic_poll`
- [ ] **Step 5:** App（**必改变量名**）：

```c
#include "sensor/wink_ultrasonic_poll.h"
/* 原 smoke_sonar → smoke_ultrasonic（device_tree / 局部变量 / 注释一并改） */
wink_ultrasonic_poll_start(&smoke_ultrasonic, 500);
```

- [ ] **Step 6:** 验证（Expected: **零命中**）：

```powershell
rg -n "\bsonar\b|wink_sonar_" wink-micro-os/bal wink-micro-os/test wink-micro-app --glob '*.{c,h}'
```

- [ ] **Step 7: Commit** `refactor(bal): replace wink_sonar_helper with wink_ultrasonic_poll`

---

### Task 4: Servo sweep — 统一 stop/period/is_running/reset 前缀

> **Note:** `.c` 路径在 Task 7 处理。

**Files:**
- Create: `bal/include/actuator/wink_servo_sweep.h`
- Delete: `bal/include/actuator/wink_servo_helper.h`
- Modify: `bal/src/wink_servo_helper.c`
- Modify: `test/test_bal_servo.c` → 建议 `test_bal_servo_sweep.c` + CMake

**Produces:** 无 `wink_servo_helper_*`

- [ ] **Step 1:** 头文件导出：
  - `wink_servo_sweep_start` / `_start_ex` / `_stop` / `_set_period` / `_is_running` / `_reset`
  - `wink_servo_set_angle`
  - `WINK_SERVO_SWEEP_MAX`
- [ ] **Step 2:** `.c` + 测试全量 `wink_servo_helper_` → `wink_servo_sweep_`
- [ ] **Step 3: Commit** `refactor(bal): rename servo helper APIs to wink_servo_sweep_*`

---

### Task 5: Telemetry default — 仅文件/宏

> **Note:** `.c` 路径在 Task 7 处理。

**Files:**
- Create: `bal/include/comm/wink_telemetry_default.h`
- Delete: `bal/include/comm/wink_telemetry_helper.h`
- Modify: `bal/src/wink_telemetry_helper.c`（include + `WINK_TELEMETRY_DEFAULT_MAX`）
- Modify: `test/test_bal_telemetry.c`
- Delete: `wink-micro-app/common/include/wink_telemetry_helper.h`、`wink_default_telemetry.h`
- Modify: 任何仍 include 旧路径的 App → `"comm/wink_telemetry_default.h"`

**Produces:** `#include "comm/wink_telemetry_default.h"`；API 仍为 `wink_telemetry_default_*`

- [ ] **Step 1–3:** rename 头/宏/测试 include；删 shim  
- [ ] **Step 4: Commit** `refactor(bal): rename telemetry helper header to wink_telemetry_default`

---

### Task 6: Chassis — 去掉 `_controller` 文件名 + 宏全量替换

> **Note:** `.c` 路径在 Task 7 处理；本 Task 必须改完 `.c` 内宏与头 doxygen。

**Files:**
- Create: `bal/include/control/wink_chassis.h`
- Delete: `bal/include/control/wink_chassis_controller.h`
- Modify: `bal/src/wink_chassis_controller.c`（**全部** `WINK_CHASSIS_HELPER_MAX` → `WINK_CHASSIS_MAX`）
- Modify: `test/test_bal_chassis_controller.c` → `test_bal_chassis.c` + CMake

**Produces:** 无 `*_controller.h`；无 `WINK_CHASSIS_HELPER_MAX`

- [ ] **Step 1:** 新头 `wink_chassis.h`：API `wink_chassis_*` 不变；`#include "wink_bal_opts.h"`；doxygen 中  
  `WINK_CHASSIS_HELPER_MAX` → `WINK_CHASSIS_MAX`（原 L33 附近「Slot 池已满」注释）。
- [ ] **Step 2:** 在 `wink_chassis_controller.c` 中**全局替换** `WINK_CHASSIS_HELPER_MAX` → `WINK_CHASSIS_MAX`（定义处 `#ifndef`/`#define`/`#if`/`#else`/`#endif` 及循环上界等，约 **8 处**；可用）：

```powershell
rg -n "WINK_CHASSIS_HELPER_MAX" wink-micro-os/bal
```

Expected after: 零命中。

- [ ] **Step 3:** 测试文件与 CMake 目标改名为 `test_bal_chassis`
- [ ] **Step 4: Commit** `refactor(bal): rename wink_chassis_controller.h to wink_chassis.h`

---

### Task 7: `src/` 镜像 + CMake PUBLIC 收窄 + test `-I` 闭环

**Files:**
- Modify: `wink-micro-os/bal/CMakeLists.txt`（sources、PUBLIC include、**文件头注释**去掉 `helper`/`sonar` 旧术语）
- Move: §3.3 全部路径（含 `wink_button_events_internal.h`）
- Modify: `wink-micro-os/test/CMakeLists.txt` — **删除**所有 `…/bal/include/<subdir>` 的 PRIVATE include，只保留（若仍需要显式）`…/bal/include`；优先依赖 `target_link_libraries(… wink_bal)` 传播的 PUBLIC `include`

**目标 `target_sources`：**

```cmake
target_sources(wink_bal PRIVATE
    src/wink_bal_stub.c
    src/output/wink_led_blink.c
    src/input/wink_button_events.c
    src/input/wink_button_events_irq.c
    src/sensor/wink_ultrasonic_poll.c
    src/sensor/wink_ultrasonic_distance_events.c
    src/actuator/wink_servo_sweep.c
    src/comm/wink_telemetry_default.c
    src/math/wink_pid.c
    src/math/wink_diff_drive_kinematics.c
    src/control/wink_closed_loop_motor.c
    src/control/wink_chassis.c
)

target_include_directories(wink_bal PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
# 删除 include/output、include/input、include/sensor、include/actuator、
# include/display、include/comm、include/math、include/control 等多余 PUBLIC 条目
```

**CMake 注释示例（替换旧「helper / sonar」表述）：**

```cmake
# BAL (Business Abstraction Layer) — reusable device enhancements,
# math algorithms, and domain controllers (ADR-0038 / 06-bal-layer).
```

- [ ] **Step 1:** `git mv` 按 §3.3；确认 `src/input/wink_button_events_internal.h` 与两 `.c` 同目录；`#include "wink_button_events_internal.h"` **无需改路径**（编译器先搜当前 TU 目录）。
- [ ] **Step 2:** 更新 `bal/CMakeLists.txt` 的 `target_sources` / PUBLIC include / 顶部注释。
- [ ] **Step 3:** 清理 `test/CMakeLists.txt`：对 `test_button_events*`、`test_led_blink`、`test_bal_telemetry`、`test_bal_ultrasonic_poll`、`test_ultrasonic_distance_events`、`test_bal_servo_sweep`、`test_bal_closed_loop_motor`、`test_bal_chassis`、`test_pid` 等，去掉形如：

```cmake
${CMAKE_CURRENT_SOURCE_DIR}/../bal/include/output
${CMAKE_CURRENT_SOURCE_DIR}/../bal/include/input
${CMAKE_CURRENT_SOURCE_DIR}/../bal/include/sensor
${CMAKE_CURRENT_SOURCE_DIR}/../bal/include/actuator
${CMAKE_CURRENT_SOURCE_DIR}/../bal/include/comm
${CMAKE_CURRENT_SOURCE_DIR}/../bal/include/control
${CMAKE_CURRENT_SOURCE_DIR}/../bal/include/math
```

仅保留根 `../bal/include`（若 `wink_bal` PUBLIC 已足够，可尝试完全依赖 link 传播；以能配置编译为准）。

- [ ] **Step 4:** 确认所有测试 TU 使用带域前缀 include（如 `"output/wink_led_blink.h"`）。**抽查：** 临时把某测试改成 `#include "wink_led_blink.h"` 应**编译失败**（证明子目录 `-I` 已去掉）。
- [ ] **Step 5:** 配置并编译 host tests（或跑受影响子集）确认链接成功。
- [ ] **Step 6: Commit** `refactor(bal): mirror src/ domains, narrow PUBLIC includes, clean test -I`

---

### Task 8: 启用 06-bal-layer §6 CI 门禁

**Files:**
- Modify: `wink-micro-os/bal/CMakeLists.txt`

**Produces:** configure 期自动 fail

- [x] **Step 1:** 在现有 `pal_` 扫描旁增加：

```cmake
# BAL-NAME-1
file(GLOB_RECURSE _BAL_BAD_NAMES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*_helper.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*_controller.h")
if(_BAL_BAD_NAMES)
    message(FATAL_ERROR "BAL-NAME-1: forbidden helper/controller headers: ${_BAL_BAD_NAMES}")
endif()

# BAL-MATH-1: include/math/*.h must not match dal_|wink_runtime|wink_periodic|pal_
# BAL-NAME-2: include/**/*.h must not match \bsonar\b (including comments)
# BAL-INC-2: PUBLIC include dirs must be only .../bal/include (assert in this file)
```

- [x] **Step 2:** BAL-SRC-1：每个 `include/<domain>/*.h`（除 `wink_bal_opts.h`）对应 `src/<domain>/` 下实现 `.c`（可用显式白名单：`wink_bal_opts.h`、仅声明无独立 `.c` 的情况）。**仅在 Task 7 完成后启用**，与 Global Constraints 一致。
- [x] **Step 3:** 临时创建违例文件确认 FATAL，再删除。
- [x] **Step 4: Commit** `build(bal): enforce ADR-0038 naming and math-layer CI gates`

---

### Task 9: 文档示例与 residual 清扫 + 全量测试

**Files:**
- Modify: `docs/design/07-platform-governance/coding-conventions.md` 旧示例
- Modify: 其它**活规范**中的旧 API 示例（历史 ADR 可只加顶注）
- Modify: `wink-micro-os/README.md` / `CHANGELOG.md`（Breaking：shim 删除与符号 rename）

- [x] **Step 1:** residual grep（必须 **0** 命中）：

```powershell
rg -n "wink_helper_opts|wink_blink_helper|wink_sonar_helper|wink_servo_helper|wink_telemetry_helper|wink_chassis_controller|wink_servo_helper_|wink_sonar_helper_|WINK_.*HELPER_MAX|WINK_HELPER_OPTS|WINK_CHASSIS_HELPER_MAX" `
  wink-micro-os/bal wink-micro-os/test wink-micro-app --glob '*.{c,h,cmake,txt}'

rg -n "\bsonar\b" wink-micro-os/bal wink-micro-os/test wink-micro-app --glob '*.{c,h}'

rg -n "_helper\.h|_controller\.h" wink-micro-os/bal/include

rg -n "bal/include/(output|input|sensor|actuator|comm|control|math)" wink-micro-os/test/CMakeLists.txt
```

最后一条 Expected: 无 `target_include_directories` 再挂子目录（注释里若提及旧路径也应改掉以免误导）。

- [x] **Step 2:** 全量测试，对照 Task 0 基线：

```powershell
python wink-tools/wink.py test
```

Expected: 全部 Passed。

- [ ] **Step 3:**（可选）ESP32 `devkitc_smoke` 配置编译冒烟
- [x] **Step 4: Commit** `docs: align BAL examples with ADR-0038 hard cut; verify tests`

---

### Task 10: 计划收口

- [x] 将本计划状态改为 ✅ 已完成；ADR-0038「落地顺序」短注实现完成日期
- [ ] PR 标题建议：`refactor(bal): ADR-0038 naming hard cut`；正文链到本计划与 06-bal-layer；注明 **Breaking**（shim 删除、符号 rename）
- [x] **Commit**（若有状态勾选）：`docs(plan): close BAL naming hard-cut plan`

---

## 5. 风险与回滚

| 风险 | 缓解 |
|------|------|
| App shim 删除导致外部仓编译失败 | 本仓 `wink-micro-app` 同步改；CHANGELOG 写 Breaking |
| 漏改宏（尤其 chassis `.c` 内 8 处） | Task 6 显式步骤 + Task 9 `HELPER_MAX` grep |
| 漏改 `sonar` 局部名 | Task 3 必改 + Task 9 `\bsonar\b` |
| CMake include 收窄后短 include 仍能编过 | Task 7 清理 test 子目录 `-I` + 抽查负向编译 |
| 中间态被单独 cherry-pick | Global Constraints 原子合入；BAL-SRC-1 仅 Task 8 |
| 基线本就红 | Task 0 pre-flight |
| 并行改 BAL 冲突 | 执行窗口冻结其它 BAL PR |

**回滚（整批）：** 不保留半切割分支。若 PR 含多个 commit（约 Task 1–9），一次性回滚示例：

```bash
# 在合入该 PR 的分支上；将 N 换成硬切割 commit 数量
git revert --no-commit HEAD~N..HEAD
git commit -m "revert: undo ADR-0038 BAL naming hard cut"
```

或直接 `git revert -m 1 <merge_commit>`（若为 merge PR）。

---

## 6. Spec 覆盖自检

| SSOT / ADR-0038 / 评审要求 | 对应 Task |
|----------------------------|-----------|
| opts 改名 + 显式文件清单 | Task 1 |
| blink / poll / servo / telemetry / chassis | Task 2–6 |
| chassis `.c` 宏 + doxygen | Task 6 |
| 删除 App shim；sim_echo 注释 | Task 2/5 |
| `smoke_*` 去 sonar | Task 3 |
| `src/` 镜像含 `internal.h` | Task 7 |
| PUBLIC include 仅根；test `-I` 闭环 | Task 7 |
| CMake 注释去 helper/sonar | Task 7 |
| CI §6 门禁（含 SRC-1 时机） | Task 8 |
| residual + 全测 | Task 9 |
| Pre-flight / 原子合入 / 回滚命令 | Task 0、Global Constraints、§5 |
| DAL 域前缀确认 | Global Constraints 确认记录 |

**占位符扫描：** 无 TBD。  
**类型一致性：** 一律 `wink_bal_opts_t`；servo/poll 前缀与头文件 stem 一致。

---

## 7. 建议 Commit 序列（汇总）

0. （可选）无 commit — Task 0 仅本地基线  
1. `refactor(bal): rename wink_helper_opts to wink_bal_opts (ADR-0038)`  
2. `refactor(bal): rename blink helper headers to wink_led_blink`  
3. `refactor(bal): replace wink_sonar_helper with wink_ultrasonic_poll`  
4. `refactor(bal): rename servo helper APIs to wink_servo_sweep_*`  
5. `refactor(bal): rename telemetry helper header to wink_telemetry_default`  
6. `refactor(bal): rename wink_chassis_controller.h to wink_chassis.h`  
7. `refactor(bal): mirror src/ domains, narrow PUBLIC includes, clean test -I`  
8. `build(bal): enforce ADR-0038 naming and math-layer CI gates`  
9. `docs: align BAL examples with ADR-0038 hard cut; verify tests`  

可将 Task 2–6 合并为更少 commit，但 **Task 7/8 勿与早期 rename 拆到不同 PR**。

