# ADR-0038：BAL 命名硬切割与层规范 SSOT

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-07-18 |
| 触发 | BAL 三域落地后，文件/API 方言（`_helper` / `_controller` / `sonar` vs `ultrasonic`）与 include 卫生阻碍长期维护；Owner 选定硬切割（方案 C） |
| 影响范围 | `wink-micro-os/bal/**` 目录与符号；`wink-micro-app/**`、codegen golden、测试、文档示例中的 BAL 引用；CI 门禁；**一次全仓对齐，不保留 deprecated 双名** |
| 决策者 | 项目 Owner |
| 关联 ADR | [ADR-0023](0023-bal-business-abstraction-layer.md)、[ADR-0032](0032-bal-role-operation-naming-classes.md)、[ADR-0037](0037-bal-domain-partition-and-closed-loop-motor.md) |
| 关联活规范（SSOT） | [06-bal-layer.md](../../zh/design/02-wink-micro-os/06-bal-layer.md)；指针：[coding-conventions.md §4](../../zh/design/07-platform-governance/coding-conventions.md) |

---

## 背景（Context）

ADR-0037 已确立 `math/` / `control/` / 物理增强三域，但存量命名仍混用：

1. 文件后缀 `_helper` / `_events` / `_controller` / 无后缀并存，且文件名 stem 常与公共 API 前缀不一致（如 `wink_chassis_controller.h` vs `wink_chassis_start`）。
2. 词表分裂：`sonar`（旧 helper）与 DAL/`ultrasonic`（新事件 API）并存。
3. `bal/src/` 扁平、`include/` 分层；CMake 将各子目录加入 PUBLIC include，削弱「目录即命名空间」。
4. ADR-0032 曾约定「既有 A 类不机械改名」——在组件数量仍可控时，Owner 决定改为**硬切割**，避免永久双轨。

---

## 方案比选（Options）

| 方案 | 结论 |
|------|------|
| A. 冻结祖父，仅约束新代码 | ❌ 方言永久残留，AI/评审无单一真相 |
| B. 软迁移（deprecated → 数个 minor 后删） | ❌ 双名窗口长，与「严格标准」目标不符 |
| **C. 硬切割（单 PR 全仓 rename）** | ✅ **采纳** |

---

## 决策结论（Decision）

1. **活规范 SSOT**：BAL 域划分、命名、依赖、实现形态、CI 以 [`06-bal-layer.md`](../../zh/design/02-wink-micro-os/06-bal-layer.md) 为唯一正文；本 ADR 只锁决策与硬切割映射。
2. **三域目录冻结**（继承 ADR-0037）：物理增强 / `math/` / `control/`；`src/` **必须镜像** `include/` 子目录。
3. **命名**：新规范禁止裸 `_helper`、`_controller` 文件后缀；文件名 stem = 公共 API 前缀；外设词表与 DAL 对齐（`ultrasonic`）。
4. **API 动词**：继续遵守 ADR-0032 A/B/C；本 ADR **部分取代** ADR-0032「既有 A 类不机械改名」——BAL 物理增强与 opts 符号按下方映射表一次性改名。
5. **Include 卫生**：`wink_bal` PUBLIC include **仅** `bal/include`；一律带域前缀 `#include "math/…"`.
6. **硬切割范围**：`wink-micro-os` + `wink-micro-app` + codegen golden + 测试 + 设计/示例文档中的符号与路径，**一次改干净**；不保留 `WINK_DEPRECATED` 双名过渡（实现 PR 合入前可短时双名仅用于本地编译过渡，合入时不得残留旧公开符号）。
7. **共享选项头**：`wink_helper_opts.h` / `wink_helper_opts_t` → `wink_bal_opts.h` / `wink_bal_opts_t`。

### 硬切割映射表（目标态）

| 现状 | 目标态 |
|:---|:---|
| `include/wink_helper_opts.h` | `include/wink_bal_opts.h`（`wink_bal_opts_t`，宏 `WINK_BAL_OPTS_*`） |
| `output/wink_blink_helper.h` | `output/wink_led_blink.h` → `wink_led_blink_start` / `_stop` / `_start_ex` |
| `sensor/wink_sonar_helper.h` | `sensor/wink_ultrasonic_poll.h` → `wink_ultrasonic_poll_start` / `_stop` / `_start_ex` / `_set_period` |
| `actuator/wink_servo_helper.h` | `actuator/wink_rc_servo_sweep.h` → `wink_rc_servo_sweep_start` / `_stop` / `_start_ex` / `_set_period`；`wink_rc_servo_set_angle` 保留同文件 |
| `comm/wink_telemetry_helper.h` | `comm/wink_telemetry_default.h`；API 保留 `wink_telemetry_default_start` / `_start_ex` / `_stop`（`default` 为能力名，非 `_helper`） |
| `control/wink_chassis_controller.h` | `control/wink_chassis.h`（API 已是 `wink_chassis_*`，仅改文件名） |
| `input/wink_button_events.h` | **保留**（已符合 B 类文件规则） |
| `sensor/wink_ultrasonic_distance_events.h` | **保留** |
| `math/wink_pid.h`、`math/wink_diff_drive_kinematics.h` | **保留** |
| `control/wink_closed_loop_motor.h` | **后由 [ADR-0049](0049-bal-closed-loop-dc-motor-naming.md) 正名为** `control/wink_closed_loop_dc_motor.h`（本 ADR 硬切割时曾保留泛称） |
| `src/*.c` 扁平 | 迁入 `src/<domain>/` 与 include 镜像 |
| `WINK_*_HELPER_MAX` 等宏 | `WINK_<CAPABILITY>_MAX`（如 `WINK_LED_BLINK_MAX`、`WINK_ULTRASONIC_POLL_MAX`、`WINK_RC_SERVO_SWEEP_MAX`、`WINK_CHASSIS_MAX`） |

### 落地顺序（实现阶段，非本 ADR 正文）

1. 本 ADR Accepted + 活规范已回写（本提交）。
2. 单实施计划 + 单（或原子化）硬切割 PR：rename → 引用 → CMake → 测试 → golden。
3. 启用 [`06-bal-layer.md`](../../zh/design/02-wink-micro-os/06-bal-layer.md) §6 CI 门禁。（**2026-07-18 落地**）
4. `python wink-tools/wink.py test` 全绿后合入。（**2026-07-18**：host 61/62 PASS；`wasm_node_smoke` 缺 wasm 构建产物，环境预存）

---

## 后果与约束（Consequences）

### 正面

- 目录、文件名、API 前缀可机械推导；AI/codegen 训练面单一。
- 消灭 `sonar`/`helper`/`controller` 公开方言。
- ADR-0037 域模型与命名层闭环。

### 代价

- 一次全仓 churn：App、测试、文档、golden 必须同 PR 或紧密串联合入。
- **部分取代** ADR-0032「不机械改名」条款；A/B/C **动词规则不变**。
- 合入窗口内禁止并行大改 BAL，避免冲突。

### 不在范围

- DAL/PAL 符号与目录。
- Arduino compat 沙箱（ADR-0035/0036）。
- 未落地的 FOC/SimpleFOC 方案（ADR-0026）——未来按本 SSOT 落 `control/` + `math/`。

---

*状态变更记录：*
- 2026-07-18：Accepted（Owner；硬切割 C；SSOT = `06-bal-layer.md`）
- 2026-07-18：Implementation complete on branch `feat/bal-naming-hard-cut`（Tasks 0–9；CI §6 gates enabled）
- 2026-07-28：映射表 `wink_closed_loop_motor` 行由 [ADR-0049](0049-bal-closed-loop-dc-motor-naming.md) 更新为 `wink_closed_loop_dc_motor`（与 ADR-0048 DAL 正名对齐）

