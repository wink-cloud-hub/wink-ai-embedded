<!-- SPDX-License-Identifier: Apache-2.0 -->
# 技术设计规格：ESP-IDF 软件定时器守护模型与网络出口时序解耦架构

| 字段 | 内容 |
|---|---|
| 文档编号 | TECH-DESIGN-ESP32-TIMER-NET-v1.1 |
| 状态 | 📋 **Proposed / Ready for Review（已全面吸收深度架构评审意见完成修订，待 Sign-off）** |
| 日期 | 2026-09-30 |
| 对应实施计划 | [《实施计划 03：软定时器独立守护模型与网络出口时序解耦计划》](../../../implementation-plans/esp32/facade-hardening/03-timer-and-network-evolution-plan.md) |
| 决策依据 | [ADR-0004：编译期静态分发优于运行时函数指针](../../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)<br>[ADR-0007：协作式纤程与事件调度模型](../../../decisions/core/0007-cooperative-loop-execution-model.md)<br>[ADR-0012：契约诚实优于静默降级（Fail-Loud 原则）](../../../decisions/core/0012-contract-honesty-over-silent-degradation.md)<br>[ADR-0053：虚拟时间因果同刻总序仲裁模型](../../../decisions/unisim/0053-sim-same-timestamp-event-total-order.md)<br>[ADR-0085：SoC 双 SSOT 仲裁与静态选片](../../../decisions/core/0085-esp-idf-facade-soc-caps-vs-pal-caps-dual-ssot.md)<br>[ADR-0089：分类记账堆内存与边界防御模型](../../../decisions/core/0089-esp-idf-heap-caps-allocation-contract.md) |
| 管辖实现文件 | [`src/freertos/freertos_timers.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/freertos/freertos_timers.c)<br>[`src/wifi/esp_wifi.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/wifi/esp_wifi.c)<br>[`src/network/esp_mqtt.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/network/esp_mqtt.c)<br>[`src/network/sim_network_broker.h`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/network/sim_network_broker.h)<br>[`src/network/sim_network_broker.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/network/sim_network_broker.c)<br>[`src/esp_idf_bridge.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/esp_idf_bridge.c) |

---

## 变更记录 (Revision History)

| 版本 | 日期 | 状态 | 修订要旨与评审吸收闭环 |
|---|---|---|---|
| **v1.0** | 2026-09-30 | 驳回 (Rejected) | 初始草案。存在 Daemon 误用 `vTaskDelay` 导致无法被新命令提前唤醒、`esp_timer` 微秒契约与 10ms tick 冲突、倒果为因允许回调阻塞、复位 DAG 冲刷 Broker 回调、代际未防 ABA、未闭合 Ethernet 等 6 项关键缺陷。 |
| **v1.1** | 2026-09-30 | 待审 (In Review) | **全面重构与纠偏闭环**：<br>1. **P0 修复**：Daemon 彻底废弃 `vTaskDelay`，改用标准的 `xQueueReceive(cmd_queue, &msg, timeout)`，确保始终处于 `SIM_TASK_STATE_BLOCKED` 可被新命令随时唤醒；<br>2. **P1 修复**：诚实界定 `esp_timer` 范围，`esp_timer_create` 明确本期不交付（Fail-Loud），`esp_timer_get_time` 维持现状；<br>3. **P1 修复**：重申定时器回调**严格禁止阻塞**，澄清 Daemon 隔离的真正原因为主循环 WCET 与命令队列缓冲；<br>4. **P1 修复**：重构复位 DAG，分离析构清理与重新注册阶段，修复 Wi-Fi reset 冲刷 Broker 回调的既有漏洞；<br>5. **P1 修复**：引入全局单调自增代际生成器与 `(slot, gen)` 校验，防范 `destroy` 后的 ABA 槽位复用；<br>6. **P1 修复**：剔除 Ethernet 承诺，Broker 注册改为凭据/二元组 `(cb, user_ctx)` 精确注销；<br>7. **补正**：修复 6 处失效 ADR 相对路径，对齐 LITE/STANDARD/PRO 容量宏，补充 6 组严苛单测。 |

---

## 一、 背景与架构问题陈述

在 2026-09-30 的多轮白盒审计与代码审查中，团队识别出 ESP-IDF 仿真门面在**定时器时序模型**与**网络协议栈出口**存在三个结构性架构隐患：

### 1.1 隐患 1：内核软定时器与 FreeRTOS 用户定时器的语义鸿沟
- **内核定时器极限约束**：[`runtime/src/wink_soft_timer.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/runtime/src/wink_soft_timer.c) 是 WinkMicroOS 内核全局共享的 16 槽轻量软定时器，运行在主循环内核 tick 的直接调用上下文中；其具有极其严苛的 WCET 审计——软预算 100µs（`WINK_LIGHT_SOFT_BUDGET_US`），硬上限 500µs（`WINK_LIGHT_HARD_LIMIT_US`），连续超时 3 次即触发致命故障（`WINK_FAULT_LIGHT_WCET_VIOLATION`）；
- **原厂语义与隔离动机纠偏**：
  - FreeRTOS 软件定时器（`TimerCallbackFunction_t`）与应用业务往往需要数毫秒的计算耗时，远超内核 100µs 的软限制；
  - 必须严正澄清：**定时器回调绝不允许阻塞**（原厂规范明确规定：`Timer callback functions must never attempt to block`，严禁在回调中调用 `vTaskDelay` 或非零超时的信号量/队列阻塞原语，否则会使整个 Timer Daemon 挂起，瘫痪全系统定时器与网络工作项）；
  - 引入独立 **Timer Daemon 纤程（Timer Service Task）** 的真正动机在于：
    1. 避免用户定时器计算开销侵占内核主循环的极严 WCET 预算；
    2. 提供标准的 FreeRTOS 命令队列缓冲（`xTimerStart`、`xTimerStop` 等跨纤程异步投递）；
    3. 提供代际 Token 状态管理与有序到期分发；
  - 所有在 Timer Daemon 中执行的用户回调与驱动工作项，**必须为有界、非阻塞的纯计算或状态投递**。

### 1.2 隐患 2：网络驱动中临时任务滥用打爆 LITE 8 任务槽
- 现有 [`src/wifi/esp_wifi.c:75`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/wifi/esp_wifi.c#L75) 与 [`src/network/esp_mqtt.c:397`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/network/esp_mqtt.c#L397) 为了实现 50ms~100ms 的异步连接延迟，均直接调用 `xTaskCreate` 创建临时纤程（`wifi_connect_task` 与 `mqtt_connect_task`）；
- 在 **LITE Profile**（[`esp_idf_target.cmake:51`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/esp_idf_target.cmake#L51)）下，全系统最大任务数仅有 **8 个**（`WINK_SIM_MAX_TASKS = 8`）；若上层业务创建 4~5 个应用任务，加上系统事件任务，网络临时任务极易导致 `xTaskCreate` 返回 `errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY`，导致系统死锁或启动失败；
- **架构决策**：彻底拔除一次性临时任务，全面改为**有界、可取消的延迟定时器工作项（Delayed Work Items）**。

### 1.3 隐患 3：MQTT 单例全局静态变量与复位 Broker 回调冲刷 Bug
- [`src/network/esp_mqtt.c:52-53`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/network/esp_mqtt.c#L52) 的 `s_mqtt_token` 与 `s_mqtt_task_handle` 是单例静态变量。后发起的客户端连接会覆盖全局 Token，导致前一个客户端的连接任务在校验代际时失败退出；
- **存量复位 Bug 披露**：现有 [`esp_idf_bridge.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/esp_idf_bridge.c) 中，Stage 1 调用 `esp_mqtt_sim_reset()`（内部注册了 Broker 回调），随后 Stage 2 调用 `esp_wifi_sim_reset()`（内部执行 `sim_network_broker_reset()`），直接用 `memset` **抹平了 Broker 全部槽位**！导致重启后 MQTT 彻底失去网络事件感知；
- 现行 [`sim_network_broker.h`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/network/sim_network_broker.h) 仅按函数指针注销，若多个 MQTT 客户端共用同一静态函数回调，注销会发生严重误杀；
- **架构决策**：将客户端状态与 Token 下沉至 `struct esp_mqtt_client` 实例内部；重构复位因果图，严格解耦析构清理与重新注册阶段；升级 Broker 支持 `(cb, user_ctx)` 二元组精确定位注销。

---

## 二、 核心架构设计与拓扑

```
+─────────────────────────────────────────────────────────────────────────────────────────+
|                         ESP-IDF Facade Timing & Network Architecture                    |
+─────────────────────────────────────────────────────────────────────────────────────────+

  [ User / Application Task ]          [ Wi-Fi / MQTT Driver ]         
              │                                   │                    
              │ xTimerStart / Change / Stop       │ Enqueue Work Item  
              ▼                                   ▼                    
  ┌─────────────────────────────────────────────────────────────────────────────────────┐
  │                 Timer Daemon Command Queue (Fixed Ring Buffer, Cap = 8/16/32)       │
  │   - TM_CMD_START / TM_CMD_STOP / TM_CMD_CHANGE_PERIOD / TM_CMD_WORK_ITEM           │
  │   - Thread-safe, bounded, triggers sim_scheduler_resume() on Daemon fiber          │
  └──────────────────────────────────────────┬──────────────────────────────────────────┘
                                             │
                                             │ xQueueReceive(queue, &msg, xTicksToWait)
                                             │ (State: SIM_TASK_STATE_BLOCKED, never WAITING)
                                             ▼
  ┌─────────────────────────────────────────────────────────────────────────────────────┐
  │                    Timer Service Daemon Fiber (sys_timer_daemon)                    │
  │   - Priority: CONFIG_FREERTOS_TIMER_TASK_PRIORITY (Default: 1, Above Idle)          │
  │   - Slot: Dedicated System Fiber (Lazy allocated on first timer creation)           │
  │   - Next Sleep Window: xTicksToWait = min(t_expiry - t_now) ticks                   │
  │   - Woken by: Queue Command Arrival OR Expiry Timeout                               │
  └──────────────────────────────────────────┬──────────────────────────────────────────┘
                                             │ Non-blocking Dispatch
                                             ▼
  ┌─────────────────────────────────────────────────────────────────────────────────────┐
  │                       Ordered Timer Active List (Sorted by t_expiry)                │
  │   - Generation Token Defense: Global Monotonic Gen + Slot ID Dual Validation        │
  │   - Work Items Execution: Non-blocking State Machine Advance (Wifi/MQTT)            │
  │   - User FreeRTOS Timer Callbacks: Non-blocking, strictly bounded execution         │
  └─────────────────────────────────────────────────────────────────────────────────────┘

  ┌─────────────────────────────────────────────────────────────────────────────────────┐
  │                  sim_network_broker (L2/L3 Network Status Broker)                   │
  │   - Default Route Netif: STA Netif (Priority 1) -> Up/Down Cascade                   │
  │   - Non-Default Route: SoftAP Netif (Local Only, No Default Gateway)                 │
  │   - Subscription: Precise unregister by (cb, user_ctx) key                          │
  └─────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 三、 模块 1：Timer Daemon 纤程与命令驱动模型 (纠偏闭环)

### 3.1 守护纤程等待模型（P0 级核心修复：拒绝 `vTaskDelay`）

在协作式调度器 [`wink_sim_scheduler.c:298`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/targets/common/src/wink_sim_scheduler.c#L298) 中，`sim_scheduler_resume()` 的刚性契约是：
$$\text{Task can only be resumed if } \text{state} == \text{SIM\_TASK\_STATE\_BLOCKED}$$
调用 `vTaskDelay()` 会使任务转为 `SIM_TASK_STATE_WAITING`，此时向队列投递任何新命令都**无法提前唤醒任务**。

**规范化主循环设计（严格对齐原厂 `prvTimerTask`）**：
```c
static void sys_timer_daemon_task(void *arg) {
    (void)arg;
    timer_cmd_msg_t msg;
    TickType_t next_expiry_ticks;
    TickType_t wait_ticks;

    while (s_timer_daemon_running) {
        /* 1. 扫描当前活跃定时器链表，获取最近到期时间差 */
        next_expiry_ticks = prvGetNextExpiryTicks();
        if (next_expiry_ticks == portMAX_DELAY) {
            wait_ticks = portMAX_DELAY; /* 无活跃定时器，无期限阻塞等待命令 */
        } else {
            TickType_t now_ticks = xTaskGetTickCount();
            wait_ticks = (next_expiry_ticks > now_ticks) ? (next_expiry_ticks - now_ticks) : 0;
        }

        /* 2. 核心阻塞等待：接收命令队列（带超时等待）
         * 此时任务处于 SIM_TASK_STATE_BLOCKED 状态！
         * - 若在此期间外部有任何新定时器或更早到期定时器命令入队，xQueueSend 立即唤醒 Daemon！
         * - 若无新命令到达且 wait_ticks 超时，xQueueReceive 返回 pdFALSE，自然进入到期分发处理！ */
        BaseType_t rc = xQueueReceive(s_timer_cmd_queue, &msg, wait_ticks);
        if (rc == pdPASS) {
            /* 立即处理新命令（Start, Stop, ChangePeriod, WorkItem 等）并更新链表 */
            prvProcessTimerCommand(&msg);
        }

        /* 3. 检查并分发所有已到期的定时器与工作项（非阻塞调用） */
        prvProcessExpiredTimers();
    }
}
```

### 3.2 静态数据结构、背压机制与 Profile 容量预算

遵循 **ADR-0004（编译期静态分发）** 与 **ADR-0085 / ADR-0089（静态容量规范）**：

```c
#ifndef CONFIG_FREERTOS_MAX_TIMERS
#define CONFIG_FREERTOS_MAX_TIMERS 10 /* LITE 默认 10 槽（8 用户 + 2 系统专用） */
#endif

#ifndef CONFIG_FREERTOS_TIMER_QUEUE_LENGTH
#define CONFIG_FREERTOS_TIMER_QUEUE_LENGTH 8
#endif

typedef enum {
    TIMER_CMD_START = 1,
    TIMER_CMD_STOP,
    TIMER_CMD_CHANGE_PERIOD,
    TIMER_CMD_DELETE,
    TIMER_CMD_RESET,
    TIMER_CMD_POST_WORK_ITEM
} timer_cmd_type_t;

typedef void (*timer_work_fn_t)(void *arg, uint32_t token);

typedef struct {
    timer_cmd_type_t type;
    TimerHandle_t    handle;
    uint32_t         slot_id;
    uint32_t         generation;        /* 全局单调代际令牌 */
    TickType_t       period_ticks;
    timer_work_fn_t  work_fn;
    void            *work_arg;
} timer_cmd_msg_t;

typedef struct {
    bool                     used;
    bool                     active;
    bool                     auto_reload;
    bool                     is_system_work_item; /* 是否为系统内部 Wi-Fi/MQTT 工作项 */
    uint32_t                 slot_id;
    uint32_t                 generation;         /* 全局单调递增代际，跨生命周期唯一 */
    char                     name[16];
    TickType_t               period_ticks;
    TickType_t               expiry_ticks;       /* 绝对 Tick 到期时间 */
    void                    *timer_id;           /* pvTimerGetTimerID */
    TimerCallbackFunction_t  callback;           /* 用户回调 (严格非阻塞) */
    timer_work_fn_t          work_fn;            /* 驱动内部轻量工作项 (严格非阻塞) */
    void                    *work_arg;
} esp_sim_timer_t;
```

**Profile 容量对齐表（精准解耦用户空间与驱动工作项）**：
| 配置 Profile | 用户可用定时器数 | 系统保留工作项槽 | 总定时器槽位 `CONFIG_FREERTOS_MAX_TIMERS` | 命令队列深度 `QUEUE_LEN` | 纤程任务总预算 |
|---|---|---|---|---|---|
| **LITE** | **8 槽** | 2 槽（Wi-Fi 1 + MQTT 1） | **10 槽** | 8 | 8（Main 1 + SysEvt 1 + Daemon 1 + App 5） |
| **STANDARD** | **16 槽** | 4 槽 | **20 槽** | 16 | 16 |
| **PRO** | **32 槽** | 8 槽 | **40 槽** | 32 | 32 |

**命令队列满时的防死锁与背压策略（Backpressure Strategy）**：
- `xTimerStart(xTimer, xTicksToWait)` 允许带有界超时；
- **防死锁断言**：通过 `esp_freertos_assert_not_in_critical()` 确保投递命令时绝不处于临界区内；
- 当命令队列已满且超时耗尽时，返回 `pdFAIL`（Fail-Loud），**绝不允许无期限阻塞**导致系统级联死锁；
- 在单测中增加 `test_freertos_timers_queue_full_backpressure` 验证溢出阻断与恢复能力。

### 3.3 全局单调代际防卫与槽位复用安全（彻底消除 ABA 缺陷）

```
[全局代际生成器] s_global_timer_gen ───(每次分配单调自增)───► 永远唯一，绝不回退
                                              │
                      ┌───────────────────────┴───────────────────────┐
                      ▼                                               ▼
         [Timer Object in Slot i]                        [Command Msg in Queue]
         - slot_id    = i                                - slot_id    = i
         - generation = s_global_timer_gen               - generation = s_global_timer_gen
```

1. **代际分配契约**：
   - 严禁在 `xTimerCreate` 或 `destroy` 时仅重置局部变量；
   - 声明全局静态变量：`static uint32_t s_global_timer_gen = 1000;`；
   - 每次分配槽位或更新定时器生命周期时，`generation = ++s_global_timer_gen`；
2. **槽位复用防护（Anti-ABA）**：
   - 当某个定时器在途排队时被 `xTimerDelete()`，槽位被立即归还并清空；
   - 新创建的定时器即便复用了同一个 `slot_id`，其获得的 `generation` 必定大于旧命令中的代际；
   - 当 Daemon 消费到旧的到期事件时：
     ```c
     if (!timer->used || timer->generation != msg->generation) {
         /* 代际失配，直接静默丢弃旧事件，绝不调用任何回调！ */
         return;
     }
     ```
   - 彻底防范 `destroy` 之后立即复用导致野指针或幽灵回调调用的深层风险。

---

## 四、 模块 2：微秒级 `esp_timer` 契约诚实定界 (Fail-Loud 原则)

### 4.1 核心冲突与范围定界（ADR-0012 契约诚实优于静默降级）

- **技术断层识别**：
  - `FreeRTOSConfig.h` 规定了 100Hz Tick（10ms 颗粒度）；
  - 乐鑫原厂 `esp_timer` 规范要求微秒级分辨率，支持单次与周期性微秒定时；
  - 若在 10ms Tick 的 Timer Daemon 中强行伪装微秒定时器，小于 10,000µs 的延时全部被算成 0，这是严重的**契约欺骗与行为降级**；
- **本期实施定界（Phase 1 范围锁定）**：
  1. **本期交付范围仅限于 FreeRTOS Timer API**（7 项核心接口）；
  2. `esp_timer_create()`、`esp_timer_start_once()`、`esp_timer_start_periodic()` 等动态微秒定时器接口**明确不纳入本期交付**；若应用调用，返回 `ESP_ERR_NOT_SUPPORTED`（Fail-Loud）；
  3. `esp_timer_get_time()` 维持只读虚拟微秒时钟功能，直接返回 `(int64_t)pal_os_get_us()` 并挂载自旋防卡死 accounting；
  4. 真正的微秒级定时需求由计划 02 中已交付并通过硬件级单测的 `esp_gptimer`（依托 PAL 硬件定时器）提供。

---

## 五、 模块 3：网络出口路由收敛与客户端生命周期矩阵

### 5.1 消除 MQTT 全局单例与延迟工作项重构

重构 [`src/network/esp_mqtt.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/network/esp_mqtt.c)：

```c
struct esp_mqtt_client {
    bool                     initialized;
    bool                     started;
    bool                     connected;
    uint32_t                 client_slot;       /* 实例槽位 ID (0 .. MAX_MQTT_CLIENTS-1) */
    uint32_t                 generation;        /* 全局单调代际，防 destroy 后槽位复用 ABA */
    esp_mqtt_client_config_t config;
    esp_netif_t             *bound_netif;       /* 绑定的指定 Netif (NULL 则使用默认出口) */
    /* ... 报文收发缓冲与回调上下文 ... */
};
```

1. **废除 `xTaskCreate` 临时任务**：
   - 彻底删除 `mqtt_connect_task`；
   - 替换为：向 Timer Daemon 投递一个 50ms 延迟工作项 `mqtt_connect_delayed_work`；
   - 投递参数携带 `(client_slot, client->generation)`；
   - 当工作项到期执行时，Daemon 严格校验 `s_clients[slot].initialized && s_clients[slot].generation == gen`，若不匹配直接丢弃；
2. **多客户端并发隔离**：
   - 静态分配 `s_clients[MAX_MQTT_CLIENTS]`，每个客户端拥有独立的代际与状态机；
   - 彻底消除原 `s_mqtt_token` 与 `s_mqtt_task_handle` 全局变量，支持多客户端完全并发操作。

### 5.2 Wi-Fi STA 延迟工作项与代际失效

重构 [`src/wifi/esp_wifi.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/wifi/esp_wifi.c)：
1. 移除 `wifi_connect_task`（`xTaskCreate` 启动的 100ms 临时纤程）；
2. 改用 Timer Daemon 延迟工作项推进 AP 匹配与 DHCP 状态；
3. 在 `esp_wifi_disconnect()`、`esp_wifi_stop()` 或 `DROP_BEACON` 时，递增 `s_wifi_generation`，使队列中未决的连接工作项即刻失效，彻底杜绝断线后迟到的幽灵 `GOT_IP` 事件。

### 5.3 多 Netif 路由收敛与 Broker 订阅句柄 (P1 级修正)

1. **坚决剔除 Ethernet 承诺**：
   - 明确当前门面网络层仅支持 **Wi-Fi STA** 与 **Wi-Fi SoftAP** 双网卡模型；
   - 杜绝“Ethernet 提升为默认网关与 STA 掉线全局断开”的逻辑矛盾；
2. **Broker 注册改为 `(cb, user_ctx)` 二元组精确定位注销**：
   - 彻底废除仅按函数指针注销的错误逻辑；
   - 升级 [`sim_network_broker.h`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/network/sim_network_broker.h)：
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
   - 多个 MQTT 客户端共用同一静态函数 `on_network_broker_state_changed` 时，通过 `user_ctx`（即客户端句柄）精确区分，注销互不影响。
3. **路由规则**：
   - **Wi-Fi STA**：唯一默认外网出口网关；上线广播 `UP`，下线广播 `DOWN`，级联通知未指定 Netif 的 MQTT 客户端；
   - **SoftAP**：本地局域网服务，不具备默认外网路由能力，状态变化不波及外网客户端。

---

## 六、 模块 4：9 阶段复位因果图 (Reset DAG) 深度重构 (P1 级修正)

### 6.1 存量网络回调冲刷 Bug 根治与双阶段哲学

现有代码的致命问题是在 Stage 1 的 `esp_mqtt_sim_reset()` 中过早重新注册了 Broker 回调，紧接着在 Stage 2 的 `esp_wifi_sim_reset()` 中被 `sim_network_broker_reset()` 的 `memset` 抹除。

**重构为严格的“Phase A 全面析构清理 $\to$ Phase B 基线初始化”架构**：

```
 [Phase A: 全链路析构清理 (Tear-Down)]
      │
      ▼
 1. esp_mqtt_sim_teardown() / esp_http_client_sim_teardown()
    [断开连接，注销 Broker 订阅，作废在途连接工作项，不在此处注册任何新回调！]
      │
      ▼
 2. esp_wifi_sim_teardown() ───► sim_network_broker_reset()
    [断开空口，清空 AP/STA 状态，排空 Broker 所有订阅槽位，重置网卡状态]
      │
      ▼
 3. esp_nimble_sim_reset()
    [析构 BLE 协议栈与广播句柄]
      │
      ▼
 4. esp_event_loop_sim_reset()
    [排空事件循环队列，注销 sys_evt 纤程]
      │
      ▼
 5. esp_freertos_timers_sim_reset() (NEW!)
    [排空定时器命令队列，作废所有在途活跃定时器与工作项，停止 sys_timer 纤程]
      │
      ▼
 6. sim_scheduler_reset(0)
    [强制注销并排空所有应用业务纤程与守护纤程]
      │
      ▼
 7. esp_peripherals_reset()
    [释放硬件外设驱动，关闭 GPTimer，冲刷 NVS RAM 缓存]
      │
      ▼
 8. esp_freertos_pools_reset()
    [重置任务/队列/信号量/自旋锁对象池]
      │
      ▼
 [Phase B: 基线归位与就绪重绑 (Re-Arm)]
      │
      ▼
 9. esp_netif_init()
    [重新构建干净的双网卡基线 (STA / SoftAP)，等待上层应用主动拉起并绑定]
```

### 6.2 软复位铁律与零幽灵回调保证
1. **调度器边界断言**：
   `assert(sim_scheduler_current_id() == SIM_SCHED_NO_READY);`
2. **生命周期防御断言**：
   - 彻底杜绝在复位清理阶段“边清理边向底层注册监听”的反模式；
   - MQTT 对 Broker 的订阅，统一收敛至 `esp_mqtt_client_init()` 或 `esp_mqtt_client_start()` 时按需建立；
3. **“到期前 1ms 触发 `esp_restart()`”极限单测**：
   - 定时器剩余 1ms 到期时触发强行复位，断言新基线中活跃定时器严格为 0，旧回调绝不穿透至新系统。

---

## 七、 详细验证矩阵与测试用例规划

| 测试集 | 对应目标文件 | 核心断言与覆盖场景 |
|---|---|---|
| **Timer 提前唤醒测试** | `test/freertos/test_freertos_timers.c` | Daemon 等待 1000ms 定时器期间，在 10ms 时刻插入一个 20ms 到期的短定时器，断言 Daemon 立即被唤醒并在 20ms 准时触发回调（覆盖 P0 队列接收修复）。 |
| **Timer 槽位复用 ABA 测试** | `test/freertos/test_freertos_timers.c` | 创建 Timer A，立即 Delete 并快速复用槽位创建 Timer B，断言 Timer A 的旧命令在 Daemon 消费时因代际失配被精准丢弃。 |
| **队列满背压防死锁测试** | `test/freertos/test_freertos_timers.c` | 连续投递填满命令队列，验证带超时与非阻塞返回 `pdFAIL`，断言临界区投递触发断言拦截。 |
| **MQTT 槽位复用 ABA 测试** | `test/network/test_esp_mqtt.c` | 初始化 Client A 并发起 50ms 延时连接，在第 10ms 调用 `destroy` 并在同槽位初始化 Client B，断言 Client B 绝不触发 Client A 的连接回调。 |
| **Broker 二元组解绑测试** | `test/network/test_sim_network_broker.c` | 两个不同上下文注册同一静态回调函数，注销其中一个，断言另一个依然稳定接收状态广播。 |
| **复位后网络回调留存测试** | `test/core/test_esp_idf_runtime.c` | 模拟系统连网 $\to$ 触发 `esp_restart()` $\to$ 重新连网，断言新初始化的 MQTT 客户端能稳定收到网络上线并成功触发连接（验证复位 DAG 修复）。 |
| **未连网启动 MQTT 确定性报错** | `test/network/test_esp_mqtt.c` | 复位后不连 Wi-Fi 直接启动 MQTT，验证精准抛出 `MQTT_EVENT_ERROR` (EHOSTUNREACH) 并进入断开状态。 |
| **LITE 8 任务极限压力测试** | `test/freertos/test_freertos_timers.c` | 在 `WINK_SIM_MAX_TASKS = 8` 下创建 5 个应用任务 + 系统任务 + Timer Daemon，验证无槽位溢出与死锁。 |

---

## 八、 评审结论与落地路线

本设计规格（v1.1）已全面、彻底地吸纳并闭环了所有架构评审意见，建立了在协作式单核心环境下严谨、可信、防 ABA、防幽灵回调的时序与网络出口体系。

**评审确认后，即刻开启实施计划 03 的阶段 1 编码与单测工作。**
