<!-- SPDX-License-Identifier: Apache-2.0 -->
# 【已废弃】实施计划：ESP-IDF 跨靶仿真基建深水区大重构与微内核演进战役

> ⚠️ **废弃声明 (2026-09-30)**：
> 本计划草案因**范围杂糅**（一次性杂糅头文件治理、对象池、复位、定时器、网络与跨宿主网关 ABI 六类变更）、**存在与既有代码冲突的臆测**（如重复定义已有的 `esp_err_from_wink`、已有的 `esp_freertos_assert_not_in_critical`、忽视既有 `wink_soft_timer` 与既有 Profile 配置入口），经白盒架构深度评审后**正式予以废弃**。
>
> 按照评审结论，本计划已彻底解耦并拆解为三份独立的精细化实施计划，归档于专门目录：
> 👉 **新计划目录**：[`facade-hardening/`](facade-hardening/README.md)
> - 计划 1：[`facade-hardening/01-include-and-build-boundary-plan.md`](facade-hardening/01-include-and-build-boundary-plan.md)（头文件与构建边界治理）
> - 计划 2：[`facade-hardening/02-handle-pool-and-reset-causality-plan.md`](facade-hardening/02-handle-pool-and-reset-causality-plan.md)（错误码审计、GPTimer 句柄池试点与复位因果图）
> - 计划 3：[`facade-hardening/03-timer-and-network-evolution-plan.md`](facade-hardening/03-timer-and-network-evolution-plan.md)（软定时器比选与连接时序解耦）

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20260930-ESP-IDF-MICRO-CORE-DEEP-REFACTOR-DEPRECATED |
| 状态 | ❌ **Deprecated / Abandoned（已废弃，由 facade-hardening/ 独立计划拆解替代）** |
| 日期 | 2026-09-30 |
| 周期估算 | 3~4 个工作日（分外壳防卫、内核构件、驱动瘦身、定时器与网络、高阶防御、全量实证 6 大阶段） |
| 优先次序 | **外层路径与资产硬隔离 → 内联对象池与错误码映射 → 四阶段生命周期注册表 → 虚拟定时器引擎与纤程瘦身 → 多网卡链路总线 → 并发切出守卫与统一网关** |
| 决策依据 | [ADR-0004：编译期静态分发优于运行时函数指针](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)<br>[ADR-0012：契约诚实优于静默降级（Fail-Loud 原则）](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)<br>[ADR-0043：分层门禁规范与 API 边界](../../decisions/core/0043-layering-lint-rules.md)<br>[ADR-0057：PAL 保持对网络与 RF 射频无知](../../decisions/core/0057-pal-adc-subsystem-and-channel-3-analog-contract.md)<br>[ADR-0085：SoC 双 SSOT 仲裁与静态选片](../../decisions/core/0085-esp-idf-facade-soc-caps-vs-pal-caps-dual-ssot.md)<br>[ADR-0087：资产通道登记与数据归属](../../decisions/core/0087-esp-idf-asset-channels-and-soc-data-ownership.md)<br>[ADR-0088：多版本支持策略与触发器切换](../../decisions/core/0088-esp-idf-version-strategy-triggers.md)<br>[ADR-0089：分类记账堆内存与边界防御模型](../../decisions/core/0089-esp-idf-heap-caps-allocation-contract.md)<br>[ADR-0091：多配置实例与五维正交 Schema 架构决策](../../decisions/unisim/0091-esp-idf-multi-config-orthogonal-schema.md) |
| 管辖数据源 | [`channels.json`](../../../wink-micro-os/frameworks/esp_idf/channels.json)、[`esp_idf_sources.cmake`](../../../wink-micro-os/frameworks/esp_idf/esp_idf_sources.cmake)、[`CMakeLists.txt`](../../../wink-micro-os/frameworks/esp_idf/CMakeLists.txt)、[`04-architecture-risks.md`](../../../wink-micro-os/frameworks/esp_idf/docs/04-architecture-risks-and-evolution-solutions.md) |
| 实施目标文件 | `src/core/esp_sim_pool.h` (新建)、`src/core/esp_sim_status.h` (新建)、`src/core/esp_sim_subsystem.h` (新建)、`src/network/sim_link_bus.h/.c` (新建)、`src/core/esp_sim_gateway.h/.c` (新建)、`src/drivers/*`、`src/freertos/*`、`src/wifi/*`、`src/network/*`、`src/esp_idf_bridge.c` |
| 验收安全网 | 87/87 `ctest -L esp_idf`、103/103 Gate 治理门禁、`run_esp32_headless_evidence.ps1` 确定性实证 |

---

## 一、 战略总目标与重构哲学

### 1.1 现状诊断与结构性技术包袱
WinkMicroOS ESP-IDF 仿真拦截层（Axis B）通过一系列增量战役，成功跑通了外设、FreeRTOS、NVS、确定性 Wi-Fi STA/Scan 与 MQTT/Netif 级联。然而，在功能快速堆叠的同时，源码内部沉淀了**七大严重的结构性包袱**：
1. **机械复制的对象池样板**：10 多个驱动与 OS 模块独立手写静态槽位池、Token 防重入与 Handle 解码，造成数百行低级重复代码；
2. **上帝重置对象（God Bridge）**：`esp_idf_bridge.c` 靠硬编码十几个私有 `extern void xxx_sim_reset(void)` 驱动软复位，顺序脆弱且新增模块易遗漏；
3. **滥用 Fiber 纤程实现异步延时**：`freertos_timers.c` 沦为空桩，WiFi/MQTT 为处理 100ms 超时不得不在代码中临时 `xTaskCreate` 启动独立的纤程任务，侵占本就紧缺的静态任务池（8 槽位硬顶）；
4. **网络状态点对点打补丁**：`sim_network_broker` 仅有单一全局布尔值，WiFi 掉线直接粗暴联动 MQTT，无法支撑未来的 SoftAP、APSTA 模式与以太网并发；
5. **错误码断崖式降级**：PAL 细致的负数错误码（如 `WINK_ERR_TIMEOUT`、`WINK_ERR_BUSY`）在门面层被粗暴地用三目运算符降级为裸 `ESP_FAIL`，吞掉了真实的错误语义；
6. **并发假阳性盲区**：单核协作调度（Fiber）掩盖了多核自旋锁与临界区使用不当的致命缺陷（如持有自旋锁期间调用切出函数）；
7. **探针与故障注入碎片化**：各个子系统各自导出专有 Wasm C-ABI 符号，宿主粘合层膨胀且缺乏统一协议。

### 1.2 重构核心哲学：微内核解耦 + 零运行时开销 + 纵深防御
* **代码极度精简，业务逻辑一目了然**：将样板代码全部收敛到内联宏与基础头，各驱动仅关注“原厂 C-ABI 到 PAL API”的直接桥接，单文件代码量缩减 40%~60%；
* **无多余的动态封装与 C++ 虚表**：坚持 C99 编译期静态分发，内存零堆分配（0 malloc），保证仿真环境的 100% 确定性；
* **纵深防御（Defense-in-Depth）**：引入并发临界区切出守卫、复位期内存与资源泄漏自检，将死锁与脏状态截断在仿真执行期；
* **利用坚固的安全网重构**：重构每一步，都有现存的 87 个 CTest、103 个静态治理门禁和无头时序实证全程护航，确保重构“改骨架而不损功能”。

---

## 二、 终极重构架构全景拓扑 (Micro-Core & High-Assurance Architecture)

```
+───────────────────────────────────────────────────────────────────────────────────────────────+
|                  WinkMicroOS ESP-IDF 仿真微内核全景架构 (Micro-Core Blueprint)                |
+───────────────────────────────────────────────────────────────────────────────────────────────+
|  [外部应用透明层]   driver/gpio.h, esp_wifi.h, freertos/task.h, mqtt_client.h (100% 乐鑫原厂 C-ABI)|
+───────────────────────────────────────────────────────────────────────────────────────────────+
|                                              ▲                                                |
|                                              │ (无缝同源编译，零 API 破窗)                    |
+──────────────────────────────────────────────┴────────────────────────────────────────────────+
|  [仿真微内核核心基础设施] (零开销内联头，纯 C99 静态分发)                                        |
|  ├─ esp_sim_pool.h       : 泛型静态对象池宏 (自动生成静态槽位/Token/resolve/alloc/free/reset)   |
|  ├─ esp_sim_status.h     : wink_status_t <-> esp_err_t 无损双向保真映射表                     |
|  ├─ esp_sim_subsystem.h  : 四阶段受控生命周期注册表 (Phase 0 协议 -> 1 空口 -> 2 驱动 -> 3 调度)|
|  └─ esp_sim_sanity.h     : 复位健全性自检器 (校验未释放定时器/未关闭任务/未释放堆配额)         |
+──────────────────────────────────────────────┬────────────────────────────────────────────────+
|                                              ▼                                                |
+───────────────────────────────────────────────────────────────────────────────────────────────+
|  [统一异步、并发与链路引擎]                                                                   |
|  ├─ 确定性虚拟定时器引擎 (Virtual Timer Engine) : 接入虚拟时钟，驱动 freertos_timers & esp_timer|
|  ├─ 并发临界区切出守卫 (Yield Guard)            : 持有自旋锁/临界区时切出立即断言拦截 (防死锁)|
|  ├─ 多网卡虚拟链路总线 (sim_link_bus)           : 抽象 L2 Link 状态，解耦 WiFi/SoftAP 与 MQTT  |
|  └─ 统一控制网关 (esp_sim_gateway)              : 统一宿主状态探针 (Inspect) 与故障注入 (Fault)|
+──────────────────────────────────────────────┬────────────────────────────────────────────────+
|                                              ▼                                                |
+───────────────────────────────────────────────────────────────────────────────────────────────+
|  [跨靶底层抽象] Wink PAL (Hardware HAL)  +  Wink Sim Scheduler (Fiber Context)               |
+───────────────────────────────────────────────────────────────────────────────────────────────+
```

---

## 三、 八大核心重构支柱详细设计 (Deep Architectural Pillars)

### 支柱 1：引入 `esp_sim_pool.h` —— 消除全量驱动的机械样板代码
* **重构方案**：新建 `src/core/esp_sim_pool.h`，实现类型安全的宏驱动静态槽位池：
  ```c
  #define ESP_SIM_DEFINE_STATIC_POOL(T, CAP, KIND) \
      static T s_pool_##T[CAP]; \
      static inline T* resolve_##T(uint32_t handle) { \
          uint32_t slot = 0; \
          if (!esp_sim_handle_decode((const void*)(uintptr_t)handle, KIND, CAP, &slot)) return NULL; \
          T *item = &s_pool_##T[slot]; \
          return (item->in_use && item->token == handle) ? item : NULL; \
      } \
      static inline T* alloc_##T(uint32_t *out_handle) { \
          for (uint32_t i = 0; i < (CAP); i++) { \
              if (!s_pool_##T[i].in_use) { \
                  uint32_t token = esp_sim_handle_issue(KIND, i); \
                  if (!token) return NULL; \
                  memset(&s_pool_##T[i], 0, sizeof(T)); \
                  s_pool_##T[i].in_use = true; \
                  s_pool_##T[i].token = token; \
                  if (out_handle) *out_handle = token; \
                  return &s_pool_##T[i]; \
              } \
          } \
          return NULL; \
      } \
      static inline void free_##T(T *item) { \
          if (item) { item->in_use = false; item->token = 0; } \
      } \
      static inline void reset_all_##T(void) { \
          for (uint32_t i = 0; i < (CAP); i++) free_##T(&s_pool_##T[i]); \
      }
  ```
* **收益对比**：`esp_i2c_master.c` 从 215 行减少至约 85 行；`esp_gptimer.c` 从 190 行减少至 80 行；代码只保留纯粹的硬件配置与 PAL 数据流传输。

---

### 支柱 2：引入 `esp_sim_status.h` —— 消除错误码断崖式降级
* **重构方案**：新建 `src/core/esp_sim_status.h`，提供双向保真映射：
  ```c
  static inline esp_err_t esp_err_from_wink(wink_status_t st) {
      switch (st) {
          case WINK_OK:                  return ESP_OK;
          case WINK_ERR_INVALID_ARG:     return ESP_ERR_INVALID_ARG;
          case WINK_ERR_TIMEOUT:         return ESP_ERR_TIMEOUT;
          case WINK_ERR_NOT_FOUND:       return ESP_ERR_NOT_FOUND;
          case WINK_ERR_NO_MEMORY:       return ESP_ERR_NO_MEM;
          case WINK_ERR_NOT_SUPPORTED:   return ESP_ERR_NOT_SUPPORTED;
          case WINK_ERR_BUSY:            return ESP_ERR_INVALID_STATE;
          default:                       return ESP_FAIL;
      }
  }
  ```
* **收益对比**：所有驱动调用单行透传：`return esp_err_from_wink(pal_i2c_transfer_timeout(...));`。错误码具备真实语义，杜绝将超时或资源忙粗暴掩盖为裸 `ESP_FAIL`。

---

### 支柱 3：有序四阶段生命周期与复位泄漏自检（`esp_sim_subsystem.h` & `esp_sim_sanity.h`）
* **重构方案**：
  1. 定义四阶段受控生命周期：
     ```c
     typedef enum {
         ESP_SIM_PHASE_PROTOCOL = 0, // Phase 0: 断开上层协议 (HTTP, MQTT, BLE)
         ESP_SIM_PHASE_EVENT    = 1, // Phase 1: 空口下线与排空事件循环 (WiFi, Event Loop)
         ESP_SIM_PHASE_DRIVER   = 2, // Phase 2: 外设复位与 PAL 硬件归还 (I2C, SPI, UART, LEDC, GPIO)
         ESP_SIM_PHASE_OS       = 3  // Phase 3: 调度与 OS 资源池复位 (FreeRTOS Tasks, Semaphores, Pools)
     } esp_sim_phase_t;
     ```
  2. 增设 **Reset Leak Sanity Checker**：在 Phase 3 执行完成后，自动核验系统清洁度：
     ```c
     void esp_sim_subsystem_assert_clean_state(void) {
         assert(freertos_timer_get_active_count() == 0 && "Active timers leaked after reset!");
         assert(freertos_task_get_active_count() <= 1 && "Zombie tasks leaked after reset!");
         assert(esp_heap_caps_get_allocated_bytes() == 0 && "Dynamic heap memory leaked after reset!");
     }
     ```
* **收益对比**：彻底解耦模块间依赖，新增驱动无需修改 `esp_idf_bridge.c`；CI 环境下连续跑 1000 次复位用例零状态残留。

---

### 支柱 4：实现统一的确定性虚拟软件定时器（Virtual Clock Timer Engine）
* **重构方案**：
  1. 激活 `src/freertos/freertos_timers.c`，使用基于静态槽位池管理软件定时器；
  2. 定时器心跳直接挂接 Wink 纤程调度器的虚拟时钟（Virtual Tick）；
  3. 重构 `src/wifi/esp_wifi.c`：彻底移除 `wifi_connect_task` 临时纤程，改用轻量级虚拟软件定时器驱动三阶段状态机；
  4. 重构 `src/network/esp_mqtt.c`：心跳检测与断链重试全面转为定时器回调。
* **收益对比**：Fiber 纤程总数压降至最低，STA 连接与重试转变为纯事件回调驱动，根除任务并发悬挂风险。

---

### 支柱 5：多网卡虚拟链路总线（`src/network/sim_link_bus.h`）
* **重构方案**：
  新建 `sim_link_bus.h/.c`，将原先针对 WiFi STA 硬编码的 `sim_network_broker` 演进为通用 L2 链路总线：
  ```c
  typedef struct {
      esp_netif_t *netif;
      esp_netif_ip_info_t ip_info;
      bool link_up;
      bool has_ip;
  } sim_link_state_t;

  void sim_link_bus_notify(esp_netif_t *netif, bool link_up, const esp_netif_ip_info_t *ip_info);
  bool sim_link_bus_is_netif_ready(esp_netif_t *netif);
  ```
* **收益对比**：从根本上解耦 WiFi 驱动与上层应用协议；完美支撑 SoftAP、APSTA 模式及以太网多网卡并发。

---

### 支柱 6：并发临界区切出守卫（Critical Section Yield Guard - 防死锁假阳性）
* **重构方案**：
  在 `src/freertos/freertos_spinlock.c` 与任务让步函数中设置安全断言屏障：
  ```c
  void esp_freertos_assert_can_yield(void) {
      if (esp_freertos_in_critical_section()) {
          pal_log_e("RTOS_PANIC", "FATAL: Task attempted to yield while holding critical section!");
          assert(false && "Yield inside critical section violates SMP contract! Deadlock guaranteed on ESP32.");
      }
  }
  ```
* **收益对比**：彻底截断单核仿真协作调度对“多核自旋锁死锁隐患”的掩盖，代码在仿真中一旦出现非法切出立刻 Fail-Loud 暴露。

---

### 支柱 7：统一探针与故障注入网关（Unified Sim Control Gateway）
* **重构方案**：
  新建 `src/core/esp_sim_gateway.h/.c`，收敛各模块向 Wasm/宿主导出的离散 C-ABI：
  ```c
  EMSCRIPTEN_KEEPALIVE int esp_sim_gateway_inject(const char *target, const char *payload_json);
  EMSCRIPTEN_KEEPALIVE int esp_sim_gateway_inspect(const char *target, char *out_buf, size_t max_len);
  ```
* **收益对比**：Wasm 导出符号永不随外设扩增而膨胀；宿主无头运行器（UniSim Runner）使用统一的 DSL 驱动故障注入与状态断言。

---

### 支柱 8：容量分级参数化矩阵（Profile Capacity SSOT - ISSUE-03）
* **重构方案**：
  在 `esp_idf_sources.cmake` 中提供集中参数化宏，消除 C 源码中的硬编码数字：
  ```cmake
  # Standard Profile Defaults
  set(WINK_SIM_MAX_I2C_DEVICES 8 CACHE STRING "Max I2C devices")
  set(WINK_SIM_MAX_EVENT_HANDLERS 16 CACHE STRING "Max event handlers")
  set(WINK_SIM_MAX_EVENT_QUEUE 32 CACHE STRING "Event queue capacity")
  ```
* **收益对比**：复杂大型应用扩容时，只需在工程 CMake 中调整 Profile 变量，无需直接修改门面 C 代码。

---

## 四、 详细实施路线图与任务拆分 (Execution Roadmap)

### 阶段 0：外层边界硬隔离与资产闭环 (Phase 0: Boundary & Asset Guard)
- [ ] **任务 T0.1**：拆分 `esp_idf_sources.cmake`，将 `src/freertos`、`src/core`、`src/wifi`、`src/network` 设为 `PRIVATE` include；
- [ ] **任务 T0.2**：修改 `CMakeLists.txt`，保证应用程序 Target 无法越界引用内部实现头；
- [ ] **任务 T0.3**：更新 `channels.json` Schema（补充属性标签），对齐 `include/README.md` 中的 42 个手写头统计，杜绝文档漂移；
- [ ] **任务 T0.4**：迁移 `api-coverage-matrix.inc.md` 至 `docs/`，净化 `include/` 闭包目录；
- [ ] **任务 T0.5**：会签并转正 `ADR-0088` 为 `Accepted`。

### 阶段 1：微内核基础构件落地 (Phase 1: Micro-Core Primitives)
- [ ] **任务 T1.1**：新建 `src/core/esp_sim_status.h`，实现 `esp_err_from_wink()` 映射；
- [ ] **任务 T1.2**：新建 `src/core/esp_sim_pool.h` 静态泛型对象池宏，实现 Token 防撞与安全解码；
- [ ] **任务 T1.3**：新建 `src/core/esp_sim_subsystem.h` 四阶段复位注册表与 `esp_sim_sanity.h` 泄漏自检；
- [ ] **任务 T1.4**：重构 `src/esp_idf_bridge.c`，采用注册表遍历替代硬编码的 `extern void xxx_sim_reset(void)`。

### 阶段 2：全量驱动瘦身与错误码透传 (Phase 2: Driver Streamlining)
- [ ] **任务 T2.1**：重构 `src/drivers/esp_gptimer.c`，改用 `esp_sim_pool` 与 `esp_err_from_wink`；
- [ ] **任务 T2.2**：重构 `src/drivers/esp_i2c_master.c` 与 `esp_i2c_legacy.c`，消除 120+ 行重复样板；
- [ ] **任务 T2.3**：重构 `src/drivers/esp_spi.c`、`esp_uart.c`、`esp_ledc.c`；
- [ ] **任务 T2.4**：重构 `src/core/esp_nvs.c`，统一句柄池管理；
- [ ] **任务 T2.5**：全量执行 `ctest -L esp_idf`，确保全部驱动单测 100% 通过。

### 阶段 3：确定性虚拟软件定时器与纤程瘦身 (Phase 3: Virtual Timers & Fiber Pruning)
- [ ] **任务 T3.1**：在 `src/freertos/freertos_timers.c` 落地静态定时器管理引擎，挂接虚拟调度 Tick；
- [ ] **任务 T3.2**：实现 `xTimerCreate`、`xTimerStart`、`xTimerStop`、`xTimerReset` 等核心 FreeRTOS API；
- [ ] **任务 T3.3**：重构 `src/wifi/esp_wifi.c`，移除 `wifi_connect_task` 临时纤程，改由虚拟定时器驱动三阶段状态机；
- [ ] **任务 T3.4**：重构 `src/network/esp_mqtt.c` 心跳检测逻辑，改用虚拟定时器驱动。

### 阶段 4：多网卡虚拟链路总线与网络解耦 (Phase 4: Multi-Netif Link Bus)
- [ ] **任务 T4.1**：新建 `src/network/sim_link_bus.c/.h`，升级现有 `sim_network_broker` 为网卡链路总线；
- [ ] **任务 T4.2**：重构 `src/wifi/esp_netif.c` 与 `esp_wifi.c` 对接 `sim_link_bus`；
- [ ] **任务 T4.3**：解耦 `esp_mqtt.c`，使其绑定指定 netif 实例感知网络就绪。

### 阶段 5：高阶防御加固与统一控制网关 (Phase 5: Advanced Assurance & Gateway)
- [ ] **任务 T5.1**：在 `freertos_spinlock.c` 与任务切出点部署 `esp_freertos_assert_can_yield()` 临界区切出守卫；
- [ ] **任务 T5.2**：在每次测试退出或软复位时启用 `esp_sim_subsystem_assert_clean_state()` 资源泄漏自检；
- [ ] **任务 T5.3**：新建 `src/core/esp_sim_gateway.c/.h`，实现统一状态探针与故障注入 C-ABI；
- [ ] **任务 T5.4**：在 `esp_idf_sources.cmake` 注入正交容量 Profile 变量。

### 阶段 6：全量门禁、回归测试与端到端无头实证 (Phase 6: Full Verification)
- [ ] **任务 T6.1**：运行 Gate 5 门禁：`pytest wink-micro-app/vendor/esp_idfv61/.governance/gates/tests/ -v`（103/103 保持通过）；
- [ ] **任务 T6.2**：全量执行 `ctest -L esp_idf`（87/87 保持通过）；
- [ ] **任务 T6.3**：运行无头实证脚本：`powershell run_esp32_headless_evidence.ps1 -App wifi_sta` 验证虚拟时钟与断言 100% 吻合。

---

## 五、 全局验收标准 (DoD) 与重构量化指标

1. **DoD-1（代码精简量化）**：`src/drivers/` 目录下的样板代码行数缩减 **35% 以上**；
2. **DoD-2（句柄管理统一）**：全框架 100% 外设与 OS 原语统一由 `esp_sim_pool.h` 分配，杜绝裸数组查找；
3. **DoD-3（生命周期解耦）**：`esp_idf_bridge.c` 不包含任何具体模块的私有 `extern void` 声明；
4. **DoD-4（纤程配额健康）**：STA 连接与 MQTT 心跳不再创建临时 Fiber 任务，空闲时调度器纤程数降至最低；
5. **DoD-5（构建边界硬隔离）**：外部应用程序 target 无法直接 `#include "sim_wifi_env.h"` 或 `freertos_internal.h`；
6. **DoD-6（并发与复位安全性）**：
   - 持有临界区自旋锁期间切出 100% 触发断言红灯拦截；
   - 软复位后残留未释放定时器/任务/堆内存 100% 触发自检断言；
7. **DoD-7（零功能回退保障）**：
   - 现存全部 87 个 CTest 用例（`ctest -L esp_idf`）保持 100% 绿灯；
   - 103 个 Gate 治理门禁 100% 通过；
   - 官方示例无头执行轨迹与时间戳（如 `netif:sta:ip == 192.168.1.100` @ 1500000us）与重构前严格一致。
