<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划 03：软定时器独立守护模型与网络出口时序解耦计划

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20260930-ESP-IDF-TIMER-AND-NETWORK-v1.5 |
| 状态 | 🟡 **In Review（阶段 0 技术设计已吸收全部评审意见更新至 v1.1，待 Sign-off 后正式启动代码实施）** |
| 日期 | 2026-09-30 |
| 周期估算 | 2~2.5 个工作日 |
| 前置产出物 | [《定时器与网络出口架构技术设计规格》](../../../zh/tech-designs/esp32/03-timer-and-network-architecture.md)（已完成物理落盘并升级至 v1.1） |
| 优先次序 | **前置架构技术设计（已完成 v1.1） → 定时器独立守护纤程 (Timer Daemon Fiber) 落地 → 强制回写复位 DAG（拆分 Tear-down 与 Re-arm） → MQTT 实例状态下沉 (消除全局单例与 ABA 槽位复用) → Wi-Fi / MQTT 临时任务收敛 → 多 Netif 路由与 Broker 扩展** |
| 决策依据 | [ADR-0004：编译期静态分发优于运行时函数指针](../../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)<br>[ADR-0007：协作式纤程与事件调度模型](../../../decisions/core/0007-cooperative-loop-execution-model.md)<br>[ADR-0012：契约诚实优于静默降级（Fail-Loud 原则）](../../../decisions/core/0012-contract-honesty-over-silent-degradation.md)<br>[ADR-0053：虚拟时间因果同刻总序仲裁模型](../../../decisions/unisim/0053-sim-same-timestamp-event-total-order.md)<br>[ADR-0085：SoC 双 SSOT 仲裁与静态选片](../../../decisions/core/0085-esp-idf-facade-soc-caps-vs-pal-caps-dual-ssot.md)<br>[ADR-0089：分类记账堆内存与边界防御模型](../../../decisions/core/0089-esp-idf-heap-caps-allocation-contract.md) |
| 管辖数据源 | [`src/freertos/freertos_timers.c`](../../../../wink-micro-os/frameworks/esp_idf/src/freertos/freertos_timers.c)、[`runtime/src/wink_soft_timer.c`](../../../../wink-micro-os/runtime/src/wink_soft_timer.c)、[`src/wifi/esp_wifi.c`](../../../../wink-micro-os/frameworks/esp_idf/src/wifi/esp_wifi.c)、[`src/network/esp_mqtt.c`](../../../../wink-micro-os/frameworks/esp_idf/src/network/esp_mqtt.c)、[`src/network/sim_network_broker.c`](../../../../wink-micro-os/frameworks/esp_idf/src/network/sim_network_broker.c) |
| 实施目标文件 | `src/freertos/freertos_timers.c`、`src/wifi/esp_wifi.c`、`src/network/esp_mqtt.c`、`src/network/sim_network_broker.*`、`src/esp_idf_bridge.c` |
| 验收门禁 | 技术设计规格评审 (v1.1)、提前插入定时器即刻唤醒单测、队列满背压单测、定时器/MQTT 槽位复用 ABA 防护单测、Broker `(cb, user_ctx)` 解绑单测、复位后网络回调留存单测、未连网启动 MQTT 确定性单测、`run_esp32_headless_evidence.ps1 -App wifi_sta` 确定性时序、全量 CTest 真实回归 |

---

## 一、 计划背景与评审纠偏要旨 (Absorption of Review Feedback)

在 2026-09-30 的深度评审中，团队识别出原技术设计与现有源码中的多项致命隐患，本实施计划据此进行全面对齐纠偏：

1. **P0 级纠偏：守护纤程等待模型（拒绝 `vTaskDelay`，严格使用 `xQueueReceive` 超时）**：
   - 协作式调度器 [`wink_sim_scheduler.c:298`](../../../../wink-micro-os/targets/common/src/wink_sim_scheduler.c) 仅在任务处于 `SIM_TASK_STATE_BLOCKED` 时才会被 `sim_scheduler_resume` 提前唤醒；`vTaskDelay` 产生的是 `SIM_TASK_STATE_WAITING` 状态；
   - 若使用 `vTaskDelay`，当 Daemon 正在等待 1000ms 定时器时，外界新插入一个 10ms 短定时器，Daemon 无法被唤醒，导致时序严重挂死；
   - **实施规范**：严格使用 `xQueueReceive(s_timer_cmd_queue, &msg, wait_ticks)`，Daemon 保持在 `SIM_TASK_STATE_BLOCKED`，新命令入队通过 `sim_scheduler_resume` 即刻唤醒 Daemon 并重算超时。
2. **P1 级纠偏：微秒契约诚实定界（Fail-Loud 原则）**：
   - 当前系统 `configTICK_RATE_HZ = 100`（10ms 颗粒度），毫秒级 Timer Daemon 物理上无法履约微秒级 `esp_timer`；
   - 本期明确仅交付 FreeRTOS Timer 7 项核心 API；`esp_timer_create` 等接口暂不交付，一律返回 `ESP_ERR_NOT_SUPPORTED`（Fail-Loud），`esp_timer_get_time()` 维持只读虚拟时钟现状。
3. **P1 级纠偏：回调非阻塞铁律**：
   - 严正澄清：Timer 回调严格禁止任何阻塞操作（禁止 `vTaskDelay` 或非零等待原语），必须为有界非阻塞操作；
   - 引入 Daemon 的根本原因在于保护 `wink_soft_timer` 内核主循环的 100µs/500µs WCET 限制，以及提供异步命令队列。
4. **P1 级纠偏：复位 DAG 存量网络回调冲刷漏洞根治**：
   - 揭露并修复现有代码漏洞：原复位在 Stage 1 `esp_mqtt_sim_reset()` 注册 Broker 回调，紧接着在 Stage 2 `esp_wifi_sim_reset()` 中被 `sim_network_broker_reset()` 的 `memset` 彻底冲刷抹除；
   - 重构为双阶段复位：**Phase A: 全链路析构清理（Tear-down）** $\to$ **Phase B: 基线初始化（Re-arm）**；上层协议严禁在 Tear-down 阶段提前注册新监听。
5. **P1 级纠偏：全局单调代际令牌（彻底解决 ABA 换代漏洞）**：
   - 修复 `esp_mqtt_client_destroy()` 仅用 `memset` 导致槽位复用时 `token` 重归于 1 的 ABA 漏洞；
   - 建立全局单调自增代际分配器，排队命令与回调执行严格校验 `(slot_id, generation)` 双元组。
6. **P1 级纠偏：多 Netif 路由收敛与 Broker 订阅句柄**：
   - 坚决剔除 Ethernet 承诺，锁定 STA / SoftAP 双网卡；
   - 升级 Broker 注销接口为 `(cb, user_ctx)` 二元组精确解绑，防止共用静态回调时的误杀。

---

## 二、 详细实施任务拆解 (Action Items)

### 阶段 0：前置技术设计规格产出（Prerequisite Tech Design Spec - 硬门禁）

- [x] **任务 T0.1**：编写并归档 [《定时器与网络出口架构技术设计规格》](../../../zh/tech-designs/esp32/03-timer-and-network-architecture.md)：
  - **物理落盘要求**：完成物理落盘并升级至 v1.1，全面闭环吸收 6 项架构评审意见；
  - **模块 1：Timer Daemon 纤程与工作项队列设计**：
    - 定义 `xQueueReceive(queue, &msg, timeout)` 唤醒机制；
    - 确立定时器回调非阻塞契约与命令队列满背压防死锁策略；
    - 确立全局单调代际与槽位复用防 ABA 机制；
  - **模块 2：`esp_timer` 诚实定界与 Fail-Loud**：
    - 明确本期不交付 `esp_timer_create`，保持只读微秒时钟 `esp_timer_get_time`；
  - **模块 3：多 Netif 路由与 Broker 二元组解绑**：
    - 确立 STA 唯一默认外网出口与 SoftAP 本地直连属性；
    - 升级 Broker 为 `(cb, user_ctx)` 注册/注销契约；
  - **模块 4：9 阶段复位因果图 (Reset DAG) 重构**：
    - 确立 Phase A 析构与 Phase B 基线初始化的双阶段模型。

---

### 阶段 1：FreeRTOS 定时器守护纤程与交付矩阵 (Timer Daemon Fiber & Delivery Matrix)

- [ ] **任务 T1.1**：逐项落实 FreeRTOS Timer API 交付矩阵与 Daemon 驱动引擎：
  | API 名称 | 交付归属 | 语义契约与实现策略 |
  |---|---|---|
  | `xTimerCreate` | **本期实现** | 分配静态槽位，获取全局自增代际 `generation`，初始化结构体 |
  | `xTimerStart` / `xTimerStop` | **本期实现** | 向 `s_timer_cmd_queue` 投递命令，唤醒 Daemon 纤程 |
  | `xTimerChangePeriod` | **本期实现** | 向命令队列投递新周期命令，Daemon 更新链表并动态重算等待时间 |
  | `xTimerDelete` | **本期实现** | 释放槽位，生成新代际使在途待执行工作项自动失效 |
  | `xTimerReset` | **本期实现** | 重新计算到期时间并向队列投递重置命令 |
  | `xTimerIsTimerActive` | **本期实现** | 瞬态运行状态查询 |
  | `pvTimerGetTimerID` | **本期实现** | 获取用户上下文 ID |
  | `xTimerStartFromISR` 等 | **显式不支持** | 门面当前无独立硬件 ISR 线程，统一调用标准版本或返回错误 |
  | `esp_timer_create` 等 | **显式不支持** | Fail-Loud：返回 `ESP_ERR_NOT_SUPPORTED` |

- [ ] **任务 T1.2**：编写定时器单测集 `test/freertos/test_freertos_timers.c`：
  - **提前插入即刻唤醒单测（P0 验证）**：Daemon 正在阻塞等待 1000ms 定时器时，从另一任务插入 20ms 短定时器，验证 Daemon 被 `xQueueSend` 立即唤醒并在 20ms 到期时准时分发；
  - **槽位复用 ABA 单测（P1 验证）**：创建 Timer A，启动后立即 Delete 并在同一槽位快速分配 Timer B，验证 Timer A 在队列中的旧命令到期被代际防御丢弃；
  - **队列满背压与防死锁单测**：连续填满命令队列，验证带超时与非阻塞返回 `pdFAIL`，验证临界区内投递触发断言拦截；
  - **LITE 配置（8 任务槽）极限容量单测**：验证在仅有 8 个任务槽的极限环境下，Daemon 与 5 个应用任务共存稳定运行。

- [ ] **任务 T1.3**：【防腐硬约束】新组件回写 [`src/esp_idf_bridge.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/esp_idf_bridge.c) 双阶段复位 DAG：
  - 在复位流程的 Stage 5 增加 `esp_freertos_timers_sim_reset()`：彻底排空定时器命令队列与待执行工作项，停止 Daemon 纤程；
  - 编写专门的**“定时器即将触发瞬间强行重启”**单测：在到期前 1ms 触发 `esp_restart()`，断言重启后旧定时器零残留、新基线完全干净。

---

### 阶段 2：临时纤程任务精准收敛（Task Pruning & Ghost Event Elimination）

- [ ] **任务 T2.1**：重构 `src/wifi/esp_wifi.c` STA 连接状态机：
  - 移除通过 `xTaskCreate` 启动的一次性临时任务 `wifi_connect_task`；
  - 采用 Timer Daemon 工作项投递 100ms 延迟连接事件；
  - 严格保持全局代际（`s_wifi_generation`），在调用 `esp_wifi_disconnect()` 或收到 `DROP_BEACON` 时即刻失效在途工作项，彻底消除迟到的幽灵 `GOT_IP` 事件；
- [ ] **任务 T2.2**：重构 `src/network/esp_mqtt.c` 实例状态管理与任务收敛：
  - **消除全局单例**：彻底废除 `s_mqtt_token` 与 `s_mqtt_task_handle`，下沉至 `struct esp_mqtt_client` 实例内部结构体，支持最多 `MAX_MQTT_CLIENTS` 个独立客户端并发连接；
  - 移除通过 `xTaskCreate` 启动的 50ms 一次性临时连接任务 `mqtt_connect_task`，改为实例绑定的延迟事件投递；
  - **MQTT 槽位复用 ABA 单测**：验证 Client A 发起连接延时期间立即 `destroy` 并在同槽位分配 Client B，Client B 绝不触发 Client A 的回调；
  - **未连网启动 MQTT 确定性单测**：复位后不连 Wi-Fi 立即启动 MQTT，断言精准抛出 `MQTT_EVENT_ERROR` (EHOSTUNREACH) 并进入断开状态。

---

### 阶段 3：多 Netif 链路状态感知与 Broker 演进 (Network Broker Refinement)

- [ ] **任务 T3.1**：升级 [`src/network/sim_network_broker.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/network/sim_network_broker.c)：
  - 扩展接口为携带 Netif 实例身份与 `(cb, user_ctx)` 二元组精确定位注销：
    ```c
    typedef enum {
        SIM_NETIF_EVT_DOWN = 0,
        SIM_NETIF_EVT_UP   = 1,
    } sim_netif_event_t;
    typedef void (*sim_netif_event_cb_t)(esp_netif_t *netif, sim_netif_event_t event, void *user_ctx);
    int  sim_network_broker_register_cb(sim_netif_event_cb_t cb, void *user_ctx);
    void sim_network_broker_unregister_cb(sim_netif_event_cb_t cb, void *user_ctx);
    void sim_network_broker_notify_netif(esp_netif_t *netif, sim_netif_event_t event);
    ```
  - 编写单测验证两个客户端共用同一静态函数回调时，注销互不干扰；
- [ ] **任务 T3.2**：重构复位链并验证网络回调留存：
  - 彻底拆分复位 Tear-down 与 Re-arm 阶段，解耦 `sim_network_broker_reset()` 与上层监听注册；
  - 编写“联网 $\to$ 软复位 $\to$ 重新联网”单测，断言新客户端能稳定收到网络上线事件并成功连接；
- [ ] **任务 T3.3**：端到端无头时序实证回归：
  - 执行 `powershell ./wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1 -App wifi_sta`；
  - 验证虚拟时钟、因果事件先后序与无头实证场景全绿通过。

---

## 三、 风险评估与回滚方案 (Risk & Rollback Matrix)

| 风险项 | 触发场景 | 预防与缓解措施 | 回滚操作 (Rollback Action) |
|---|---|---|---|
| **R-01 守护纤程因错误等待挂死** | 误用 `vTaskDelay` 或阻塞导致新定时器无法唤醒 | 严格通过 `xQueueReceive(queue, &msg, timeout)` 保持在 BLOCKED 状态，单测强制覆盖短定时器提前唤醒场景 | 检查 Daemon 等待原语 |
| **R-02 队列满级联阻塞死锁** | 业务高频向满队列阻塞投递命令 | 严格限制有界等待，禁止在临界区投递，队列满超时返回 `pdFAIL` | 优化队列深度配置或检查应用定时器创建频率 |
| **R-03 槽位复用 ABA 幽灵回调** | 定时器或 MQTT 实例快速析构并重建 | 采用全局单调自增代际生成器，槽位与命令双校验代际匹配 | 检查代际自增点与重置逻辑 |
| **R-04 复位后网络状态脱节** | 复位清理与重新注册时序倒错 | 贯彻双阶段复位拓扑，上层协议统一在 `client_init` 或启动时按需订阅 Broker | 回滚至复位 DAG 明确的解耦设计 |

---

## 四、 全局验收标准 (Definition of Done)

1. **DoD-1（前置技术设计规格 v1.1 评审通过）**：
   - 《定时器与网络出口架构技术设计规格》v1.1 完成物理落盘并获得确认签署；
2. **DoD-2（FreeRTOS 定时器核心交付与时序唤醒）**：
   - FreeRTOS Timer 7 项核心 API 交付并通过全部单测；
   - 提前到期唤醒测试 100% 通过，验证 Daemon 随时可被短定时器唤醒；
   - 队列满背压单测与槽位复用 ABA 防御单测 100% 通过；
   - LITE 配置（8 任务槽）极限容量测试通过，证明无任务槽泄漏与死锁；
3. **DoD-3（复位 DAG 修复与幽灵事件拦截）**：
   - 彻底修复 Wi-Fi reset 冲刷 Broker 回调的既有漏洞；
   - “联网 $\to$ 软复位 $\to$ 重新联网”链路状态感知单测 100% 绿灯；
   - “定时器到期前 1ms 强行重启”零残留、基线干净；
4. **DoD-4（临时纤程彻底清零与多客户端并发安全）**：
   - Wi-Fi 与 MQTT 临时纤程彻底清零（无 `wifi_connect_task` 与 `mqtt_connect_task`）；
   - MQTT 实例状态下沉，多客户端并发连接单测全绿，无 Token 竞争覆盖；
   - 未连网启动 MQTT 确定性单测 100% 绿灯；
5. **DoD-5（无头因果时序与全量 CTest 回归）**：
   - `run_esp32_headless_evidence.ps1 -App wifi_sta` 实证通过；
   - 全量 `ctest -L esp_idf` 保持 100% 全绿通过（保存完整执行日志）。
