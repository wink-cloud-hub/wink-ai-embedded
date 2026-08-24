// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_hal_hwtimer_host.c
 * @brief Host first-class target PAL HWTIMER implementation with soft stepping stub.
 */
#include "hal/pal_hwtimer.h"
#include "pal_resource.h"
#include "pal_spinlock.h"
#include "pal_hwtimer_stub.h"
#include <string.h>

typedef struct {
    bool              in_use;
    bool              is_running;
    pal_hwtimer_cfg_t cfg;
    uint32_t          call_count;
} host_hwtimer_slot_t;

static host_hwtimer_slot_t s_timers[PAL_HWTIMERS_MAX];
static pal_spinlock_t s_hwtimer_lock = PAL_SPINLOCK_INITIALIZER;

/* --- Testing Stub Hooks --- */

uint32_t stub_hwtimer_get_callback_count(uint8_t timer_id) {
    if (timer_id >= PAL_HWTIMERS_MAX) {
        return 0;
    }
    pal_spinlock_lock(&s_hwtimer_lock);
    uint32_t count = s_timers[timer_id].call_count;
    pal_spinlock_unlock(&s_hwtimer_lock);
    return count;
}

bool stub_hwtimer_is_running(uint8_t timer_id) {
    if (timer_id >= PAL_HWTIMERS_MAX) {
        return false;
    }
    pal_spinlock_lock(&s_hwtimer_lock);
    bool r = s_timers[timer_id].is_running;
    pal_spinlock_unlock(&s_hwtimer_lock);
    return r;
}

void stub_hwtimer_reset(uint8_t timer_id) {
    if (timer_id >= PAL_HWTIMERS_MAX) {
        return;
    }
    pal_spinlock_lock(&s_hwtimer_lock);
    s_timers[timer_id].call_count = 0;
    pal_spinlock_unlock(&s_hwtimer_lock);
}

/* --- PAL Public HWTIMER API --- */

WINK_WARN_UNUSED_RESULT
wink_status_t pal_hwtimer_init(const pal_hwtimer_cfg_t *cfg) {
    if (cfg == NULL || cfg->timer_id >= PAL_HWTIMERS_MAX || cfg->period_us == 0 || cfg->uses_fpu) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_hwtimer_lock);
    host_hwtimer_slot_t *slot = &s_timers[cfg->timer_id];
    if (slot->in_use) {
        pal_spinlock_unlock(&s_hwtimer_lock);
        return WINK_ERR_BUSY;
    }

    wink_status_t st = pal_resource_claim(PAL_RESOURCE_HWTIMER, cfg->timer_id, "pal_hwtimer_host");
    if (st != WINK_OK) {
        pal_spinlock_unlock(&s_hwtimer_lock);
        return st;
    }

    slot->in_use = true;
    slot->is_running = cfg->auto_start;
    slot->cfg = *cfg;
    slot->call_count = 0;

    pal_spinlock_unlock(&s_hwtimer_lock);
    return WINK_OK;
}

wink_status_t pal_hwtimer_start(uint8_t timer_id) {
    if (timer_id >= PAL_HWTIMERS_MAX) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_hwtimer_lock);
    host_hwtimer_slot_t *slot = &s_timers[timer_id];
    if (!slot->in_use) {
        pal_spinlock_unlock(&s_hwtimer_lock);
        return WINK_ERR_INVALID_STATE;
    }

    slot->is_running = true;
    pal_spinlock_unlock(&s_hwtimer_lock);
    return WINK_OK;
}

wink_status_t pal_hwtimer_stop(uint8_t timer_id) {
    if (timer_id >= PAL_HWTIMERS_MAX) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_hwtimer_lock);
    host_hwtimer_slot_t *slot = &s_timers[timer_id];
    if (!slot->in_use) {
        pal_spinlock_unlock(&s_hwtimer_lock);
        return WINK_ERR_INVALID_STATE;
    }

    slot->is_running = false;
    pal_spinlock_unlock(&s_hwtimer_lock);
    return WINK_OK;
}

wink_status_t pal_hwtimer_change_period(uint8_t timer_id, uint32_t new_period_us) {
    if (timer_id >= PAL_HWTIMERS_MAX || new_period_us == 0) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_hwtimer_lock);
    host_hwtimer_slot_t *slot = &s_timers[timer_id];
    if (!slot->in_use) {
        pal_spinlock_unlock(&s_hwtimer_lock);
        return WINK_ERR_INVALID_STATE;
    }

    slot->cfg.period_us = new_period_us;
    pal_spinlock_unlock(&s_hwtimer_lock);
    return WINK_OK;
}

void pal_hwtimer_deinit(uint8_t timer_id) {
    if (timer_id >= PAL_HWTIMERS_MAX) {
        return;
    }

    pal_spinlock_lock(&s_hwtimer_lock);
    host_hwtimer_slot_t *slot = &s_timers[timer_id];
    if (!slot->in_use) {
        pal_spinlock_unlock(&s_hwtimer_lock);
        return;
    }

    pal_resource_release(PAL_RESOURCE_HWTIMER, timer_id, "pal_hwtimer_host");
    slot->in_use = false;
    slot->is_running = false;
    slot->call_count = 0;
    pal_spinlock_unlock(&s_hwtimer_lock);
}

wink_status_t pal_hwtimer_fire_soft(uint8_t timer_id) {
    if (timer_id >= PAL_HWTIMERS_MAX) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_hwtimer_lock);
    host_hwtimer_slot_t *slot = &s_timers[timer_id];
    if (!slot->in_use || !slot->is_running) {
        pal_spinlock_unlock(&s_hwtimer_lock);
        return WINK_ERR_INVALID_STATE;
    }

    pal_hwtimer_isr_t cb = slot->cfg.callback;
    void *cb_arg = slot->cfg.callback_arg;
    slot->call_count++;

    if (slot->cfg.oneshot) {
        slot->is_running = false;
    }
    pal_spinlock_unlock(&s_hwtimer_lock);

    if (cb != NULL) {
        cb(cb_arg);
    }
    return WINK_OK;
}
