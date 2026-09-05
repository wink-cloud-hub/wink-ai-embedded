# 并发与线程安全（范式无关）

> 适用范围：两种架构风格共用。本项目普遍多任务（FreeRTOS / Wasm 单线程 + Asyncify）。

---

## 共享资源保护机制选择表

| 场景 | 保护机制 |
|------|----------|
| 线程 ↔ 线程 | 互斥锁 / 信号量（mutex / semaphore） |
| ISR ↔ 线程 | 临界区 / 关中断（critical section / disable IRQ） |
| 读多写少的数据 | 读写锁（RW lock，若可用） |
| 简单标志位 | 原子操作（atomic） |
| 数据交换 | **消息队列（推荐）**——完全避免共享状态 |

> 需要优先级继承（防优先级反转）时必须用 **mutex**（`xSemaphoreCreateMutex`）；
> binary / counting semaphore **不**做优先级继承。

---

## 临界区四规

1. **尽可能短**。
2. **绝不调用阻塞函数**。
3. **绝不分配内存**。
4. **文档说明**为什么需要这个临界区。

### Mutex 持有时间预算（强制）

持有 mutex 的代码路径必须有明确的最坏执行时间预算：

```c
/* ✅ 正确：持有 mutex 路径 < 1ms */
void motor_set_target(motor_t *self, float target)
{
    mutex_lock(self->state_mutex);  /* 开始计时 */
    self->target_angle = target;    /* 简单赋值，< 1us */
    mutex_unlock(self->state_mutex);
}

/* ❌ 错误：持有 mutex 时做重计算 */
void motor_update_pid(...) {
    mutex_lock(self->state_mutex);
    pid_calculate(&self->pid);  /* 复杂浮点计算 > 10ms
    mutex_unlock(self->state_mutex);
}
```

> 规则：
> 1. 普通 mutex：持有时间 < 1ms
> 2. 高优先级路径使用的 mutex：持有时间 < 100us
> 3. 超过预算的代码必须重构：先拷贝数据后释放锁，再在无锁状态下计算

---

## ⭐ Task / ISR 双入口显式分流（ADR-0016）

**问题：** 一个模块的共享静态状态既可能被 task 上下文调用（正常主流程），也可能被 ISR 上下文调用（中断回调、fault handler）。二者在 ESP32 上必须走**不同**的临界区原语：

- Task 上下文：`portENTER_CRITICAL(&mux)` —— task-only 原语；ISR 调用会触发 assert / SMP 死锁。
- ISR 上下文：`portENTER_CRITICAL_ISR(&mux)` —— ISR-only；从 task 调则保护范围不匹配。

### 选项对比与推荐

| 方案 | 优点 | 缺点 | 结论 |
|-----|------|------|------|
| **A · 单一入口 + 内部 detect** | 调用方无感 | ❌ 违反契约诚实；`xPortInIsrContext()` 在 host/wasm 无对等 API；掩盖误用 | ❌ 拒绝 |
| **B · 双入口显式分流**（推荐） | 契约明确；与 ESP-IDF `xxxFromISR` 惯例对齐；AI Codegen 训练数据一致 | 调用方需选对入口（code review 覆盖） | ✅ 采纳 |
| **C · 单一入口 + doxygen 禁止 ISR** | diff 最小 | ❌ 丢失 ISR fault 记录能力；无演进路径 | ❌ 拒绝 |

### 落地范式

```c
/* 头文件：显式命名，让调用方一眼看出上下文 */
uint32_t pal_os_critical_enter(void);          /* TASK 上下文 */
void     pal_os_critical_exit(uint32_t key);
uint32_t pal_os_critical_enter_isr(void);      /* ISR 上下文 */
void     pal_os_critical_exit_isr(uint32_t key);

/* 上层被 task 和 ISR 都调用的服务同步拆双入口 —— 命名后缀 `_from_isr` */
void wink_trace_fault(uint32_t code);          /* TASK-only */
void wink_trace_fault_from_isr(uint32_t code); /* ISR-only */

/* 实现：抽公共写入到 static inline，两条路径 bit-for-bit 等价 */
static inline void s_record_fault_locked(uint32_t code) {
    s_buffer[s_head] = code;
    s_head = (s_head + 1u) % CAPACITY;
    s_count++;
}
void wink_trace_fault(uint32_t code) {
    uint32_t key = pal_os_critical_enter();
    s_record_fault_locked(code);
    pal_os_critical_exit(key);
}
void wink_trace_fault_from_isr(uint32_t code) {
    uint32_t key = pal_os_critical_enter_isr();
    s_record_fault_locked(code);
    pal_os_critical_exit_isr(key);
}
```

### ISR 内的强约束

- 🚨 **ISR 内绝对禁止**同步调用可能阻塞 / I/O 的用户回调（`wink_runtime_fault`、`on_fault` 等）；只做**静态日志记录**（`wink_trace_fault_from_isr`），真正的 Safe-off 关断链**延迟到 TASK 层**（主 Loop tick 回收期）。
- 🚨 ISR 内不得调用 task 版临界区（`portENTER_CRITICAL` 在 ESP32 assert / SMP deadlock）。
- 🚨 task 内不得调用 ISR 版（保护范围不匹配，task/task 竞态无保护）。

### 跨 Target 行为矩阵

| Target | task 版 | ISR 版 | 契约执行 |
|-------|--------|--------|---------|
| ESP32 (SMP) | `portENTER_CRITICAL(&mux)` | `portENTER_CRITICAL_ISR(&mux)`（共享 mux） | 真机原生 |
| Cortex-M / baremetal | 关中断 | 关中断 | 关中断即已同时保护 task/ISR |
| host / wasm (单线程) | no-op + `assert(!in_isr)` | no-op + `assert(in_isr)` | 靠 sim-hook 强校验 |

### Host / Wasm sim-hook（可选但强推）

单线程 host / wasm 语义上两条路径都 no-op 就够，但那样一来入口误用**在 host 单测里根本不会失败**——真机 assert 直到刷板才暴露。**推荐**在 host/wasm 的 OSAL 里加一对 sim-hook：

```c
static bool s_sim_in_isr = false;
void pal_os_set_sim_isr_context(bool in_isr) { s_sim_in_isr = in_isr; }
bool pal_os_in_sim_isr_context(void)         { return s_sim_in_isr; }

uint32_t pal_os_critical_enter(void) {
    assert(!s_sim_in_isr && "task-only entry called from ISR context");
    return 0;
}
uint32_t pal_os_critical_enter_isr(void) {
    assert(s_sim_in_isr && "ISR-only entry called from task context");
    return 0;
}
```

仿真器 / 单测在向模拟中断回调分发前后夹紧 `set_sim_isr_context(true) → callback → set_sim_isr_context(false)`。这样入口误用在 Debug 构建下**立即命中 assert**，属"编译期契约 + 运行期强校验"双保险。

### 验收硬门槛：等价性测试

拆双入口后**必须**加一条等价性单测：同一 fault code 序列，纯 task 路径 vs 交替 task/ISR 路径，`buffer/head/count` 状态**bit-for-bit 相同**。范式代码见 `wink-micro-os/test/test_wink_trace_isr_equivalence.c`。

### 演进路径

- 引入多虚拟核 / 多核（若单核决策被推翻）：`s_global_mux` 需按核区分，API 签名不变。
- Host 单测多线程化：`s_sim_in_isr` 升 `_Thread_local`（防跨线程竞态）。
- 嵌套模拟中断（高优先级抢占低优先级）：`s_sim_in_isr` bool 改嵌套计数器（`s_sim_isr_nesting_level`），防内层退出时提早清标志。

参考：
- ADR-0016 `pal_os_critical_enter` 任务/ISR 双入口决策：`docs/design/decisions/0016-pal-critical-section-task-isr-dual-entry.md`
- ADR-0012 契约诚实优于静默降级：`docs/design/decisions/0012-contract-honesty-over-silent-degradation.md`
- 实施记录：PLAN-20260701-WMOS-CODE-OPTIMIZATION-Q3 §Track D

---

## 单核 vs 多核差异对照表

| 场景 | 单核（Cortex-M） | 多核（ESP32 Xtensa） |
|------|------------------|----------------------|
| 线程间共享 | mutex（优先级继承 | mutex |
| ISR ↔ 线程 | 关中断 / 临界区 | portMUX_TYPE spinlock + 内存屏障 |
| 标志位（单写多读） | volatile 通常足够 | `_Atomic` C11 必须 |
| RMW 操作（计数器） | volatile + 关中断 | `_Atomic` + acquire/release 内存序 |

> ESP32 Xtensa 是宽松内存模型，多核环境下必须格外小心。

---

## 死锁检测（Debug 构建启用）

Debug 构建中必须启用死锁检测机制：

```c
/* Debug 构建：记录 mutex 持有者和持有时间
void mutex_lock_debug(mutex_t *mutex)
{
    uint32_t start = get_time_us();
    platform_mutex_lock(mutex->handle);
    
    /* 记录持有者 */
    mutex->holder_task = current_task_id();
    mutex->hold_start = get_time_us();
    
    /* 持有时间超时报警 */
    if (mutex->hold_start - start > 1000) {
        ASSERT_MSG("Mutex held too long!");
    }
}
```

> 检测规则：
1. 同一任务按固定顺序获取锁（避免循环等待）
2. 嵌套锁层级检查：任务不能持有 A 等 B，同时持有 B 等 A
3. 超时检测：持有超过 10ms 断言失败

---

## 可重入性

- 被多上下文调用的函数必须可重入：**不依赖隐藏的 static / 全局可变状态**。
- 若不可重入，则通过模块 API 串行化访问。
- ISR 安全的函数**不得**调用不可重入函数。

---

## ⭐ ISR 优先级上限（RTOS API 调用铁律）

**调任何 `*FromISR` / ISR 安全 API 的中断，优先级必须在 RTOS 允许的门槛以下**，否则内存损坏 /
崩溃——这是 ESP32 + FreeRTOS 头号实战雷。

| 平台 | 门槛 |
|------|------|
| **Cortex-M** | ISR 数值优先级 **≥** `configMAX_SYSCALL_INTERRUPT_PRIORITY`（注意：数值越大优先级越低） |
| **ESP32（Xtensa 双核）** | 仅中断 **level 1**（必要时 level 3）的 ISR 能调 RTOS API；更高 level 只能置 flag / queue 再降级处理 |

高于门槛的 ISR：**只**读写寄存器 + 释放信号量（ISR 安全变体），**绝不**调任何 RTOS API。

### 契约诚实 > 静默降级（ADR-0012）

PAL 抽象跨平台中断优先级时，如果目标平台**不满足**某个优先级的语义（例如 ESP32 无真正的 NMI C-ISR，
不支持"非 RTOS 安全"级），**必须显式返回 `WINK_ERR_UNSUPPORTED`，禁止静默降级到其它级别**。
静默降级会让"仿真通过 → 真机通过"关系失效，掩盖跨平台契约违反。

- 头文件写什么，实现就必须兑现；实现兑现不了的能力，头文件必须诚实标注。
- 仿真侧（host/wasm）默认与真机拒接方案一致；仅在编译期宏（如 `WINK_HOST_ALLOW_REALTIME_FOR_TESTING`）
  显式 opt-in 时才放行，且首次注册需打印一次性 warning。
- 具体案例：`pal_irq_enable(PAL_IRQ_PRIO_REALTIME, ...)` 在三 target 上均返回 `WINK_ERR_UNSUPPORTED`。

详见 [ADR-0012 契约诚实优于静默降级](../../../docs/design/decisions/0012-contract-honesty-over-silent-degradation.md)
及其子系统级映射 ADR-IRQ-008 / ADR-IRQ-009。

---

## volatile ≠ 原子 ≠ 内存序

`volatile` 只保证「每次访问真去内存、不被编译器优化掉」，**不**保证：

- **原子性**：`count++` 即使变量是 `volatile`，读-改-写仍非原子（单核也竞争 ISR）。
- **内存序 / 可见性**：不插入内存屏障，CPU 可乱序；多核（ESP32-S3 双核）另一核可能读到旧值。

| 场景 | 正确保护 |
|------|----------|
| 单核 ISR 共享的 RMW（如计数器） | `volatile` + **关中断 / 临界区** |
| 单标志位（单写多读，位宽 ≤ 体系字长） | `volatile` 通常够（仍是单核约定） |
| 跨核 / 需跨核可见性 | `_Atomic`（C11，带 acquire/release 序）或 `portMUX_TYPE spinlock` |

> 铁律：**`volatile` 解决编译器优化，原子性靠临界区 / 原子操作，可见性与序靠内存屏障
> （atomic / spinlock）——三者不可互相替代。**

---

## ⭐ ISR → 信号量 → 高优先级工作线程（核心模式）

> 源于 zhaoming `hardware-interaction.md`「关键设计规则——历史教训」。

**原则：ISR 尽可能短，只做一件事——释放信号量或置 flag。实活交给任务上下文。**

```
ISR（最短）→ osSemaphoreRelease / xSemaphoreGiveFromISR（ISR 安全）
              ↓
高优先级工作线程（任务上下文）被唤醒 → 读硬件 → 回调
              ↓
回调在任务上下文执行 → 可用任意 RTOS / 框架 API
```

为什么不在 ISR 里直接回调：
- 强迫回调用 `FromISR` 变体；
- 紧急信号（如掉电）被排到队列尾，无法抢占已排队事件；
- 把驱动设计与 ISR 约束耦合；
- 大多框架的 publish 无 LIFO 变体，且任务级 LIFO post 不能在 ISR 调。

**新中断驱动驱动的默认范式：高优先级工作线程 + ISR 只发信号量 + 回调在任务上下文 + 应用决定投递方式（post / post-LIFO / publish）。驱动不得包含事件框架头、不得知道上层活动对象。**

---

## 消息队列：推荐的跨上下文 IPC

无共享状态的并发：生产者打包消息入队，消费者线程出队处理。

```c
typedef struct {
    uint8_t  cmd;
    uint16_t data_len;
    uint8_t  data[MAX_MSG_DATA_SIZE];
} driver_msg_t;

/* 生产者（应用 / ISR） */
driver_send_async(self, data, len);   /* 打包 → queue_send，立即返回 */

/* 消费者（驱动内部工作线程） */
while (self->running) {
    queue_recv(self->msg_queue, &msg, POLL_INTERVAL_MS);
    process_message(self, &msg);      /* 阻塞在此 OK，因为是工作线程 */
}
```

这是「非阻塞驱动 = 工作线程 + 消息队列 + 回调」的具体落地（详见 [realtime-hardware.md](./realtime-hardware.md)）。

---

## 两项目的实例（注意差异）

**chigo-micro（FreeRTOS，真多任务）**
- 共享状态用 `SemaphoreHandle_t`；**PID 回调内用非阻塞** `xSemaphoreTake(mutex, 0)`（绝不阻塞快路径）。
- ISR/回调 ↔ 任务通信：`_Atomic bool`（`stdatomic.h`，如 `g_collision_detected`）+ `portMUX_TYPE spinlock`（多核）。
- 碰撞检测在 PID 回调 1kHz 内联执行（快路径），过流/过温在 `safety_task` 100Hz 执行（慢路径）。

**wink-micro-os（Wasm 仿真）**
- ⚠ **Wasm 是单线程，PAL mutex 是假锁**（`pal_mutex_create` 返回常量、lock/unlock 空操作）。
- **不要依赖 PAL mutex 在仿真里保证跨任务正确性**——仿真无真实竞争。真机的并发正确性靠 xtensa target 的真实 OSAL 保证。

---

## osThreadNew / osSemaphoreNew 陷阱（CMSIS-RTOS2）

- `osThreadNew` 的 `cb_mem` 与 `stack_mem` 必须**同时提供或同时为 NULL**；混搭（一个提供一个 NULL）→ 返回 NULL → 启动崩溃。先搜项目现有用法再跟随。
- `osSemaphoreNew` 可在 `osKernelStart()` 前调用；`osSemaphoreRelease()` 是 **ISR 安全**的。

> ISR 安全的完整讨论：优先保持 ISR 最小化；运行期多态中的 ISR dispatch 细节见对应范式参考的 pitfalls 文档。

---
> **源出（溯源）**：zhaoming `memory-safety.md`（线程安全）+ `hardware-interaction.md`（ISR→工作线程）、chigo-micro `main.c` + `c-embedded.md`、wink-micro-os `pal_osal.h` + `targets/wasm/pal_hal_wasm.c`。
> 「ISR 优先级上限」「volatile ≠ 原子 ≠ 内存序」两节为**项目本地技术修正**（非引自 zhaoming），基于 FreeRTOS / Cortex-M / Xtensa 双核工程实践。
