# DAL Actuator 电机分类与命名规范评审

**评审日期**：2026-07-28
**评审对象**：`wink-micro-os/dal/include/actuator/`（`dal_motor.h` / `dal_servo.h`）
**评审视角**：资深嵌入式架构师，静态分发范式（ADR-0004）
**关联**：`wink_actuator_registry.h`（safe-off 语义边界）、ADR-0026（FOC 电机分层，roadmap）、Device Model Registry
**结论**：当前 `dal_motor` + `dal_servo` 二分法**不足以覆盖**步进 / 伺服 / 直流三类电机，且 `dal_motor` 命名名不副实，建议重命名并按控制语义扩展。

---

## 一、总体结论

**当前只覆盖了 2 类执行器，不是 3 类：**
- `dal_motor` 实际是「H 桥有刷直流电机（DC brushed）」的**开环占空比**驱动，被起了一个过于泛化的名字。
- `dal_servo` 是「航模舵机（SG90 类）」的 **50Hz PWM 绝对角度**驱动。
- **步进电机、闭环伺服电机、无刷 BLDC/FOC 全部未覆盖。**

核心病根：**按「是不是电机」命名，而非按「控制语义」拆分驱动。** 三类电机的控制物理量完全不同（占空比 vs 步数/位置 vs 绝对角度 vs 闭环力矩），不能共用一个 `dal_motor`。

---

## 二、`dal_motor` 的真实身份论证

从 `dal_motor.h` 的字段可确定其物理模型只能是「H 桥有刷 DC」：

| 证据 | 位置 | 说明 |
|---|---|---|
| `pwm_channel` + `dir_pin_a` + `dir_pin_b` | `dal_motor.h:18-20` | 典型 H 桥拓扑（L298N / TB6612 / DRV8833） |
| `set_speed(-1.0 ~ 1.0)` | `dal_motor.h:42` | 开环占空比控制，无位置/步数/闭环 |
| 状态仅 `current_speed` | `dal_motor.h:29` | 无位置累计、无步计数 |

**对三类电机的适配度：**

| 电机类型 | 控制原语 | `dal_motor`（H桥+duty）能否覆盖 |
|---|---|---|
| 有刷直流 DC | PWM 占空比 + 方向 | ✅ 正好为它设计 |
| 步进 Stepper | 脉冲序列（step/dir）或相序，核心是**步数/位置** | ❌ 无 step 计数、无相位时序、无 move_steps/set_position |
| 航模舵机 Servo | 50Hz PWM 脉宽 → 绝对角度 | ❌ 由 `dal_servo` 覆盖 |
| 闭环伺服电机 | 位置/速度/力矩环，多走总线（CAN/485） | ❌ 完全没有 |
| 无刷 BLDC / FOC | 换相 / Clarke-Park-SVPWM | ❌ 完全没有（ADR-0026 roadmap） |

---

## 三、命名问题：`dal_motor` 名不副实（首要修复项）

`motor` 是电机总称，但该驱动只能开环控制 H 桥有刷电机。危害：

1. **误导 AI 生成**：AI 看到 `dal_motor` 会以为「任何电机都用它」，给步进电机也生成 `dal_motor_set_speed`，编译通过但物理上完全错误。
2. **未来命名冲突**：加入 `dal_stepper` 后，`dal_motor` 反而像「那个不是步进的电机」，语义混乱。

**建议**：`dal_motor` → **`dal_dc_motor`**（有刷直流），与 DAL 设计文档 §2.1 已提到的 `dal_dc_motor.c` / `dal_gpio_motor` / `dal_i2c_motor` 命名思路一致。`motor` 泛称留给 codegen 的 Capability 别名层（`left_wheel_set_speed` → 绑定具体驱动）。

---

## 四、safe_off 语义边界（现有认知很专业，需落到分类上）

`dal_servo.h:93-96` 与 `wink_actuator_registry.h:8-10` 已明确：**duty=0 对舵机 = limp = 安全，但对 DC 电机可能是 coast（滑行）而非 brake（制动），不是通用安全态。**

这恰恰证明「通用 `dal_motor`」是错的——各类电机的安全关断语义都不同：

| 电机 | 安全关断语义 |
|---|---|
| DC 有刷 | 需区分 `brake`（H 桥下桥短接制动）vs `coast`（全关滑行） |
| 步进 | 保持力矩关断（可手动转）vs 锁定 |
| 闭环伺服 | 使能失效 / 抱闸 |

当前 `dal_motor_safe_off` 注释写「滑行/刹车」二选一未定，语义模糊。

**建议**：`dal_dc_motor` 显式提供 `dal_dc_motor_brake()` 与 `dal_dc_motor_coast()` 两个 API，`safe_off` 明确绑定其一并文档化。

---

## 五、推荐的完整电机执行器分类与命名规范（参考基线）

> 原则：**按控制语义（控制物理量）拆分 DAL，而非按「是不是电机」。** 目录统一置于 `dal/include/actuator/`，无需新建子目录。命名遵循 `dal_<device>_<action>()`，返回 `wink_status_t`，POD + 命名 API（ADR-0004）。

### 5.1 分类总表

| # | DAL 驱动 | 控制语义 | 典型器件 / 芯片 | 关断语义 | 优先级 |
|---|---|---|---|---|---|
| 1 | `dal_dc_motor` | 占空比 / 速度（开环） | 有刷 DC + H 桥（L298N / TB6612 / DRV8833） | brake / coast 二选一显式 | 🔴 现有，改名 |
| 2 | `dal_servo` | 绝对角度（开环 PWM） | SG90 / MG996R 航模舵机 | limp（duty=0） | ✅ 已有 |
| 3 | `dal_stepper` | 步数 / 位置（开环） | 28BYJ-48、A4988、DRV8825、TMC2209 | hold（保持力矩）/ release | 🟡 建议新增 |
| 4 | `dal_servo_motor` | 闭环位置 / 速度 / 力矩 | 工业伺服、ODrive、VESC（总线型） | disable / 抱闸 | 🟢 roadmap |
| 5 | `dal_bldc` | 换相 / FOC | 云台 / 轮毂 BLDC（本地 FOC） | disable（三相断开） | 🟢 roadmap（ADR-0026） |

> ⚠ **命名陷阱**：`dal_servo`（航模舵机，50Hz PWM 开环给角度）与 `dal_servo_motor`（工业闭环伺服，带编码器的位置/速度/力矩环）中文都叫「舵机/伺服」，但是**两个完全不同的器件**。命名必须区分，切勿合并。

### 5.2 各驱动推荐 API 契约

**1) `dal_dc_motor`（有刷直流，改名自 dal_motor）**
```c
wink_status_t dal_dc_motor_init(dal_dc_motor_t *dev, const dal_dc_motor_config_t *cfg);
wink_status_t dal_dc_motor_set_speed(dal_dc_motor_t *dev, float speed);  /* -1.0~1.0 */
wink_status_t dal_dc_motor_brake(dal_dc_motor_t *dev);   /* H 桥短接制动 */
wink_status_t dal_dc_motor_coast(dal_dc_motor_t *dev);   /* 全关滑行 */
wink_status_t dal_dc_motor_safe_off(dal_dc_motor_t *dev);/* 绑定 brake 或 coast，文档钉死 */
wink_status_t dal_dc_motor_deinit(dal_dc_motor_t *dev);
```

**2) `dal_servo`（航模舵机，已有）**
```c
wink_status_t dal_servo_init(dal_servo_t *dev, const dal_servo_config_t *cfg);
wink_status_t dal_servo_set_angle(dal_servo_t *dev, float angle);  /* 0~180 度 */
wink_status_t dal_servo_safe_off(dal_servo_t *dev);  /* duty=0 = limp */
wink_status_t dal_servo_deinit(dal_servo_t *dev);
```

**3) `dal_stepper`（步进，建议新增）**
```c
wink_status_t dal_stepper_init(dal_stepper_t *dev, const dal_stepper_config_t *cfg);
wink_status_t dal_stepper_move_steps(dal_stepper_t *dev, int32_t steps);       /* 相对步数 */
wink_status_t dal_stepper_set_target_position(dal_stepper_t *dev, int32_t pos);/* 绝对步位 */
wink_status_t dal_stepper_get_position(const dal_stepper_t *dev, int32_t *pos);
wink_status_t dal_stepper_set_speed(dal_stepper_t *dev, float steps_per_s);    /* 速度上限 */
wink_status_t dal_stepper_home(dal_stepper_t *dev);       /* 回零（可选限位） */
wink_status_t dal_stepper_hold(dal_stepper_t *dev);       /* 保持力矩 */
wink_status_t dal_stepper_release(dal_stepper_t *dev);    /* 释放（可手动转） */
wink_status_t dal_stepper_safe_off(dal_stepper_t *dev);   /* 绑定 hold 或 release */
wink_status_t dal_stepper_deinit(dal_stepper_t *dev);
```
> 步进核心是**步数/位置**语义，与 DC 的占空比语义正交。config 需含 step/dir 引脚（或 4 相引脚）、每圈步数、微步数。脉冲时序生成属驱动内部（真机可用定时器/RMT，仿真旁路到位置增量）。

**4) `dal_servo_motor`（闭环伺服，roadmap）**
```c
wink_status_t dal_servo_motor_init(...);
wink_status_t dal_servo_motor_set_position(dal_servo_motor_t *dev, float pos);   /* 单位文档钉死 */
wink_status_t dal_servo_motor_set_velocity(dal_servo_motor_t *dev, float vel);
wink_status_t dal_servo_motor_set_torque(dal_servo_motor_t *dev, float torque);
wink_status_t dal_servo_motor_get_feedback(const dal_servo_motor_t *dev, dal_servo_motor_feedback_t *fb);
wink_status_t dal_servo_motor_safe_off(dal_servo_motor_t *dev);  /* disable / 抱闸 */
```
> 总线型（CAN/485/EtherCAT）：DAL 负责组帧/校验/解析，对上屏蔽通信细节（见 DAL §8.1 外部总线型智能驱动）。

**5) `dal_bldc`（无刷/FOC，roadmap，ADR-0026）**
```c
wink_status_t dal_bldc_init(...);
wink_status_t dal_bldc_set_torque(dal_bldc_t *dev, float torque);
wink_status_t dal_bldc_set_velocity(dal_bldc_t *dev, float vel);
wink_status_t dal_bldc_safe_off(dal_bldc_t *dev);  /* 三相断开 */
```
> ⚠ 本地 FOC 的 10kHz+ 实时换相环（Clarke/Park/SVPWM）属**平台强相关的前台 ISR**，其分层归属需先由 ADR 裁决（见前评审 P0-2 + PAL 缺硬件定时器契约问题）。BAL/control 只在低频协作循环读写目标缓冲。

### 5.3 命名规范速记

| 规则 | 说明 |
|---|---|
| 按控制语义命名 | `dal_dc_motor`（占空比）/ `dal_stepper`（步数）/ `dal_servo`（角度）/ `dal_servo_motor`（闭环）/ `dal_bldc`（FOC） |
| `motor` 不做器件名 | 泛称仅用于 codegen Capability 别名层（`left_wheel_*`），不做具体 DAL 前缀 |
| 舵机 ≠ 伺服电机 | `dal_servo`（航模）与 `dal_servo_motor`（工业闭环）严格区分 |
| 关断语义随器件 | 每类各自注册语义正确的 safe-off（`wink_actuator_registry`），不外推通用范式 |
| 目录归属 | 全部 `dal/include/actuator/`，不新建子目录 |

---

## 六、行动建议（按优先级）

| 优先级 | 建议 | 理由 |
|---|---|---|
| 🔴 P0 | `dal_motor` → 重命名 `dal_dc_motor` | 名不副实、误导 AI；现仅 chassis/closed_loop 引用，改名成本最低 |
| 🔴 P0 | `dal_dc_motor` 拆 `brake()` / `coast()`，`safe_off` 明确绑定 | 安全语义，当前模糊 |
| 🟡 P1 | 新增 `dal_stepper`（step/dir + 位置语义） | 步进是低代码/教育高频需求，当前完全空白 |
| 🟡 P1 | 预留 `dal_servo_motor` 命名，避免与 `dal_servo` 撞名 | 概念澄清，防未来混淆 |
| 🟢 P2 | `dal_bldc` / FOC 延后，配合 ISR 分层 ADR | 复杂度高，非当前需求（ADR-0026） |

**一句话结论**：目录分类（`actuator/`）没问题，问题在驱动粒度与命名。`dal_motor` 应正名为 `dal_dc_motor`，并按控制语义补齐 `dal_stepper` 等，从源头杜绝 AI 用错驱动。

---

## 七、需要 Owner 确认

1. 是否接受 `dal_motor` → `dal_dc_motor` 重命名（建议起一个 ADR 记录）？
2. `dal_stepper` 是否纳入近期路线（低代码画布是否已有步进电机需求）？
