/**
 * @file wink_event.c
 * @brief OS Event Queue / mbox asynchronous primitives implementation.
 *
 * Copyright (c) 2026 Wink-AI.
 */
#define LOG_TAG "wink.event"

#include "wink_event.h"
#include "pal_osal.h"
#include "pal_log.h"
#include <string.h>

/* Allow calling blocking semaphore take inside this TU */
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#  pragma warning(disable: 4996)
#endif

static struct {
    pal_os_ringbuf_handle_t ringbuf;
    pal_os_sem_t            sem;
    uint32_t                capacity;
    bool                    initialized;
} s_queue = {0};

static uint32_t round_up_pow2(uint32_t v) {
    if (v == 0) return 1;
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

wink_status_t wink_event_queue_init(uint32_t capacity) {
    if (s_queue.initialized) {
        return WINK_OK; /* Idempotent */
    }

    /* Capacity must be power of 2 (ringbuf requirement) */
    if (capacity == 0 || (capacity & (capacity - 1)) != 0) {
        LOG_E("init: capacity must be a power of 2 (got %u)", (unsigned)capacity);
        return WINK_ERR_INVALID_ARG;
    }

    uint32_t ringbuf_size = round_up_pow2(capacity * sizeof(wink_event_t));
    s_queue.ringbuf = pal_os_ringbuf_create(ringbuf_size);
    if (s_queue.ringbuf == NULL) {
        LOG_E("init: failed to create ringbuffer (size %u)", (unsigned)ringbuf_size);
        return WINK_ERR_NO_MEM;
    }

    s_queue.sem = pal_os_sem_create();
    if (s_queue.sem == NULL) {
        LOG_E("init: failed to create semaphore");
        pal_os_ringbuf_destroy(s_queue.ringbuf);
        s_queue.ringbuf = NULL;
        return WINK_ERR_NO_MEM;
    }

    s_queue.capacity = capacity;
    s_queue.initialized = true;
    return WINK_OK;
}

void wink_event_queue_deinit(void) {
    if (!s_queue.initialized) {
        return;
    }

    s_queue.initialized = false;

    if (s_queue.ringbuf != NULL) {
        pal_os_ringbuf_destroy(s_queue.ringbuf);
        s_queue.ringbuf = NULL;
    }

    if (s_queue.sem != NULL) {
        pal_os_sem_destroy(s_queue.sem);
        s_queue.sem = NULL;
    }

    s_queue.capacity = 0;
}

wink_status_t wink_event_post(const wink_event_t *event) {
    if (!s_queue.initialized) {
        return WINK_ERR_INVALID_STATE;
    }
    if (event == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    /* Push event copy into ringbuffer */
    wink_status_t st = pal_os_ringbuf_push(s_queue.ringbuf, event, sizeof(wink_event_t));
    if (st == WINK_ERR_FULL) {
        LOG_W("post: event queue full");
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }
    if (wink_status_is_error(st)) {
        return st;
    }

    /* Wake up any task blocking on the queue */
    if (pal_os_in_isr()) {
        WINK_IGNORE_RESULT(pal_os_sem_give_isr(s_queue.sem));
    } else {
        WINK_IGNORE_RESULT(pal_os_sem_give(s_queue.sem));
    }

    return WINK_OK;
}

wink_status_t wink_event_pend(wink_event_t *out_event, uint32_t timeout_ms) {
    if (!s_queue.initialized) {
        return WINK_ERR_INVALID_STATE;
    }
    if (out_event == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    /* First try non-blocking pop in case there is already an event in queue */
    wink_status_t st = pal_os_ringbuf_pop(s_queue.ringbuf, out_event, sizeof(wink_event_t));
    if (st == WINK_OK) {
        return WINK_OK;
    }

    if (timeout_ms == 0) {
        return WINK_ERR_EMPTY;
    }

    uint64_t start_ms = pal_os_get_ms();
    uint32_t remaining_ms = timeout_ms;

    while (1) {
        /* Wait on semaphore (blocks) */
        st = pal_os_sem_take(s_queue.sem, (timeout_ms == WINK_MUTEX_WAIT_FOREVER) ? WINK_MUTEX_WAIT_FOREVER : remaining_ms);
        if (st == WINK_ERR_TIMEOUT) {
            return WINK_ERR_TIMEOUT;
        }
        if (wink_status_is_error(st)) {
            return st;
        }

        /* Try pop again after waking up */
        st = pal_os_ringbuf_pop(s_queue.ringbuf, out_event, sizeof(wink_event_t));
        if (st == WINK_OK) {
            return WINK_OK;
        }

        /* Spurious wakeup or pop failed: recompute timeout */
        if (timeout_ms != WINK_MUTEX_WAIT_FOREVER) {
            uint64_t elapsed_ms = pal_os_get_ms() - start_ms;
            if (elapsed_ms >= timeout_ms) {
                return WINK_ERR_TIMEOUT;
            }
            remaining_ms = timeout_ms - (uint32_t)elapsed_ms;
            if (remaining_ms == 0) {
                return WINK_ERR_TIMEOUT;
            }
        }
    }
}
