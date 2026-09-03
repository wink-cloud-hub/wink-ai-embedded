// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_deferred_wasm.c
 * @brief Wasm implementation of PAL Deferred-Call Worker subsystem.
 */
#include "osal/pal_deferred.h"
#include <string.h>

typedef struct {
    pal_deferred_cb_t cb;
    void             *arg;
} pal_deferred_slot_t;

typedef struct {
    pal_deferred_slot_t  *slots;
    size_t                capacity;
    size_t                head;
    size_t                tail;
    size_t                count;
    size_t                high_water;
    uint32_t              dropped;
} pal_deferred_wasm_queue_t;

static pal_deferred_slot_t s_hi_slots[PAL_DEFERRED_HI_QUEUE_CAPACITY];
static pal_deferred_slot_t s_lo_slots[PAL_DEFERRED_LO_QUEUE_CAPACITY];
static pal_deferred_wasm_queue_t s_queues[PAL_DEFERRED_PRI_COUNT];
static bool s_initialized = false;

wink_status_t pal_deferred_init(uint8_t core_id) {
    (void)core_id;
    if (s_initialized) {
        return WINK_OK;
    }

    memset(s_queues, 0, sizeof(s_queues));
    s_queues[PAL_DEFERRED_HI].slots = s_hi_slots;
    s_queues[PAL_DEFERRED_HI].capacity = PAL_DEFERRED_HI_QUEUE_CAPACITY;

    s_queues[PAL_DEFERRED_LO].slots = s_lo_slots;
    s_queues[PAL_DEFERRED_LO].capacity = PAL_DEFERRED_LO_QUEUE_CAPACITY;

    s_initialized = true;
    return WINK_OK;
}

void pal_deferred_deinit(void) {
    s_initialized = false;
}

void pal_wasm_drain_deferred(void) {
    if (!s_initialized) return;

    for (int p = 0; p < PAL_DEFERRED_PRI_COUNT; p++) {
        pal_deferred_wasm_queue_t *q = &s_queues[p];
        while (q->count > 0) {
            pal_deferred_cb_t cb = q->slots[q->tail].cb;
            void *arg = q->slots[q->tail].arg;
            q->slots[q->tail].cb = NULL;
            q->slots[q->tail].arg = NULL;
            q->tail = (q->tail + 1) % q->capacity;
            q->count--;

            if (cb != NULL) {
                cb(arg);
            }
        }
    }
}

static wink_status_t deferred_post_internal(pal_deferred_pri_t pri,
                                            pal_deferred_policy_t policy,
                                            pal_deferred_cb_t cb,
                                            void *arg) {
    if (!s_initialized) {
        wink_status_t init_status = pal_deferred_init(0);
        if (init_status != WINK_OK) {
            return init_status;
        }
    }
    if (pri >= PAL_DEFERRED_PRI_COUNT || cb == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_deferred_wasm_queue_t *q = &s_queues[pri];
    if (q->count >= q->capacity) {
        q->dropped++;
        return WINK_ERR_BUSY;
    }

    q->slots[q->head].cb = cb;
    q->slots[q->head].arg = arg;
    q->head = (q->head + 1) % q->capacity;
    q->count++;
    if (q->count > q->high_water) {
        q->high_water = q->count;
    }

    return WINK_OK;
}

wink_status_t pal_deferred_post_from_isr(pal_deferred_pri_t pri,
                                        pal_deferred_policy_t policy,
                                        pal_deferred_cb_t cb,
                                        void *arg) {
    return deferred_post_internal(pri, policy, cb, arg);
}

wink_status_t pal_deferred_post(pal_deferred_pri_t pri,
                               pal_deferred_policy_t policy,
                               pal_deferred_cb_t cb,
                               void *arg) {
    return deferred_post_internal(pri, policy, cb, arg);
}

void pal_deferred_get_metrics(pal_deferred_pri_t pri,
                             size_t *out_high_water_slots,
                             uint32_t *out_dropped_count) {
    if (pri >= PAL_DEFERRED_PRI_COUNT) {
        if (out_high_water_slots) *out_high_water_slots = 0;
        if (out_dropped_count) *out_dropped_count = 0;
        return;
    }

    pal_deferred_wasm_queue_t *q = &s_queues[pri];
    if (out_high_water_slots) *out_high_water_slots = q->high_water;
    if (out_dropped_count) *out_dropped_count = q->dropped;
}
