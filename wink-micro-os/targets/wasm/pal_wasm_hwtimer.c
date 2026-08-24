// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_wasm_hwtimer.c
 * @brief Wasm target hardware timer subsystem driven by virtual clock.
 */
#include "hal/pal_hwtimer.h"
#include "pal_wasm_hwtimer.h"
#include "pal_wasm_common.h"
#include "pal_osal.h"
#include "pal_log.h"
#include <string.h>

#define LOG_TAG "pal_wasm_hwtimer"

#if defined(__EMSCRIPTEN__) || (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4)
_Static_assert(sizeof(pal_hwtimer_cfg_t) >= 12, "pal_hwtimer_cfg_t ABI size consistency check");
#endif

typedef struct {
    bool              in_use;
    bool              is_running;
    pal_hwtimer_cfg_t cfg;
    uint32_t          pending_period_us;
    uint64_t          next_fire_us;
} wasm_hwtimer_slot_t;

static wasm_hwtimer_slot_t s_timers[PAL_HWTIMERS_MAX];

WINK_WARN_UNUSED_RESULT
wink_status_t pal_hwtimer_init(const pal_hwtimer_cfg_t *cfg) {
    if (cfg == NULL || cfg->timer_id >= PAL_HWTIMERS_MAX || cfg->period_us == 0) {
        return WINK_ERR_INVALID_ARG;
    }

    wasm_hwtimer_slot_t *slot = &s_timers[cfg->timer_id];
    if (slot->in_use) {
        return WINK_ERR_BUSY;
    }

    slot->in_use = true;
    slot->is_running = cfg->auto_start;
    slot->cfg = *cfg;
    slot->pending_period_us = 0;
    slot->next_fire_us = pal_os_get_us() + (uint64_t)cfg->period_us;

    return WINK_OK;
}

wink_status_t pal_hwtimer_start(uint8_t timer_id) {
    if (timer_id >= PAL_HWTIMERS_MAX) return WINK_ERR_INVALID_ARG;
    wasm_hwtimer_slot_t *slot = &s_timers[timer_id];
    if (!slot->in_use) return WINK_ERR_INVALID_STATE;

    slot->is_running = true;
    slot->next_fire_us = pal_os_get_us() + (uint64_t)slot->cfg.period_us;
    return WINK_OK;
}

wink_status_t pal_hwtimer_stop(uint8_t timer_id) {
    if (timer_id >= PAL_HWTIMERS_MAX) return WINK_ERR_INVALID_ARG;
    wasm_hwtimer_slot_t *slot = &s_timers[timer_id];
    if (!slot->in_use) return WINK_ERR_INVALID_STATE;

    slot->is_running = false;
    return WINK_OK;
}

wink_status_t pal_hwtimer_change_period(uint8_t timer_id, uint32_t new_period_us) {
    if (timer_id >= PAL_HWTIMERS_MAX || new_period_us == 0) return WINK_ERR_INVALID_ARG;
    wasm_hwtimer_slot_t *slot = &s_timers[timer_id];
    if (!slot->in_use) return WINK_ERR_INVALID_STATE;

    /* Preserve phase: update period on next fire event */
    slot->pending_period_us = new_period_us;
    return WINK_OK;
}

void pal_hwtimer_deinit(uint8_t timer_id) {
    if (timer_id >= PAL_HWTIMERS_MAX) return;
    s_timers[timer_id].in_use = false;
    s_timers[timer_id].is_running = false;
    s_timers[timer_id].pending_period_us = 0;
}

wink_status_t pal_hwtimer_fire_soft(uint8_t timer_id) {
    if (timer_id >= PAL_HWTIMERS_MAX) return WINK_ERR_INVALID_ARG;
    wasm_hwtimer_slot_t *slot = &s_timers[timer_id];
    if (!slot->in_use || !slot->is_running) return WINK_ERR_INVALID_STATE;

    if (slot->cfg.callback != NULL) {
        slot->cfg.callback(slot->cfg.callback_arg);
    }
    if (slot->cfg.oneshot) {
        slot->is_running = false;
    } else {
        if (slot->pending_period_us > 0) {
            slot->cfg.period_us = slot->pending_period_us;
            slot->pending_period_us = 0;
        }
        slot->next_fire_us += (uint64_t)slot->cfg.period_us;
    }
    return WINK_OK;
}

void pal_wasm_hwtimer_drain(void) {
    uint64_t now = pal_os_get_us();
    for (int i = 0; i < PAL_HWTIMERS_MAX; i++) {
        wasm_hwtimer_slot_t *slot = &s_timers[i];
        if (!slot->in_use || !slot->is_running) continue;

        uint32_t catchup_count = 0;
        while (slot->is_running && slot->next_fire_us <= now) {
            catchup_count++;
            if (catchup_count > PAL_WASM_HWTIMER_MAX_CATCHUP) {
#if defined(PAL_WASM_HWTIMER_REALTIME_MODE)
                LOG_W(LOG_TAG, "Timer %d exceeded catchup limit (%d), skipping to now", i, PAL_WASM_HWTIMER_MAX_CATCHUP);
                slot->next_fire_us = now + (uint64_t)slot->cfg.period_us;
                break;
#else
                /* Headless mode: overrun means simulation loss of fidelity -> fatal stop */
                pal_wasm_log_fault(FAULT_TYPE_TIMER_OVERRUN, (uint16_t)i);
                pal_wasm_invoke_fault(8005);
                slot->is_running = false;
                break;
#endif
            }

            if (slot->cfg.callback != NULL) {
                slot->cfg.callback(slot->cfg.callback_arg);
            }

            if (slot->cfg.oneshot) {
                slot->is_running = false;
                break;
            } else {
                if (slot->pending_period_us > 0) {
                    slot->cfg.period_us = slot->pending_period_us;
                    slot->pending_period_us = 0;
                }
                slot->next_fire_us += (uint64_t)slot->cfg.period_us;
            }
        }
    }
}
