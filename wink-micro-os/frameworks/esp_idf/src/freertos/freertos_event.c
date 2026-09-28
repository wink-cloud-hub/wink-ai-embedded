/* SPDX-License-Identifier: LGPL-3.0-only */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos_sync.h"
#include "wink_sim_scheduler.h"
#include "pal_log.h"
#include "../core/esp_sim_handle.h"
extern void esp_idf_ensure_framework_ready(void); /* Phase 2 Task 4 cold-start */

#ifndef FREERTOS_MAX_EVENT_GROUPS
#  ifdef CONFIG_FREERTOS_MAX_EVENT_GROUPS
#    define FREERTOS_MAX_EVENT_GROUPS CONFIG_FREERTOS_MAX_EVENT_GROUPS
#  else
#    define FREERTOS_MAX_EVENT_GROUPS 8
#  endif
#endif

typedef struct {
    uint32_t     sim_id;
    EventBits_t  bits_to_wait_for;
    EventBits_t  captured_bits;
    uint8_t      clear_on_exit;
    uint8_t      wait_for_all;
    uint8_t      woken;
    uint8_t      reserved;
} event_waiter_t;

typedef struct {
    bool           used;
    uint32_t       token;
    EventBits_t    cur_bits;
    event_waiter_t waiters[WINK_SIM_MAX_TASKS];
    uint8_t        waiter_count;
} esp_event_group_t;

_Static_assert(sizeof(esp_event_group_t) <= (16 + (WINK_SIM_MAX_TASKS * sizeof(event_waiter_t))), "esp_event_group_t size budget exceeded");

static esp_event_group_t s_events[FREERTOS_MAX_EVENT_GROUPS];

static inline esp_event_group_t* resolve_event_group(EventGroupHandle_t eg) {
    uint32_t slot;
    if (!esp_sim_handle_decode(eg, ESP_SIM_HANDLE_EVENT_GROUP,
                               FREERTOS_MAX_EVENT_GROUPS, &slot)) return NULL;
    esp_event_group_t* candidate = &s_events[slot];
    if (!candidate->used || candidate->token != (uint32_t)(uintptr_t)eg) return NULL;
    return candidate;
}

static inline bool event_condition_met(EventBits_t cur, EventBits_t wait_bits, BaseType_t wait_all) {
    if (wait_all) {
        return (cur & wait_bits) == wait_bits;
    } else {
        return (cur & wait_bits) != 0;
    }
}

void esp_freertos_event_pool_reset(void) {
    memset(s_events, 0, sizeof(s_events));
}

EventGroupHandle_t xEventGroupCreate(void) {
    esp_idf_ensure_framework_ready(); /* Phase 2 Task 4.2 */
    for (uint32_t i = 0; i < FREERTOS_MAX_EVENT_GROUPS; ++i) {
        if (!s_events[i].used) {
            uint32_t token = esp_sim_handle_issue(ESP_SIM_HANDLE_EVENT_GROUP, i);
            if (token == 0) return NULL;
            memset(&s_events[i], 0, sizeof(esp_event_group_t));
            s_events[i].used = true;
            s_events[i].token = token;
            return (EventGroupHandle_t)(uintptr_t)token;
        }
    }
    pal_log_w("FREERTOS", "No free event group slot");
    return NULL;
}

EventBits_t xEventGroupWaitBits(EventGroupHandle_t xEventGroup,
                                const EventBits_t uxBitsToWaitFor,
                                const BaseType_t xClearOnExit,
                                const BaseType_t xWaitForAllBits,
                                TickType_t xTicksToWait) {
    esp_event_group_t* eg = resolve_event_group(xEventGroup);
    if (eg == NULL) {
        return 0;
    }

    EventBits_t wait_bits = uxBitsToWaitFor & 0x00FFFFFFu;

    if (event_condition_met(eg->cur_bits, wait_bits, xWaitForAllBits)) {
        EventBits_t ret = eg->cur_bits;
        if (xClearOnExit) {
            eg->cur_bits &= ~wait_bits;
        }
        return ret;
    }

    if (xTicksToWait == 0) {
        return eg->cur_bits;
    }

    /* Phase 3 Task 2.6: cannot block inside ISR */
    esp_freertos_assert_not_in_isr("xEventGroupWaitBits");
    /* Phase 2 Task 2.3: about to block on event group — guard */
    esp_freertos_assert_not_in_critical("xEventGroupWaitBits");

    uint32_t self = sim_scheduler_current_id();
    if (self == SIM_SCHED_NO_READY) {
        return eg->cur_bits;
    }

    if (eg->waiter_count < WINK_SIM_MAX_TASKS) {
        eg->waiters[eg->waiter_count].sim_id = self;
        eg->waiters[eg->waiter_count].bits_to_wait_for = wait_bits;
        eg->waiters[eg->waiter_count].captured_bits = 0;
        eg->waiters[eg->waiter_count].clear_on_exit = (uint8_t)(xClearOnExit ? 1 : 0);
        eg->waiters[eg->waiter_count].wait_for_all = (uint8_t)(xWaitForAllBits ? 1 : 0);
        eg->waiters[eg->waiter_count].woken = 0;
        eg->waiters[eg->waiter_count].reserved = 0;
        eg->waiter_count++;
    }

    uint32_t eg_idx = (uint32_t)(eg - s_events);
    (void)sync_block(FREERTOS_MAKE_RES_ID(FREERTOS_TAG_EVENT, eg_idx), xTicksToWait);
    if (resolve_event_group(xEventGroup) != eg) return 0;

    EventBits_t ret = 0;
    bool was_woken = false;

    for (uint8_t i = 0; i < eg->waiter_count; ++i) {
        if (eg->waiters[i].sim_id == self) {
            ret = eg->waiters[i].captured_bits;
            was_woken = (eg->waiters[i].woken != 0);
            for (uint8_t j = i; j + 1 < eg->waiter_count; ++j) {
                eg->waiters[j] = eg->waiters[j + 1];
            }
            eg->waiter_count--;
            break;
        }
    }

    if (!was_woken) {
        /* Woken by timeout or unblock without matching SetBits */
        ret = eg->cur_bits;
        if (event_condition_met(eg->cur_bits, wait_bits, xWaitForAllBits)) {
            if (xClearOnExit) {
                eg->cur_bits &= ~wait_bits;
            }
        }
    }

    return ret;
}

EventBits_t xEventGroupSetBits(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet) {
    esp_event_group_t* eg = resolve_event_group(xEventGroup);
    if (eg == NULL) {
        return 0;
    }

    eg->cur_bits |= (uxBitsToSet & 0x00FFFFFFu);

    /* Collect bits to clear AFTER all waiters capture their snapshot (FreeRTOS semantics:
     * all woken tasks observe the full bits snapshot before any xClearOnExit takes effect) */
    EventBits_t bits_to_clear = 0;

    for (uint8_t i = 0; i < eg->waiter_count; ++i) {
        event_waiter_t* w = &eg->waiters[i];
        if (event_condition_met(eg->cur_bits, w->bits_to_wait_for, w->wait_for_all)) {
            w->captured_bits = eg->cur_bits;
            w->woken = 1;
            sim_scheduler_resume(w->sim_id);
            if (w->clear_on_exit) {
                bits_to_clear |= w->bits_to_wait_for;
            }
        }
    }

    /* Deferred clear: all woken tasks see the pre-clear snapshot */
    if (bits_to_clear != 0) {
        eg->cur_bits &= ~bits_to_clear;
    }

    return eg->cur_bits;
}

EventBits_t xEventGroupClearBits(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear) {
    esp_event_group_t* eg = resolve_event_group(xEventGroup);
    if (eg == NULL) {
        return 0;
    }
    EventBits_t prev = eg->cur_bits;
    eg->cur_bits &= ~(uxBitsToClear & 0x00FFFFFFu);
    return prev;
}

EventBits_t xEventGroupGetBits(EventGroupHandle_t xEventGroup) {
    esp_event_group_t* eg = resolve_event_group(xEventGroup);
    return eg ? eg->cur_bits : 0;
}

void vEventGroupDelete(EventGroupHandle_t xEventGroup) {
    esp_event_group_t* eg = resolve_event_group(xEventGroup);
    if (eg == NULL) return;

    for (uint8_t i = 0; i < eg->waiter_count; ++i) {
        sim_scheduler_resume(eg->waiters[i].sim_id);
    }
    memset(eg, 0, sizeof(esp_event_group_t));
}

BaseType_t xEventGroupSetBitsFromISR(EventGroupHandle_t xEventGroup,
                                     const EventBits_t uxBitsToSet,
                                     BaseType_t * const pxHigherPriorityTaskWoken) {
    if (pxHigherPriorityTaskWoken != NULL) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }
    esp_event_group_t* eg = resolve_event_group(xEventGroup);
    if (eg == NULL) {
        return pdPASS;
    }

    eg->cur_bits |= (uxBitsToSet & 0x00FFFFFFu);

    EventBits_t bits_to_clear = 0;
    bool higher_woken = false;
    uint32_t cur_id = sim_scheduler_current_id();
    int32_t cur_prio = (cur_id == SIM_SCHED_NO_READY) ? -1 : esp_freertos_get_task_prio(cur_id);

    for (uint8_t i = 0; i < eg->waiter_count; ++i) {
        event_waiter_t* w = &eg->waiters[i];
        if (event_condition_met(eg->cur_bits, w->bits_to_wait_for, w->wait_for_all)) {
            w->captured_bits = eg->cur_bits;
            w->woken = 1;
            sim_scheduler_resume(w->sim_id);
            int32_t wprio = esp_freertos_get_task_prio(w->sim_id);
            if (wprio > cur_prio) {
                higher_woken = true;
            }
            if (w->clear_on_exit) {
                bits_to_clear |= w->bits_to_wait_for;
            }
        }
    }

    if (bits_to_clear != 0) {
        eg->cur_bits &= ~bits_to_clear;
    }

    if (higher_woken) {
        if (pxHigherPriorityTaskWoken != NULL) {
            *pxHigherPriorityTaskWoken = pdTRUE;
        }
        esp_freertos_request_isr_yield();
    }

    return pdPASS;
}

BaseType_t xEventGroupClearBitsFromISR(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear) {
    (void)xEventGroupClearBits(xEventGroup, uxBitsToClear);
    return pdPASS;
}

EventBits_t xEventGroupGetBitsFromISR(EventGroupHandle_t xEventGroup) {
    return xEventGroupGetBits(xEventGroup);
}
