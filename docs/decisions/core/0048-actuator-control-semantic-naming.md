# ADR-0048：Actuator 按控制语义分类与 DAL 命名规范

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-07-28 |
| 触发 | [2026-07-28 DAL Actuator 电机分类与命名规范评审](../../reviews/core/2026-07-28-dal-actuator-motor-taxonomy-review.md) §七 Owner 确认（C-006 / D-004） |
| 影响范围 | `dal/include/actuator/`；`wink_actuator_registry`；codegen Capability 别名层；[01-dal-device-abstraction.md](../../zh/design/02-wink-micro-os/01-dal-device-abstraction.md) |
| 决策者 | 项目 Owner |
| 关联 ADR | [ADR-0004](0004-static-dispatch-vs-runtime-ops.md)（POD + 命名 API）；[ADR-0026](0026-foc-motor-dal-bal-separation.md)（`dal_bldc` roadmap）；[ADR-0047](0047-foc-isr-layering-and-pal-hwtimer.md)（FOC 前后台切分，与本 ADR 的 `dal_bldc` 命名交叉引用） |
| 关联计划 | [implementation-plans/2026-07-28-dal-bal-followup-plan.md](../../implementation-plans/core/2026-07-28-dal-bal-followup-plan.md)（Task T0/T1） |

---

## 背景（Context）

1. 当前 `dal_motor` + `dal_rc_servo` 二分法**按「是不是电机」命名**，而非按**控制语义**（控制物理量）拆分，导致：
   - `dal_motor` 名不副实：实为 H 桥有刷 DC 开环占空比驱动，却易被 AI/codegen 误用于步进、BLDC、闭环伺服。
   - 步进、闭环伺服、无刷 BLDC/FOC 均未覆盖。
2. 各类执行器的**安全关断（safe_off）语义正交**：DC 需 brake/coast；航模舵机 duty=0=limp；步进 hold/release；闭环伺服 disable/抱闸——不能共用一个泛化 `motor` safe_off 范式（见评审 §四）。
3. **命名陷阱**：中文「舵机/伺服」混用，但 `dal_rc_servo`（航模 50Hz PWM 开环角度）与 `dal_industrial_servo`（工业闭环位置/速度/力矩）是**完全不同器件**，必须命名区分。

## 方案比选（Options）

| 方案 | 结论 |
|------|------|
| A. 维持 `dal_motor` 泛称，文档注释区分 | ❌ 无法阻止 AI 误选驱动；与未来 `dal_stepper` 语义冲突 |
| B. 单驱动 + `config.ctrl_mode` 枚举覆盖全部电机类型 | ❌ 控制原语正交（占空比 vs 步数 vs 闭环力矩），API 与 safe_off 无法统一 |
| **C. 按控制语义拆分 DAL 前缀；`motor` 仅保留在 Capability 别名层** | ✅ **采纳**（评审 §5.1） |

## 决策结论（Decision）

### 1. Owner 确认项（C-006 / D-004，已锁定）

| 确认项 | 裁决 |
|--------|------|
| `dal_motor` → `dal_dc_motor` 重命名 | **是**（Task T1 落地代码；本 ADR 记录决策） |
| `dal_dc_motor_safe_off` 默认绑定 | **brake**（H 桥下桥短接制动），**非** coast |
| `dal_stepper` 近期排期 | **否**——仅预留命名；实现挂 [计划 C3](../../implementation-plans/core/2026-07-28-dal-bal-followup-plan.md)，不阻塞 Wave A |

### 2. 控制语义分类总表

| # | DAL 驱动 | 控制语义 | 典型器件 / 芯片 | 关断语义 | 状态 |
|---|---|---|---|---|---|
| 1 | `dal_dc_motor` | 占空比 / 速度（开环） | 有刷 DC + H 桥（L298N / TB6612 / DRV8833） | `brake()` / `coast()` 显式；`safe_off` → **brake** | 🔴 现有（T1 改名自 `dal_motor`） |
| 2 | `dal_rc_servo` | 绝对角度（开环 PWM） | SG90 / MG996R 航模舵机 | `safe_off` → limp（duty=0） | ✅ 已有 |
| 3 | `dal_stepper` | 步数 / 位置（开环） | 28BYJ-48、A4988、DRV8825、TMC2209 | `hold()` / `release()`；`safe_off` 绑定其一 | 🟡 **预留**（C3 触发，本 ADR 不排期实现） |
| 4 | `dal_industrial_servo` | 闭环位置 / 速度 / 力矩 | 工业伺服、ODrive、VESC（总线型） | `safe_off` → disable / 抱闸 | 🟢 roadmap（C3 / 独立计划） |
| 5 | `dal_bldc` | 换相 / FOC | 云台 / 轮毂 BLDC（本地 FOC） | `safe_off` → 三相断开 | 🟢 roadmap（[ADR-0026](0026-foc-motor-dal-bal-separation.md)；ISR 分层见 [ADR-0047](0047-foc-isr-layering-and-pal-hwtimer.md)） |

> ⚠ **命名陷阱**：`dal_rc_servo` ≠ `dal_industrial_servo`。前者为航模开环 PWM 角度；后者为带编码器的工业闭环伺服。**禁止合并或混用前缀。**

### 3. 命名规范

| 规则 | 说明 |
|---|---|
| 按控制语义命名 | `dal_dc_motor`（占空比）/ `dal_stepper`（步数）/ `dal_rc_servo`（角度）/ `dal_industrial_servo`（闭环）/ `dal_bldc`（FOC） |
| **`motor` 不做具体 DAL 前缀** | 泛称 `motor` 仅用于 codegen **Capability 别名层**（如 `left_wheel_set_speed` → 绑定具体驱动实例） |
| 关断语义随器件 | 每类在 `wink_actuator_registry` 注册语义正确的 safe-off；**不外推**通用范式 |
| 目录归属 | 全部 `dal/include/actuator/` 与 `dal/src/actuator/`，不新建子目录 |
| `dal_bldc` 与 ADR-0047 对齐 | 本地 FOC 快环 ISR 宿主、DI 形态由 ADR-0047 裁决；本 ADR 只锁定 DAL 前缀与关断语义，避免三套积木名 |

### 4. `dal_dc_motor` API 契约（T1 目标形态）

```c
wink_status_t dal_dc_motor_init(dal_dc_motor_t *dev, const dal_dc_motor_config_t *cfg);
wink_status_t dal_dc_motor_set_speed(dal_dc_motor_t *dev, float speed);  /* -1.0~1.0 */
wink_status_t dal_dc_motor_brake(dal_dc_motor_t *dev);   /* H 桥短接制动 */
wink_status_t dal_dc_motor_coast(dal_dc_motor_t *dev);   /* 全关滑行 */
wink_status_t dal_dc_motor_safe_off(dal_dc_motor_t *dev);/* 绑定 brake（本 ADR 钉死） */
wink_status_t dal_dc_motor_deinit(dal_dc_motor_t *dev);
```

### 5. Non-goals（本 ADR 明确不做）

- 不实现 `dal_stepper` / `dal_industrial_servo` / `dal_bldc` 驱动代码
- 不实现 FOC 换相环、VESC/ODrive 协议驱动（见 C3 / ADR-0026 / ADR-0047）
- 不修改生产源码（`dal_motor.c` 等留给 Task T1）

## 后果与约束（Consequences & Constraints）

| 正面 | 负面 / 缓解 |
|------|-------------|
| AI/codegen 按控制语义选驱动，降低物理误配 | T1 须全仓 `rg dal_motor` + codegen/registry 同步改名 |
| safe_off 语义可审计、可测试 | `dal_dc_motor_safe_off` 必须调用 `brake()`，与 registry 注释一致 |
| 预留命名避免 `servo` / `industrial_servo` / `bldc` 三套名字分叉 | `dal_bldc` 须与 ADR-0047 合入时交叉核对积木名 |

## 遵循与后续（Compliance & Follow-up）

Accepted 后必须：

- [x] 回写 [01-dal-device-abstraction.md](../../zh/design/02-wink-micro-os/01-dal-device-abstraction.md) §6.2 actuator 控制语义分类表 — 2026-07-28
- [x] Task T1：`dal_motor` → `dal_dc_motor` 代码改名 + `brake()`/`coast()`/`safe_off`→`brake` 落地 — 2026-07-28
- [ ] C3 触发时再开 `dal_stepper` / `dal_vesc` 独立实施计划

---

## 附录 A：`dal_dc_motor` 拓扑命名与 `safe_off` 层级（Phase 1 回写，2026-07-29）

> 来源：[user-surface-phase1-plan](../../implementation-plans/frontend/2026-07-28-user-surface-phase1-plan.md) Task A3/C1；**不修改** §决策结论「无 enable 时 `safe_off` 仍绑 `brake()`」。

### A.1 拓扑枚举命名（纠正 `phase_enable` 误称）

| 枚举 | 含义 | Phase 1 状态 |
|------|------|--------------|
| `DAL_DC_MOTOR_MODE_IN_IN`（默认 0） | PWM 调速 + IN_A/IN_B（今日实现路径） | ✅ 已实现 |
| `DAL_DC_MOTOR_MODE_PHASE_ENABLE`（1） | 单 PHASE + ENABLE/PWM（业界 Phase/Enable） | 预留 → `WINK_ERR_UNSUPPORTED` |
| `DAL_DC_MOTOR_MODE_PWM_ON_IN`（2） | PWM 打在输入脚 | 预留 → `WINK_ERR_UNSUPPORTED` |

JSON `drive_mode` 字符串默认省略或 `"in_in"`。**勿**将今日 IN/IN 接线称为 `phase_enable`。

**IN/IN 真值表**（`dir_pin_a`=A，`dir_pin_b`=B）：

```text
dir_a  dir_b | state
  0      0   | coast
  1      0   | forward
  0      1   | reverse
  1      1   | brake (short)
```

### A.2 `safe_off` 层级（含可选 `enable_pin`）

1. **`enable_pin >= 0`**（init 后存储值；STBY/nSLEEP **高有效**）：若 `dir_pin_b >= 0` 则先 **brake**，再拉低 enable（硬关断）；返回 `WINK_OK`。
2. **无 enable 且 `dir_pin_b >= 0`**：调用 **`dal_dc_motor_brake()`**（本 ADR §决策结论：仍绑 brake）。
3. **无 enable 且单方向脚**：**`WINK_ERR_UNSUPPORTED`**（禁止静默 coast；单脚改 coast+OK 需另修 ADR）。

活规范回写：[01-dal-device-abstraction.md §6.3](../../zh/design/02-wink-micro-os/01-dal-device-abstraction.md)、[`dal-best-practices.md`](../../../wink-micro-os/docs/dal-development-guide/dal-best-practices.md) §3.1。

---

*本 ADR 状态变更请在此记录：*
- 2026-07-28：Proposed（配合 dal-bal-followup 计划 Task T0；引用 taxonomy 评审）
- 2026-07-28：Accepted（Owner 确认 C-006/D-004：`dal_dc_motor` 改名、`safe_off`=brake、`dal_stepper` 挂 C3）
- 2026-07-28：航模/工业伺服预留名由 [ADR-0050](0050-rc-servo-industrial-servo-naming.md) 更新为 `dal_rc_servo` / `dal_industrial_servo`（取代 `dal_servo` / `dal_servo_motor`）
- 2026-07-29：附录 A 回写 `in_in` 拓扑命名、`enable_pin`+`safe_off` 层级（Phase 1 Track C；单脚仍 UNSUPPORTED）

