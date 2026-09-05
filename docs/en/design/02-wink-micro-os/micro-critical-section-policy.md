# Micro-Critical Section Policy

| Metadata | Description |
| :--- | :--- |
| **Document ID** | POLICY-0002 |
| **Module** | `wink-micro-os/pal/include/pal_irq.h`, `pal_spinlock.h`, `pal_osal.h`, DAL/BAL |
| **Related ADRs** | [ADR-0007 Cooperative Loop](../../decisions/core/0007-cooperative-loop-execution-model.md), [ADR-0012 Contract Honesty](../../decisions/core/0012-contract-honesty-over-silent-degradation.md), [ADR-0017 Blocking Isolation](../../decisions/core/0017-blocking-api-hard-isolation.md) |
| **Status** | **Active (Stage 1)** |

---

## 1. Mechanism Comparison & Decision Matrix

| Scenario | Recommended Mechanism | Underlying Primitive | Rationale |
|---|---|---|---|
| **Single-Core ISR & Task Shared Data** (e.g. GPIO IRQ edge counters, soft timer lists) | `PAL_CRITICAL_SECTION` / `pal_irq_save_rtos_safe` | Local core interrupt masking (`portSET_INTERRUPT_MASK_FROM_ISR`) | Zero spinlock overhead, single-core latency < 100ns |
| **Dual-Core Task Mutex** (e.g. global resource claim table, I2C bus arbiter) | `pal_spinlock_t` / `pal_os_critical_enter` | SMP cross-core spinlock (`portENTER_CRITICAL`) | Prevents Core 0 & Core 1 concurrent access |
| **Cross-Core Task & ISR Shared Data** (e.g. driver instance concurrent read/write) | `pal_spinlock_lock_isr` / `pal_spinlock_unlock_isr` | SMP spinlock + ISR mask (`portENTER_CRITICAL_ISR`) | Prevents deadlock and data races across cores |
| **Operations taking > 100 µs** | **PROHIBITED from disabling interrupts / holding spinlocks** | Use asynchronous state machine (`request`/`poll`/`get_cached`) or DMA/RMT hardware engines | Avoids violating hard real-time latency (Red-line 2) |

---

## 2. The Three Prohibitions in Critical Sections & ISRs

Within any critical section block (`PAL_CRITICAL_SECTION`, `pal_os_critical_enter`, `pal_spinlock_lock`) and `PAL_ISR` interrupt callbacks, the following three operations are **strictly prohibited**:

1. ❌ **No `pal_log*` calls**
   - **Rationale**: Logging invokes string formatting, flash memory lookups, and mutex locking, causing unpredictable millisecond delays or deadlocks.
2. ❌ **No Dynamic Memory Allocation (`malloc` / `free` / `calloc` / `realloc`)**
   - **Rationale**: Heap allocation involves severe non-deterministic latency under fragmentation, and invoking heap routines from ISR triggers kernel panic.
3. ❌ **No `pal_os_sleep_ms` / `pal_os_yield` or Blocking Calls**
   - **Rationale**: Context switches while holding spinlocks or with interrupts disabled lead to immediate system deadlock and watchdog reset.

---

## 3. Post-Processing Separation Paradigm

Developers must strictly adhere to the **"Sample inside critical section, Compute outside critical section"** rule:

```c
/* ❌ Bad: Float arithmetic, filtering, and logging inside critical section */
pal_os_critical_enter();
raw24 = bitbang_read();
float weight = (float)(raw24 - offset) / factor; // Consumes CPU cycles
WINK_LOGI("weight = %f", weight);                // Fatal error: logging in critical section
pal_os_critical_exit();

/* ✅ Good: Critical section only shields nanosecond/microsecond hardware pulses */
pal_os_critical_enter();
raw24 = bitbang_read();
pal_os_critical_exit();

/* Post-processing executed safely outside critical section */
int32_t signed_raw = (int32_t)raw24;
if (raw24 & 0x800000u) signed_raw |= (int32_t)0xFF000000u;
dev->last_weight_g = (float)(signed_raw - dev->config.zero_offset) / dev->config.calibration_factor;
```

---

## 4. Cross-Target Semantics (Host / Wasm / ESP32)

- **ESP32 (SMP)**: `pal_spinlock_t` provides full dual-core spinlock and interrupt masking.
- **Host (Windows / Linux)**: `pal_spinlock_t` degrades to atomic compiler/memory barriers.
- **Wasm (WebAssembly Sandbox)**: `pal_spinlock_t` degrades to `__atomic_signal_fence(__ATOMIC_ACQ_REL)` for single-threaded cooperative execution.
