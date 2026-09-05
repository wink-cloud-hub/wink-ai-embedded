# ADR-0058：继电器执行器分类与磁保持语义裁决（含轮询闭环与 SINGLE_PIN 收敛）

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-08-05 |
| 触发 | `dal_relay` 架构评审（2026-08-05）发现四个真机/安全缺口：① 磁保持 `dal_relay_poll()` 全仓无生产调用者，SET 线圈拉 active 后永不清除 → 线圈持续通电过热；② 继电器 `is_actuator: false` 被排除在 `wink_actuator_safe_off_all()` 故障关断之外（同 role 的 LED 却是 `true`）；③ `LATCHING_SINGLE_PIN`（单线圈 H 桥）与双线圈共用逻辑，无 break-before-make 死区，存在 H 桥穿通风险且无硬件验证；④ deinit 对磁保持承诺"安全断开"但非阻塞下 RESET 脉宽≈0，`initial_state` 对磁保持也不发脉冲导致 `is_on` 与物理态不一致。 |
| 影响范围 | `wink-micro-os/dal/{include,src}/output/dal_relay.{h,c}`；`wink-micro-os/codegen/drivers/relay.yaml` + `templates/relay_*.c.j2`；codegen 通用轮询接线（`app_codegen.py`、`device_tree.c.j2`、`driver_record.py`、`drivers/base.py`）；`runtime/include/wink_runtime.h`（`WINK_DEFINE_POLL_THUNK`）；host PAL 测试钩子（`pal_hal_host.c`、`host_test_ctrl.h`）；DAL 单测 `test_dal_relay.c` |
| 决策者 | 项目 Owner（2026-08-05 确认四项建议：执行器+safe_off、移除 SINGLE_PIN、codegen 自动注册 poll、文档如实声明+init 建立已知态） |
| 关联 ADR | [ADR-0048](0048-actuator-control-semantic-naming.md)（执行器命名与 safe_off 绑定）；[ADR-0004](0004-static-dispatch-vs-runtime-ops.md)（POD + 静态命名分发）；[ADR-0024](0024-fault-three-phase-model-and-dal-deinit-contract.md)（deinit 契约）；[ADR-0056](0056-cross-profile-quantity-ab-class-and-scaled-integers.md)（binary/actuator_command 量纲） |
| 关联计划 | [implementation-plans/2026-08-05-wokwi-dal-type-coverage-plan/04-p0-output-relay-plan.md](../../implementation-plans/frontend/04-p0-output-relay-plan.md) |

---

## 背景（Context）

`dal_relay` 作为 P0 批次首个"零 PAL 欠账"外设于 2026-08-05 落地（commit f655208），骨架质量高（静态分发、资源治理、ABI 静态断言、init 失败回滚、编译期裁剪 stub），但评审发现四类会在真机磁保持继电器上造成硬件/安全后果的缺口：

1. **脉冲调度闭环缺失**：`dal_relay_poll()` 设计为非阻塞脉冲清除器（`turn_on` 仅拉线圈脚并记录 `pulse_start_ms`，到点由 poll 拉回），但除单测外无任何调用者。runtime 已提供 `wink_runtime_register_poll` 且每个 tick 派发（`wink_runtime.c` 的 SIMULATION 与 native 两条路径），继电器却未注册。结果：磁保持 SET 线圈持续通电，远超 `pulse_duration_ms`，可烧毁线圈；而单测既不推进 host 虚拟时间、也不断言 `pulse_active` 清除，给出"已覆盖"的虚假信心。
2. **故障安全关断缺位**：`relay.yaml` 标 `is_actuator: false`、`safe_off_fn: ""`；而同属 `binary_indicator` role 的 LED 是 `is_actuator: true` 并有 `dal_led_safe_off`。`wink_actuator_safe_off_all()` 在 watchdog/panic/assert/异常回滚路径统一关断执行器，继电器（可能切换负载/强电）却不会被关。
3. **未验证的 H 桥变体**：`LATCHING_SINGLE_PIN`（Panasonic TX-1 / HFE9 类单线圈 H 桥磁保持）与 `LATCHING_DUAL_PIN` 共用 `set_state` 分支。H 桥靠电流方向置位，快速反向时两脚需 break-before-make 死区，否则同侧上下管直通（shoot-through）。仓库无真实硬件、无死区、无该变体单测。
4. **磁保持语义失实**：deinit 文档承诺"自动断开线圈（安全状态）"，但非阻塞下 `turn_off` 发起 RESET 脉冲后立刻 `gpio_reset_pin`，脉宽≈0，磁保持触点物理保持导通——软件以为断开。init 对磁保持只置 inactive 而 `is_on = initial_state`，不发 SET/RESET 脉冲，`is_on` 对物理态撒谎。

## 决策（Decision）

### D1. 继电器归为执行器，补 `dal_relay_safe_off`
- `relay.yaml` 置 `is_actuator: true`，`config.safe_off_fn: dal_relay_safe_off`，codegen 自动生成 `WINK_DEFINE_ACTUATOR_THUNK` 并在 init 后注册到 `wink_actuator_registry`。
- `dal_relay_safe_off` 对齐 DAL-L-020~022（参照 `dal_led_safe_off`）：不标 `WINK_WARN_UNUSED_RESULT`、未 init 返回 `WINK_OK`、best-effort 调 `dal_relay_off`。绑定到 **off**（去激励），不是 brake/coast（继电器无制动语义）。
- 理由：故障时去激励的安全需求与 LED 同等或更强；`binary_indicator` 下两个器件应执行器分类一致。

### D2. 移除 `LATCHING_SINGLE_PIN` 变体（YAGNI）
- 删除枚举值 `DAL_RELAY_VARIANT_LATCHING_SINGLE_PIN` 及 yaml enum/map 项，保留 `DIRECT_GPIO` / `SSR` / `LATCHING_DUAL_PIN` 三个已可正确驱动的拓扑。
- 未来有真实 H 桥硬件时，以**独立变体**加回，并必须实现 break-before-make 相位序列（先两脚 inactive → 方向建立 → 脉冲）与死区文档/测试，不得复用双线圈分支。

### D3. 磁保持 poll 经 codegen 自动注册到 runtime tick
- codegen 通用化轮询接线：driver config 新增可选 `poll_fn`（如 `dal_relay_poll`）；`device_tree.c.j2` 为有 `poll_fn` 的器件生成 `WINK_DEFINE_POLL_THUNK(name, fn, type)`（新宏，签名 `void(void*)`，定义于 `wink_runtime.h`，仿 `WINK_DEFINE_ACTUATOR_THUNK`），并在 init 后 `wink_runtime_register_poll`。
- `dal_relay_poll` 对直驱/SSR 是廉价 no-op；磁保持到点把两脚写 inactive 清 `pulse_active`。这是 `wink_runtime_register_poll` 的首个 DAL 生产消费方（此前仅 button 经 BAL soft-timer 用），机制对后续非阻塞 DAL 状态机通用。
- poll 注册无 unregister（与 actuator 一致）：静态设备树生命周期内不重复 init；deinit 后 `initialized=false` 使 poll 成为 no-op，安全。

### D4. 磁保持语义如实声明 + init 建立已知态
- **init**：磁保持变体按 `initial_state` 发起一次 SET（true）或 RESET（false）脉冲（break-before-make 后驱动目标脚），使 init 返回后 `is_on` 与物理触点一致；脉冲由 poll 清除，两脚回到 inactive（零静态功耗）。`initial_state` 默认 false（断开）符合上电安全；这是有意的"建立已知态"，区别于 DAL-L-006 零能量——继电器默认态即断开。
- **deinit**：删除"自动断开（安全状态）"的泛化承诺。直驱/SSR deinit 确实写 inactive（去激励）；磁保持 deinit best-effort 发起 RESET 脉冲并写 inactive 后立即释放引脚，**非阻塞路径不保证 RESET 达到 `pulse_duration_ms` 宽度，因此不保证物理触点断开**（磁保持硬件特性）。运行期可靠断开由 `dal_relay_off()` + poll（或自动注册的 tick poll）完成；故障路径由 `dal_relay_safe_off()` 处理。
- `toggle` 文档显式警告：磁保持基于软件缓存 `is_on` 取反，需在 init 已知态建立后使用。

### 配套硬化（同批复）
- **脉宽校验**：`pulse_duration_ms == 0 → DAL_RELAY_DEFAULT_PULSE_MS(50)`；`> DAL_RELAY_MAX_PULSE_MS(1000)` init 直接 `WINK_ERR_INVALID_ARG`（防 uint16 最大值 65s 烧线圈）。
- **break-before-make**：`set_state` 对磁保持每次新脉冲前先把两脚写 inactive，消除快速反向时双脚同时 active。
- **API 命名对齐 §5.3**：`turn_on/turn_off/set_state` 是黑名单动词，重命名为 `dal_relay_on/off/set`（与 `dal_led_on/off/set` 一致）。驱动新、无 App 消费，此刻重命名成本最低。
- **release helper 死代码修正**：主 pin 是 `uint16_t`，移除恒真的 `!= (uint16_t)-1` 判断。
- **可观测性**：补 `dal_relay_get_last_status()` getter，一致更新 `last_status`。
- **host 测试钩子**：`pal_hal_host.c` 记录 `pal_gpio_write` 电平，新增 `pal_host_get_gpio_level/reset_gpio_levels`，使极性、脉冲序列、break-before-make 可断言。
- 单测 16 项：推进虚拟时间验证脉冲清除、active-high/low 极性、SET/RESET 脉冲走向、break-before-make 不重叠、脉宽越界/默认、safe_off、last_status、资源冲突等。

## 后果与约束（Consequences & Constraints）

- **正向**：磁保持继电器脉冲闭环（线圈静态功耗归零、防烧毁）；继电器纳入故障安全关断；消除 H 桥穿通隐患；`is_on`/deinit 语义与物理一致；`poll_fn` codegen 机制为后续非阻塞 DAL 复用；单测从"虚假覆盖"变为真实时间推进+电平断言。
- **约束**：
  - 磁保持继电器**必须**经 codegen 设备树使用（自动注册 poll）；裸用 DAL 而不调 `dal_relay_poll` 会导致线圈持续通电——头文件顶层注释已明示。
  - deinit/关机路径若需可靠断开磁保持负载，应由板级在断电前留出 ≥`pulse_duration_ms` 让 poll 完成 RESET 脉冲；软件无法在非阻塞 deinit 内保证。
  - 未来加回 SINGLE_PIN 必须独立变体 + break-before-make 死区 + 硬件验证。
- **兼容性**：公开 API 重命名（`turn_on/off/set_state` → `on/off/set`）。该驱动于同日落地、无 App/BAL 消费方、`binary_indicator` role binding 内部同步更新，无外部破坏。枚举删除 `LATCHING_SINGLE_PIN` 同样无使用方。
- **不做**：不实现阻塞式 deinit 脉冲（用户已选文档如实声明路径）；不给 poll 加 unregister（静态设备树，deinit 终态）；不改 BAL role 动词。

## 遵循与后续（Compliance & Follow-up）
- 回写 `docs/dal-development-guide/dal-role-architecture-spec.md`：relay 行执行器语义、`binary_indicator` 下 LED/relay 一致分类。
- 回写 `docs/dal-development-guide/dal-best-practices.md`：磁保持脉冲 + runtime poll 自动注册模式、break-before-make 范式。
- 更新实施计划 `04-p0-output-relay-plan.md` 对齐最终实现（变体收敛、safe_off、poll 接入、init 已知态）。

---

*本 ADR 状态变更请在此记录：*
- 2026-08-05：Proposed（架构评审后起草）。
- 2026-08-05：Accepted（Owner 确认四项决策；代码与测试同日落地，16/16 host 单测通过，codegen 207 测试通过）。

