<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划 02：全门面驱动生命周期与错误语义审计、复位因果图固化与并发切出守卫计划

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20260930-ESP-IDF-LIFECYCLE-RESET-CRITICAL-v1.3 |
| 状态 | 📋 **Draft / Planned（待计划 01 验收后正式启动）** |
| 日期 | 2026-09-30 |
| 周期估算 | 1.5~2 个工作日 |
| 优先次序 | **错误语义精细审计与故障注入 → GPTimer 及外设生命周期与异常回滚缺口审计 → 复位完整因果图 (含 Netif/Broker) 与复位前 Delta 记账 → 阻塞切出点全梳理与临界区守卫补齐** |
| 决策依据 | [ADR-0004：编译期静态分发优于运行时函数指针](../../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)<br>[ADR-0012：契约诚实优于静默降级（Fail-Loud 原则）](../../../decisions/core/0012-contract-honesty-over-silent-degradation.md)<br>[ADR-0085：SoC 双 SSOT 仲裁与静态选片](../../../decisions/core/0085-esp-idf-facade-soc-caps-vs-pal-caps-dual-ssot.md)<br>[ADR-0089：分类记账堆内存与边界防御模型](../../../decisions/core/0089-esp-idf-heap-caps-allocation-contract.md) |
| 管辖数据源 | [`src/core/esp_err.c`](../../../../wink-micro-os/frameworks/esp_idf/src/core/esp_err.c)、[`src/drivers/esp_gptimer.c`](../../../../wink-micro-os/frameworks/esp_idf/src/drivers/esp_gptimer.c)、[`src/core/esp_sim_handle.c`](../../../../wink-micro-os/frameworks/esp_idf/src/core/esp_sim_handle.c)、[`src/esp_idf_bridge.c`](../../../../wink-micro-os/frameworks/esp_idf/src/esp_idf_bridge.c)、[`src/freertos/freertos_spinlock.c`](../../../../wink-micro-os/frameworks/esp_idf/src/freertos/freertos_spinlock.c)、[`src/core/esp_nvs.c`](../../../../wink-micro-os/frameworks/esp_idf/src/core/esp_nvs.c) |
| 实施目标文件 | `src/core/esp_err.c`、`src/drivers/*`、`src/esp_idf_bridge.c`、`src/core/esp_heap_caps.c`、`src/freertos/*`、`test/*` |
| 验收门禁 | 驱动生命周期与回滚单测、优雅退出 Delta 零泄漏校验、运行中重启基线重置、临界区阻塞切出负测、全量 CTest 真实回归 |

---

## 一、 计划背景与范围定界

### 1.1 核心定界原则与纠偏要旨
* **生命周期缺口审计取代虚假试点（严禁重复造轮子）**：
  - 白盒核查表明：[`src/drivers/esp_gptimer.c:23`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/drivers/esp_gptimer.c#L23) 已基于真实常量 `PAL_HWTIMERS_MAX` 实现了静态池，并通过 [`esp_sim_handle.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/core/esp_sim_handle.c) 实现了代际 Token 与删除注销；[`test_esp_gptimer.c:115-150`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/test/core/test_esp_gptimer.c#L115) 也已覆盖了容量满、旧句柄拦截和 ABA 测试；本仓不存在虚构的 `SOC_TIMER_GROUP_TOTAL_TIMERS`；
  - 本计划彻底剔除重复造轮子任务，转向**深层生命周期缺口审计**：重点审计底层硬件启动失败时的状态回滚、停止失败时是否保持真实硬件状态、运行中修改 alarm 的原子性竞态、以及复位/删除后未决中断回调的排空防卫与代际失效；以行为合规性和代码复杂度决定是否抽取通用句柄池；
* **错误码契约精细化，拒绝有损机械替换**：
  - 现有 [`esp_err.c:7`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/core/esp_err.c#L7) 的 `esp_err_from_wink()` 是有损的（例如将 `WINK_ERR_BUSY` 映射为 `ESP_ERR_INVALID_STATE`，且多项映射到 `ESP_FAIL`）；
  - 严禁机械地在所有驱动中做全局字符串替换，必须针对每个驱动 API（I2C/SPI/UART/GPIO/GPTimer）的乐鑫原厂契约建立精确映射，并增加故障注入单测验证；
* **复位因果图 (DAG) 补齐与科学断言分流**：
  - 补齐复位调用链中缺失的 `(void)esp_netif_init()`，以及 `esp_wifi_sim_reset()` 间接重置网络 broker 的隐式依赖；在 `esp_idf_bridge.c` 中固化“调度器边界前/后（`sim_scheduler_current_id() == SIM_SCHED_NO_READY`）”的不变量；
  - 修正 NVS 持久化契约：已提交（commit）的 NVS 属于模拟物理 Flash 的持久化数据，虚拟断电或软复位均不得丢失，断电仅清除 RAM 缓存；
  - **科学分流复位断言**：
    1. **应用正常优雅退出（Graceful Shutdown）**：业务自行销毁所有资源，断言复位前活跃内存与任务差额（Delta）为 0；
    2. **应用运行中强行重启（In-flight `esp_restart()`）**：允许复位前持有合法未释放资源，断言复位过程按因果顺序完全释放，复位后新基线干净（活跃为 0），且旧句柄全部失效；
  - 明确相关统计函数（`esp_heap_caps_get_active_allocations()` 与 `esp_freertos_get_active_task_count()`）为本计划待新增的测试观测接口（Test Harness Hooks）；
* **解耦 NVS 数据条目与句柄槽位上限**：
  - 澄清：`CONFIG_NVS_MAX_ENTRIES` 是在 RAM 中分配的 key-value 数据项上限（如 16/32/64 项），并不直接受句柄编码的 64 限制；受 64 槽限制的是 `esp_sim_handle` 管理的 `nvs_handle_t`（当前代码中 `NVS_MAX_HANDLES = 4`）；
* **全面梳理阻塞切出点，补充深层临界区守卫**：
  - 核实确认：`vTaskDelay` 与 `vTaskDelayUntil` 当前已有 `esp_freertos_assert_not_in_critical` 守卫，不搞重复测试；
  - 重点排查全门面所有实际调用 `sim_scheduler_yield_context()` 的阻塞 API（`xSemaphoreTake`、`xQueueSend`、`xQueueReceive`、`xEventGroupWaitBits`、`xTaskNotifyWait` 等）；区分 API 违规切出与调度器内部让步，补齐断言并编写独立负测。

---

## 二、 详细实施任务拆解 (Action Items)

### 阶段 1：错误码映射审计与驱动契约故障注入 (Error Code Precision Audit)

- [ ] **任务 T1.1**：逐驱动梳理错误码映射契约表：
  - 建立驱动错误码映射矩阵：
    | 驱动模块 | 底层 Wink / PAL 状态 | 当前门面返回值 | 原厂期望标准返回值 | 改进措施 |
    |---|---|---|---|---|
    | `esp_i2c` | `WINK_ERR_TIMEOUT` | `ESP_FAIL` | `ESP_ERR_TIMEOUT` | 精准映射超时 |
    | `esp_i2c` | `WINK_ERR_NOT_FOUND` (NACK) | `ESP_FAIL` | `ESP_ERR_NOT_FOUND` | 区分寻址无应答 |
    | `esp_spi` | `WINK_ERR_BUSY` | `ESP_FAIL` | `ESP_ERR_INVALID_STATE` | 避免有损覆盖 |
    | `esp_uart`| 缓冲区满 / 溢出 | `ESP_FAIL` | `ESP_ERR_NO_MEM` / `ESP_FAIL` | 对齐原厂 driver API |
    | `esp_gptimer`| 未配置报警即启动 | `ESP_FAIL` | `ESP_ERR_INVALID_STATE` | 规范状态前置校验 |
- [ ] **任务 T1.2**：按契约精细化重构驱动错误返回：
  - 替换驱动中裸露的 `return (st == WINK_OK) ? ESP_OK : ESP_FAIL;`；
  - 严禁盲目调用单一宏，必须结合 API 文档与上下文语义返回精确错误码；
- [ ] **任务 T1.3**：增加故障注入测试用例（Fault Injection Tests）：
  - 在 `test/drivers/` 相应单测中增加故障注入分支，验证底层返回超时或硬件错误时，门面准确向应用传递期望的乐鑫原厂标准错误码。

---

### 阶段 2：GPTimer 及外设驱动生命周期缺口审计 (Lifecycle Gap Audit)

- [ ] **任务 T2.1**：底层启动与停止异常回滚审计（Rollback on Failure）：
  - 审查 [`src/drivers/esp_gptimer.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/drivers/esp_gptimer.c)：
    - 若 `pal_hwtimer_start()` 返回失败，必须立即将 `t->running` 回退为 `false`，并保持错误码精确向上传递；
    - 若 `pal_hwtimer_stop()` 返回失败，严禁擅自修改内部运行标志，确保软件状态与底层硬件真实状态 100% 同步；
- [ ] **任务 T2.2**：运行中修改 Alarm 与并发竞态审计：
  - 审计 `gptimer_set_alarm_action()` 在 Timer 处于 `running` 状态下的行为：
    - 检查是否需要加锁或在临界区内更新 `alarm_count` 和 `alarm_cb`，防止更新到一半时触发底层 ISR 造成野指针回调；
- [ ] **任务 T2.3**：删除与复位瞬间的悬空回调（Stale ISR Defense）：
  - 审计 `gptimer_del_timer()` 与 `esp_peripherals_reset()`：
    - 确保在清空槽位 `in_use = false` 前，底层 `pal_hwtimer_stop()` 和 `pal_hwtimer_deinit()` 已经完成；
    - 引入代际 Token 校验与回调排空边界，消除在注销瞬间未决的中断回调打入已被复用的新句柄；
- [ ] **任务 T2.4**：编写生命周期缺口回归单测：
  - 在 `test/core/test_esp_gptimer.c` 中新增专门测试：底层启动失败状态回滚测试、运行中动态更新 alarm 测试、删除后中断不泄露测试。

---

### 阶段 3：固化复位因果依赖图 (DAG) 与双断言模型 (Reset DAG & Dual Assertion Model)

- [ ] **任务 T3.1**：因果固化与时序审计 [`src/esp_idf_bridge.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/esp_idf_bridge.c) 完整复位依赖图：
  - 确认既有代码第 84 行的 `(void)esp_netif_init()` 处于正确的拓扑位置，并显式记录与审计 `esp_wifi_sim_reset()` 间接调用 `sim_network_broker_reset()` 的副作用链路；
  - 在 `esp_idf_bridge.c` 源码中固化完整的因果撤回时序图：
    ```
    调度器安全边界断言 (sim_scheduler_current_id() == SIM_SCHED_NO_READY)
         │
         ▼
    1. esp_http_client_sim_reset() / esp_mqtt_sim_reset()  [断开上层连接，排空在途事件]
         │
         ▼
    2. esp_wifi_sim_reset() ───(间接副作用)───► sim_network_broker_reset() [断开空口，重置网络状态]
         │
         ▼
    3. (void)esp_netif_init()                              [重新初始化网络接口基线]
         │
         ▼
    4. esp_nimble_sim_reset()                              [注销 BLE 协议栈与广播]
         │
         ▼
    5. esp_event_loop_sim_reset()                          [排空系统事件队列，注销 sys_evt 纤程]
         │
         ▼
    6. sim_scheduler_reset(0)                              [调度器复位，撤销一切普通纤程]
         │
         ▼
    7. esp_peripherals_reset()                             [释放硬件 PAL，停止所有底层硬件定时器]
         │
         ▼
    8. esp_freertos_pools_reset()                          [最后重置 OS 槽位池与内核对象]
    ```
- [ ] **任务 T3.2**：NVS 持久化状态与 RAM 缓存隔离：
  - 明确 NVS 语义：已提交（`nvs_commit`）的数据代表物理 Flash，虚拟断电或软复位均必须保留；
  - 软复位与虚拟掉电仅丢弃未提交的写缓冲与 RAM 临时上下文，确保其行为与物理 ESP32 芯片完全吻合；
- [ ] **任务 T3.3**：实现测试观测接口与双断言模型（Dual Assertion Model）：
  - 在 `src/core/esp_heap_caps.c` 与 `src/freertos/freertos_task.c` 中导出专用观测钩子：
    ```c
    size_t esp_heap_caps_get_active_allocations(void);
    uint32_t esp_freertos_get_active_task_count(void);
    ```
  - 编写两类专门的复位断言单测：
    - **模式 A（正常退出零泄漏审计）**：App 正常 return/exit 后，断言 `esp_heap_caps_get_active_allocations() == 0`；
    - **模式 B（运行中强行重启审计）**：App 在任务并发运行中强行调用 `esp_restart()`，断言复位完成后 `esp_freertos_get_active_task_count() == 0`，堆内存全部回收，且旧句柄无法操作新系统。

---

### 阶段 4：并发临界区阻塞切出守卫补齐 (Critical Section Yield Guard)

- [ ] **任务 T4.1**：梳理全门面所有阻塞切出点并补齐断言：
  - 排查 `src/freertos/` 中所有直接或间接调用 `sim_scheduler_yield_context()` 的 API：
    - `xSemaphoreTake`（阻塞等待分支：`ticks_to_wait > 0`）；
    - `xQueueSend` / `xQueueReceive`（阻塞等待分支：`ticks_to_wait > 0`）；
    - `xEventGroupWaitBits`（阻塞等待分支：`xTicksToWait > 0`）；
    - `xTaskNotifyWait`（阻塞等待分支：`xTicksToWait > 0`）；
  - 严格确保在触发切出让步前调用 `esp_freertos_assert_not_in_critical(__func__)`；
  - 区分“用户 API 违规切出”与“调度器内部让步”，避免内部正常轮转触发误报；
- [ ] **任务 T4.2**：编写阻塞切出负向测试用例：
  - 在 `test/freertos/test_freertos_spinlock.c` 中增设针对 `xSemaphoreTake` 与 `xQueueReceive` 的负向拦截测试；
  - 验证在持有 `portENTER_CRITICAL()` 期间若强行阻塞等待，测试 Harness 能精准捕获致命断言拦截。

---

### 阶段 5：容量配置校验与既有 Profile 扩展 (Profile Capacity Audit)

- [ ] **任务 T5.1**：校验 [`esp_idf_target.cmake`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/esp_idf_target.cmake) 中的 LITE/STANDARD/PRO 容量与编码上限：
  - 检查 `CONFIG_FREERTOS_MAX_QUEUES`、`CONFIG_FREERTOS_MAX_TASKS` 等与 `esp_sim_handle.c` 的 `MAX_SLOTS = 64` 约束；
  - 澄清并解耦：`CONFIG_NVS_MAX_ENTRIES` 是数据条目数（可在 PRO 配置下设为 128/256），其句柄池 `NVS_MAX_HANDLES = 4` 符合句柄上限；
  - 增加静态断言（编译期检查），确保任何 Profile 配置下的句柄槽位数绝不突破 64 编码上限；
  - 严禁在 `esp_idf_sources.cmake` 制造任何重复的独立 CACHE 变量。

---

## 三、 风险评估与回滚方案 (Risk & Rollback Matrix)

| 风险项 | 触发场景 | 预防与缓解措施 | 回滚操作 (Rollback Action) |
|---|---|---|---|
| **R-01 驱动错误码变更破坏现有测试断言** | 现有某测试写死了 `assert(err == ESP_FAIL)`，替换为 `ESP_ERR_TIMEOUT` 后报错 | 逐文件修改时同步执行对应单测，确认断言语义是否合理；优先核对官方原厂头文件声明的返回值 | 修正单测中的不合理断言或针对该驱动局部回滚 |
| **R-02 状态回退引发既有调度异常** | 硬件启动失败后清理不彻底 | 为每个异常分支建立单测覆盖；确保 `in_use` 与 `running` 状态解耦 | `git checkout -- <modified_driver>` |
| **R-03 临界区守卫在合法代码中误触发** | 原厂某种无阻塞但带锁查询的代码误触发断言 | 精确区分阻塞切出（`ticks_to_wait > 0`）与立即返回查询（`ticks_to_wait == 0`） | 仅在有潜在切出可能性的分支上加装断言 |

---

## 四、 全局验收标准 (Definition of Done)

1. **DoD-1（错误码语义精确度与故障注入）**：
   - 驱动中不再出现无意义的 `(st == WINK_OK) ? ESP_OK : ESP_FAIL`；
   - 超时、参数错误、状态错误等均返回原厂对齐的标准错误码，并有对应故障注入单测验证；
2. **DoD-2（生命周期与异常回滚完备性）**：
   - 底层启动/停止硬件失败时，软件状态 100% 诚实回滚；
   - 动态更新 alarm 具备并发安全性；删除与复位后无悬空中断回调污染；
3. **DoD-3（复位因果图完整性与双断言覆盖）**：
   - `esp_idf_bridge.c` 代码与注释完整覆盖 8 阶段复位因果链（含 Netif 与 Broker 副作用）；
   - 测试 Harness 在优雅退出时断言 Delta 为 0，在并发运行中 `esp_restart()` 时断言重置基线干净且旧句柄失效；
   - NVS 已提交数据在软复位与虚拟断电后完整保留；
4. **DoD-4（阻塞切出守卫覆盖度）**：
   - 所有带阻塞等待的 FreeRTOS 同步原语均加装临界区安全守卫，并在负测中验证拦截；
5. **DoD-5（全量 CTest 真实回归）**：
   - 在宿主环境中执行全量 `ctest -L esp_idf`，保存真实运行日志与退出码，保持 100% 全绿。
