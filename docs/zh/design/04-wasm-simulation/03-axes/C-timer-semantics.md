# 轴 C — 定时器硬件语义

| 项 | 内容 |
|---|---|
| 层 | Ⅱb 薄索引 |
| 状态 | **Active**（2026-08-02 切换；Wasm 仿真现行 SSOT） |
| 定义出处 | [`../01-overview/02-axes-af.md`](../01-overview/02-axes-af.md)（禁止改写定义措辞） |

## 1. 回答的问题

HW timer / PWM 周期 / capture

定义见 [`../01-overview/02-axes-af.md`](../01-overview/02-axes-af.md) 轴 C 行。与轴 B 的字母分工亦见该文对照表。

## 2. 主机制（primary）

- [`../02-mechanisms/09-timer-and-pwm-semantics.md`](../02-mechanisms/09-timer-and-pwm-semantics.md) — 软步进 / duty / capture / `pal_hwtimer` / FOC 行为级边界

> **禁止**以 `08-channel-routing.md` 的子锚点作为本轴唯一主链。

## 3. 次机制（secondary）

- PWM 作为通道 1b 的**路由/选型**（duty 百分比「数据从哪来」）→ [`../02-mechanisms/08-channel-routing.md`](../02-mechanisms/08-channel-routing.md)（非硬件周期语义 SSOT）
- 真机 FOC / `pal_hwtimer` 分层契约 → [ADR-0047](../../../decisions/core/0047-foc-isr-layering-and-pal-hwtimer.md)

## 4. 典型上限（展开版）

在 overview 缩略「无 10kHz+ 硬 ISR；FOC 快环行为级」之上展开：

1. **模型上限**：**无**芯片级 10kHz+ 硬定时器 ISR；**无** PWM–ADC 硬件触发真同步；不得用仿真证明硬实时延迟或抢占序。
2. **PWM L2**：`pal_pwm_set_duty` 旁路到插件 duty 百分比；**不**仿真载波边沿、死区、中心对齐。
3. **FOC / 快环**：真机契约见 ADR-0047；仿真侧为虚拟时间确定性软步进（禁墙钟/`rand`）——行为级可复现，硬实时仍须 HIL。
4. **capture**：当前主要靠 GPIO 脉宽 / Pin Event；通用 HW capture 通道抽象尚未作为独立能力闭合。
5. **资源独占**：引脚与定时器冲突可做到行为级门禁，非 cycle-accurate 周期精确。
6. **交叉宣称**：舵机/PWM 输出属 **A+C**（+ Accuracy Mode）；超声波脉宽属 **A+B**，不要把通道 1b 路由文当成轴 C 主文档。

## 5. 相关 C 场景

场景契约 → [`../04-assurance/01-consistency-spec.md`](../04-assurance/01-consistency-spec.md)。可测状态只查 [`../04-assurance/02-consistency-checklist.md`](../04-assurance/02-consistency-checklist.md)。

- **C10** — 快环 ISR（FOC / 硬定时器）
- **C17** — 外设资源互斥 / 时基耦合

