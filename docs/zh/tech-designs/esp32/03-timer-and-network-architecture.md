<!-- SPDX-License-Identifier: Apache-2.0 -->
# 技术设计规格：ESP-IDF 软件定时器守护模型与网络出口时序解耦架构

| 字段 | 内容 |
|---|---|
| 文档编号 | TECH-DESIGN-ESP32-TIMER-NET-v1.0 |
| 状态 | 📋 **Proposed / Ready for Review（已完成架构设计，待评审 Sign-off）** |
| 日期 | 2026-09-30 |
| 对应实施计划 | [《实施计划 03：软定时器独立守护模型与网络出口时序解耦计划》](../../../implementation-plans/esp32/facade-hardening/03-timer-and-network-evolution-plan.md) |
| 决策依据 | [ADR-0004：编译期静态分发优于运行时函数指针](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)<br>[ADR-0007：协作式纤程与事件调度模型](../../decisions/core/0007-cooperative-loop-execution-model.md)<br>[ADR-0012：契约诚实优于静默降级（Fail-Loud 原则）](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)<br>[ADR-0053：虚拟时间因果同刻总序仲裁模型](../../decisions/unisim/0053-sim-same-timestamp-event-total-order.md)<br>[ADR-0088：仿真静态资源池与内存 Profile 规范](../../decisions/unisim/0088-esp-idf-simulation-profiles-and-static-limits.md) |
| 管辖实现文件 | [`src/freertos/freertos_timers.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/freertos/freertos_timers.c)<br>[`src/wifi/esp_wifi.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/wifi/esp_wifi.c)<br>[`src/network/esp_mqtt.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/network/esp_mqtt.c)<br>[`src/network/sim_network_broker.h`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/network/sim_network_broker.h)<br>[`src/network/sim_network_broker.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/network/sim_network_broker.c)<br>[`src/esp_idf_bridge.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/esp_idf_bridge.c) |

---

## 一、 背景与架构问题陈述

在 2026-09-30 的多轮白盒审计与代码审查中，团队识别出 ESP-IDF 仿真门面在**定时器时序模型**与**网络协议栈出口**存在三个结构性架构隐患：

### 1.1 隐患 1：内核软定时器与 FreeRTOS 用户定时器的语义鸿沟
- **内核定时器极限约束**：[`runtime/src/wink_soft_timer.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/runtime/src/wink_soft_timer.c) 是 WinkMicroOS 内核全局共享的 16 槽轻量软定时器，运行在主循环内核 tick 的直接调用上下文中；其具有极其严苛的 WCET 审计——软预算 100µs（`WINK_LIGHT_SOFT_BUDGET_US`），硬上限 500µs（`WINK_LIGHT_HARD_LIMIT_US`），连续超时 3 次即触发致命故障（`WINK_FAULT_LIGHT_WCET_VIOLATION`）；
- **原厂语义冲突**：FreeRTOS 软件定时器（`TimerCallbackFunction_t`）与乐鑫应用层业务逻辑允许执行数十毫秒的较重计算，甚至包含阻塞等待/纤程切出。若直接把 FreeRTOS 定时器或网络异步流程塞入 `wink_soft_timer`，会导致主循环被直接挂死或触发 WCET 崩溃；
- **架构决策**：必须遵循 FreeRTOS 原厂架构，引入独立的 **Timer Daemon 纤程（Timer Service Task）**，将定时器回调隔离在独立纤程中执行。

### 1.2 隐患 2：网络驱动中临时任务滥用打爆 LITE 8 任务槽
- 现有 [`src/wifi/esp_wifi.c:75`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/wifi/esp_wifi.c#L75) 与 [`src/network/esp_mqtt.c:397`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/network/esp_mqtt.c#L397) 为了实现 50ms~100ms 的异步连接延迟，均直接调用 `xTaskCreate` 创建临时纤程（`wifi_connect_task` 与 `mqtt_connect_task`）；
- 在 **LITE Profile**（[`esp_idf_target.cmake:51`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/esp_idf_target.cmake#L51)）下，全系统最大任务数仅有 **8 个**（`WINK_SIM_MAX_TASKS = 8`）；若上层业务创建 4~5 个应用任务，加上系统事件任务，网络临时任务极易导致 `xTaskCreate` 返回 `errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY`，导致系统死锁或启动失败；
- **架构决策**：彻底拔除一次性临时任务，全面改为**有界、可取消的延迟定时器工作项（Delayed Work Items）**。

### 1.3 隐患 3：MQTT 单例全局静态变量导致多客户端并发冲突
- [`src/network/esp_mqtt.c:52-53`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/network/esp_mqtt.c#L52) 的 `s_mqtt_token` 与 `s_mqtt_task_handle` 是单例静态变量。后发起的客户端连接会覆盖全局 Token，导致前一个客户端的连接任务在校验代际时失败退出；
- 现行 [`sim_network_broker.h`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/network/sim_network_broker.h) 仅维护单一全局布尔值，无法表达多个网络接口（STA、SoftAP、ETH）的状态与路由仲裁；
- **架构决策**：将客户端状态与 Token 下沉至 `struct esp_mqtt_client` 实例内部；扩展 `sim_network_broker` 支持 Netif 身份与事件枚举，建立明确的出站路由与级联断开规则。

---

## 二、 核心架构设计

```
+─────────────────────────────────────────────────────────────────────────────────────────+
|                         ESP-IDF Facade Timing & Network Architecture                    |
+─────────────────────────────────────────────────────────────────────────────────────────+

  [ User / Application Task ]          [ Wi-Fi / MQTT Driver ]           [ esp_timer API ]
              │                                   │                               │
              │ xTimerStart / Change              │ Enqueue Delayed Work          │ esp_timer_start
              ▼                                   ▼                               ▼
  ┌─────────────────────────────────────────────────────────────────────────────────────┐
  │                    Timer Daemon Command Queue (Thread-Safe Ring Buffer)             │
  │   - TM_CMD_START / TM_CMD_STOP / TM_CMD_CHANGE_PERIOD / TM_CMD_WORK_ITEM           │
  └──────────────────────────────────────────┬──────────────────────────────────────────┘
                                             │ Wakeup Daemon
                                             ▼
  ┌─────────────────────────────────────────────────────────────────────────────────────┐
  │                    Timer Service Daemon Fiber (sys_timer_daemon)                    │
  │   - Priority: CONFIG_FREERTOS_TIMER_TASK_PRIORITY (Default: 1, Above Idle)          │
  │   - Slot: Dedicated System Fiber (Lazy or Pre-allocated)                            │
  │   - Next Sleep Window: min(t_expiry - t_now) -> vTaskDelay(ticks)                   │
  └──────────────────────────────────────────┬──────────────────────────────────────────┘
                                             │ Expiry Dispatch
                                             ▼
  ┌─────────────────────────────────────────────────────────────────────────────────────┐
  │                       Ordered Timer Active List (Sorted by t_expiry)                │
  │   - Generation Token Defense: Stale ISR & Cancelled Item Drop                       │
  │   - Work Items Execution: Wifi State Transition / MQTT Connection Transition        │
  │   - User FreeRTOS Timer Callbacks: (pxCallbackFunction(xTimer))                     │
  └─────────────────────────────────────────────────────────────────────────────────────┘

  ┌─────────────────────────────────────────────────────────────────────────────────────┐
  │                  sim_network_broker (L2/L3 Network Status Broker)                   │
  │   - Default Route Netif: STA Netif (Priority 1) -> Up/Down Cascade                   │
  │   - Non-Default Route: SoftAP Netif (Local Only, No Default Gateway)                 │
  │   - Event Callback: sim_netif_event_cb_t(netif, SIM_NETIF_EVT_UP/DOWN, user_ctx)   │
  └─────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 三、 模块 1：Timer Daemon 纤程与工作项队列设计

### 3.1 守护纤程生命周期模型
FreeRTOS 原厂规范中，软件定时器均由 `prvTimerTask`（守护任务）在 FreeRTOS 调度器中运行。
在 Wink 协作式仿真体系下：
1. **守护纤程名称**：`"sys_timer"`；
2. **注册时机（懒加载 vs 预分配）**：
   - 为避免在不需要使用 Timer 的极简示例中浪费 1 个任务槽，采用**按需懒加载（Lazy Initialization）**：
   - 首次调用 `xTimerCreate` 或 `esp_timer_create` 时，内部执行 `esp_freertos_timers_init()`，注册 Timer Daemon 纤程；
   - 在复位 DAG（`esp_freertos_timers_sim_reset`）中销毁/注销，归还任务槽位。
3. **优先级与调度策略**：
   - 守护纤程优先级设为 `CONFIG_FREERTOS_TIMER_TASK_PRIORITY`（默认为 1）；
   - 当无定时器处于激活状态时，Timer Daemon 阻塞等待于内部命令队列信号量；
   - 当存在激活定时器时，Timer Daemon 以下一个最近到期的绝对虚拟时间差进行阻塞等待：
     $$\Delta t = \min_{i} (t^{(i)}_{\text{expiry}} - t_{\text{current}})$$
     通过 `vTaskDelay(pdMS_TO_TICKS(\Delta t))` 切出，到达时刻被调度器自动唤醒。

### 3.2 静态数据结构与 Profile 容量预算

遵循 **ADR-0004（编译期静态分发）** 与 **ADR-0088（静态池规范）**，绝不进行动态堆分配：

```c
#ifndef CONFIG_FREERTOS_MAX_TIMERS
#define CONFIG_FREERTOS_MAX_TIMERS 16
#endif

#define TIMER_CMD_QUEUE_SIZE 16

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
    TickType_t       period_ticks;
    uint32_t         token;
    timer_work_fn_t  work_fn;
    void            *work_arg;
} timer_cmd_msg_t;

typedef struct {
    bool                     used;
    bool                     active;
    bool                     auto_reload;
    uint32_t                 token;             /* 代际令牌，防 ABA 与迟到事件 */
    uint32_t                 sim_handle_token;  /* esp_sim_handle 句柄池令牌 */
    char                     name[16];
    TickType_t               period_ticks;
    uint64_t                 next_expiry_us;    /* 绝对微秒到期时间 (因果全序) */
    void                    *timer_id;          /* pvTimerGetTimerID */
    TimerCallbackFunction_t  callback;          /* 用户回调 */
    timer_work_fn_t          work_fn;           /* 驱动内部轻量工作项 */
    void                    *work_arg;
} esp_sim_timer_t;
```

**Profile 容量分配表**：
| 配置 Profile | 最大定时器数 `CONFIG_FREERTOS_MAX_TIMERS` | 命令队列深度 `TIMER_CMD_QUEUE_SIZE` | 占用的 `sim_scheduler` 任务槽 |
|---|---|---|---|
| **LITE** | 8 | 8 | 1 槽（按需占用） |
| **STANDARD** | 16 | 16 | 1 槽 |
| **PRO** | 32 | 32 | 1 槽 |

> **LITE 8 任务槽预算分配说明**：
> - 核心系统占用：Main Task (1) + Sys Event (1) + Timer Daemon (1) = 3 槽；
> - 留给用户业务应用：5 槽（满足全部官方 LITE 示例，且 Wi-Fi/MQTT 连接不再抢占临时任务槽，实际净省 2 个瞬态槽位）。

### 3.3 代际令牌防卫与幽灵回调消除（Generation Token Defense）

```
[Timer Created]  ───> token = 1
         │
[Timer Stopped]  ───> token = 2 (旧 token = 1 的在途队列命令到期一律静默丢弃)
         │
[Timer Deleted]  ───> used = false, token = 3 (彻底失效)
```
- **核心契约**：
  1. 定时器在 `xTimerStop`、`xTimerDelete`、`xTimerChangePeriod` 时，`token` 严格递增；
  2. 当 Timer Daemon 从激活链表弹出到期项时，若发现当前定时器的 `token` 与到期时记录的快照不一致，判定为“已取消或重置的失效事件”，直接丢弃，绝不调用回调；
  3. 彻底消除 Wi-Fi 掉线或 MQTT 析构后仍然收到回调的**幽灵事件（Ghost Callback）**。

---

## 四、 模块 2：微秒级 `esp_timer` 独立承载模型

### 4.1 乐鑫 `esp_timer` 原厂契约与差异分析

| 维度 | FreeRTOS 软件定时器 (`xTimer`) | 乐鑫高精度定时器 (`esp_timer`) |
|---|---|---|
| **时间基准** | Tick（默认 10ms 或 1ms） | 微秒（Microseconds, µs） |
| **主要 API** | `xTimerCreate`, `xTimerStart` | `esp_timer_create`, `esp_timer_start_periodic` |
| **回调上下文** | Timer Service Task | 原厂中断上下文或专用高优先级分发任务 |
| **开销敏感度** | 毫秒级任务调度 | 微秒级高频触发 |

### 4.2 统一事件分发方案（方案比选与决策）

- **备选方案 A（分立运行）**：单独为 `esp_timer` 拉起一个微秒纤程。
  - *缺点*：在 LITE Profile 下再消耗 1 个宝贵任务槽，导致业务可用任务槽降为 4 个，极易引发容量不足。
- **备选方案 B（统一时钟轮询分发引擎 - 推荐决策）**：
  - 将 `esp_timer` 与 FreeRTOS Timer 统一归入同一套底层时间事件排序链表；
  - 到期时间统一计算为微秒绝对时间戳 `next_expiry_us`（通过 `pal_os_get_us()` 获取基线）；
  - `esp_timer_create` 生成的对象使用统一的 `esp_sim_timer_t` 结构体，标记分发类型为 `DISPATCH_ESP_TIMER`；
  - 到期时，Timer Daemon 按照绝对时间统一调度，保持 ADR-0053 的因果全序性。

```c
typedef struct {
    esp_timer_cb_t callback;
    void *arg;
    esp_timer_dispatch_t dispatch_method;
    const char *name;
    bool skip_unhandled_events;
} esp_timer_create_args_t;

esp_err_t esp_timer_create(const esp_timer_create_args_t* create_args, esp_timer_handle_t* out_handle);
esp_err_t esp_timer_start_once(esp_timer_handle_t timer, uint64_t timeout_us);
esp_err_t esp_timer_start_periodic(esp_timer_handle_t timer, uint64_t period_us);
esp_err_t esp_timer_stop(esp_timer_handle_t timer);
esp_err_t esp_timer_delete(esp_timer_handle_t timer);
```

---

## 五、 模块 3：网络出口路由契约与客户端生命周期矩阵

### 5.1 消除 MQTT 全局单例并发冲突

彻底重构 [`src/network/esp_mqtt.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/network/esp_mqtt.c)，将全局静态变量下沉至实例中：

```c
struct esp_mqtt_client {
    bool                     initialized;
    bool                     started;
    bool                     connected;
    uint32_t                 client_id;         /* 实例槽位 ID */
    uint32_t                 token;             /* 实例私有代际 Token */
    esp_mqtt_client_config_t config;
    esp_netif_t             *bound_netif;       /* 绑定的指定 Netif (NULL 则使用默认网关) */
    TimerHandle_t            connect_timer;     /* 代替原先的 xTaskCreate 临时任务 */
    /* ... 报文收发缓冲与回调上下文 ... */
};
```

1. **临时连接任务拔除**：
   - 废除 `xTaskCreate(mqtt_connect_task, "mqtt_conn", ...)`；
   - 替换为：向 Timer Daemon 投递一个 50ms 延迟工作项 `mqtt_connect_delayed_work`；
   - 若在 50ms 窗口期内调用了 `esp_mqtt_client_stop` 或 `esp_mqtt_client_destroy`，立即增加 `client->token`，工作项到期自动作废，杜绝内存访问野指针；
2. **多客户端隔离**：
   - `s_clients[MAX_MQTT_CLIENTS]` 彼此独立，各自持有独立的 `token` 与定时器工作项，彻底支持 2 个（或以上）MQTT 客户端完全并发连接。

### 5.2 Wi-Fi STA 状态机工作项收敛

重构 [`src/wifi/esp_wifi.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/wifi/esp_wifi.c)：
1. 移除 `wifi_connect_task`（`xTaskCreate` 启动的 100ms 任务）；
2. 采用 Timer Daemon 工作项投递 100ms 延迟事件：
   ```c
   static void wifi_connect_delayed_work(void *arg, uint32_t token) {
       if (!s_wifi.initialized || s_wifi.state != WIFI_SIM_CONNECTING || s_connect_token != token) {
           return;
       }
       /* 执行虚拟 AP 匹配与 DHCP 状态机推进 */
   }
   ```
3. 在 `esp_wifi_disconnect()`、`esp_wifi_stop()` 或 `DROP_BEACON` 信标丢失时，`s_connect_token++`，精准作废未决工作项，彻底防止产生迟到的幽灵 `GOT_IP` 事件。

### 5.3 多 Netif 出口路由仲裁与 Broker 演进

扩展 [`src/network/sim_network_broker.h`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/network/sim_network_broker.h) 接口：

```c
typedef enum {
    SIM_NETIF_EVT_DOWN = 0,
    SIM_NETIF_EVT_UP   = 1,
} sim_netif_event_t;

typedef void (*sim_netif_event_cb_t)(esp_netif_t *netif, sim_netif_event_t event, void *user_ctx);

int  sim_network_broker_register_cb(sim_netif_event_cb_t cb, void *user_ctx);
void sim_network_broker_unregister_cb(sim_netif_event_cb_t cb);

/* 网卡状态更新由具体网络实现（Wi-Fi STA, AP, ETH）主动驱动 */
void sim_network_broker_notify_netif(esp_netif_t *netif, sim_netif_event_t event);
esp_netif_t* sim_network_broker_get_default_netif(void);
```

**多 Netif 路由矩阵与级联规则**：
| Netif 类型 | 默认网关优先级 | 外网路由能力 | 掉线级联影响 |
|---|---|---|---|
| **Wi-Fi STA** | **最高（Priority 1）** | 是 | 触发全局默认出口 DOWN；级联通知所有未指定 Netif 的 MQTT 客户端断开连接并进入重连 |
| **SoftAP** | **无（本地直连）** | 否（仅本地虚拟广播域） | 不影响上层外网 MQTT 客户端；仅影响连接至本 AP 的客户端设备 |
| **Ethernet** | **备用（Priority 2）** | 是 | 若 STA 未就绪则提升为默认网关 |

---

## 六、 模块 4：复位因果图 (Reset DAG) 回写与生命周期防御

将 [`src/esp_idf_bridge.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/esp_idf_bridge.c) 的复位调用链升级为清晰的 9 阶段因果图：

```
 Scheduler boundary check: assert(sim_scheduler_current_id() == SIM_SCHED_NO_READY)
      │
      ▼
 1. esp_http_client_sim_reset() / esp_mqtt_sim_reset()  [Disconnect upper clients, cancel connect timers]
      │
      ▼
 2. esp_wifi_sim_reset() ───(indirect)───► sim_network_broker_reset() [Cancel Wi-Fi work items, clear broker]
      │
      ▼
 3. (void)esp_netif_init()                              [Re-initialize default network interface baseline]
      │
      ▼
 4. esp_nimble_sim_reset()                              [Tear down BLE stack & advertise handles]
      │
      ▼
 5. esp_event_loop_sim_reset()                          [Drain default event queue, unregister sys_evt fiber]
      │
      ▼
 5.5 esp_freertos_timers_sim_reset()                    [NEW! Drain timer command queue, cancel all active timers]
      │
      ▼
 6. sim_scheduler_reset(0)                              [Reset scheduler, evict all application task fibers]
      │
      ▼
 7. esp_peripherals_reset()                             [Release hardware PALs, stop GPTimers, flush NVS cache]
      │
      ▼
 8. esp_freertos_pools_reset()                          [Reset FreeRTOS object pools & spinlocks]
```

### 极限防御单测：“到期前 1ms 触发 `esp_restart()`”
- 在定时器设定 10ms 到期后的第 9ms，在另一个任务中执行 `esp_restart()`；
- 断言：
  1. 复位过程按因果顺序平稳排空定时器队列，无段错误；
  2. 重启后新系统创建的定时器正常工作，绝无旧定时器的回调遗留（Zero Late Ghost Callbacks）；
  3. 活跃任务数与活跃定时器数在新基线初始化后严格归零重置。

---

## 七、 实施交付与验收矩阵 (Verification Matrix)

| 阶段 | 交付目标 | 核心验证手段 | 退出标准 |
|---|---|---|---|
| **Phase 1** | FreeRTOS 定时器守护纤程与交付矩阵 | `test_freertos_timers.c` | 单次、周期、动态周期、删除、代际取消全绿；LITE 8 任务极限无死锁 |
| **Phase 2** | Wi-Fi 与 MQTT 临时任务拔除 | `test_esp_wifi.c`, `test_esp_mqtt.c` | 内存中无临时纤程创建；双 MQTT 客户端并发连接无 Token 冲突 |
| **Phase 3** | 多 Netif 与 Broker 事件扩展 | `test_sim_network_broker.c` | STA 掉线精准联动 MQTT 抛出 DISCONNECTED；SoftAP 隔离不干扰 |
| **Phase 4** | 复位 DAG 回写与端到端无头时序实证 | `run_esp32_headless_evidence.ps1` | STA 无头时序实证通过；全量 94+ 项 CTest 100% 绿灯 |

---

## 八、 评审结论与后续动作 (Sign-off & Transition)

- 本技术设计已彻底闭环了实施计划 03 的阶段 0 硬门禁要求；
- 评审通过后，本规格将作为实施依据，正式进入 [03-timer-and-network-evolution-plan.md](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/docs/implementation-plans/esp32/facade-hardening/03-timer-and-network-evolution-plan.md) 的阶段 1 编码与测试工作。
