<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划 03：软定时器独立守护模型与网络出口时序解耦计划

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20260930-ESP-IDF-TIMER-AND-NETWORK-v1.3 |
| 状态 | 📋 **Blocked on Tech Design（待前置技术设计规格评审通过后正式启动）** |
| 日期 | 2026-09-30 |
| 周期估算 | 2~2.5 个工作日 |
| 前置产出物 | [《定时器与网络出口架构技术设计规格》](../../../zh/tech-designs/esp32/03-timer-and-network-architecture.md)（物理落盘硬门禁） |
| 优先次序 | **前置架构技术设计 → 定时器独立守护纤程 (Timer Daemon Fiber) 落地 → 强制回写复位 DAG → MQTT 实例状态下沉 (消除全局单例冲突) → Wi-Fi / MQTT 临时任务收敛 → 多 Netif 路由与 Broker 扩展** |
| 决策依据 | [ADR-0007：协作式纤程与事件调度模型](../../../decisions/core/0007-cooperative-loop-execution-model.md)<br>[ADR-0053：虚拟时间因果同刻总序仲裁模型](../../../decisions/unisim/0053-sim-same-timestamp-event-total-order.md)<br>[ADR-0057：PAL ADC 子系统与通道 3（模拟量）仿真契约](../../../decisions/core/0057-pal-adc-subsystem-and-channel-3-analog-contract.md) |
| 管辖数据源 | [`src/freertos/freertos_timers.c`](../../../../wink-micro-os/frameworks/esp_idf/src/freertos/freertos_timers.c)、[`runtime/src/wink_soft_timer.c`](../../../../wink-micro-os/runtime/src/wink_soft_timer.c)、[`src/wifi/esp_wifi.c`](../../../../wink-micro-os/frameworks/esp_idf/src/wifi/esp_wifi.c)、[`src/network/esp_mqtt.c`](../../../../wink-micro-os/frameworks/esp_idf/src/network/esp_mqtt.c)、[`src/network/sim_network_broker.c`](../../../../wink-micro-os/frameworks/esp_idf/src/network/sim_network_broker.c) |
| 实施目标文件 | `src/freertos/freertos_timers.c`、`src/wifi/esp_wifi.c`、`src/network/esp_mqtt.c`、`src/network/sim_network_broker.*`、`src/esp_idf_bridge.c` |
| 验收门禁 | 技术设计规格评审、Wi-Fi STA/Scan 单测、MQTT 多客户端并发单测、重启瞬间定时器无幽灵回调单测、`run_esp32_headless_evidence.ps1 -App wifi_sta` 确定性时序、全量 CTest 真实回归 |

---

## 一、 计划背景与范围定界

### 1.1 核心定界原则与架构纠偏
* **严禁将网络业务与 FreeRTOS 定时器直接塞入全局 soft_timer（最核心技术防线）**：
  - 白盒深度排查表明：[`wink-micro-os/runtime/src/wink_soft_timer.c:159-204`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/runtime/src/wink_soft_timer.c#L159) 是整个 WinkMicroOS 内核全局共享的 16 槽软定时器，在主循环 tick 中由内核调度；且其回调受 `WINK_LIGHT_SOFT_BUDGET_US` 和 `WINK_LIGHT_HARD_LIMIT_US` 强力审计（连续 3 次超出硬阈值才记录 `WINK_FAULT_LIGHT_WCET_VIOLATION`）；
  - 若将 AP 匹配、DHCP 状态机转换、网络连接、事件投递甚至外部 FreeRTOS 用户定时器回调直接绑在其上，将不可避免地导致主循环被长时间挂起，严重击穿轻量任务预算；若回调中调用任何包含 yield/block 的 API，更会导致协作式调度器上下文崩溃；
  - **架构决策**：FreeRTOS 软件定时器必须严格遵循 FreeRTOS 原厂语义，由专门的**独立 Timer Daemon 纤程**驱动；定时器内部回调仅作为**有界、可取消的工作项（Bounded Cancellable Work Item）**投递，外部用户回调在 Daemon 上下文中执行，绝不侵占系统内核主循环；
* **高精度 `esp_timer` 独立架构决策**：
  - 现有 [`include/esp_timer.h`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/include/esp_timer.h) 全长仅 19 行，仅导出了 `esp_timer_get_time()` 读取微秒时钟；
  - 乐鑫原厂的 `esp_timer_create`、`esp_timer_start_once` 等高精度微秒级接口具有独立的调度与中断上下文属性，不得与 FreeRTOS 毫秒级软定时器混为一谈，必须在前置技术设计中单独定义其承载模型；
* **消除 MQTT 全局单例冲突与网络出口路由契约**：
  - 白盒核查揭示：[`src/network/esp_mqtt.c:52-53`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/network/esp_mqtt.c#L52) 的 `s_mqtt_token` 与 `s_mqtt_task_handle` 是文件级单例静态变量。若两个 MQTT 客户端并发发起连接，后一个客户端的启动会直接覆盖全局 Token，导致前一个客户端的异步任务失效；
  - 现有 [`sim_network_broker.h:19`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/network/sim_network_broker.h#L19) 接口实际名为 `sim_network_broker_register_cb`。必须对齐命名，并将回调扩展为携带 Netif 实例指针与事件类型枚举：
    ```c
    typedef void (*sim_netif_event_cb_t)(esp_netif_t *netif, sim_netif_event_t event, void *user_ctx);
    ```
  - 明确多 Netif 的核心契约：“默认出站网卡仲裁”、“按客户端绑定接口”、“网卡销毁与回调注销规则”；显式声明 SoftAP 仅提供本地接入，不具备默认外网路由能力；
  - 明确 HTTP 客户端仅在技术设计中对齐多 Netif 绑定模型，本计划具体实施集中于 MQTT，HTTP 顺延至下一批次；
* **时序断言语义客观化（拒绝硬编码采样时刻）**：
  - `wifi_sta.scenario.json` 中的 1500ms 是测试场景配置中定义的一个**状态采样观测点（Assertion Point）**，并非系统内部产生“已取得 IP”事件的固定刻度；
  - 验收断言必须以**事件因果顺序（Event Causality）**、**规定时间窗内状态收敛（Bounded Convergence）**与**取消后无迟到事件（Zero Late Arrival）**为核心。

---

## 二、 详细实施任务拆解 (Action Items)

### 阶段 0：前置技术设计规格产出（Prerequisite Tech Design Spec - 硬门禁）

- [ ] **任务 T0.1**：编写并归档 [《定时器与网络出口架构技术设计规格》](../../../zh/tech-designs/esp32/03-timer-and-network-architecture.md)：
  - **物理落盘要求**：本实施计划在阶段 0 技术设计文件完成物理落盘并获得架构团队 Sign-off 之前，严禁编写阶段 1~3 的 C 代码；
  - **模块 1：Timer Daemon 纤程与工作项队列设计**：
    - 定义 FreeRTOS 软件定时器在 `sim_scheduler` 协作式体系下的守护纤程模型；
    - 设计轻量、有界、带代际 Token 的定时器命令队列与回调队列；评估 LITE 配置（仅 8 任务槽）的容量预算；
  - **模块 2：`esp_timer` 微秒级 API 适配设计**：
    - 明确 `esp_timer_create`、`esp_timer_start_periodic` 的时钟源与轮询分发策略；
  - **模块 3：多 Netif 出口路由与客户端生命周期矩阵**：
    - 明确 STA / AP 默认网关优先级与 SoftAP 本地属性；
    - 明确 MQTT / HTTP 客户端与 Netif 绑定的生命周期图谱（绑定、网络中断、自动重连、析构注销）。

---

### 阶段 1：FreeRTOS 定时器守护纤程与交付矩阵 (Timer Daemon Fiber & Delivery Matrix)

- [ ] **任务 T1.1**：逐项落实 FreeRTOS Timer API 交付矩阵：
  | API 名称 | 交付归属 | 语义契约与实现策略 |
  |---|---|---|
  | `xTimerCreate` | **本期实现** | 分配静态槽位，初始化代际 token 与回调函数 |
  | `xTimerStart` / `xTimerStop` | **本期实现** | 向守护纤程队列投递命令，唤醒守护纤程 |
  | `xTimerChangePeriod` | **本期实现** | 动态调整周期并重启定时器 |
  | `xTimerDelete` | **本期实现** | 释放槽位，废弃在途待执行工作项 |
  | `xTimerReset` | **本期实现** | 重新计算到期时间 |
  | `xTimerIsTimerActive` | **本期实现** | 瞬态运行状态查询 |
  | `pvTimerGetTimerID` | **本期实现** | 获取用户上下文 ID |
  | `xTimerStartFromISR` 等 | **显式不支持** | 门面当前无独立硬件 ISR 线程，统一调用标准版本或返回错误 |
- [ ] **任务 T1.2**：编写定时器单测集 `test/freertos/test_freertos_timers.c`：
  - 验证单次到期触发、周期自动重载、运行中动态修改周期、删除后迟到回调拦截；
  - 增加 **LITE 配置（8 任务槽）容量测试**：验证在仅有 8 个任务槽的极限环境下，Timer Daemon 正常拉起且不压垮业务任务创建。
- [ ] **任务 T1.3**：【防腐硬约束】新组件强制回写 [`src/esp_idf_bridge.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/esp_idf_bridge.c) 复位 DAG：
  - 在复位流程的第 5 阶段（`esp_event_loop_sim_reset`）与第 6 阶段（`sim_scheduler_reset`）之间，增加：
    `esp_freertos_timers_sim_reset()`：彻底排空定时器命令队列与待执行工作项；
  - 编写专门的**“定时器即将触发瞬间强行重启”**单测：验证在 Timer 到期前 1ms 触发 `esp_restart()`，系统重启后绝无旧工作项泄漏到新系统中。

---

### 阶段 2：临时纤程任务精准收敛（Task Pruning & Ghost Event Elimination）

- [ ] **任务 T2.1**：重构 `src/wifi/esp_wifi.c` STA 连接状态机：
  - 移除通过 `xTaskCreate` 启动的一次性临时任务 `wifi_connect_task`；
  - 采用定时器守护纤程投递延迟连接工作项，完成虚拟 AP 匹配与 DHCP 状态推进；
  - 严格保持代际令牌（`generation_token`），在调用 `esp_wifi_disconnect()` 或收到 `DROP_BEACON` 时，精准取消未决工作项，彻底消除迟到的幽灵 `GOT_IP` 事件；
- [ ] **任务 T2.2**：重构 `src/network/esp_mqtt.c` 实例状态管理与任务收敛：
  - **消除全局单例**：将 `s_mqtt_token` 与 `s_mqtt_task_handle` 下沉至 `struct esp_mqtt_client` 实例内部结构体中，支持最多 `MAX_MQTT_CLIENTS` 个独立客户端并发连接；
  - 移除通过 `xTaskCreate` 启动的 50ms 一次性临时连接任务 `mqtt_connect_task`，改为实例绑定的延迟事件投递；
  - 验证：运行 `test_esp_mqtt` 并在单测中覆盖“两个 MQTT 客户端同时连接”场景，确认互不干扰。

---

### 阶段 3：多 Netif 链路状态感知与 Broker 演进 (Network Broker Refinement)

- [ ] **任务 T3.1**：扩展 [`src/network/sim_network_broker.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/network/sim_network_broker.c)：
  - 扩展回调接口携带 Netif 实例身份与事件枚举：
    ```c
    typedef enum {
        SIM_NETIF_EVT_DOWN = 0,
        SIM_NETIF_EVT_UP   = 1,
    } sim_netif_event_t;
    typedef void (*sim_netif_event_cb_t)(esp_netif_t *netif, sim_netif_event_t event, void *user_ctx);
    int  sim_network_broker_register_cb(sim_netif_event_cb_t cb, void *user_ctx);
    void sim_network_broker_unregister_cb(sim_netif_event_cb_t cb);
    ```
- [ ] **任务 T3.2**：实现网络断开 $\to$ 上层客户端级联联动：
  - 当 Wi-Fi STA 接口掉线时，精确联动绑定在该 Netif 上的 MQTT 客户端抛出 `MQTT_EVENT_DISCONNECTED` 并进入重试状态机；
  - 支持客户端注销时的回调解绑，并在复位流程中回写注销，杜绝野指针回调；
- [ ] **任务 T3.3**：端到端无头时序实证回归：
  - 执行 `powershell ./wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1 -App wifi_sta`；
  - 验证虚拟时钟、因果事件先后序与无头实证场景全绿通过。

---

## 三、 风险评估与回滚方案 (Risk & Rollback Matrix)

| 风险项 | 触发场景 | 预防与缓解措施 | 回滚操作 (Rollback Action) |
|---|---|---|---|
| **R-01 定时器回调侵占调度器** | 业务在 Timer 回调中执行长耗时操作 | 严格限制内部驱动仅投递工作项，在技术设计阶段做硬性边界审查 | 退回技术设计阶段重新校对纤程切出模型 |
| **R-02 多 MQTT 客户端并发死锁** | 实例下沉改造时锁竞争或槽位泄漏 | 严格单测覆盖并发启停流程，在 `esp_mqtt_destroy` 处全面审计清理链路 | `git checkout -- wink-micro-os/frameworks/esp_idf/src/network/esp_mqtt.c` |
| **R-03 幽灵事件回潮** | Wi-Fi 断链时取消定时器未失效在途事件 | 采用代际 Token 与原子标志双校验，确保取消瞬间之后到达的回调一律被丢弃 | 回退连接状态机重构分支 |

---

## 四、 全局验收标准 (Definition of Done)

1. **DoD-1（前置技术设计硬门禁通过）**：
   - 《定时器与网络出口架构技术设计规格》物理落盘并获得架构评审通过；
2. **DoD-2（定时器交付矩阵与 LITE 容量验证）**：
   - FreeRTOS Timer 交付矩阵中的 7 项核心 API 全部绿灯，未实现项显式不支持；
   - LITE 配置（8 任务槽）下单测验证通过，证明 Timer Daemon 稳定运行且不挤垮业务任务；
3. **DoD-3（复位回写与幽灵事件拦截）**：
   - 新增组件完成 `esp_idf_bridge.c` 复位 DAG 回写；
   - 定时器即将触发瞬间强行重启测试 100% 绿灯，旧工作项零泄漏、基线干净；
4. **DoD-4（临时纤程清零与多客户端并发安全）**：
   - Wi-Fi 与 MQTT 临时纤程彻底清零；
   - MQTT 实例状态完成下沉，多客户端并发连接单测全绿，无 Token 竞争覆盖；
5. **DoD-5（无头因果时序与全量 CTest 回归）**：
   - `run_esp32_headless_evidence.ps1 -App wifi_sta` 实证通过；
   - 全量 `ctest -L esp_idf` 保持 100% 全绿通过（保存完整执行日志）。
