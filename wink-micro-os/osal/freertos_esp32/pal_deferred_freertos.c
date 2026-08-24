// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_deferred_freertos.c
 * @brief FreeRTOS ESP32 implementation of PAL Deferred-Call Worker subsystem.
 */
#include "osal/pal_deferred.h"
#include "pal_spinlock.h"
#include "pal_log.h"
#include <string.h>

#if defined(ESP_PLATFORM) || defined(FREERTOS_CONFIG_H) || defined(INC_FREERTOS_H)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define LOG_TAG "pal_deferred"

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
    pal_spinlock_t        lock;
    SemaphoreHandle_t     sem;
    StaticSemaphore_t     sem_buffer;
    TaskHandle_t          task_handle;
    StaticTask_t          tcb;
    StackType_t          *stack;
    uint32_t              stack_words;
} pal_deferred_queue_t;

#define HI_STACK_WORDS 2048
#define LO_STACK_WORDS 3072

static pal_deferred_slot_t s_hi_slots[PAL_DEFERRED_HI_QUEUE_CAPACITY];
static pal_deferred_slot_t s_lo_slots[PAL_DEFERRED_LO_QUEUE_CAPACITY];
static StackType_t         s_hi_stack[HI_STACK_WORDS];
static StackType_t         s_lo_stack[LO_STACK_WORDS];

static pal_deferred_queue_t s_queues[PAL_DEFERRED_PRI_COUNT];
static bool s_initialized = false;

static void deferred_worker_task(void *arg) {
    pal_deferred_pri_t pri = (pal_deferred_pri_t)(uintptr_t)arg;
    pal_deferred_queue_t *q = &s_queues[pri];

    while (1) {
        /* Wait for work to arrive */
        if (xSemaphoreTake(q->sem, portMAX_DELAY) == pdTRUE) {
            /* Drain all pending work in a loop to ensure zero missed notifications */
            while (1) {
                pal_deferred_cb_t cb = NULL;
                void *cb_arg = NULL;

                pal_spinlock_lock(&q->lock);
                if (q->count > 0) {
                    cb = q->slots[q->tail].cb;
                    cb_arg = q->slots[q->tail].arg;
                    q->slots[q->tail].cb = NULL;
                    q->slots[q->tail].arg = NULL;
                    q->tail = (q->tail + 1) % q->capacity;
                    q->count--;
                }
                pal_spinlock_unlock(&q->lock);

                if (cb == NULL) {
                    break;
                }

                /* Execute callback in worker task context */
                cb(cb_arg);
            }
        }
    }
}

wink_status_t pal_deferred_init(uint8_t core_id) {
    if (s_initialized) {
        return WINK_OK;
    }

    memset(s_queues, 0, sizeof(s_queues));

    /* Initialize HI Priority Queue */
    s_queues[PAL_DEFERRED_HI].slots = s_hi_slots;
    s_queues[PAL_DEFERRED_HI].capacity = PAL_DEFERRED_HI_QUEUE_CAPACITY;
    s_queues[PAL_DEFERRED_HI].stack = s_hi_stack;
    s_queues[PAL_DEFERRED_HI].stack_words = HI_STACK_WORDS;
    pal_spinlock_init(&s_queues[PAL_DEFERRED_HI].lock);
    s_queues[PAL_DEFERRED_HI].sem = xSemaphoreCreateCountingStatic(
        PAL_DEFERRED_HI_QUEUE_CAPACITY,
        0,
        &s_queues[PAL_DEFERRED_HI].sem_buffer
    );

    /* Initialize LO Priority Queue */
    s_queues[PAL_DEFERRED_LO].slots = s_lo_slots;
    s_queues[PAL_DEFERRED_LO].capacity = PAL_DEFERRED_LO_QUEUE_CAPACITY;
    s_queues[PAL_DEFERRED_LO].stack = s_lo_stack;
    s_queues[PAL_DEFERRED_LO].stack_words = LO_STACK_WORDS;
    pal_spinlock_init(&s_queues[PAL_DEFERRED_LO].lock);
    s_queues[PAL_DEFERRED_LO].sem = xSemaphoreCreateCountingStatic(
        PAL_DEFERRED_LO_QUEUE_CAPACITY,
        0,
        &s_queues[PAL_DEFERRED_LO].sem_buffer
    );

    /* Create Worker Tasks statically */
    BaseType_t hi_core = (core_id == 0xFF) ? tskNO_AFFINITY : (BaseType_t)(core_id & 0x01);
    UBaseType_t hi_prio = configMAX_PRIORITIES - 2;
    if (hi_prio <= tskIDLE_PRIORITY) {
        hi_prio = tskIDLE_PRIORITY + 1;
    }

    s_queues[PAL_DEFERRED_HI].task_handle = xTaskCreatePinnedToCoreStatic(
        deferred_worker_task,
        "pal_defer_hi",
        HI_STACK_WORDS,
        (void *)(uintptr_t)PAL_DEFERRED_HI,
        hi_prio,
        s_hi_stack,
        &s_queues[PAL_DEFERRED_HI].tcb,
        hi_core
    );

    UBaseType_t lo_prio = tskIDLE_PRIORITY + 2;
    s_queues[PAL_DEFERRED_LO].task_handle = xTaskCreatePinnedToCoreStatic(
        deferred_worker_task,
        "pal_defer_lo",
        LO_STACK_WORDS,
        (void *)(uintptr_t)PAL_DEFERRED_LO,
        lo_prio,
        s_lo_stack,
        &s_queues[PAL_DEFERRED_LO].tcb,
        tskNO_AFFINITY
    );

    s_initialized = true;
    return WINK_OK;
}

void pal_deferred_deinit(void) {
    if (!s_initialized) {
        return;
    }
    for (int i = 0; i < PAL_DEFERRED_PRI_COUNT; i++) {
        if (s_queues[i].task_handle != NULL) {
            vTaskDelete(s_queues[i].task_handle);
            s_queues[i].task_handle = NULL;
        }
    }
    s_initialized = false;
}

wink_status_t pal_deferred_post_from_isr(pal_deferred_pri_t pri,
                                        pal_deferred_policy_t policy,
                                        pal_deferred_cb_t cb,
                                        void *arg) {
    if (!s_initialized || pri >= PAL_DEFERRED_PRI_COUNT || cb == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_deferred_queue_t *q = &s_queues[pri];
    bool posted = false;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    pal_spinlock_lock_isr(&q->lock);
    if (q->count < q->capacity) {
        q->slots[q->head].cb = cb;
        q->slots[q->head].arg = arg;
        q->head = (q->head + 1) % q->capacity;
        q->count++;
        if (q->count > q->high_water) {
            q->high_water = q->count;
        }
        posted = true;
    } else {
        q->dropped++;
    }
    pal_spinlock_unlock_isr(&q->lock);

    if (!posted) {
        if (policy == PAL_DEFERRED_CRITICAL) {
            /* Log critical error if available */
        }
        return WINK_ERR_BUSY;
    }

    xSemaphoreGiveFromISR(q->sem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

    return WINK_OK;
}

wink_status_t pal_deferred_post(pal_deferred_pri_t pri,
                               pal_deferred_policy_t policy,
                               pal_deferred_cb_t cb,
                               void *arg) {
    if (!s_initialized || pri >= PAL_DEFERRED_PRI_COUNT || cb == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_deferred_queue_t *q = &s_queues[pri];
    bool posted = false;

    pal_spinlock_lock(&q->lock);
    if (q->count < q->capacity) {
        q->slots[q->head].cb = cb;
        q->slots[q->head].arg = arg;
        q->head = (q->head + 1) % q->capacity;
        q->count++;
        if (q->count > q->high_water) {
            q->high_water = q->count;
        }
        posted = true;
    } else {
        q->dropped++;
    }
    pal_spinlock_unlock(&q->lock);

    if (!posted) {
        return WINK_ERR_BUSY;
    }

    xSemaphoreGive(q->sem);
    return WINK_OK;
}

void pal_deferred_get_metrics(pal_deferred_pri_t pri,
                             size_t *out_high_water_slots,
                             uint32_t *out_dropped_count) {
    if (pri >= PAL_DEFERRED_PRI_COUNT) {
        if (out_high_water_slots) *out_high_water_slots = 0;
        if (out_dropped_count) *out_dropped_count = 0;
        return;
    }

    pal_deferred_queue_t *q = &s_queues[pri];
    pal_spinlock_lock(&q->lock);
    if (out_high_water_slots) *out_high_water_slots = q->high_water;
    if (out_dropped_count) *out_dropped_count = q->dropped;
    pal_spinlock_unlock(&q->lock);
}

#else

/* Non-FreeRTOS Fallback for Static Analysis */
wink_status_t pal_deferred_init(uint8_t core_id) { (void)core_id; return WINK_OK; }
void pal_deferred_deinit(void) {}
wink_status_t pal_deferred_post_from_isr(pal_deferred_pri_t pri, pal_deferred_policy_t policy, pal_deferred_cb_t cb, void *arg) { (void)pri; (void)policy; if (cb) cb(arg); return WINK_OK; }
wink_status_t pal_deferred_post(pal_deferred_pri_t pri, pal_deferred_policy_t policy, pal_deferred_cb_t cb, void *arg) { (void)pri; (void)policy; if (cb) cb(arg); return WINK_OK; }
void pal_deferred_get_metrics(pal_deferred_pri_t pri, size_t *out_high_water_slots, uint32_t *out_dropped_count) { (void)pri; if (out_high_water_slots) *out_high_water_slots = 0; if (out_dropped_count) *out_dropped_count = 0; }

#endif
