/**
 * @file pal_osal.h
 * @brief 操作系统与内核环境抽象接口 (OSAL)
 */

#ifndef PAL_OSAL_H
#define PAL_OSAL_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/*                                1. 系统时间与时延                            */
/* ========================================================================== */

#ifndef WINK_STRICT_NONBLOCKING
/**
 * @brief 系统毫秒延时 (主动阻塞让出 CPU 调度)
 * @note Blocking: Yes. Not available under WINK_STRICT_NONBLOCKING (ADR-0017 层 2 硬隔离).
 *       协作式运行时 (10ms tick) 不得调用；使用 wink_soft_timer 或 PT_WAIT_DEADLINE。
 */
WINK_BLOCKING
void pal_os_sleep_ms(uint32_t ms);
#endif /* WINK_STRICT_NONBLOCKING */

/**
 * @brief 系统微秒延时 (高精度短等待，忙等 —— 阻塞 CPU)
 * @note 未标 WINK_BLOCKING：ADR-0017 §决策 定义 blocking 门槛为「单次 > 一个
 *       runtime tick (10ms)」；本 API 的典型用途为 GPIO strobe / SPI half-cycle 等
 *       <100μs 硬件时序，位于允许范围。调用方应将 us 上限视为 ~1000（>1ms 视 anti-pattern，
 *       用 pal_os_sleep_ms 替代）。
 */
void pal_os_busy_wait_us(uint32_t us);

/**
 * @brief 获取系统从启动至今的毫秒数
 */
uint64_t pal_os_get_ms(void);

/**
 * @brief 获取系统从启动至今的微秒数 (用于高精度时序测算)
 */
uint64_t pal_os_get_us(void);


/* ========================================================================== */
/*                           2. 简单互斥锁/信号量支撑                          */
/* ========================================================================== */

typedef void* pal_os_mutex_t;

/** @brief 「永久等待」超时哨兵值（pal_os_mutex_lock 的 timeout_ms 传入此值代表无限等待）。 */
#define WINK_MUTEX_WAIT_FOREVER 0xFFFFFFFFu

/**
 * @brief 创建一个互斥锁句柄
 */
pal_os_mutex_t pal_os_mutex_create(void);

#ifndef WINK_STRICT_NONBLOCKING
/**
 * @brief 获取互斥锁 (锁定)
 * @param mutex 锁句柄
 * @param timeout_ms 阻塞超时时间，传入 WINK_MUTEX_WAIT_FOREVER 代表无限等待
 * @note 失败型：NULL mutex → WINK_ERR_INVALID_ARG；timeout → WINK_ERR_TIMEOUT；
 *       不支持 target → WINK_ERR_UNSUPPORTED。
 * @note Blocking: Yes（协作式调度器下可能让出）。Not available under WINK_STRICT_NONBLOCKING.
 *       PT 上下文与 ISR 上下文禁用；使用非阻塞资源交换（ringbuf / atomic flag）。
 */
WINK_BLOCKING
WINK_WARN_UNUSED_RESULT wink_status_t pal_os_mutex_lock(pal_os_mutex_t mutex, uint32_t timeout_ms);
#endif /* WINK_STRICT_NONBLOCKING */

/**
 * @brief 释放互斥锁 (解锁)
 */
WINK_WARN_UNUSED_RESULT wink_status_t pal_os_mutex_unlock(pal_os_mutex_t mutex);

void pal_os_mutex_destroy(pal_os_mutex_t mutex);


/* ─────────────────────────────────────────────────────────
 * 2b. 二值信号量支撑 (Semaphore)
 * ───────────────────────────────────────────────────────── */

typedef void* pal_os_sem_t;

/**
 * @brief 创建一个二值信号量句柄
 */
WINK_WARN_UNUSED_RESULT pal_os_sem_t pal_os_sem_create(void);

#ifndef WINK_STRICT_NONBLOCKING
/**
 * @brief 获取信号量 (锁定/等待)
 * @param sem 信号量句柄
 * @param timeout_ms 阻塞超时时间，传入 WINK_MUTEX_WAIT_FOREVER 代表无限等待
 */
WINK_BLOCKING
WINK_WARN_UNUSED_RESULT wink_status_t pal_os_sem_take(pal_os_sem_t sem, uint32_t timeout_ms);
#endif /* WINK_STRICT_NONBLOCKING */

/**
 * @brief 释放信号量 (给/发送) - TASK 上下文
 */
WINK_WARN_UNUSED_RESULT wink_status_t pal_os_sem_give(pal_os_sem_t sem);

/**
 * @brief 释放信号量 (给/发送) - ISR 上下文 (ADR-0016)
 */
WINK_WARN_UNUSED_RESULT wink_status_t pal_os_sem_give_isr(pal_os_sem_t sem);

/**
 * @brief 销毁信号量并释放内存
 */
void pal_os_sem_destroy(pal_os_sem_t sem);


/* ========================================================================== */
/*                        3. 看门狗与复位原因 (WDT / Reset)                    */
/* ========================================================================== */

/** @brief 复位原因（Phase 5 Task 5-4） */
typedef enum {
    PAL_OS_RESET_REASON_UNKNOWN  = 0,
    PAL_OS_RESET_REASON_POWER_ON = 1,
    PAL_OS_RESET_REASON_WATCHDOG = 2,   /* boot safe-lock 触发（wink_runtime.c WATCHDOG|PANIC 判定） */
    PAL_OS_RESET_REASON_PANIC    = 3,   /* boot safe-lock 触发 */
    PAL_OS_RESET_REASON_SOFTWARE = 4,   /* 软件主动复位（如 esp_reset/ESP_RST_SW），不触发 safe-lock */
    PAL_OS_RESET_REASON_BROWNOUT = 5,   /* 掉电复位，信息性记录 */
} pal_os_reset_reason_t;

/**
 * @brief 读取上次复位原因（boot safe-lock 判定用，Phase 5 Task 5-5）。
 * @note host 返回可配置值（供测试）；wasm 返回 UNKNOWN；esp32 映射 esp_reset_reason()（随 P2-6）。
 *       WATCHDOG/PANIC 进入 wink_runtime 的 boot safe-lock 计数判定（ADR-0010：连续 N 次才锁死，
 *       单次/偶发放行恢复）；其余仅信息性。
 */
pal_os_reset_reason_t pal_os_get_reset_reason(void);

/**
 * @brief 读取连续异常复位计数（boot safe-lock 恢复策略，ADR-0010）。
 * @note esp32 读 RTC_NOINIT（+ magic 守卫），跨 WDT/panic 复位保留、断电或无效返回 0；
 *       host 读可注入静态（供单测）；wasm/baremetal 恒 0。
 */
uint32_t pal_os_get_abnormal_boot_count(void);

/**
 * @brief 写连续异常复位计数（boot safe-lock 恢复策略，ADR-0010）。
 * @note esp32 写 RTC_NOIT（持久化）；host 写静态；wasm/baremetal no-op。
 */
void pal_os_set_abnormal_boot_count(uint32_t count);

/**
 * @brief 初始化硬件看门狗
 * @note host 为无操作 stub（WINK_OK）；wasm 返回 WINK_ERR_UNSUPPORTED（无浏览器 watchdog 策略）；
 *       esp32 映射 ESP-IDF task/RTC watchdog（随 P2-6）。
 */
WINK_WARN_UNUSED_RESULT wink_status_t pal_os_wdt_init(uint32_t timeout_ms);

/** @brief 喂狗（周期调用防止复位）。target 规则同 pal_os_wdt_init。 */
WINK_WARN_UNUSED_RESULT wink_status_t pal_os_wdt_feed(void);


/* ========================================================================== */
/*                           4. 全局临界区与并发安全支撑                        */
/* ========================================================================== */

/**
 * @brief 进入全局临界区 (TASK 上下文, 屏蔽中断/多核自旋锁)
 * @return 恢复临界区所需的原始状态键值
 * @note 仅用于极短时间的原子操作保护（如环形队列读写、状态标记更新），严禁在临界区内调用阻塞/延时 API。
 * @warning **仅限 TASK 上下文调用**（ADR-0016 双入口显式分流）：
 *          - ESP32 使用 `portENTER_CRITICAL`（task-only 原语），ISR 调用会触发 assert / SMP 死锁；
 *          - host/wasm 单线程语义上安全，但契约不匹配（Debug 构建下会因 `s_sim_in_isr` 而 assert 命中）。
 *          从 ISR 上下文改用 pal_os_critical_enter_isr()。
 */
uint32_t pal_os_critical_enter(void);

/**
 * @brief 退出全局临界区并恢复之前的状态（TASK 上下文）
 * @param key 进入临界区时返回的状态键值
 * @warning 与 pal_os_critical_enter 配对，同样禁止 ISR 上下文调用（ADR-0016）。
 */
void pal_os_critical_exit(uint32_t key);

/**
 * @brief 进入全局临界区 (ISR 上下文, ESP-IDF `xxxFromISR` 惯例, ADR-0016)。
 * @return 恢复临界区所需的原始状态键值
 * @note - ESP32：使用 `portENTER_CRITICAL_ISR(&s_global_mux)`，与 task 版共享同一 mux，
 *         task/ISR 互斥仍然成立；
 *       - host/wasm：单线程退化为与 task 版语义等价（Debug 构建下 assert `s_sim_in_isr`）；
 *       - baremetal：与 task 版共用关中断原语实现（`pal_bsp_irq_save/restore`）。
 *
 * 与 `pal_os_critical_enter` 的对称契约由 ADR-0016 定义（选项 B：双入口显式分流）。
 */
uint32_t pal_os_critical_enter_isr(void);

/**
 * @brief 退出全局临界区（ISR 上下文）
 * @param key 进入临界区时返回的状态键值
 */
void pal_os_critical_exit_isr(uint32_t key);

/**
 * @brief 切换模拟 ISR 上下文标志（仅 host/wasm 生效, ADR-0016 §4.2）。
 *
 * 仿真器/单测在向模拟中断回调分发前调 `pal_os_set_sim_isr_context(true)`、
 * 返回后调 `pal_os_set_sim_isr_context(false)`；这样 `pal_os_critical_enter`
 * 与 `pal_os_critical_enter_isr` 在 Debug 构建下可通过 assert 立即捕获入口误用。
 *
 * ESP32 / baremetal 上此函数为 no-op（真机使用 `xPortInIsrContext()` / BSP 硬件寄存器判定）。
 */
void pal_os_set_sim_isr_context(bool in_isr);

/**
 * @brief 读取当前模拟 ISR 上下文标志（仅 host/wasm 有意义, ADR-0016 §4.2）。
 * @return host/wasm 返回内部 `s_sim_in_isr`；esp32 / baremetal 恒返回 false。
 * @note 供单测断言与 wink_trace ISR 分流实现使用；生产代码通常不需读取此标志。
 */
bool pal_os_in_sim_isr_context(void);
 
 /**
  * @brief 检测当前执行上下文是否在中断服务程序 (ISR) 中。
  * @return true 表示在 ISR 上下文中，false 表示在任务 (Task) 上下文中。
  */
 bool pal_os_in_isr(void);

/**
 * @brief 切换模拟 PT 协程上下文标志（仅 host/wasm 生效, ADR-0017 §4.2）。
 */
void pal_os_set_sim_pt_context(bool in_pt);

/**
 * @brief 读取当前模拟 PT 协程上下文标志（仅 host/wasm 生效）。
 */
bool pal_os_in_sim_pt_context(void);


/* ========================================================================== */
/*                          4. 任务创建与多核亲和性                            */
/* ========================================================================== */

/**
 * @brief Task handle (opaque pointer - implementation varies by target)
 */
typedef void* pal_os_task_handle_t;

/**
 * @brief Core ID for task affinity scheduling
 */
typedef enum {
    PAL_OS_CORE_0   = 0,  /**< Pin to Core 0 (typically networking/communication) */
    PAL_OS_CORE_1   = 1,  /**< Pin to Core 1 (typically control loop / real-time) */
    PAL_OS_CORE_ANY = -1, /**< No affinity - scheduler decides */
} pal_os_core_id_t;

#ifndef WINK_STRICT_NONBLOCKING
/**
 * @brief Create a new task with optional core affinity
 *
 * @param func Task entry function (takes void* arg, returns void)
 * @param name Task name (for debugging, max 16 chars recommended)
 * @param stack_depth Stack depth in bytes (FreeRTOS converts to words internally)
 * @param arg Argument passed to task entry function
 * @param priority Task priority (higher = more important, target-specific range)
 * @param core_id Core affinity: PAL_OS_CORE_0, PAL_OS_CORE_1, or PAL_OS_CORE_ANY
 * @param[out] task_handle Output task handle pointer (may be NULL if not needed)
 * @return wink_status_t WINK_OK on success, WINK_ERR_NO_MEM if insufficient memory,
 *         WINK_ERR_UNSUPPORTED if target doesn't support multi-tasking
 *
 * @note WASM/bare-metal targets may call func synchronously or return UNSUPPORTED.
 *       App code must handle UNSUPPORTED gracefully as graceful degradation.
 * @note Blocking: Yes（FreeRTOS 堆分配 + 调度器 bookkeeping 可能等待）。
 *       Not available under WINK_STRICT_NONBLOCKING.
 *       协作式调度器下的任务应静态创建（编译期决定），运行时创建违反 ADR-0017。
 */
WINK_BLOCKING
WINK_WARN_UNUSED_RESULT
wink_status_t pal_os_task_create(
    void (*func)(void* arg),
    const char* name,
    uint32_t stack_depth,
    void* arg,
    int32_t priority,
    pal_os_core_id_t core_id,
    pal_os_task_handle_t* task_handle
);
#endif /* WINK_STRICT_NONBLOCKING */

/**
 * @brief Delete a task (PAL 统一接口，替代 FreeRTOS vTaskDelete)
 *
 * @param task_handle Task handle to delete, or NULL to delete current task
 *
 * @note WASM/bare-metal targets may do nothing (single-threaded environment).
 *       App code should not assume task deletion is always available.
 */
void pal_os_task_delete(pal_os_task_handle_t task_handle);


/**
 * @brief Return current free heap size in bytes.
 *
 * ESP32: wraps `xPortGetFreeHeapSize()` ( FreeRTOS-maintained counter;
 *        O(1), ISR-safe, no allocation).
 * Host/wasm: returns 0 (no portable cross-platform introspection; 0 is a
 *        valid "not available" sentinel, not an error).
 *
 * @return Free bytes, or 0 when unsupported on the current target.
 */
uint32_t pal_os_get_free_heap_size(void);

/**
 * @brief Return the minimum free heap size seen since boot (high-water mark).
 *
 * ESP32: wraps `xPortGetMinimumEverFreeHeapSize()`. Monotonically
 *        non-increasing; useful for fleet-safety telemetry ("did we ever
 *        get close to OOM?").
 * Host/wasm: returns 0.
 *
 * @return Historical minimum free bytes, or 0 when unsupported.
 */
uint32_t pal_os_get_min_free_heap_size(void);

/**
 * @brief Return unused stack bytes for the **calling** task (high-water mark).
 *
 * ESP32: wraps `uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t)` —
 *        bytes of stack that have NEVER been touched since task start.
 *        `wink_runtime_get_stats()` calls this from the main-loop task
 *        context, so it reports the runtime/init task's watermark.
 * Host/wasm: returns 0.
 *
 * @return Unused stack bytes for current task, or 0 when unsupported.
 * @note Not a system-wide minimum (would require task enumeration via
 *       `uxTaskGetSystemState`, v2 work). Honest v1: reports the task that
 *       asks.
 */
uint32_t pal_os_get_current_task_stack_free(void);


/* ========================================================================== */
/*                        5. 跨核通信环形缓冲区 (Ringbuf)                      */
/* ========================================================================== */

/**
 * @brief Ring buffer handle (opaque pointer - implementation varies)
 *
 * Thread-safe, non-blocking ring buffer for cross-core communication.
 * Push is non-blocking and may fail if full. Pop is non-blocking and may fail
 * if empty. Designed for zero-copy or fixed-size element patterns.
 */
typedef struct pal_os_ringbuf* pal_os_ringbuf_handle_t;

/**
 * @brief Create a new ring buffer
 *
 * @param size Buffer capacity in bytes. Must be power of 2 for efficient wrapping.
 * @return pal_os_ringbuf_handle_t Buffer handle or NULL on failure
 *
 * @note On ESP32, wraps FreeRTOS xRingbufferCreate with RINGBUF_TYPE_BYTEBUF.
 *       On WASM/host, uses a simple volatile-head-tail implementation.
 *       On bare-metal, uses interrupt-disable critical sections for atomicity.
 */
pal_os_ringbuf_handle_t pal_os_ringbuf_create(uint32_t size);

/**
 * @brief Push data to ring buffer (non-blocking)
 *
 * @param rb Ring buffer handle
 * @param data Pointer to data to push
 * @param size Size of data in bytes
 * @return wink_status_t WINK_OK on success, WINK_ERR_FULL if buffer full,
 *         WINK_ERR_INVALID_ARG if NULL handle/data
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_os_ringbuf_push(
    pal_os_ringbuf_handle_t rb,
    const void* data,
    uint32_t size
);

/**
 * @brief Pop data from ring buffer (non-blocking)
 *
 * @param rb Ring buffer handle
 * @param data Output buffer to receive data
 * @param size Exact number of bytes to pop
 * @return wink_status_t WINK_OK on success, WINK_ERR_EMPTY if buffer empty,
 *         WINK_ERR_INVALID_ARG if NULL handle/data or size mismatch
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_os_ringbuf_pop(
    pal_os_ringbuf_handle_t rb,
    void* data,
    uint32_t size
);

/**
 * @brief Get current used bytes in ring buffer
 *
 * @param rb Ring buffer handle
 * @return uint32_t Number of bytes currently available to pop
 */
uint32_t pal_os_ringbuf_used(pal_os_ringbuf_handle_t rb);

/**
 * @brief Destroy a ring buffer and free all resources
 *
 * @param rb Ring buffer handle (NULL-safe)
 */
void pal_os_ringbuf_destroy(pal_os_ringbuf_handle_t rb);


/* ========================================================================== */
/*                        4. 仿真专用模式控制接口 (Simulation Mode)            */
/* ========================================================================== */

typedef enum {
    WINK_SIM_MODE_INTERACTIVE = 0,
    WINK_SIM_MODE_HEADLESS    = 1,
} wink_sim_mode_t;

/**
 * @brief 设置仿真模式（仅在仿真平台有效，实机为 no-op）
 */
void wink_sim_set_mode(wink_sim_mode_t mode);

/**
 * @brief 获取当前仿真模式（仅在仿真平台有效，实机返回 WINK_SIM_MODE_INTERACTIVE）
 */
wink_sim_mode_t wink_sim_get_mode(void);


#ifdef __cplusplus
}
#endif

#endif /* PAL_OSAL_H */
