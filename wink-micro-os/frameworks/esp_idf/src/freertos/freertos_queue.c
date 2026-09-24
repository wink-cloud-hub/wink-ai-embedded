/* SPDX-License-Identifier: LGPL-3.0-only */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos_sync.h"
#include "wink_sim_scheduler.h"
#include "pal_log.h"

#define FREERTOS_MAX_QUEUES 8
#define FREERTOS_QUEUE_STORAGE_SIZE 512

typedef struct {
    bool     used;
    uint32_t item_size;
    uint32_t max_items;
    uint32_t cur_items;
    uint32_t read_idx;
    uint32_t write_idx;
    uint8_t  storage[FREERTOS_QUEUE_STORAGE_SIZE];
    uint32_t rx_waiters[WINK_SIM_MAX_TASKS];
    uint8_t  rx_waiter_count;
    uint32_t tx_waiters[WINK_SIM_MAX_TASKS];
    uint8_t  tx_waiter_count;
} esp_queue_t;

_Static_assert(sizeof(esp_queue_t) <= 640, "esp_queue_t size budget exceeded");

static esp_queue_t s_queues[FREERTOS_MAX_QUEUES];

static inline esp_queue_t* resolve_queue(QueueHandle_t q) {
    if (q == NULL) return NULL;
    esp_queue_t* candidate = (esp_queue_t*)q;
    if (candidate < &s_queues[0] || candidate >= &s_queues[FREERTOS_MAX_QUEUES]) {
        return NULL;
    }
    if (!candidate->used) return NULL;
    return candidate;
}

static void waiter_add(uint32_t* arr, uint8_t* count, uint32_t sim_id) {
    for (uint8_t i = 0; i < *count; ++i) {
        if (arr[i] == sim_id) return;
    }
    if (*count < WINK_SIM_MAX_TASKS) {
        arr[(*count)++] = sim_id;
    }
}

static void waiter_remove(uint32_t* arr, uint8_t* count, uint32_t sim_id) {
    for (uint8_t i = 0; i < *count; ++i) {
        if (arr[i] == sim_id) {
            for (uint8_t j = i; j + 1 < *count; ++j) {
                arr[j] = arr[j + 1];
            }
            (*count)--;
            return;
        }
    }
}

static uint32_t waiter_pop_first(uint32_t* arr, uint8_t* count) {
    if (*count == 0) return SIM_SCHED_NO_READY;
    uint32_t res = arr[0];
    for (uint8_t j = 0; j + 1 < *count; ++j) {
        arr[j] = arr[j + 1];
    }
    (*count)--;
    return res;
}

void esp_freertos_queue_pool_reset(void) {
    memset(s_queues, 0, sizeof(s_queues));
}

QueueHandle_t xQueueCreate(const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize) {
    if (uxQueueLength == 0) {
        return NULL;
    }
    uint64_t total_bytes = (uint64_t)uxQueueLength * (uint64_t)uxItemSize;
    if (total_bytes > FREERTOS_QUEUE_STORAGE_SIZE) {
        pal_log_w("FREERTOS", "Queue size %llu exceeds 512B budget", (unsigned long long)total_bytes);
        return NULL;
    }

    for (uint32_t i = 0; i < FREERTOS_MAX_QUEUES; ++i) {
        if (!s_queues[i].used) {
            memset(&s_queues[i], 0, sizeof(esp_queue_t));
            s_queues[i].used = true;
            s_queues[i].max_items = (uint32_t)uxQueueLength;
            s_queues[i].item_size = (uint32_t)uxItemSize;
            return (QueueHandle_t)&s_queues[i];
        }
    }
    pal_log_w("FREERTOS", "No free queue slot");
    return NULL;
}

BaseType_t xQueueSend(QueueHandle_t xQueue, const void * const pvItemToQueue, TickType_t xTicksToWait) {
    esp_queue_t* q = resolve_queue(xQueue);
    if (q == NULL) {
        return errQUEUE_FULL;
    }

    uint32_t q_idx = (uint32_t)(q - s_queues);

    while (q->cur_items >= q->max_items) {
        if (xTicksToWait == 0) {
            return errQUEUE_FULL;
        }
        uint32_t self = sim_scheduler_current_id();
        if (self == SIM_SCHED_NO_READY) {
            return errQUEUE_FULL;
        }

        waiter_add(q->tx_waiters, &q->tx_waiter_count, self);
        bool ok = sync_block(FREERTOS_MAKE_RES_ID(FREERTOS_TAG_QUEUE, q_idx), xTicksToWait);
        waiter_remove(q->tx_waiters, &q->tx_waiter_count, self);

        if (!ok && q->cur_items >= q->max_items) {
            return errQUEUE_FULL;
        }
    }

    if (pvItemToQueue != NULL && q->item_size > 0) {
        memcpy(&q->storage[q->write_idx * q->item_size], pvItemToQueue, q->item_size);
    }
    q->write_idx = (q->write_idx + 1) % q->max_items;
    q->cur_items++;

    uint32_t rx_id = waiter_pop_first(q->rx_waiters, &q->rx_waiter_count);
    if (rx_id != SIM_SCHED_NO_READY) {
        sim_scheduler_resume(rx_id);
    }

    return pdPASS;
}

BaseType_t xQueueReceive(QueueHandle_t xQueue, void * const pvBuffer, TickType_t xTicksToWait) {
    esp_queue_t* q = resolve_queue(xQueue);
    if (q == NULL) {
        return errQUEUE_EMPTY;
    }

    uint32_t q_idx = (uint32_t)(q - s_queues);

    while (q->cur_items == 0) {
        if (xTicksToWait == 0) {
            return errQUEUE_EMPTY;
        }
        uint32_t self = sim_scheduler_current_id();
        if (self == SIM_SCHED_NO_READY) {
            return errQUEUE_EMPTY;
        }

        waiter_add(q->rx_waiters, &q->rx_waiter_count, self);
        bool ok = sync_block(FREERTOS_MAKE_RES_ID(FREERTOS_TAG_QUEUE, q_idx), xTicksToWait);
        waiter_remove(q->rx_waiters, &q->rx_waiter_count, self);

        if (!ok && q->cur_items == 0) {
            return errQUEUE_EMPTY;
        }
    }

    if (pvBuffer != NULL && q->item_size > 0) {
        memcpy(pvBuffer, &q->storage[q->read_idx * q->item_size], q->item_size);
    }
    q->read_idx = (q->read_idx + 1) % q->max_items;
    q->cur_items--;

    uint32_t tx_id = waiter_pop_first(q->tx_waiters, &q->tx_waiter_count);
    if (tx_id != SIM_SCHED_NO_READY) {
        sim_scheduler_resume(tx_id);
    }

    return pdPASS;
}

BaseType_t xQueuePeek(QueueHandle_t xQueue, void * const pvBuffer, TickType_t xTicksToWait) {
    esp_queue_t* q = resolve_queue(xQueue);
    if (q == NULL) {
        return errQUEUE_EMPTY;
    }

    uint32_t q_idx = (uint32_t)(q - s_queues);

    while (q->cur_items == 0) {
        if (xTicksToWait == 0) {
            return errQUEUE_EMPTY;
        }
        uint32_t self = sim_scheduler_current_id();
        if (self == SIM_SCHED_NO_READY) {
            return errQUEUE_EMPTY;
        }

        waiter_add(q->rx_waiters, &q->rx_waiter_count, self);
        bool ok = sync_block(FREERTOS_MAKE_RES_ID(FREERTOS_TAG_QUEUE, q_idx), xTicksToWait);
        waiter_remove(q->rx_waiters, &q->rx_waiter_count, self);

        if (!ok && q->cur_items == 0) {
            return errQUEUE_EMPTY;
        }
    }

    if (pvBuffer != NULL && q->item_size > 0) {
        memcpy(pvBuffer, &q->storage[q->read_idx * q->item_size], q->item_size);
    }

    return pdPASS;
}

void vQueueDelete(QueueHandle_t xQueue) {
    esp_queue_t* q = resolve_queue(xQueue);
    if (q == NULL) return;

    for (uint8_t i = 0; i < q->rx_waiter_count; ++i) {
        sim_scheduler_resume(q->rx_waiters[i]);
    }
    for (uint8_t i = 0; i < q->tx_waiter_count; ++i) {
        sim_scheduler_resume(q->tx_waiters[i]);
    }

    memset(q, 0, sizeof(esp_queue_t));
}

UBaseType_t uxQueueMessagesWaiting(const QueueHandle_t xQueue) {
    esp_queue_t* q = resolve_queue(xQueue);
    return q ? (UBaseType_t)q->cur_items : 0u;
}

UBaseType_t uxQueueSpacesAvailable(const QueueHandle_t xQueue) {
    esp_queue_t* q = resolve_queue(xQueue);
    return q ? (UBaseType_t)(q->max_items - q->cur_items) : 0u;
}

BaseType_t xQueueReset(QueueHandle_t xQueue) {
    esp_queue_t* q = resolve_queue(xQueue);
    if (q == NULL) return pdFAIL;
    q->read_idx = 0;
    q->write_idx = 0;
    q->cur_items = 0;
    return pdPASS;
}

BaseType_t xQueueSendFromISR(QueueHandle_t xQueue, const void * const pvItemToQueue, BaseType_t * const pxHigherPriorityTaskWoken) {
    if (pxHigherPriorityTaskWoken != NULL) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }
    return xQueueSend(xQueue, pvItemToQueue, 0);
}

BaseType_t xQueueReceiveFromISR(QueueHandle_t xQueue, void * const pvBuffer, BaseType_t * const pxHigherPriorityTaskWoken) {
    if (pxHigherPriorityTaskWoken != NULL) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }
    return xQueueReceive(xQueue, pvBuffer, 0);
}
