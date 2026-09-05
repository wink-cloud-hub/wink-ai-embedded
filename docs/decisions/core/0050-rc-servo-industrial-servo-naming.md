# ADR-0050：航模/工业伺服预留名正名为 `rc_servo` / `industrial_servo`

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-07-28 |
| 触发 | Owner 确认：`servo` / `industrial_servo` 近形对立易与中文「舵机/伺服」混淆；业界开源亦非以此对命名 |
| 影响范围 | DAL/BAL 符号与文件；codegen `type`；`wink-app.json`；活规范 [01-dal-device-abstraction.md](../../zh/design/02-wink-micro-os/01-dal-device-abstraction.md)、[06-bal-layer.md](../../zh/design/02-wink-micro-os/06-bal-layer.md)；[ADR-0048](0048-actuator-control-semantic-naming.md) 预留名；role-intent 附录 C |
| 决策者 | 项目 Owner |
| 关联 ADR | [ADR-0048](0048-actuator-control-semantic-naming.md)（本 ADR **部分更新**其 `dal_servo` / `dal_industrial_servo` 行）；[ADR-0004](0004-static-dispatch-vs-runtime-ops.md)；[ADR-0038](0038-bal-naming-hard-cut-and-layer-ssot.md) |
| 关联计划 | [wink-app-role-intent-evolution-plan](../../implementation-plans/core/2026-07-28-wink-app-role-intent-evolution-plan.md) 附录 C |

---

## 背景（Context）

1. ADR-0048 已区分航模开环 PWM 与工业闭环伺服，但预留名为 `dal_servo` vs `dal_industrial_servo`——仅差 `_motor`，AI/中文语境仍易混。
2. 业界开源（Arduino `Servo`、教材 hobby/RC vs industrial servo drive）**不**用 `servo`/`industrial_servo` 对；更常见 **RC/hobby** vs **industrial/drive**。
3. Owner 选定：`rc_servo` ↔ `industrial_servo`。

## 方案比选（Options）

| 方案 | 结论 |
|------|------|
| A. 维持 `servo` / `industrial_servo` | ❌ 近形；非业界惯例 |
| B. 仅改工业侧为 `industrial_servo`，航模保留 `servo` | ⚠️ 半清晰；航模仍泛称 |
| **C. `rc_servo` ↔ `industrial_servo` 硬切割** | ✅ **采纳** |

## 决策结论（Decision）

### 1. 命名映射（硬切割，不保留 deprecated 双名）

| 旧 | 新 | 说明 |
|---|---|---|
| `dal_servo` / `dal_rc_servo_*` | `dal_rc_servo` / `dal_rc_servo_*` | 航模 50Hz PWM 开环角度 |
| JSON / codegen `type: "rc_servo"` | `type: "rc_servo"` | registry / `WINK_USE_RC_SERVO` |
| `wink_rc_servo_sweep_*` / `wink_rc_servo_set_angle` | `wink_rc_servo_sweep_*` / `wink_rc_servo_set_angle` | BAL 物理增强与 DAL 对齐 |
| `WINK_USE_RC_SERVO` / `WINK_RC_SERVO_SWEEP_MAX` | `WINK_USE_RC_SERVO` / `WINK_RC_SERVO_SWEEP_MAX` | 裁剪宏 |
| ADR-0048 预留 `dal_industrial_servo` | **`dal_industrial_servo`** | roadmap；本 ADR **不**实现驱动 |

### 2. 控制语义分类表（取代 ADR-0048 §2 中相关两行）

| DAL 驱动 | 控制语义 | 典型器件 | 关断 | 状态 |
|---|---|---|---|---|
| `dal_rc_servo` | 绝对角度（开环 PWM） | SG90 / MG996R | limp（duty=0） | ✅ 已有（本 ADR 改名） |
| `dal_industrial_servo` | 闭环位置/速度/力矩 | 工业伺服、总线智能驱动 | disable / 抱闸 | 🟢 roadmap |

> ⚠ **命名陷阱（更新）**：禁止再用 `servo_motor` 指工业伺服；禁止把 `rc_servo` 与 `industrial_servo` 合并；Arduino 侧禁止泛称 `WinkServo`（用 `WinkRcServo` / 预留 `WinkIndustrialServo`）。

### 3. Arduino 门面与 DAL 对齐（硬切割）

| DAL | Arduino 门面 | 说明 |
|---|---|---|
| `dal_rc_servo` | **`WinkRcServo`** | 航模/RC PWM；禁止再用泛称 `WinkServo` |
| `dal_industrial_servo`（roadmap） | **`WinkIndustrialServo`**（预留名） | 工业闭环；与 RC 门面严格分家 |

- Wasm 观测导出 `pal_wasm_get_servo_angle`（通道角度观测 ABI）暂保留旧符号。
- 历史评审/已完成计划中的旧名保留为时间点快照。

### 4. Non-goals

- 不实现 `dal_industrial_servo` / `WinkIndustrialServo` 驱动代码。
- 不引入运行期 `servo` 多态门面。
- 不保留 `WinkServo` deprecated 别名（与 DAL 硬切割一致）。

## 后果与约束

| 正面 | 代价 |
|------|------|
| 与业界 RC vs industrial 说法对齐；降低 AI 混用 | 破坏性 rename（DAL/BAL/codegen/JSON/golden） |
| 与 ADR-0048 控制语义轴一致 | 旧 sample / golden 须同 PR 更新 |

## 遵循与后续

- [x] 回写 ADR-0048 状态记录 + 分类表指向本 ADR
- [x] 回写 01-dal §6.2、06-bal、role-intent 附录 C
- [x] 代码 / codegen / sample JSON 硬切割
- [x] host 相关单测（含 `test_dal_rc_servo` / `test_bal_rc_servo_sweep` / pruning_neg）与 codegen golden 全绿 — 2026-07-28

---

*状态变更记录：*
- 2026-07-28：Accepted（Owner：`rc_servo` ↔ `industrial_servo`）
- 2026-07-28：Arduino 门面硬切割为 `WinkRcServo`（预留 `WinkIndustrialServo`）；删除 `WinkServo`

