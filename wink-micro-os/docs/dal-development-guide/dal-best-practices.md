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
| **驱动面** | `type`、引脚/总线、`variant`、`enable_pin`、`*_pin` 等 advanced |
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
| A/B 量纲分类 | A 类执行器命令全 Profile 统一定标整数，B 类传感器测量用 float/定点分化（[ADR-0056](../../../docs/design/decisions/0056-cross-profile-quantity-ab-class-and-scaled-integers.md)） |
| Profile 内存模式 | Full Profile (32-bit) 采用 POD 深拷贝 `config_t`；Micro Profile (8-bit) 采用 ROM 零拷贝引用（[dal-micro-profile-spec.md](./dal-micro-profile-spec.md)） |
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
| **`role`** | 可选；缺省用驱动 `default_role` | **能力角色接口**（能力平面） | 如 `distance_sensor`、`binary_indicator`；回答「当什么用」，codegen 生成 `{name}_{verb}`。详见 [`dal-role-architecture-spec.md`](./dal-role-architecture-spec.md) SSOT。**不是**产品级叙事 |
| **`variant`** | 仅「该 type 已登记同族变体」时出现（典型 `dc_motor` / `encoder` / `mono_oled`） | 同族内**怎么接线 / 解码 / 面板** | 变体变了才需要；芯片不同但变体相同 → 可省略。本身是 **config 枚举**（runtime `switch`）；可选 `#if WINK_*_HAS_*` 由 codegen 按变体 **并集**裁 `.text`（Wave B）。**非全 type 必填** |
| **`enable_pin`** | 该芯片/模块有软件可控使能脚时 | STBY、nSLEEP 等 | 板级焊死高电平则**不写** |
| **`driver_ic`** | 可选糖衣，**一般不需要** | 芯片/模块别名（如 `tb6612`） | 经 `ic_to_variant_map` 推导 `variant`；与 `variant` 冲突应校验报错。旧键 `drive_mode`/`decode_mode`/`panel_variant` 迁移期 deprecated |

一句话（拓扑相关）：

```text
type        → 控制语义族 / 哪个 DAL（驱动平面）
role        → 能力角色 / App 怎么调（能力平面；可缺省）
variant     → 同族变体（接线 / 解码 / 面板）——变了才用
driver_ic   → 可选别名，能省则省
enable_pin  → 可选使能脚，有且要软件控才写
```

> 💡 **架构心法（三维抽象边界）**：
> - **`type` = 驱动护城河**：只要底层的通信协议、控制物理量单位（角度 / 步数 / 占空比 / 原始电压）和 C 驱动代码改变，就必须建立新的 `type`。
> - **`variant` = 同族避风港**：驱动代码框架不变，仅硬件接线、解码倍率或面板命令集等差异，就在 `type` 内部用 `variant` 枚举消化，绝不向 App 暴露新的 API。
> - **`role` = 应用变形金刚**：底层如何驱动与采集由 DAL 固化，但上层 App 想以何种角色接口称呼它、调用它（如 `hmi_dial` vs `pulse_counter`），交由 `role` 进行能力平面映射。

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

与 `variant` 的边界：`variant` 只在**同一 `type` 内**选已登记同族变体，既不替代 `type`，也不替代 `role`。

**如何实现 / 扩展 Role（codegen 专文）**：[role-interface-codegen.md](./role-interface-codegen.md)。  
角色动词表 SSOT：[01-app-business-logic.md § Role Interface](../../../docs/design/03-app-codegen/01-app-business-logic.md)。  
BAL 边界：[06-bal-layer.md](../../../docs/design/02-wink-micro-os/06-bal-layer.md)。

### 3.1 原则

- **对外 API** 按业务语义冻结：`set_speed` / `coast` / `brake` / `safe_off` 等。
- **对内**用有限枚举表示**电气拓扑**（一等公民），不是芯片型号表。
- **芯片名**最多作为 JSON `driver_ic` 别名，由 codegen 映射到 `variant` + 默认脚；**不要**进入 `dal_*.h` 的公共类型名。

推荐 `dc_motor` config 方向（落地时以头文件为准；未实现前勿假定字段已存在）：

| 字段意图 | 说明 |
|----------|------|
| `variant` | 默认 **`in_in`**（PWM + IN_A/IN_B，今日实现）；**预留** `phase_enable`、`pwm_on_in`（未实现 → init `WINK_ERR_UNSUPPORTED`） |
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

纯 runtime `switch(variant)`：**性能可忽略**；**默认不会**因某实例未选某模式而自动从 `.text` 删掉分支。体积敏感时再上 `HAS_*`；双拓扑常驻通常可接受。

### 3.3 按拓扑裁剪（可选能力宏，示意）

当第二种拓扑（如 PWM-on-IN）实现明显变长、且多数 App 只用 Phase/Enable 时，用 **codegen 扫描 JSON → 写 CMake `-D`**，在 DAL 内包住分支。示意（字段落地前勿当已编译）：

```c
/* app_options.cmake / 生成头：由 codegen 根据 wink-app.json 汇总 */
/* 若任一 dc_motor 的 variant == pwm_on_in → HAS=1，否则 0 */

wink_status_t dal_dc_motor_set_speed(dal_dc_motor_t *dev, float speed)
{
    /* … clamp / init 检查 … */
    switch (dev->config.variant) {
    case DAL_DC_MOTOR_VARIANT_IN_IN:
        /* PWM + IN_A/IN_B（当前实现路径） */
        return apply_in_in(dev, speed);

#if WINK_DC_MOTOR_HAS_PWM_ON_IN
    case DAL_DC_MOTOR_VARIANT_PWM_ON_IN:
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
| 谁写宏 | Codegen 扫本 App 所有相关实例的 `variant`（及芯片别名映射结果）的**并集** |
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
| `variant` | 默认 `x1_rising`；x2/x4 **未实现** → init `WINK_ERR_UNSUPPORTED` |
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

- JSON **`type` 为 `mono_oled`**；同族面板差异用 `variant`（`ssd1306`/`sh1106`）；异族 SPI 面板 → 新 `type`。
- App 推荐 `{name}_clear` / `draw_text` / `flush`，不依赖芯片字符串。

### 3.6 非阻塞脉冲态器件（磁保持继电器等）

某些器件的输出是**限时脉冲**而非持续电平（如双线圈磁保持继电器 SET/RESET 线圈，设计脉宽 30~100ms，持续通电会烧毁）。这类器件的标准模式（见 `dal_relay` 与 [ADR-0058](../../../docs/design/decisions/0058-relay-actuator-classification-and-latching-semantics.md)）：

- **发起端非阻塞**：`set/on/off` 只驱动目标线圈脚到 active、记录 `pulse_start_ms`、置 `pulse_active=true` 后立即返回；**严禁 busy-wait 脉冲宽度**（违反 `WINK_STRICT_NONBLOCKING`）。
- **清除端是 `dal_<type>_poll(dev)`**：到点（`pal_os_get_ms() - pulse_start_ms >= pulse_duration_ms`）把相关脚写回 inactive、清 `pulse_active`。对非脉冲拓扑它是廉价 no-op。
- **poll 必须被周期驱动**：在 driver yaml 设 `config.poll_fn: dal_<type>_poll`，codegen 自动生成 `WINK_DEFINE_POLL_THUNK` 并经 `wink_runtime_register_poll` 挂到 runtime tick（SIMULATION 与 native 两条主循环都会派发）。裸用 DAL 而不接 poll 会导致脉冲不清除——头文件顶层必须明示。
- **break-before-make**：每次发新脉冲前先把所有相关脚写 inactive，再驱动目标脚，避免快速反向时多脚同时 active（双线圈重叠 / H 桥穿通）。
- **脉宽 fail-closed**：`pulse_duration_ms == 0` 回退默认常量；超过硬上限（如 `DAL_RELAY_MAX_PULSE_MS`）init 直接 `WINK_ERR_INVALID_ARG`，防 uint16 最大值造成超长脉冲。
- **init 建立已知态**：磁保持器件掉电后物理触点保持，init 应按 `initial_state` 发一次 SET/RESET 脉冲使软件态与物理态一致；deinit 非阻塞路径不保证 RESET 脉冲满宽，故**不得**承诺"安全断开物理负载"，文档须如实声明，运行期可靠断开由 poll/`safe_off` 承担。

---

## 4. API 形态清单（新驱动自检）

每个可失败操作返回 `wink_status_t`，并通常提供：

- `dal_<type>_init` / `deinit`
- 语义读写或设定（如 `set_speed_promille`、`set_angle_ddeg`）
- **A 类量纲类型**：全 Profile 统一定标整数（千分比 ‰ / ddeg 0.1度）；参数越界执行**隐式钳位饱和（saturate）**，禁止溢出回卷（[ADR-0056](../../../docs/design/decisions/0056-cross-profile-quantity-ab-class-and-scaled-integers.md)）。
- **执行器 `safe_off`**：未初始化调用 MUST 安全返回 `WINK_OK`（确保故障消费链路连贯）；初始化后执行安全关断（如 DC 刹车 brake，ADR-0048）。
- **头文件 Stub**：`WINK_USE_*` 关闭时的 `WINK_UNAVAILABLE` stub。

执行器类别、目录归属（`actuator/` vs `comm/` 等）遵循活规范与近期 ADR（如 VESC 协议帧归 `actuator/` 等约定）。

---

## 5. Codegen 与裁剪（ADR-0051）

- **驱动描述 SSOT**：`wink-micro-os/codegen/drivers/<type>.yaml`（+ 可选 `templates/<type>_init.c.j2`）；引擎动态扫描扩展根。内置 Python 插件仅为历史例外（见 [codegen/README.md](../../codegen/README.md)）。
- **App 有 JSON**：只打开用到的 `WINK_USE_*`。
- **无 JSON / 全量驱动**：用于验收 stub 路径；若引入了拓扑 `HAS_*`，全量构建时各 `HAS_*=1`。
- **拓扑能力宏**：见 §3.3（`WINK_DC_MOTOR_HAS_PWM_ON_IN` 等）。
- **新增类型流程**：[adding-peripheral.md](./adding-peripheral.md)。

---

## 6. 仿真与旁路（[Wasm 仿真 3.0 SSOT](../../../docs/design/04-wasm-simulation-3.0/00-README.md)）

- **通道选择**：优先通过 Channel 1 (GPIO) / Channel 2b (PWM) 进行底层同源仿真测试；若要跳过底层 PAL 直接与前端交互，可通过 WASM Bridge 挂载 **Channel 4 语义 Bypass**（`dal_<type>_*` Direct Bridge，详见 [08-channel-routing.md](../../../docs/design/04-wasm-simulation-3.0/02-mechanisms/08-channel-routing.md)）。
- **Manifest 对齐**：仿真侧 Manifest / 元数据中的 `type` 字符串与 codegen YAML 的 `type:` 须**逐字一致**。

---

## 7. 相关阅读

- 手册索引：[README.md](./README.md)
- 快速上手：[dal-quickstart.md](./dal-quickstart.md)
- Role Interface codegen：[role-interface-codegen.md](./role-interface-codegen.md)
- 活规范全文：[01-dal-device-abstraction.md](../../../docs/design/02-wink-micro-os/01-dal-device-abstraction.md)
- Role 动词表 SSOT：[01-app-business-logic.md](../../../docs/design/03-app-codegen/01-app-business-logic.md)
- 未来 role/意图平面（⏸️）：[2026-07-28-wink-app-role-intent-evolution-plan.md](../../../docs/design/implementation-plans/2026-07-28-wink-app-role-intent-evolution-plan.md)
- BAL 与 DC safe-off 回退：[06-bal-layer.md](../../../docs/design/02-wink-micro-os/06-bal-layer.md)
