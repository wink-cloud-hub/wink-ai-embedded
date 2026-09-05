# ADR-0031：按键事件后端由 JSON `event_drive` 配置选择（soft_poll / gpio_irq）

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-07-14 |
| 触发 | A+B 主路径要求「改配置不改 App」切换低延迟/低功耗按键采集；与 `{name}_enable_events` Role API 对齐 |
| 影响范围 | `wink-app.json` button schema；codegen `button` 驱动插件；BAL `wink_button_enable_events`（B 类，ADR-0032；由 helper / 过渡 `events_start` 演进）；DAL button（ISR/防抖协作）；host/wasm 降级策略 |
| 决策者 | 项目 Owner |
| 关联 ADR | [ADR-0002](../unisim/0002-dual-target-compilation.md)、[ADR-0003](../unisim/0003-simulation-fidelity-boundary.md)、[ADR-0012](0012-contract-honesty-over-silent-degradation.md)、[ADR-0018](0018-pal-irq-api-narrowing.md)、[ADR-0022](0022-event-queue-mbox-async-primitives.md)、[ADR-0023](0023-bal-business-abstraction-layer.md)、[ADR-0032 操作三类命名](0032-bal-role-operation-naming-classes.md) |
| 关联技术设计 | [tech-designs/2026-07-14-button-event-drive-backends.md](../../zh/tech-designs/core/2026-07-14-button-event-drive-backends.md) |
| 关联设计规范（已回写） | [03-app-codegen/01-app-business-logic.md](../../zh/design/03-app-codegen/01-app-business-logic.md)、[03-app-codegen/02-project-manifest-schema.md](../../zh/design/03-app-codegen/02-project-manifest-schema.md) |

---

## 背景（Context）

L1 App 契约已收敛为：

```c
user_button_enable_events();   /* 后端由 JSON event_drive 选择；投递 wink_event 队列 */
/* 业务只写 app_on_event */
```

Accepted 前实现**唯一后端**是 soft_timer 周期 `dal_button_poll`（`auto_poll_ms`）。这对常供电演示板足够，但无法仅靠配置满足：

1. **低功耗 / 深睡**：CPU 睡眠后 soft poll 无法运行，需要 GPIO（或 RTC）唤醒；
2. **更高边沿响应**：可接受仍做人机防抖，但希望边沿用中断唤醒而非 10ms 心跳。

同时必须守住：

- App **零改**切换策略（A+B）；
- App **禁止**直接 `#include pal_irq` / 注册 ISR（ADR-0018）；
- host/wasm 与 ESP32 **同源业务 C**（ADR-0002）；仿真保真边界允许 IRQ 路径降级（ADR-0003），但须**诚实**（ADR-0012），不得静默假装 irq 已生效。

---

## 方案比选（Options）

### 方案 A：JSON `event_drive` + 单一 `enable_events`（采纳）

在 button 设备规格增加 `event_drive`、`debounce_ms`（一等字段）、可选 `wake_from_sleep`；`auto_poll_ms` 仅 soft_poll 必填。

### 方案 B / C / D

两个 App API、全局编译宏、一律 gpio_irq —— 均否决（见 Proposed 期讨论记录；违背「仅配置」或双 target / 防抖风险）。

---

## 决策结论（Decision）

**采纳方案 A。Owner 锁定三项细则（2026-07-14）：**

1. **稳定 App 契约**：L1 仅 `{name}_enable_events` / `{name}_disable_events`；`start_auto_poll(period)` 保留为 L2。  
2. **配置选择后端**：`event_drive` 默认 `soft_poll`；`gpio_irq` = GPIO 边沿唤醒 + 防抖 + `wink_event_post`。  
3. **`debounce_ms` 为 schema 一等字段**（两种后端共享）；缺省用文档默认值（推荐 20ms）；`0` 表示保留 DAL 默认防抖阈值（10 ms tick 下 ≈30 ms）。本分支不支持关闭防抖；如需更激进的边沿响应，请传入较小的正值。  
4. **BAL 正式 API 名（B 类能力，ADR-0032）**：`wink_button_enable_events` / `wink_button_disable_events`；旧 `wink_button_events_start/stop` 为 deprecated 薄包装（`wink_button_helper_*` 已于 pre-1.0 直接删除，未走 deprecation 窗口）。  
5. **host/wasm 遇 `gpio_irq`**：默认 **降级 soft_poll + `wink_trace_warn`（可断言）**；可选后续 `-DWINK_BUTTON_IRQ_STRICT=1` 改为 `WINK_ERR_UNSUPPORTED`。  
6. **ISR 纪律**：gpio_irq 路径 ISR 仅置位/记边沿/启动防抖；禁止 ISR 内跑 App 业务；消费仍在 `app_on_event`。  
7. **非目标**：替代 kHz 硬实时控制环；`device_tree_init` 内隐式 `enable_events`（另案）。

---

## 后果与约束（Consequences & Constraints）

- codegen 校验：`soft_poll` ∧ 缺 `auto_poll_ms` → ERROR；`gpio_irq` ∧ 无 pin → ERROR；`wake_from_sleep` 仅允许配 `gpio_irq`。  
- Role `enable_events()` 生成代码调用 `wink_button_enable_events`，把 JSON 烘焙进 `wink_button_event_config_t`。  
- ADR-0022 中「helper 不升级为 event 源」等过时表述以本文为准修订。

---

## 遵循与后续（Compliance & Follow-up）

1. ✅ 回写 `01-app-business-logic.md` / `02-project-manifest-schema.md`（本提交）。  
2. 按 [实施计划](../../implementation-plans/core/2026-07-14-button-event-drive-backends-plan.md) 切片 S1–S5 执行。  
3. host e2e：soft_poll 回归；gpio_irq 验证降级 warn。  
4. ESP32 HIL：gpio_irq / 深睡唤醒可后置。

---

*本 ADR 状态变更请在此记录：*
- 2026-07-14：Proposed（配置切换 soft_poll / gpio_irq）
- 2026-07-14：Accepted（Owner：默认降级+warn；debounce_ms 一等字段；BAL 正式名随后由 ADR-0032 定为 `enable_events`；首落符号 `events_start` 作 deprecated 兼容）

