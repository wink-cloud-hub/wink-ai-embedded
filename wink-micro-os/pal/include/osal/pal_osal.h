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

/**
 * @brief 系统毫秒延时 (主动阻塞让出 CPU 调度)
 */
void pal_os_sleep_ms(uint32_t ms);

/**
 * @brief 系统微秒延时 (高精度短等待)
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

/**
 * @brief 获取互斥锁 (锁定)
 * @param mutex 锁句柄
 * @param timeout_ms 阻塞超时时间，传入 WINK_MUTEX_WAIT_FOREVER 代表无限等待
 * @note 失败型：NULL mutex → WINK_ERR_INVALID_ARG；timeout → WINK_ERR_TIMEOUT；
 *       不支持 target → WINK_ERR_UNSUPPORTED。
 */
WINK_WARN_UNUSED_RESULT wink_status_t pal_os_mutex_lock(pal_os_mutex_t mutex, uint32_t timeout_ms);

/**
 * @brief 释放互斥锁 (解锁)
 */
WINK_WARN_UNUSED_RESULT wink_status_t pal_os_mutex_unlock(pal_os_mutex_t mutex);

/**
 * @brief 销毁互斥锁并释放内存
 */
void pal_os_mutex_destroy(pal_os_mutex_t mutex);


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
 * @brief 进入全局临界区 (屏蔽中断/多核自旋锁)
 * @return 恢复临界区所需的原始状态键值
 * @note 仅用于极短时间的原子操作保护（如环形队列读写、状态标记更新），严禁在临界区内调用阻塞/延时 API。
 */
uint32_t pal_os_critical_enter(void);

/**
 * @brief 退出全局临界区并恢复之前的状态
 * @param key 进入临界区时返回的状态键值
 */
void pal_os_critical_exit(uint32_t key);


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
 */
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

/**
 * @brief Delete a task (PAL 统一接口，替代 FreeRTOS vTaskDelete)
 *
 * @param task_handle Task handle to delete, or NULL to delete current task
 *
 * @note WASM/bare-metal targets may do nothing (single-threaded environment).
 *       App code should not assume task deletion is always available.
 */
void pal_os_task_delete(pal_os_task_handle_t task_handle);


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


#ifdef __cplusplus
}
#endif

#endif /* PAL_OSAL_H */
