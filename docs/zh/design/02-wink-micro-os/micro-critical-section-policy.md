# 微临界区选用规约 (Micro-Critical Section Policy)

| 元数据项 | 说明 |
| :--- | :--- |
| **文档编号** | POLICY-0002 |
| **所属模块** | `wink-micro-os/pal/include/pal_irq.h`, `pal_spinlock.h`, `pal_osal.h`, 全 DAL/BAL |
| **相关 ADR** | [ADR-0007 协作循环](../../decisions/core/0007-cooperative-loop-execution-model.md)、[ADR-0012 合约诚实](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)、[ADR-0017 阻塞硬隔离](../../decisions/core/0017-blocking-api-hard-isolation.md) |
| **生效状态** | **Active (Stage 1)** |

---

## 1. 核心机制对比与选用矩阵

| 场景 | 推荐机制 | 底层实现 | 理由 |
|---|---|---|---|
| **仅本核 ISR 与任务共享**（如 GPIO IRQ 计数器、软定时器链表） | `PAL_CRITICAL_SECTION` / `pal_irq_save_rtos_safe` | 本核中断屏蔽 (`portSET_INTERRUPT_MASK_FROM_ISR`) | 零自旋开销，开销极低（几条汇编指令） |
| **双核任务间数据互斥**（如 全局资源仲裁表、I2C 端口分配） | `pal_spinlock_t` / `pal_os_critical_enter` | SMP 跨核自旋锁 (`portENTER_CRITICAL`) | 阻止 Core 0 与 Core 1 任务并发 |
| **跨核任务与 ISR 共享**（如 驱动实例并发读写） | `pal_spinlock_lock_isr` / `pal_spinlock_unlock_isr` | SMP 自旋 + 中断屏蔽 (`portENTER_CRITICAL_ISR`) | 防止自旋死锁与并发破坏 |
| **可能超过 100 µs 的耗时操作** | **严禁关中断 / 严禁自旋持锁** | 改用非阻塞状态机 (`request`/`poll`/`get_cached`) 或 DMA/RMT 硬件引擎 | 破坏系统实时性（红线 2） |

---

## 2. 临界区与 ISR 的“三禁止”原则

在任何微临界区代码块（`PAL_CRITICAL_SECTION`、`pal_os_critical_enter`、`pal_spinlock_lock`）以及 `PAL_ISR` 中断回调中，**严格禁止**以下三类操作：

1. ❌ **禁止调用 `pal_log*`**
   - **原因**：日志输出可能涉及 Flash 字符串读取、格式化内存开销及后台互斥锁等待，会导致不可预测的毫秒级阻塞或死锁。
2. ❌ **禁止动态内存分配 (`malloc` / `free` / `calloc` / `realloc`)**
   - **原因**：堆分配在并发或内存碎片时存在显著耗时抖动，且在中断上下文调用可能引发严重内存越界崩溃。
3. ❌ **禁止调用 `pal_os_sleep_ms` / `pal_os_yield` 或任何可能让出 CPU 的 API**
   - **原因**：在持有自旋锁或关中断状态下触发上下文切换将导致系统死锁与看门狗复位。

---

## 3. 数据后处理剥离范式

必须严格遵循 **“数据采集在临界区内，数据换算在临界区外”** 的剥离范式：

```c
/* ❌ 错误示范：在临界区内进行浮点换算、滤波与日志 */
pal_os_critical_enter();
raw24 = bitbang_read();
float weight = (float)(raw24 - offset) / factor; // 耗费 CPU 周期
WINK_LOGI("weight = %f", weight);                // 致命错误：临界区内调日志
pal_os_critical_exit();

/* ✅ 正确示范：临界区仅保护纳秒/微秒级硬件移位，计算与日志在外完成 */
pal_os_critical_enter();
raw24 = bitbang_read();
pal_os_critical_exit();

/* 临界区外执行后处理 */
int32_t signed_raw = (int32_t)raw24;
if (raw24 & 0x800000u) signed_raw |= (int32_t)0xFF000000u;
dev->last_weight_g = (float)(signed_raw - dev->config.zero_offset) / dev->config.calibration_factor;
```

---

## 4. 跨平台语义保障 (Host / Wasm / ESP32)

- **ESP32 (SMP)**：`pal_spinlock_t` 提供完整的跨核互斥与中断屏蔽支持；
- **Host (Windows / Linux)**：`pal_spinlock_t` 退化为原子/内存屏障，配合单线程/多线程模拟器环境维持 API 签名一致；
- **Wasm (WebAssembly Sandbox)**：`pal_spinlock_t` 展开为编译器内存屏障 `__atomic_signal_fence(__ATOMIC_ACQ_REL)`，契合单线程协作式执行模型。
