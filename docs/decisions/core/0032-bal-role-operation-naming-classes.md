# ADR-0032：App Role / BAL 操作三类命名（A 活动 · B 能力 · C 动作）

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-07-14 |
| 触发 | ADR-0031：`enable_events` vs `events_start`；是否机械统一 BAL `_start` 族；事件队列消费心智 |
| 影响范围 | App Role 生成动词；BAL 公共 API；文档 / codegen / AI |
| 决策者 | 项目 Owner |
| 关联 ADR | [ADR-0004](0004-static-dispatch-vs-runtime-ops.md)、[ADR-0022](0022-event-queue-mbox-async-primitives.md)、[ADR-0023](0023-bal-business-abstraction-layer.md)、[ADR-0031](0031-button-event-drive-config.md)、[ADR-0038](0038-bal-naming-hard-cut-and-layer-ssot.md) |
| 关联活规范（SSOT） | [coding-conventions.md §3](../../zh/design/07-platform-governance/coding-conventions.md)；文件/域命名见 [06-bal-layer.md](../../zh/design/02-wink-micro-os/06-bal-layer.md) |

---

## 背景（Context）

BAL 历史方言以 `_start/_stop` 为主（周期闪灯、周期测距、遥测等）。按键「打开后往全局事件队列投递、App 只写 `on_event`」若也叫 `_start`，会与「后台活动」抢隐喻，并迫使 Role `enable_events` 与 BAL `*_start` 跨层改词。

需要按**操作主交付**分类命名，并明确与事件队列的关系。细则写在活规范，本 ADR 只锁决策。

---

## 方案比选（Options）

| 方案 | 结论 |
|------|------|
| 全部强制 `start/stop` | ❌ 事件能力命名不当 |
| 全部强制 `enable/disable` | ❌ 周期服务命名不当 |
| **按 A/B/C +「进队列优先 B」** | ✅ 采纳 |
| 仅口头约定 | ❌ 无法约束 AI/评审 |

---

## 决策结论（Decision）

1. **三类命名（A/B/C）**：A 活动 = `start/stop`；B 能力 = `enable/disable`；C 动作 = `set/get/request/…`。  
2. **第一刀**：主交付是「打开后往 `wink_event` 投、业务在 `app_on_event` 消化」→ **必须 B**；否则再按有无后台会话分为 A 或 C。  
3. **同一外设可同时拥有 A+B+C**；禁止两套名字指同一路径。  
4. **Role ↔ BAL 同操作必须同动词**（`{instance}_enable_events` ↔ `wink_button_enable_events`）。  
5. **既有 A 类 API 的文件/符号改名**：原「不机械改名」政策已被 [ADR-0038](0038-bal-naming-hard-cut-and-layer-ssot.md) **硬切割取代**（去掉 `_helper`/`sonar` 等方言）。**A/B/C 动词规则本身不变**。  
6. **默认不用 `register_*`**（除非登记回调）；禁用词序 `events_enable`。  
7. **谓词/查询函数**（返回 bool、回答"是否/能否/有没有"）统一用 `is_*` / `has_*` / `can_*` 前缀（例：`wink_button_is_debouncing`、`wink_button_events_has_irq_backend`）。现有 `*_supported` / `*_ready` 风格祖父保留，新代码走 is/has/can。  
8. **L2 `start_auto_poll/stop_auto_poll`**：保留为 **A 类便利薄包装**（调用期参数覆盖 JSON 配置、跑 soft_poll），内部转发到 `wink_button_enable_events(soft_poll cfg)`；deprecated 标注指向 B 类正名 `enable_events`。L1 App 一律用 `enable_events`。  
9. **Deprecated 时间表**：旧名（`wink_button_events_start`、`wink_button_helper_start/stop`）加 `WINK_DEPRECATED` 标注；两个 minor 版本后删除。新 codegen / 新 sample 禁止调用 deprecated 名。

细则、决策树、示例矩阵、硬性规则 → **[coding-conventions §3](../../zh/design/07-platform-governance/coding-conventions.md)**（唯一 SSOT，本 ADR 不复述整表）。

---

## 后果与约束

- 新增 API 走 §3.4 决策树；评审以 §3.5 为清单。  
- AI skill / clean-code 只链接 SSOT，不复制整表。  
- A 类若「顺带」post 事件，主名仍可 `start`；主路径改为队列消费时再增 B 类 API。  
- 仓内若仍保留 `wink_button_events_start` 符号：仅作兼容包装，文档与 codegen 以 `enable_events` 为准。  
- **视角陷阱**：A/B/C 按 App 视角的主交付分类，不看底层实现。B 类 `enable_*` 完全允许底层用 timer/daemon/ISR 实现（例：button `enable_events` 在 soft_poll 后端就起了周期 timer，在 gpio_irq 后端还起了 daemon 任务）—— 它叫 `enable` 是因为 App 关心"打开投递通路"，不关心底层。A/B 分界是"App 调它是为了收事件还是为了跑周期动作"。  
- `pause/resume` 语义（临时屏蔽不销毁 slot）不在本次分类范围，需要时另开 ADR。

---

*本 ADR 状态变更请在此记录：*
- 2026-07-14：Accepted（Owner；SSOT = coding-conventions §3；队列优先 B；与 ADR-0031 对齐）
- 2026-07-18：文件/符号硬切割改由 [ADR-0038](0038-bal-naming-hard-cut-and-layer-ssot.md) 约束；本 ADR 的 A/B/C 动词规则不变。


