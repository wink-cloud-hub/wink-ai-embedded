# DAL 规范与最佳实践

本文是活规范 [`01-dal-device-abstraction.md`](../../../docs/design/02-wink-micro-os/01-dal-device-abstraction.md) 的**实践摘要**，并固化 H 桥等变体扩展的约定。若与活规范冲突，以活规范 + Accepted ADR 为准，并回写活规范。

| 项 | 内容 |
|----|------|
| **关联技术设计** | [user-surface-insulation-design.md](../../../docs/design/tech-designs/2026-07-28-user-surface-insulation-design.md) |
| **关联实施计划** | [user-surface-phase1-plan.md](../../../docs/design/implementation-plans/2026-07-28-user-surface-phase1-plan.md) |
| **关联评审** | [dal-control-semantic-completeness-review §10](../../../docs/design/reviews/2026-07-28-dal-control-semantic-completeness-review.md)；[user-surface-phase1-plan-review.md](../../../docs/design/reviews/2026-07-28-user-surface-phase1-plan-review.md) |

---

## 0. 用户稳定面 vs 驱动面（Phase 1）

> 完整机制见 [user-surface-insulation-design.md](../../../docs/design/tech-designs/2026-07-28-user-surface-insulation-design.md)。

| 概念 | 含义 |
|------|------|
| **用户稳定面** | App C 推荐 `{name}_{verb}`（Role）；JSON 的 `role` + stable knobs |
| **驱动面** | `type`、引脚/总线、`drive_mode`、`decode_mode`、`enable_pin` 等 advanced |
| **无板卡模板** | 接线仍写在本 App `wink-app.json`；**不**锁死引脚 |
| **Escape Hatch** | `&instance` + `dal_*`；lint warn + allowlist |
| **BAL-backed** | 部分 Role 动词（如 button 事件 enable）内部调 BAL；仍属稳定面 |

**常见误称纠正**：今日 H 桥接线（PWM + IN_A + IN_B）的拓扑枚举名是 **`in_in`**，**不是** `phase_enable`。`phase_enable`（单 PHASE + ENABLE/PWM）与 `pwm_on_in` 为**预留**拓扑，init fail-closed。

---

## 1. 硬约束（写驱动前）

| 规则 | 说明 |
|------|------|
| POD + 命名 API | 无 `ops` / vtable / `container_of` 子类（[ADR-0004](../../../docs/design/decisions/0004-static-dispatch-vs-runtime-ops.md)） |
| 负数错误码 | `wink_status_t`：0 成功，负数为错（[ADR-0001](../../../docs/design/decisions/0001-error-code-sign-convention.md)） |
| 双 target | 同源可编 wasm / ESP-IDF（[ADR-0002](../../../docs/design/decisions/0002-dual-target-compilation.md)） |
| 语义命名 | 控制语义优先（如 `dc_motor`、`rc_servo`）；禁止用泛称 `motor` 当 DAL 类型前缀（[ADR-0048](../../../docs/design/decisions/0048-actuator-control-semantic-naming.md)） |
| 资源认领 | init 认领 PWM/GPIO 等；deinit / 失败路径释放 |
| 分层 | App/BAL → DAL → PAL；改完跑 `wink.py lint --pack layering --pack api` |

---

## 2. 何时合并驱动 vs 新开 `dal_*`

```text
控制原理 / 电气拓扑是否同类？
  ├─ YES（例：多种 H 桥，都是 PWM+方向或可枚举的少数拓扑）
  │     → 同一 dal_dc_motor（或同类），用 config 区分拓扑
  └─ NO（例：GPIO H 桥 vs I2C 智能驱动 vs 串口 ESC）
        → 独立 dal_* + 独立 WINK_USE_* + codegen 类型
          （设备树侧可用宏/别名对 App 保持同一调用名，见活规范机制二）
```

**禁止**：为 L298N / TB6612 / DRV8833 各建一套公共 API 或 `WINK_USE_L298N` 式芯片宏。

---

## 3. 同类芯片 / 模块：语义不变 + 拓扑枚举

### 3.0 `wink-app.json` 字段分层

**不要**把下列字段都当成「所有外设通用」。真正跨类型必填的只有 `type`；其余按**该 `type` 的 schema** 出现。

| 字段 | 适用范围 | 含义 | 要点 |
|------|----------|------|------|
| **`type`** | **全部**实例必填 | **控制语义族** + DAL 驱动绑定（驱动平面） | 如 `dc_motor`、`ultrasonic`；回答「是什么驱动 / 控什么量」，**不是**芯片名，也不是「GPIO 物理细节」 |
| **`role`** | 可选；缺省用驱动 `default_role` | **能力角色接口**（能力平面） | 如 `distance_sensor`、`binary_indicator`；回答「当什么用」，codegen 生成 `{name}_{verb}`。**不是**产品级「左轮 / 云台」叙事（那属未来意图平面） |
| **`drive_mode`** | 仅「同语义、多电气拓扑」的类型（典型 `dc_motor`） | 同族内**怎么接线/驱动** | 拓扑变了才需要；芯片不同但拓扑相同 → 可省略。本身是 **config 枚举**（runtime `switch`）；可选 `#if WINK_*_HAS_*` 由 codegen 按 mode **并集**裁 `.text`，≠「写了 mode = 条件编译」 |
| **`enable_pin`** | 该芯片/模块有软件可控使能脚时 | STBY、nSLEEP 等 | 板级焊死高电平则**不写** |
| **`driver_ic`** | 可选糖衣，**一般不需要** | 芯片/模块别名（如 `tb6612`） | 与 `drive_mode` 易重复；冲突应校验报错 |

一句话（拓扑相关）：

```text
type        → 控制语义族 / 哪个 DAL（驱动平面）
role        → 能力角色 / App 怎么调（能力平面；可缺省）
drive_mode  → 同族内拓扑（怎么驱）——接线变了才用
driver_ic   → 可选别名，能省则省
enable_pin  → 可选使能脚，有且要软件控才写
```

全局 JSON 骨架与引脚约定见 [`../wink-app-json-guide.md`](../wink-app-json-guide.md)。H 桥扩展字段落地前，以各 `drivers/<type>.py` 的 `required_fields` 为准；上表是约定方向。

#### `type` 与 `role`（勿混为一谈）

定稿句：

> **`type`**：器件的 **控制语义族与 DAL 绑定**（工程/驱动平面）。  
> **`role`**：面向 App 的 **能力角色接口 / Role Interface**（能力平面；缺省用驱动的 `default_role`；由 codegen 写入 `device_tree.h` 的 `{name}_{verb}`）。  
> **`role` 不是 BAL**：BAL 是独立的可复用算法层（PID、闭环电机、底盘等）；勿把「能力角色」说成「BAL 能力」。  
> 产品级「轮子 / 关节 / 运动意图」属于**未来**用户平面，**不要**与当前 `role` 混为一谈。

更短：**`type` 回答「是什么驱动」；`role` 回答「当什么用」（App 封装，非 BAL）。**

| 层 | 职责 | 与 JSON 字段 |
|----|------|----------------|
| DAL | 控制语义 API（`dal_*`） | **`type`** 绑定 |
| Role Interface | codegen 包一层 DAL → App 友好动词 | **`role`**（可缺省） |
| BAL | 算法 / 闭环 / 编排组件 | **无**对应 `devices[].role`；App 显式调用 `wink_*` / BAL API |
| App | 业务状态机 | 推荐 `{name}_{verb}`；复杂场景调 DAL / BAL |

| | 例 |
|--|-----|
| `type: "ultrasonic"` | 测距类 DAL（控制语义：距离） |
| `role: "distance_sensor"`（可省略） | App 用 `front_radar_read_distance()` 等角色动词 |
| ~~把 type 说成「物理语义」~~ | 易误解成 GPIO/PWM；应说 **控制语义**（占空比 / 角度 / 距离…） |
| ~~把现网 role 说成完整「业务语义」或 BAL~~ | 现网是 **App 侧 Role Interface**；「左轮要速度」等属 [role/意图演进计划](../../../docs/design/implementation-plans/2026-07-28-wink-app-role-intent-evolution-plan.md)（⏸️） |

与 `drive_mode` 的边界：`drive_mode` 只在**同一 `type` 内**选接线拓扑，既不替代 `type`，也不替代 `role`。

**如何实现 / 扩展 Role（codegen 专文）**：[role-interface-codegen.md](./role-interface-codegen.md)。  
角色动词表 SSOT：[01-app-business-logic.md § Role Interface](../../../docs/design/03-app-codegen/01-app-business-logic.md)。  
BAL 边界：[06-bal-layer.md](../../../docs/design/02-wink-micro-os/06-bal-layer.md)。

### 3.1 原则

- **对外 API** 按业务语义冻结：`set_speed` / `coast` / `brake` / `safe_off` 等。
- **对内**用有限枚举表示**电气拓扑**（一等公民），不是芯片型号表。
- **芯片名**最多作为 JSON `driver_ic` 别名，由 codegen 映射到 `drive_mode` + 默认脚；**不要**进入 `dal_*.h` 的公共类型名。

推荐 `dc_motor` config 方向（落地时以头文件为准；未实现前勿假定字段已存在）：

| 字段意图 | 说明 |
|----------|------|
| `drive_mode` | 默认 **`in_in`**（PWM + IN_A/IN_B，今日实现）；**预留** `phase_enable`、`pwm_on_in`（未实现 → init `WINK_ERR_UNSUPPORTED`） |
| `enable_pin`（可选，默认 -1） | STBY / nSLEEP（**高有效**）；板级焊死高电平则可不配 |
| 现有脚 | `pwm_channel`、`dir_pin_a`、`dir_pin_b` |

当前 `dal_dc_motor` 实现覆盖 **IN/IN**（PWM 调速 + 双方向脚；TB6612/L298N 等常见接线）。

**IN/IN 真值表**（`dir_pin_a` = A，`dir_pin_b` = B）：

```text
dir_a  dir_b | state
  0      0   | coast
  1      0   | forward
  0      1   | reverse
  1      1   | brake (short)
```

**`safe_off` 层级**（ADR-0048 + enable 路径；**无 enable 时仍绑 brake**，单脚不改为 coast+OK）：

1. `enable_pin >= 0`（init 后存储值）→ 有 `dir_pin_b` 时先 **brake**，再拉低 enable（硬关断）；返回 `WINK_OK`。
2. 无 enable 且 `dir_pin_b >= 0` → **`dal_dc_motor_brake`**（ADR-0048 默认）。
3. 无 enable 且单方向脚 → **`WINK_ERR_UNSUPPORTED`**（禁止静默 coast）。

`phase_enable` / `pwm_on_in` 属未来拓扑扩展；请求时 init 或首调返回 `WINK_ERR_UNSUPPORTED`（fail-closed）。

### 3.2 条件编译用在哪

| 宏 | 用途 |
|----|------|
| `WINK_USE_<TYPE>`（如 `WINK_USE_DC_MOTOR`） | **整类**驱动有无：声明则编入，否则 stub（已有） |
| （可选）`WINK_DC_MOTOR_HAS_<MODE>` | 按 App JSON **实际用到的拓扑**裁分支；裁的是能力，不是芯片名 |
| ~~`WINK_USE_TB6612`~~ | **不要** |

纯 runtime `switch(drive_mode)`：**性能可忽略**；**默认不会**因某实例未选某模式而自动从 `.text` 删掉分支。体积敏感时再上 `HAS_*`；双拓扑常驻通常可接受。

### 3.3 按拓扑裁剪（可选能力宏，示意）

当第二种拓扑（如 PWM-on-IN）实现明显变长、且多数 App 只用 Phase/Enable 时，用 **codegen 扫描 JSON → 写 CMake `-D`**，在 DAL 内包住分支。示意（字段落地前勿当已编译）：

```c
/* app_options.cmake / 生成头：由 codegen 根据 wink-app.json 汇总 */
/* 若任一 dc_motor 的 drive_mode == pwm_on_in → HAS=1，否则 0 */

wink_status_t dal_dc_motor_set_speed(dal_dc_motor_t *dev, float speed)
{
    /* … clamp / init 检查 … */
    switch (dev->config.drive_mode) {
    case DAL_DC_MOTOR_MODE_IN_IN:
        /* PWM + IN_A/IN_B（当前实现路径） */
        return apply_in_in(dev, speed);

#if WINK_DC_MOTOR_HAS_PWM_ON_IN
    case DAL_DC_MOTOR_MODE_PWM_ON_IN:
        /* PWM 打在输入脚（如部分 DRV8833 接线） */
        return apply_pwm_on_in(dev, speed);
#endif

    default:
        return WINK_ERR_UNSUPPORTED; /* fail-closed：未知/未编入拓扑 */
    }
}
```

约定：

| 项 | 约定 |
|----|------|
| 宏命名 | `WINK_<TYPE>_HAS_<MODE>`，MODE 与枚举后缀对齐（如 `PWM_ON_IN`） |
| 谁写宏 | Codegen 扫本 App 所有相关实例的 `drive_mode`（及芯片别名映射结果）的**并集** |
| 无 JSON / 全量驱动构建 | 各 `HAS_*` 置 **1**（或未定义时头文件 `#ifndef` 默认 1），避免 stub/CI 缺分支 |
| 仅 Phase/Enable 的 App | `WINK_DC_MOTOR_HAS_PWM_ON_IN=0` → `case PWM_ON_IN` **不进镜像**；若运行时 config 误写成该模式 → 落到 `default` → `WINK_ERR_UNSUPPORTED` |
| 与 `WINK_USE_DC_MOTOR` | `USE=OFF` 整文件 stub；`USE=ON` 且 `HAS_*=0` 只裁**拓扑分支**，API 符号仍在 |
| 禁止 | 按芯片名 `#if WINK_USE_DRV8833` 包同一逻辑 |

未上 `HAS_*` 前：保留完整 `switch` 即可，两拓扑代码常驻，一般可接受。

### 3.4 Fail-closed

未知拓扑 / 单脚无法 brake：返回 `WINK_ERR_UNSUPPORTED`，禁止静默当成 coast（DC `safe_off`→brake，见 ADR-0048）。

### 3.5 Phase 1 语义契约（encoder / rc_servo / ssd1306）

#### `encoder`（Role：`pulse_counter`）

| 项 | 契约 |
|----|------|
| `decode_mode` | 默认 `x1_rising`；x2/x4 **未实现** → init `WINK_ERR_UNSUPPORTED` |
| x1 协议 | A 上升沿采 B；B 高 → `count++`，B 低 → `count--`；无 `pin_b` → 仅递增 |
| `invert` | **交换 A/B 方向语义（换相极性）**；禁止仅在 `get_count` 取负冒充 |
| Role | `get_count` / `reset` 返回**原始脉冲**；**无 CPR**；`cpr` 名本 Phase 仅文档预留 |

#### `rc_servo`（Role：`angular_actuator`）

- `max_angle`：0 或未设 → **180.0f**；钳位 `angle ∈ [0, effective_max_angle]`。
- 脉宽映射（分母必须用 `effective_max_angle`，禁止写死 180 常量）：

```text
pulse_ms = min_pulse + (angle / effective_max_angle) * (max_pulse - min_pulse)
```

- Flash override wire v1 **不含** `max_angle`（本 Phase Non-goal）。

#### `ssd1306`（Role：`text_display`）

- JSON **`type` 保留芯片名 `ssd1306`**（本 Phase 不改名）；异族 SPI 面板 → 新 `type` 或未来 `panel_variant`。
- App 推荐 `{name}_clear` / `draw_text` / `flush`，不依赖芯片字符串。

---

## 4. API 形态清单（新驱动自检）

每个可失败操作返回 `wink_status_t`，并通常提供：

- `dal_<type>_init` / `deinit`
- 语义读写或设定（如 `set_speed`、`read`）
- 执行器：`safe_off`（语义在 ADR/头文件注释中写死，如 DC→brake）
- 头文件：`WINK_USE_*` 关闭时的 `WINK_UNAVAILABLE` stub

执行器类别、目录归属（`actuator/` vs `comm/` 等）遵循活规范与近期 ADR（如 VESC 协议帧归 `actuator/` 等约定）。

---

## 5. Codegen 与裁剪

- 插件：`wink-tools/tools/codegen/drivers/<type>.py`
- App 有 JSON：只打开用到的 `WINK_USE_*`
- 无 JSON：全量驱动（验收 stub 时需要）；若已引入拓扑 `HAS_*`，全量时各 `HAS_*=1`
- 拓扑能力宏：见 §3.3（`WINK_DC_MOTOR_HAS_PWM_ON_IN` 等）
- 新增类型流程：[adding-peripheral.md](./adding-peripheral.md)

---

## 6. 仿真与旁路

- `#ifdef SIMULATION` 旁路尽量靠下，让更多协议路径可同源测。
- 仿真 Manifest `type` 与 `DriverBase.type` 一致。

---

## 7. 相关阅读

- 手册索引：[README.md](./README.md)
- 快速上手：[dal-quickstart.md](./dal-quickstart.md)
- Role Interface codegen：[role-interface-codegen.md](./role-interface-codegen.md)
- 活规范全文：[01-dal-device-abstraction.md](../../../docs/design/02-wink-micro-os/01-dal-device-abstraction.md)
- Role 动词表 SSOT：[01-app-business-logic.md](../../../docs/design/03-app-codegen/01-app-business-logic.md)
- 未来 role/意图平面（⏸️）：[2026-07-28-wink-app-role-intent-evolution-plan.md](../../../docs/design/implementation-plans/2026-07-28-wink-app-role-intent-evolution-plan.md)
- BAL 与 DC safe-off 回退：[06-bal-layer.md](../../../docs/design/02-wink-micro-os/06-bal-layer.md)
