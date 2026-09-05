# ADR-0033：超声波测距完成事件（`enable_distance_events` → `WINK_EVENT_DISTANCE_READY`）

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-07-16 |
| 触发 | `avoidance_car` 业务被迫写 `app_loop` 轮询；与 L1 金样 `oled_dashboard`（纯 `on_event`）不对齐；`coding-conventions` §3 已预留 `enable_distance_events` |
| 影响范围 | `wink_event` 类型；BAL 新 B 类 API；codegen ultrasonic/servo Role；`avoidance_car` 样板；活规范回写 |
| 决策者 | 项目 Owner |
| 关联 ADR | [ADR-0022](0022-event-queue-mbox-async-primitives.md)、[ADR-0023](0023-bal-business-abstraction-layer.md)、[ADR-0031](0031-button-event-drive-config.md)、[ADR-0032](0032-bal-role-operation-naming-classes.md) |
| 关联技术设计 | [tech-designs/2026-07-16-ultrasonic-distance-events.md](../../zh/tech-designs/core/2026-07-16-ultrasonic-distance-events.md) |
| 关联实施计划 | [implementation-plans/2026-07-16-ultrasonic-distance-events-plan.md](../../implementation-plans/core/2026-07-16-ultrasonic-distance-events-plan.md) |

---

## 背景（Context）

L1 App 契约已收敛为：打开事件通路 → 在 `app_on_event` 消化。按键已落地 `enable_events`。超声波避障仍用 `app_loop` 相位机轮询 `request`/`get_cached`，抬高门槛且与「业务层可不写 loop」目标冲突。

既有 A 类 `wink_sonar_helper_start` 只周期触发测量，**不**投递队列，不能替代 L1 事件路径。

---

## 方案比选（Options）

| 方案 | 结论 |
|------|------|
| App 继续 `loop` 读缓存 | ❌ 与 L1 目标冲突 |
| 仅扩展 sonar helper 回调 | ❌ 绕开全局队列，与按键模型分裂 |
| **B 类 `enable_distance_events` + 周期完成 → `DISTANCE_READY`** | ✅ 采纳 |
| 扩展 `wink_event_t` 加 `float` | ❌ 破坏 POD/队列布局 |

---

## 决策结论（Decision）

1. **事件类型**：新增 `WINK_EVENT_DISTANCE_READY`（MVP）。`DISTANCE_FAULT` 后置。  
2. **语义**：每次**测量完成**投递一次 READY（周期持续采样，非终身一次）。`period_ms` / JSON `auto_poll_ms` 默认与下限 **50ms**（HC-SR04 串扰预算，与 sonar helper 一致）。  
3. **`param` 编码**：`uint32_t`，单位 **0.1 cm**（`round(cm * 10)`）。App：`float cm = evt->param / 10.0f`。  
4. **`device`**：`dal_ultrasonic_t *` 源实例。  
5. **BAL B 类 API**：`wink_ultrasonic_enable_distance_events(dev, cfg)` / `disable_distance_events(dev)`；Role `{name}_enable_distance_events()`。  
6. **后端 MVP**：soft_poll / periodic 完成边沿检测 + `wink_event_post`；不做超声波 IRQ 完成路径。  
7. **A/B 并存**：`wink_sonar_helper_start` 保留（只测不投）；**同一 `dev` 禁止与 distance-events 同时占用** → `WINK_ERR_INVALID_STATE`。  
8. **队列满**：`post` 失败 → `wink_trace_warn(WINK_WARN_DISTANCE_EVENT_QUEUE_FULL)`，不阻塞 tick。  
9. **JSON**：ultrasonic 可用 `auto_poll_ms`（缺省 50；&lt;50 codegen ERROR）。仅当 App 显式 `enable_distance_events` 时生效（ADR-0023：不在 `device_tree_init` 隐式启动）。  
10. **样板**：`avoidance_car` 迁 L1（codegen + Role + `on_event`）；手写 Flash 覆写退出该样板主路径。

---

## 后果与约束

- App 持续避障 = 多次 `DISTANCE_READY`，不是单次边沿订阅。  
- host「单 tick READY」与 ESP32 真异步差异由单测边沿覆盖 + HIL 后置。  
- 活规范矩阵中 ultrasonic B 列从「可选」改为已落地。

---

*本 ADR 状态变更请在此记录：*
- 2026-07-16：Accepted（Owner；param=0.1cm；MVP soft_poll+READY；A/B 互斥；avoidance_car L1 化）

