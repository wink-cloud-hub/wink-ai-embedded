# 评审报告：ESP-IDF 仿真拦截层实施计划 M1（FreeRTOS 调度器 Shim 与并发原语）

| 项 | 内容 |
|---|---|
| **评审对象** | [`2026-09-24-esp-idf-sim-m1-freertos-plan.md`](./2026-09-24-esp-idf-sim-m1-freertos-plan.md)（PLAN-20260924-ESP-IDF-SIM-M1 v1.0） |
| **评审日期** | 2026-09-24 |
| **评审视角** | 资深嵌入式/RTOS 内核架构师 + Wink 仿真体系架构师 |
| **评审输入** | ① ESP-IDF v6.1 / v5.1.3 LTS 官方源码与 FreeRTOS 官方 API 行为规范；② 仓库既有协作式单虚拟核调度器与 Fiber 架构（`targets/common/src/wink_sim_scheduler.c`、`sim_ctx`、`pal_osal_host.c`、`pal_osal_wasm.c`）；③ 仿真拦截实施总纲 [`PLAN-20260922-ESP-IDF-SIM-MASTER`](./2026-09-22-esp-idf-simulation-interception-master-plan.md) (v3.5)；④ 嵌入式踩坑清单 `asserts/embedded-bug-list-never.md` |
| **评审方法** | 协程上下文切换流推演、并发等待队列竞态分析、任务与句柄生命周期验证、FreeRTOS 官方行为一致性核对 |
| **评级** | **架构方向 9/10 · 协程切出机制 4/10（存在致命死循环缺陷） · 并发 Waiter 隔离 6/10 · 综合评定：需补齐 P0 切出桥与双向 Waiter 后方可准入** |
| **放置说明** | 按用户要求与计划文档同目录存放；按四层文档体系惯例归档至 Layer ④ 评审记录库，并回写派生计划 |

---

## 一、总体判断

实施计划 M1 v1.0 整体结构严谨、路线清晰，准确吸收了总纲 v3.5 的核心约束：
- 确立了 `((uint32_t)gen << 8) | index` 的 Handle generation 间接层（解决 slot 复用引发的 ABA 隐患）；
- 确立了 `resource_id = (type_tag << 24) | local_index` 的命名空间隔离标签（规避跨对象串扰）；
- 闭环了 M0 遗留的 L2 blink 限界运行与 Replay 确定性断言欠账；
- 结构体尺寸 `_Static_assert` 与静态池 `< 8KB` 锁紧了内存确定性边界。

**然而，若严格按 v1.0 文档开工编码，会立刻陷入 1 个致命架构陷阱（Fatal Trap）与 4 处关键技术遗漏**，将导致仿真系统直接发生**宿主 CPU 无限硬死循环、调度器主循环彻底失去控制权、队列读写死锁**等严重故障。

---

## 二、核心致命缺陷诊断 (P0 阻塞开工项)

### 2.1 Fiber 协程栈切出机制缺失：“标记状态 + 立即返回”的死循环陷阱

#### 1. 计划 v1.0 原文偏差
在原计划 Task M1-2 Step 3（第 255~268 行）和架构红线 11（第 115 行）中描述：
> ```c
> void vTaskDelay(const TickType_t xTicksToDelay) {
>     if (xTicksToDelay == 0) { return; } /* 纯让出：保持 READY，RR 自然轮转，禁 yield_timed(...,0) */
>     uint32_t self = sim_scheduler_current_id();
>     sim_scheduler_yield_timed(self, pal_os_get_us(), (uint64_t)xTicksToDelay * 10000ULL);
>     /* 返回即交回调度主循环（协作契约，见 01 文档） */
> }
> ```
> 红线 11：`一切阻塞 shim = “标记状态 + 立即返回”，禁在门面层自旋等待宿主条件...`

#### 2. 代码事实取证与致命根因分析
Wink 调度器在 Host/Wasm 下是一个**基于用户态 Fiber（Win32 Fiber / Emscripten Fiber）的协程系统**：
1. **状态标记与上下文切换解耦**：[`wink_sim_scheduler.c:180-198`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/targets/common/src/wink_sim_scheduler.c#L180-L198) 的 `sim_scheduler_yield_timed` 和 `sim_scheduler_block` 仅仅修改了内部结构体字段（`s_tasks[id].state = WAITING/BLOCKED` 和 `wakeup_us`），**其本身不包含任何栈切换汇编指令**！
2. **C 语言函数返回的物理事实**：在 C 语言中，`return` 仅仅是弹出当前栈帧并返回调用者。当运行在 Fiber 协程栈上的 `app_main` 执行延时操作时：
   ```c
   void app_main(void) {
       while (1) {
           gpio_set_level(GPIO_NUM_2, 1);
           vTaskDelay(100); /* 若此处仅标记状态并 return! */
           gpio_set_level(GPIO_NUM_2, 0);
           vTaskDelay(100);
       }
   }
   ```
   如果 `vTaskDelay` 只是置状态后 `return;`，控制权**完全没有交回主调度器**，而是直接按 C 语言调用约定顺序返回到 `app_main` 的下一行指令！此时 CPU 仍在同一 Fiber 栈上疯狂硬空转，直到触碰 WCET Watchdog（5000us）触发报错 `8002` 异常中止！
3. **主上下文访问隔离矛盾**：当前代码基线中，切回主调度循环必须执行 `sim_ctx_switch(cur_ctx, s_main_ctx)`。然而 `s_main_ctx` 是 `pal_osal_host.c:192` 与 `pal_osal_wasm.c:127` 内部的 `static` 私有变量。计划 v1.0 声明“只消费公开 API，不改 targets/common”，导致 `freertos/` 门面**物理上无法切出 Fiber 协程栈**！

---

### 2.2 `vTaskDelay(0)` 纯让出真机语义沦为 No-op 漏洞

- **FreeRTOS 规范事实**：`vTaskDelay(0)` 在真机中等价于 `taskYIELD()`，其语义是：任务不进入等待态，保持 `READY` 状态，将当前剩余时间片禅让给同优先级的其他就绪任务。
- **原计划缺陷**：原计划写为 `if (xTicksToDelay == 0) return;`。这导致调用 `vTaskDelay(0)` 的任务没有任何栈切换，直接顺序执行下一行代码，使得让出时间片的契约彻底沦为空操作（No-op），导致同优先级的其他任务在同 Tick 内饥饿。
- **正解**：必须执行协程切出（`sim_scheduler_yield_context()`），任务保持 `SIM_TASK_STATE_READY` 态，调度器主循环的 Round-Robin 算法自然选中下一个就绪任务。

---

## 三、关键并发与句柄模型隐患诊断 (P1 关键设计项)

### 3.1 Queue 单一 Waiter 列表导致读写混杂与死锁风险

- **并发模型事实**：队列（Queue）是双向有界缓冲对象。
  - 队列为空时：Consumer 阻塞，排队等待数据到来；
  - 队列为满时：Producer 阻塞，排队等待空间腾出。
- **原计划缺陷**：Task M1-3 Step 2 仅设计了单一的 `waiters[8]` 结构。若存在一个写满阻塞的 Producer 和一个读空阻塞的 Consumer，`xQueueSend` 写入后若从单一列表中唤醒了队首的 Producer，Producer 尝试写入依然满队，再次陷入阻塞；而本该被唤醒的 Consumer 却永远被跳过，直接导致死锁。
- **正解**：每个 Queue 控制块必须严格物理隔离为 **`rx_waiters`（读阻塞队列）** 与 **`tx_waiters`（写阻塞队列）**。`Send` 成功定向唤醒 `rx_waiters` 队首；`Receive` 成功定向唤醒 `tx_waiters` 队首。

---

### 3.2 统一 Handle 解析器缺失与 `vTaskDelete(NULL)` 自删栈破坏风险

- **FreeRTOS 规范事实**：`vTaskDelete(NULL)`、`vTaskSuspend(NULL)`、`vTaskPrioritySet(NULL, prio)` 等标准 API 普遍约定 `handle == NULL` 代表当前调用任务本身。
- **自删安全隐患**：当任务执行 `vTaskDelete(NULL)` 自删时，如果仅仅将自身标记为 `SIM_TASK_STATE_ZOMBIE` 然后 `return`，执行流将继续在该任务即将被 GC 释放的栈空间上运行，极易引发内存脏写与崩溃。
- **正解**：
  1. 封装统一的 `static esp_tcb_t* resolve_task_handle(TaskHandle_t h)`，单点支持 `NULL` 映射与 Generation 三检；
  2. `vTaskDelete` 若判定为当前任务自删，标记 ZOMBIE 后**必须立即执行协程切出永久交出控制权，严禁 return**。

---

### 3.3 EventGroup 24-bit 条件判定与 `clear_on_exit` 原子性清位规范

- **FreeRTOS 规范事实**：`xEventGroupWaitBits` 具备双标志参数组合：
  - `xWaitForAllBits == pdTRUE`：断言 `(cur_bits & uxBitsToWaitFor) == uxBitsToWaitFor`；
  - `xWaitForAllBits == pdFALSE`：断言 `(cur_bits & uxBitsToWaitFor) != 0`；
  - `xClearOnExit == pdTRUE`：在满足唤醒条件退出前，必须原子清除该任务关心的位 `cur_bits &= ~uxBitsToWaitFor`。
- **原计划缺陷**：原计划对清零时机与位掩码作用域未作展开，若直接写为清空全部 bits 会误伤其他任务所等待的事件位。

---

## 四、架构决议与方案比选 (Architectural Resolutions)

### 4.1 协程切出桥选型（Yield Context Bridge）

| 方案 | 机制描述 | 优缺点分析 | 决议 |
|:---|:---|:---|:---:|
| **方案 A：全走 `pal_os_sleep_ms`** | 延时直接调用 `pal_os_sleep_ms(ms)` | ✅ 延时场景完美；❌ 无法支持 `sync_block`（后者需处于 `BLOCKED` 态，而 `sleep_ms` 会将状态覆盖为 `WAITING`） | 仅延时采纳 |
| **方案 B：调度器公开切出桥 `sim_scheduler_yield_context`** | 在 `targets/common/wink_sim_scheduler.{h,c}` 增补统一切出入口，持有 `s_sim_main_ctx` 并执行 `sim_ctx_switch` | ✅ 5 行非破坏性代码，统一解决 `Delay(0)`、`sync_block`、`vTaskDelete(NULL)` 的协程切出；零侵入既有调度核心 | **推荐采纳 (Accepted)** |

---

### 4.2 Queue 双等待队列（Dual Waiter Queue）控制块规范

```c
typedef struct {
    bool used;
    uint32_t item_size;
    uint32_t length;
    uint32_t count;
    uint32_t head;
    uint32_t tail;
    uint8_t  storage[512];
    uint32_t rx_waiters[WINK_SIM_MAX_TASKS]; /* 等待读取的任务（空队阻塞） */
    uint32_t tx_waiters[WINK_SIM_MAX_TASKS]; /* 等待写入的任务（满队阻塞） */
} esp_sim_queue_t;
```
`xQueueSend` 成功后唤醒 `rx_waiters` 队首；`xQueueReceive` 成功后唤醒 `tx_waiters` 队首。

---

## 五、逐项审查决议表 (Action Item Verification Matrix)

| 编号 | 评审问题项 | 严重级别 | 决议结论 | 计划吸收落地位置 |
|:---:|:---|:---:|:---|:---|
| **ACT-01** | Fiber 协程切出桥缺失 | 🔴 P0 | 在 `wink_sim_scheduler.{h,c}` 增补 `sim_scheduler_yield_context()`；所有阻塞与延时严格遵循“标记状态+切出主循环” | §3.1 文件清单、§3.3 红线 11、Task M1-2 Step 3、Task M1-3 Step 1 |
| **ACT-02** | `vTaskDelay(0)` 纯让出真机语义 | 🔴 P0 | 纠正 `return;` 为执行 `sim_scheduler_yield_context()`，保持 READY 态让出时间片 | §3.3 红线 10、Task M1-2 Step 3、Task M1-5 单测 |
| **ACT-03** | Queue 读写阻塞互相串扰 | 🟠 P1 | 拆分 `rx_waiters` 与 `tx_waiters` 双向独立等待队列与定向唤醒 | Task M1-3 Step 2、Task M1-5 单测 |
| **ACT-04** | 统一 Handle 解析与自删切出 | 🟠 P1 | 封装 `resolve_task_handle`；`vTaskDelete(NULL)` 自删切出且永不返回（追加 `for(;;){}`） | Task M1-2 Step 1 & Step 2 |
| **ACT-05** | EventGroup 匹配与清零规范 | 🟡 P2 | 严格按 `xWaitForAllBits` 与 `xClearOnExit` 规范执行位运算与原子清除 | Task M1-3 Step 4、Task M1-5 单测 |

---

## 六、计划文档吸收落地核对结论 (M1 Plan v1.1 闭环验收)

经核对，实施计划文档 [`2026-09-24-esp-idf-sim-m1-freertos-plan.md`](./2026-09-24-esp-idf-sim-m1-freertos-plan.md) 已于 **2026-09-24** 彻底吸收上述全部审查意见并升级至 **v1.1 闭环版**：
1. **红线与契约强化**：红线 10 与红线 11 明确将“协作切出”作为硬性门禁；
2. **代码片段纠偏**：`vTaskDelay`、`sync_block`、`vTaskDelete` 全部修正为真实协程挂起切出代码；
3. **Queue 控制块与 Event 判定全面落地**；
4. **测试断言补齐**：L1 单测补齐双向 Waiter 隔离、Delay(0) 让出序、自删切出断言。

**最终评审判定**：**✅ 准入通过（Approved for Execution）**。M1 计划文档已具备完全的指导性和可落地性，可立即开工！
