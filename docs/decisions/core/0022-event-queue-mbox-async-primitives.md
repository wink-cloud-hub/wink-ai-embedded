# ADR-0022：Event Queue / mbox 异步原语（占位 Proposed）

| 项 | 内容 |
|---|---|
| 状态 | **Proposed（占位，未启动设计）** |
| 日期 | 2026-07-06 |
| 触发 | P2 收尾时 Future Work 梳理（见 app-layer-lowcode-unification-design.md §9） |
| 影响范围 | PAL OSAL / Runtime / App 层协作调度模型；跨三 target |
| 决策者 | Wink-AI 嵌入式团队（待评审） |

---

## 背景（Context）

当前协作式确定性调度器（ADR-0013/0014）下，App 层的"事件"通过以下机制分发：
1. `app_loop()` 心跳契约（App 每 tick 主动 poll 按键/传感器）；
2. `wink_button_helper` soft_timer 周期 poll + 同步回调（在 timer task / host 虚拟时钟上下文）；
3. BAL service 启动（blink/sim_echo/telemetry）通过 WINK_PT 自动 fiber 周期执行。

随着低代码平台支持的设备类型增多（蜂鸣器、电机、IMU、WiFi/MQTT 等），app_loop 会逐渐演变成"巨型 poll 分发器"，且 AI 生成的 App 业务逻辑难以在"传感器数据到达"vs"定时触发"vs"用户按键"之间优雅表达——目前全部依赖 App 手写状态机 poll。

业界 RTOS 常见解法是 **event queue / mbox** 异步原语：生产者（ISR、soft_timer、sensor poll 完成、网络收包）post 事件到队列，消费者（App fiber）pend 等待、按事件类型分发。但引入该原语会影响：

- ADR-0013/0014 的确定性调度器（确定性需要事件注入顺序可复现；mbox 引入跨 fiber 竞争）；
- PAL OSAL 三 target 实现（host/wasm/ESP32 语义一致性）；
- ISR-safe post 与 ADR-0018（PAL IRQ 收窄）的交互；
- App 层回调 vs 事件循环风格的 API surface。

## 方案比选（Options）

（设计阶段详细展开，占位列出候选）

1. **不分发/不分时**：保持现状 `app_loop()` 心跳 + soft_timer 回调。
   - 优点：零新增机制、确定性最强；
   - 缺点：AI 生成 App 业务逻辑复杂度随设备数线性上升；网络/异步传感器接入无标准通道。

2. **宏内核风格 mbox**：单一全局 `wink_event_t` 队列，任何 producer/consumer 都走这个队列。
   - 优点：API 最少；
   - 缺点：事件 ID 全局命名空间爆炸；难以多消费者；ISR 上下文 post 需严格设计。

3. **纤程本地邮箱 + 多播通道**（类似 protothread + channel）：每个 PT fiber 有私有 mbox，另有 pub/sub 通道。
   - 优点：和 ADR-0013/0014 协作模型天然吻合；确定性最易保证；
   - 缺点：设计复杂；三 target 实现量大。

4. **仅做信号（event bits / condition variable）**：不做数据队列，只做"有事件到达"位标记。
   - 优点：实现最简单；
   - 缺点：传感器数据/按键事件参数仍然走全局变量，不安全、难用。

## 决策结论（Decision）

**占位，本 ADR 当前不决策。** P0/P1/P2 阶段 App 层仍保留 `app_loop` 心跳契约，事件分发通过以下机制足够覆盖：
- 按键：`wink_button_helper` soft_timer + 同步回调（或 App 自 poll）；
- 传感器：App 在 `app_loop` 内主动 request_measurement + get_cached_distance；
- 定时业务：`wink_os_soft_timer` 周期回调；
- 后台服务（blink/sim_echo/telemetry）：BAL helper 自管 fiber。

当下列任一触发条件满足时再启动本 ADR 的正式设计：
- AI 生成的 App 出现"同时处理 ≥3 类异步事件源"的真实需求（非 demo）；
- 网络栈（WiFi/MQTT）接入并需要在 App 层异步接收数据；
- 蜂鸣器/电机/IMU 等新设备 BAL helper 出现，button_helper 的软定时器回调模型无法优雅复用。

## 后果与约束（Consequences & Constraints）

- P 系列（P0-P3）不引入 event queue，所有 App 代码仍使用现有 poll + soft_timer 模型。
- codegen 模板（P1 已交付）不生成任何 mbox/event 相关代码；后续 ADR-0022 Accepted 后再扩展 codegen 插件。
- button_helper 继续使用 soft_timer 周期回调，不升级为 event 源。
- BAL 服务启动（blink/sim_echo/telemetry）继续在 app_callbacks.c 手写，等 service 插件接口 + event queue 都落地后再迁入 codegen。

## 遵循与后续（Compliance & Follow-up）

- P2 收尾（本次）：在 tech-design §9 Future Work 中引用 ADR-0022 占位，避免重复讨论"要不要做 event queue"。
- 后续触发条件达到时：重开 ADR 评审，选择 §方案比选 中的具体方案并更新 §决策结论。

---

*本 ADR 状态变更请在此记录：*
- 2026-07-06：Proposed（占位）
