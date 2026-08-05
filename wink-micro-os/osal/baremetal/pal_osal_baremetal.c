// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_osal_baremetal.c
 * @brief Bare-metal (no RTOS) PAL OSAL implementation.
 *
 * Designed for low-end MCUs without FreeRTOS:
 * - Task creation returns WINK_ERR_UNSUPPORTED
 * - Ring buffer uses interrupt disable critical section for atomicity
 * - All time functions require BSP implementation
 */

#include "pal_osal.h"
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------
 * System Time and Delay (BSP implementation required)
 * --------------------------------------------------------- */

/* BSP provided clock access functions */
extern uint64_t pal_bsp_get_us(void);
extern void pal_bsp_delay_us(uint32_t us);

void pal_os_sleep_ms(uint32_t ms) {
    while (ms > 0) {
        pal_bsp_delay_us(1000);
        ms--;
    }
}

void pal_os_busy_wait_us(uint32_t us) {
    pal_bsp_delay_us(us);
}

uint64_t pal_os_get_ms(void) {
    return pal_bsp_get_us() / 1000U;
}

uint64_t pal_os_get_us(void) {
    return pal_bsp_get_us();
}

/* ---------------------------------------------------------
 * Mutex (Bare-metal: interrupt disable fallback)
 * --------------------------------------------------------- */

pal_os_mutex_t pal_os_mutex_create(void) {
    /* No-op - bare metal single-threaded */
    return (pal_os_mutex_t)1;
}

wink_status_t pal_os_mutex_lock(pal_os_mutex_t mutex, uint32_t timeout_ms) {
    if (mutex == NULL) return WINK_ERR_INVALID_ARG;
    (void)timeout_ms;
    /* No-op - bare metal single-threaded */
    return WINK_OK;
}

wink_status_t pal_os_mutex_unlock(pal_os_mutex_t mutex) {
    if (mutex == NULL) return WINK_ERR_INVALID_ARG;
    /* No-op - bare metal single-threaded */
    return WINK_OK;
}

void pal_os_mutex_destroy(pal_os_mutex_t mutex) {
    (void)mutex;
}

/* ---------------------------------------------------------
 * Semaphore (Bare-metal: no-op fallback implementation)
 * --------------------------------------------------------- */

pal_os_sem_t pal_os_sem_create(void) {
    return (pal_os_sem_t)1;
}

wink_status_t pal_os_sem_take(pal_os_sem_t sem, uint32_t timeout_ms) {
    if (sem == NULL) return WINK_ERR_INVALID_ARG;
    (void)timeout_ms;
    return WINK_OK;
}

wink_status_t pal_os_sem_give(pal_os_sem_t sem) {
    if (sem == NULL) return WINK_ERR_INVALID_ARG;
    return WINK_OK;
}

wink_status_t pal_os_sem_give_isr(pal_os_sem_t sem) {
    if (sem == NULL) return WINK_ERR_INVALID_ARG;
    return WINK_OK;
}

void pal_os_sem_destroy(pal_os_sem_t sem) {
    (void)sem;
}

/* ---------------------------------------------------------
 * Reset Reason and Watchdog
 * --------------------------------------------------------- */

pal_os_reset_reason_t pal_os_get_reset_reason(void) {
    /* BSP may override to provide real reset reason */
    return PAL_OS_RESET_REASON_UNKNOWN;
}

/* ADR-0010: Bare-metal no persistent boot count semantics (BSP overridable) */
uint32_t pal_os_get_abnormal_boot_count(void) { return 0; }
void pal_os_set_abnormal_boot_count(uint32_t count) { (void)count; }

WINK_WARN_UNUSED_RESULT wink_status_t pal_os_wdt_init(uint32_t timeout_ms) {
    (void)timeout_ms;
    /* BSP may provide hardware WDT implementation */
    return WINK_ERR_UNSUPPORTED;
}

WINK_WARN_UNUSED_RESULT wink_status_t pal_os_wdt_feed(void) {
    return WINK_ERR_UNSUPPORTED;
}

/* ---------------------------------------------------------
 * Critical Section (Explicit task/ISR dual-entry dispatch, ADR-0016)
 * --------------------------------------------------------- */

/* BSP provided interrupt control functions */
extern uint32_t pal_bsp_irq_save(void);
extern void pal_bsp_irq_restore(uint32_t state);

uint32_t pal_os_critical_enter(void) {
    return pal_bsp_irq_save();
}

void pal_os_critical_exit(uint32_t key) {
    pal_bsp_irq_restore(key);
}

uint32_t pal_os_critical_enter_isr(void) {
    return pal_bsp_irq_save();
}

void pal_os_critical_exit_isr(uint32_t key) {
    pal_bsp_irq_restore(key);
}

void pal_os_set_sim_isr_context(bool in_isr) { (void)in_isr; }
bool pal_os_in_sim_isr_context(void) { return false; }
bool pal_os_in_isr(void) { return false; }

/* ---------------------------------------------------------
 * Task Creation (Bare-metal unsupported)
 * --------------------------------------------------------- */

wink_status_t pal_os_task_create(
    void (*func)(void* arg),
    const char* name,
    uint32_t stack_depth,
    void* arg,
    int32_t priority,
    pal_os_core_id_t core_id,
    pal_os_task_handle_t* task_handle
) {
    /* Bare-metal without RTOS: no multi-tasking support */
    (void)func; (void)name; (void)stack_depth; (void)arg;
    (void)priority; (void)core_id; (void)task_handle;
    return WINK_ERR_UNSUPPORTED;
}

void pal_os_task_delete(pal_os_task_handle_t task_handle) {
    (void)task_handle;  /* bare-metal: no-op */
}

/* ---------------------------------------------------------
 * Ring Buffer (Bare-metal interrupt disable protection)
 * --------------------------------------------------------- */

struct pal_os_ringbuf {
    uint8_t* buffer;
    uint32_t size;
    volatile uint32_t head;
    volatile uint32_t tail;
};

pal_os_ringbuf_handle_t pal_os_ringbuf_create(uint32_t size) {
    struct pal_os_ringbuf* rb;

    /* Size must be power of 2 (API contract) */
    if ((size & (size - 1)) != 0) {
        return NULL;
    }

    rb = malloc(sizeof(struct pal_os_ringbuf));
    if (rb == NULL) {
        return NULL;
    }

    rb->buffer = malloc(size);
    if (rb->buffer == NULL) {
        free(rb);
        return NULL;
    }

    rb->size = size;
    rb->head = 0;
    rb->tail = 0;

    return rb;
}

wink_status_t pal_os_ringbuf_push(
    pal_os_ringbuf_handle_t rb,
    const void* data,
    uint32_t size
) {
    uint32_t i;
    const uint8_t* src = (const uint8_t*)data;
    uint32_t key;

    if (rb == NULL || data == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    key = pal_os_critical_enter();

    /* Inline usage calculation (rb->head - rb->tail) to avoid re-entering critical section */
    if ((rb->head - rb->tail) + size > rb->size) {
        pal_os_critical_exit(key);
        return WINK_ERR_FULL;
    }

    for (i = 0; i < size; i++) {
        rb->buffer[rb->head & (rb->size - 1)] = src[i];
        rb->head++;
    }

    pal_os_critical_exit(key);
    return WINK_OK;
}

wink_status_t pal_os_ringbuf_pop(
    pal_os_ringbuf_handle_t rb,
    void* data,
    uint32_t size
) {
    uint32_t i;
    uint8_t* dst = (uint8_t*)data;
    uint32_t key;

    if (rb == NULL || data == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    key = pal_os_critical_enter();

    /* Inline usage calculation */
    if ((rb->head - rb->tail) < size) {
        pal_os_critical_exit(key);
        return WINK_ERR_EMPTY;
    }

    for (i = 0; i < size; i++) {
        dst[i] = rb->buffer[rb->tail & (rb->size - 1)];
        rb->tail++;
    }

    pal_os_critical_exit(key);
    return WINK_OK;
}

uint32_t pal_os_ringbuf_used(pal_os_ringbuf_handle_t rb) {
    uint32_t used, key;

    if (rb == NULL) {
        return 0;
    }

    key = pal_os_critical_enter();
    used = rb->head - rb->tail;
    pal_os_critical_exit(key);

    return used;
}

void pal_os_ringbuf_destroy(pal_os_ringbuf_handle_t rb) {
    if (rb == NULL) {
        return;
    }

    free(rb->buffer);
    free(rb);
}
