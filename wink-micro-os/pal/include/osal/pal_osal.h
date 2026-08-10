// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_osal.h
 * @brief PAL Operating System & Kernel Environment Abstraction Layer (OSAL).
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
/*                                1. Time & Delay                             */
/* ========================================================================== */

#ifndef WINK_STRICT_NONBLOCKING
/**
 * @brief System millisecond sleep delay (blocking CPU yield)
 * @param[in] ms Milliseconds to sleep
 * @note Blocking: Yes. Not available under WINK_STRICT_NONBLOCKING.
 */
WINK_BLOCKING
void pal_os_sleep_ms(uint32_t ms);
#endif /* WINK_STRICT_NONBLOCKING */

/**
 * @brief System microsecond busy wait delay (high precision short delay)
 * @param[in] us Microseconds to busy wait
 */
void pal_os_busy_wait_us(uint32_t us);

/**
 * @brief Get system uptime in milliseconds
 * @return Uptime in milliseconds
 */
uint64_t pal_os_get_ms(void);

/**
 * @brief Get system uptime in microseconds
 * @return Uptime in microseconds
 */
uint64_t pal_os_get_us(void);

/* ========================================================================== */
/*                             2. Mutex & Semaphore                           */
/* ========================================================================== */

typedef void* pal_os_mutex_t;

/** @brief Wait forever sentinel value for mutex lock timeout. */
#define WINK_MUTEX_WAIT_FOREVER 0xFFFFFFFFu

/**
 * @brief Create a mutex handle
 * @return Mutex handle or NULL on failure
 */
pal_os_mutex_t pal_os_mutex_create(void);

#ifndef WINK_STRICT_NONBLOCKING
/**
 * @brief Acquire mutex lock
 * @param[in] mutex Mutex handle
 * @param[in] timeout_ms Timeout in milliseconds or WINK_MUTEX_WAIT_FOREVER
 * @return WINK_OK on success, WINK_ERR_TIMEOUT on timeout, WINK_ERR_INVALID_ARG if NULL
 */
WINK_BLOCKING
WINK_WARN_UNUSED_RESULT wink_status_t pal_os_mutex_lock(pal_os_mutex_t mutex, uint32_t timeout_ms);
#endif /* WINK_STRICT_NONBLOCKING */

/**
 * @brief Release mutex lock
 * @param[in] mutex Mutex handle
 * @return WINK_OK on success
 */
WINK_WARN_UNUSED_RESULT wink_status_t pal_os_mutex_unlock(pal_os_mutex_t mutex);

/**
 * @brief Destroy mutex handle and free memory
 * @param[in] mutex Mutex handle
 */
void pal_os_mutex_destroy(pal_os_mutex_t mutex);

typedef void* pal_os_sem_t;

/**
 * @brief Create a binary semaphore handle
 * @return Semaphore handle or NULL on failure
 */
WINK_WARN_UNUSED_RESULT pal_os_sem_t pal_os_sem_create(void);

#ifndef WINK_STRICT_NONBLOCKING
/**
 * @brief Take binary semaphore (wait)
 * @param[in] sem Semaphore handle
 * @param[in] timeout_ms Timeout in milliseconds or WINK_MUTEX_WAIT_FOREVER
 * @return WINK_OK on success, WINK_ERR_TIMEOUT on timeout
 */
WINK_BLOCKING
WINK_WARN_UNUSED_RESULT wink_status_t pal_os_sem_take(pal_os_sem_t sem, uint32_t timeout_ms);
#endif /* WINK_STRICT_NONBLOCKING */

/**
 * @brief Give binary semaphore (post) from TASK context
 * @param[in] sem Semaphore handle
 * @return WINK_OK on success
 */
WINK_WARN_UNUSED_RESULT wink_status_t pal_os_sem_give(pal_os_sem_t sem);

/**
 * @brief Give binary semaphore (post) from ISR context
 * @param[in] sem Semaphore handle
 * @return WINK_OK on success
 */
WINK_WARN_UNUSED_RESULT wink_status_t pal_os_sem_give_isr(pal_os_sem_t sem);

/**
 * @brief Destroy binary semaphore handle and free memory
 * @param[in] sem Semaphore handle
 */
void pal_os_sem_destroy(pal_os_sem_t sem);

/* ========================================================================== */
/*                      3. Watchdog & Reset Reason (WDT / Reset)              */
/* ========================================================================== */

typedef enum {
    PAL_OS_RESET_REASON_UNKNOWN  = 0,
    PAL_OS_RESET_REASON_POWER_ON = 1,
    PAL_OS_RESET_REASON_WATCHDOG = 2,
    PAL_OS_RESET_REASON_PANIC    = 3,
    PAL_OS_RESET_REASON_SOFTWARE = 4,
    PAL_OS_RESET_REASON_BROWNOUT = 5,
} pal_os_reset_reason_t;

/**
 * @brief Read previous system reset reason
 * @return Reset reason enum
 */
pal_os_reset_reason_t pal_os_get_reset_reason(void);

/**
 * @brief Read consecutive abnormal reset count (for boot safe-lock strategy)
 * @return Reset count integer
 */
uint32_t pal_os_get_abnormal_boot_count(void);

/**
 * @brief Set consecutive abnormal reset count
 * @param[in] count New reset count
 */
void pal_os_set_abnormal_boot_count(uint32_t count);

/**
 * @brief Initialize hardware watchdog timer
 * @param[in] timeout_ms Watchdog timeout in milliseconds
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT wink_status_t pal_os_wdt_init(uint32_t timeout_ms);

/**
 * @brief Feed hardware watchdog timer
 * @return WINK_OK on success
 */
WINK_WARN_UNUSED_RESULT wink_status_t pal_os_wdt_feed(void);

/* ========================================================================== */
/*                         4. Global Critical Section                        */
/* ========================================================================== */

/**
 * @brief Enter global critical section (TASK context)
 * @return Primitive mask state key to be restored
 */
uint32_t pal_os_critical_enter(void);

/**
 * @brief Exit global critical section (TASK context)
 * @param[in] key Primitive mask state key returned by pal_os_critical_enter()
 */
void pal_os_critical_exit(uint32_t key);

/**
 * @brief Enter global critical section (ISR context)
 * @return Primitive mask state key to be restored
 */
uint32_t pal_os_critical_enter_isr(void);

/**
 * @brief Exit global critical section (ISR context)
 * @param[in] key Primitive mask state key
 */
void pal_os_critical_exit_isr(uint32_t key);

/**
 * @brief Set simulated ISR context flag (Host/Wasm simulation only)
 * @param[in] in_isr State flag
 */
void pal_os_set_sim_isr_context(bool in_isr);

/**
 * @brief Read simulated ISR context flag
 * @return true if in simulated ISR, false otherwise
 */
bool pal_os_in_sim_isr_context(void);
 
/**
 * @brief Query if current execution is in ISR context
 * @return true if in ISR context, false otherwise
 */
bool pal_os_in_isr(void);

/**
 * @brief Set simulated PT coroutine context flag
 * @param[in] in_pt State flag
 */
void pal_os_set_sim_pt_context(bool in_pt);

/**
 * @brief Read simulated PT coroutine context flag
 * @return true if in simulated PT coroutine, false otherwise
 */
bool pal_os_in_sim_pt_context(void);

/* ========================================================================== */
/*                         5. Task Creation & Affinity                       */
/* ========================================================================== */

typedef void* pal_os_task_handle_t;

typedef enum {
    PAL_OS_CORE_0   = 0,  /**< Pin to Core 0 */
    PAL_OS_CORE_1   = 1,  /**< Pin to Core 1 */
    PAL_OS_CORE_ANY = -1, /**< No core affinity */
} pal_os_core_id_t;

#ifndef WINK_STRICT_NONBLOCKING
/**
 * @brief Create a new task with optional core affinity
 * @param[in] func Task entry function
 * @param[in] name Task name string
 * @param[in] stack_depth Stack size in bytes
 * @param[in] arg Context argument pointer
 * @param[in] priority Task priority level
 * @param[in] core_id Core affinity
 * @param[out] task_handle Output pointer for task handle
 * @return WINK_OK on success, error status code otherwise
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
 * @brief Delete task instance
 * @param[in] task_handle Task handle to delete or NULL for current calling task
 */
void pal_os_task_delete(pal_os_task_handle_t task_handle);

/**
 * @brief Return current free heap size in bytes
 * @return Free bytes
 */
uint32_t pal_os_get_free_heap_size(void);

/**
 * @brief Return minimum free heap size seen since boot
 * @return Minimum free bytes
 */
uint32_t pal_os_get_min_free_heap_size(void);

/**
 * @brief Return unused stack bytes for calling task
 * @return Unused stack bytes
 */
uint32_t pal_os_get_current_task_stack_free(void);

/* ========================================================================== */
/*                      6. Inter-Core Ring Buffer (Ringbuf)                   */
/* ========================================================================== */

typedef struct pal_os_ringbuf* pal_os_ringbuf_handle_t;

/**
 * @brief Create a new ring buffer
 * @param[in] size Capacity in bytes
 * @return Ring buffer handle or NULL on failure
 */
pal_os_ringbuf_handle_t pal_os_ringbuf_create(uint32_t size);

/**
 * @brief Push data to ring buffer (non-blocking)
 * @param[in] rb Ring buffer handle
 * @param[in] data Data buffer pointer
 * @param[in] size Data size in bytes
 * @return WINK_OK on success, WINK_ERR_FULL if buffer full
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_os_ringbuf_push(
    pal_os_ringbuf_handle_t rb,
    const void* data,
    uint32_t size
);

/**
 * @brief Pop data from ring buffer (non-blocking)
 * @param[in] rb Ring buffer handle
 * @param[out] data Output data buffer
 * @param[in] size Number of bytes to pop
 * @return WINK_OK on success, WINK_ERR_EMPTY if buffer empty
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_os_ringbuf_pop(
    pal_os_ringbuf_handle_t rb,
    void* data,
    uint32_t size
);

/**
 * @brief Search unconsumed ringbuffer entries and coalesce (update in-place) matching entry.
 */
bool pal_os_ringbuf_coalesce_event(
    pal_os_ringbuf_handle_t rb,
    const void* event_ptr,
    uint32_t event_size,
    uint32_t match_offset,
    uint32_t match_size
);

/**
 * @brief Get used bytes in ring buffer
 * @param[in] rb Ring buffer handle
 * @return Number of available bytes
 */
uint32_t pal_os_ringbuf_used(pal_os_ringbuf_handle_t rb);

/**
 * @brief Destroy ring buffer handle
 * @param[in] rb Ring buffer handle
 */
void pal_os_ringbuf_destroy(pal_os_ringbuf_handle_t rb);

/* ========================================================================== */
/*                      7. Simulation Mode Control Interfaces                  */
/* ========================================================================== */

typedef enum {
    WINK_SIM_MODE_INTERACTIVE = 0,
    WINK_SIM_MODE_HEADLESS    = 1,
} wink_sim_mode_t;

/**
 * @brief Set simulation execution mode
 * @param[in] mode Target simulation mode
 */
void wink_sim_set_mode(wink_sim_mode_t mode);

/**
 * @brief Query current simulation execution mode
 * @return Current simulation mode
 */
wink_sim_mode_t wink_sim_get_mode(void);

#ifdef __cplusplus
}
#endif

#endif /* PAL_OSAL_H */
