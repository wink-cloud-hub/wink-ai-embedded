# ESP-IDF 仿真 Phase 4：模块级 Wasm 彻底热重启与跨实例序号交接技术设计规格

| 字段 | 内容 |
|---|---|
| 状态 | **已实现并验证（Accepted，2026-09-28）** |
| 日期 | 2026-09-28 |
| 范围 | `wink-micro-os/frameworks/esp_idf/`、`targets/wasm/`、`runtime_ctor_app` 及 Wasm 模块运行时生命周期 |
| 关联实施计划 | [ESP-IDF 仿真基建加固 Phase 4](../../../implementation-plans/esp32/2026-09-28-esp-idf-simulation-hardening-plan.md) |
| 关联决策与设计 | [ADR-0012 契约诚实](../../../decisions/core/0012-contract-honesty-over-silent-degradation.md)、[ADR-0082 复位语义](../../../decisions/core/0082-mcs51-reset-semantics-fiber-exit-and-reentry.md)、[H6 代际句柄令牌原型与集成边界](2026-09-28-esp-idf-h6-generational-handle-spike.md) |
| 关联测试证据 | `esp_idf_wasm_runtime_ctor`（多实例真实 Node 实例化）、Host `test_esp_idf_freertos` |

---

## 1. 背景与核心问题（Context & Problem）

在现有的仿真体系中，系统的软复位（Soft Reset，由 H2/B3 落地）通过 `pal_wasm_clear_pending_reset`、`pal_wasm_reset_app_state` 和各模块的 `_reset()` 函数完成。这种软复位发生在**同一个 WebAssembly.Instance 及其线性内存内部**。

然而，在 ESP32 物理硬件上，系统发生硬件复位（WDT、SWRST、上电或 Deep-sleep 唤醒）时，执行的是真正的冷启动/物理重启：
1. **内存映像重置**：全部片上 SRAM 重新根据 Flash 映像初始化，`.data` 段被重新拷贝，`.bss` 段被完全清零；
2. **C++ 全局构造函数重执行**：全局/静态对象的构造函数（`__wasm_call_ctors`）重新执行，重新初始化静态资源；
3. **动态堆内存彻底清除**：所有先前 `malloc` 的堆内存不复存在，零内存泄漏或碎片残留；
4. **外部异步闭包隔离**：前一运行周期所遗留的宿主闭包、定时器与事件监听器彻底失效。

在 WebAssembly 环境中，单纯的内部内存清理无法重跑静态构造函数，也无法保证修改过的全局/静态变量彻底恢复初始值。因此，必须引入 **Phase 4 模块级彻底热重启机制**：由宿主（Unisim TS Runner 或 Node 测试环境）彻底销毁旧的 `WebAssembly.Instance`，并重新调用 `instantiate()`（或 Emscripten `createModule()`）创建全新的实例。

### 核心挑战：跨实例 ABA 悬挂指针与序号交接

当销毁旧 Wasm 实例并重新实例化新 Wasm 实例时，若新实例的句柄分配序号从 0 开始计数：
- 旧实例曾经发放给外部持有者（如宿主前端 UI、数据流分析器或外部 RPC 客户端）的句柄（如 Queue、Task、Semaphore、GPTimer、NVS 句柄）若未被完全同步回收；
- 新实例创建静态资源（如 C++ 全局构造函数创建的静态队列）或首批动态资源时，将重新从序号 1、2... 开始发放令牌；
- 这将导致新旧两个完全不同生命周期的实例之间的句柄发生**代际序号碰撞与槽位别名（Cross-Instance ABA Hazard）**。

因此，Phase 4 必须建立**跨实例单调序号交接（Cross-Instance Sequence Handover）契约**，确保句柄代际单调性在模块销毁与重新实例化过程中绝对保持。

---

## 2. 架构设计与生命周期模型（Architecture & Lifecycle）

```
 +-----------------------------------------------------------------------+
 |                     Host Environment (Unisim / Node)                  |
 +-----------------------------------------------------------------------+
         |                                                   ^
  [1. Running]                                               |
         |                                                   |
         v                                                   |
 +-------------------+                                       |
 | Wasm Instance 1   |                                       |
 | (Memory 1, Gen N) |                                       |
 +-------------------+                                       |
         |                                                   |
   [2. Reset Req] -> pal_wasm_has_pending_reset() == 1       |
         |                                                   |
   [3. Handover]  -> seq1 = _esp_sim_handle_get_sequence() --+
         |                                                   |
   [4. Teardown]  -> Terminate & Release Instance 1          |
                                                             |
                                                             v
 +-----------------------------------------------------------------------+
 | [5. Fresh Re-instantiation] createModule({ initialSequenceBase: seq1 })|
 +-----------------------------------------------------------------------+
         |
         +---------------------------------------+
         |                                       |
         v                                       v
 [Wasm Engine Loading]              [Native __wasm_call_ctors]
         |                                       |
   Load clean .wasm                        Read initialSequenceBase (seq1)
   Fresh Memory 2                          via inline EM_JS probe
   Zero .bss, Reset .data                  s_next_sequence = seq1
         |                                       |
         |                          Global Ctors Create Resources
         |                          Tokens get: seq1 + 1, seq1 + 2...
         |                                       |
         v                                       v
 +-----------------------------------------------------------------------+
 | Wasm Instance 2 (Memory 2, Fresh State, Base Sequence = seq1)         |
 |                                                                       |
 | - Stale Tokens from Instance 1 (seq <= seq1) STRICTLY REJECTED        |
 | - Fresh Dynamic Handles strictly monotonic (seq > seq1)               |
 | - All C++ Constructor resources fresh and functional                  |
 +-----------------------------------------------------------------------+
```

---

## 3. 跨实例序号交接协议（Handover Protocol）

### 3.1 导出与导入 ABI 契约

1. **导出查询接口**：
   - `EMSCRIPTEN_KEEPALIVE uint32_t esp_sim_handle_get_sequence(void)`：读取当前实例发放句柄的最高单调序号。软复位不清零该序号。
2. **导入启动注入接口（零时延构造期感知）**：
   - 为避免 Emscripten 在 `preRun` 阶段调用本地函数触发的断言拦截，利用 Emscripten 内联 `EM_JS` 机制在首次执行 `esp_sim_handle_issue` 时直接向宿主 `Module['initialSequenceBase']` 探针读取交接基准；
   - 保证甚至在 `__wasm_call_ctors` 执行 C++ 全局构造函数之前，`s_next_sequence` 就已经无缝对齐 `seq1`。
3. **运行时显式设置接口**：
   - `EMSCRIPTEN_KEEPALIVE void esp_sim_handle_set_sequence_base(uint32_t base)`：允许宿主在运行时随时调整单调基准，用于热插拔或多阶段跳变。

### 3.2 单调性不变量（Monotonic Invariant）

对于任何连续的 Wasm 实例集合 $M_1, M_2, \dots, M_k$：
$$\text{Base}(M_{i+1}) \ge \text{MaxSeq}(M_i)$$
$$\forall t \in \text{Tokens}(M_{i+1}), \quad \text{Sequence}(t) > \text{Base}(M_{i+1}) \ge \text{Sequence}(t_{\text{stale}}), \quad \forall t_{\text{stale}} \in \text{Tokens}(M_i)$$

该数学不变量确保：**旧实例中创建的任何句柄，其序号位必定严格小于新实例中创建的任何句柄**。

---

## 4. 全家族失效隔离验证矩阵（Rejection Matrix）

在模块重建后，旧句柄传入新实例的 API 时，句柄解析器执行严格的双重核对：
1. 校验令牌槽位与家族合法性；
2. 比对静态池对象当前存活令牌与传入令牌：`candidate->token != raw_token`（因序号位不同，绝对不匹配）。

| 家族 | 传入 Instance 1 的失效句柄 | Instance 2 执行操作 | 预期行为与返回值 | 验证证据 |
|---|---|---|---|---|
| **Queue** | `q_stale` | `xQueueSend` / `xQueueReceive` / `xQueuePeek` | 返回 `pdFALSE`，拒绝写入或读取 | PASS |
| **Semaphore** | `sem_stale` | `xSemaphoreGive` / `xSemaphoreTake` | 返回 `pdFALSE`，拒绝释放或获取 | PASS |
| **EventGroup** | `eg_stale` | `xEventGroupSetBits` / `xEventGroupGetBits` | 返回 0，位掩码未触发 | PASS |
| **Task** | `task_stale` | `eTaskGetState` / `uxTaskPriorityGet` | `eTaskGetState` 返回 `eDeleted`，优先级返回 0 | PASS |
| **GPTimer** | `timer_stale` | `gptimer_start` / `gptimer_stop` / `get_raw_count` | 返回 `ESP_ERR_INVALID_ARG`，硬件定时器未响应 | PASS |
| **NVS** | `nvs_stale` | `nvs_get_i32` / `nvs_set_i32` / `nvs_commit` | 返回 `ESP_ERR_INVALID_ARG`，只读存储安全 | PASS |

---

## 5. 序号耗尽与边界保护（Ceiling & Exhaustion）

令牌的序号空间为 21 位（最大值 `ESP_SIM_HANDLE_MAX_SEQUENCE = (1 << 21) - 1 = 2,097,151`）。

1. **拒绝回绕原则（No Wrap-Around）**：
   - 当 `s_next_sequence >= ESP_SIM_HANDLE_MAX_SEQUENCE` 时，`esp_sim_handle_issue()` 强制返回 0；
   - 序号计数器锁定在最大值，绝不发生 `0` 或 `1` 的回绕；
2. **Fail-Loud 资源创建**：
   - 达到上限后，`xQueueCreate` 返回 `NULL`，`nvs_open` 返回 `ESP_ERR_NVS_NOT_ENOUGH_SPACE`，`gptimer_new_timer` 返回 `ESP_ERR_NO_MEM`；
   - 彻底杜绝跨周期回绕导致的代际碰撞。

---

## 6. 验证证据清单

1. **Host 独立单元测试**：
   - `test_esp_idf_freertos.c` 新增 `test_freertos_cross_instance_sequence_handover_and_monotonicity`；
   - 覆盖模拟多实例复位、跨实例序号交接、失效句柄拒绝、单调序号递增和 `MAX_SEQUENCE` 耗尽保护（40/40 PASS）。
2. **Wasm 真实 Node 多实例生命周期测试**：
   - `esp_idf_wasm_runtime_ctor` 驱动真实编译生成的 `wink_simulator.js`：
     - **Instance 1**：启动、消费 C++ 全局构造资源、执行堆契约与 H4 同刻测试、导出 8 类句柄，捕获末尾序号 `seq1`，触发热重启请求；
     - **销毁 Instance 1**：解除引用模拟宿主资源回收；
     - **Instance 2**：传入 `initialSequenceBase: seq1` 实例化，验证静态构造与 `app_main` 全新重跑，验证 8 类旧句柄全部被拒绝，验证新句柄单调递增并正常工作；
     - **Instance 3**：验证极限边界耗尽拒绝与防回绕机制；
     - 输出 `ESP_IDF_WASM_PHASE4_HOT_RESTART_OK`。
3. **全量门禁与静态合规**：
   - ESP-IDF 专项 CTest 全部 84/84 项（31 Host + 8 corpus + 35 Wasm + 10 vendor）100% PASS；
   - 静态检查：`check_license_map.py` 合规、`check_harvested_headers.py` 0 errors、`winkcli lint --pack layering --pack api --pack wasm` 0 findings。
