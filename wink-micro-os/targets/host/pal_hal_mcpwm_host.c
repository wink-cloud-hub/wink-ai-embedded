// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_hal_mcpwm_host.c
 * @brief Host first-class target PAL MCPWM simulation driver.
 */
#include "hal/pal_mcpwm.h"
#include "pal_resource.h"
#include "pal_spinlock.h"
#include "pal_mcpwm_stub.h"
#include <string.h>

#define HOST_MCPWM_TIMERS_MAX 6
#define HOST_MCPWM_OPERS_MAX  6
#define HOST_MCPWM_CMPS_MAX   6
#define HOST_MCPWM_FAULTS_MAX 6
#define HOST_MCPWM_CAPS_MAX   6

struct pal_mcpwm_timer_s {
    bool                  in_use;
    bool                  is_running;
    pal_mcpwm_timer_cfg_t cfg;
    uint32_t              phase_ticks;
    bool                  phase_locked;
};

struct pal_mcpwm_oper_s {
    bool                 in_use;
    pal_mcpwm_oper_cfg_t cfg;
};

struct pal_mcpwm_cmp_s {
    bool                in_use;
    pal_mcpwm_cmp_cfg_t cfg;
    uint32_t            duty_ticks;
};

struct pal_mcpwm_fault_s {
    bool                  in_use;
    pal_mcpwm_fault_cfg_t cfg;
    bool                  tripped;
};

struct pal_mcpwm_cap_s {
    bool                in_use;
    pal_mcpwm_cap_cfg_t cfg;
};

static struct pal_mcpwm_timer_s s_timers[HOST_MCPWM_TIMERS_MAX];
static struct pal_mcpwm_oper_s  s_opers[HOST_MCPWM_OPERS_MAX];
static struct pal_mcpwm_cmp_s   s_cmps[HOST_MCPWM_CMPS_MAX];
static struct pal_mcpwm_fault_s s_faults[HOST_MCPWM_FAULTS_MAX];
static struct pal_mcpwm_cap_s   s_caps[HOST_MCPWM_CAPS_MAX];

static pal_spinlock_t s_mcpwm_lock = PAL_SPINLOCK_INITIALIZER;

/* --- Testing Stub Hooks --- */

uint32_t stub_mcpwm_get_duty_ticks(pal_mcpwm_cmp_handle_t cmp) {
    if (cmp == NULL) {
        return 0;
    }
    pal_spinlock_lock(&s_mcpwm_lock);
    uint32_t d = cmp->duty_ticks;
    pal_spinlock_unlock(&s_mcpwm_lock);
    return d;
}

bool stub_mcpwm_is_timer_running(pal_mcpwm_timer_handle_t t) {
    if (t == NULL) {
        return false;
    }
    pal_spinlock_lock(&s_mcpwm_lock);
    bool r = t->is_running;
    pal_spinlock_unlock(&s_mcpwm_lock);
    return r;
}

void stub_mcpwm_trigger_brake(pal_mcpwm_fault_handle_t f) {
    if (f == NULL) {
        return;
    }
    pal_spinlock_lock(&s_mcpwm_lock);
    f->tripped = true;
    void (*cb)(void *) = f->cfg.on_brake_isr;
    void *cb_arg = f->cfg.on_brake_arg;
    pal_spinlock_unlock(&s_mcpwm_lock);

    if (cb != NULL) {
        cb(cb_arg);
    }
}

void stub_mcpwm_trigger_capture(pal_mcpwm_cap_handle_t cap, uint32_t ts_ns, bool rising) {
    if (cap == NULL) {
        return;
    }
    pal_spinlock_lock(&s_mcpwm_lock);
    void (*cb)(void *, uint32_t, bool) = cap->cfg.on_capture_isr;
    void *cb_arg = cap->cfg.on_capture_arg;
    pal_spinlock_unlock(&s_mcpwm_lock);

    if (cb != NULL) {
        cb(cb_arg, ts_ns, rising);
    }
}

/* --- Public PAL MCPWM API --- */

WINK_WARN_UNUSED_RESULT
wink_status_t pal_mcpwm_new_timer(const pal_mcpwm_timer_cfg_t *cfg, pal_mcpwm_timer_handle_t *out_timer) {
    if (cfg == NULL || out_timer == NULL || cfg->mcpwm_unit > 1 || cfg->timer_id > 2) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_mcpwm_lock);
    struct pal_mcpwm_timer_s *slot = NULL;
    for (int i = 0; i < HOST_MCPWM_TIMERS_MAX; i++) {
        if (!s_timers[i].in_use) {
            slot = &s_timers[i];
            break;
        }
    }
    if (slot == NULL) {
        pal_spinlock_unlock(&s_mcpwm_lock);
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    wink_status_t st = pal_resource_claim(PAL_RESOURCE_MCPWM_TIMER, (uint32_t)(cfg->mcpwm_unit * 3 + cfg->timer_id), "pal_mcpwm_host");
    if (st != WINK_OK) {
        pal_spinlock_unlock(&s_mcpwm_lock);
        return st;
    }

    slot->in_use = true;
    slot->is_running = false;
    slot->cfg = *cfg;
    slot->phase_ticks = 0;
    slot->phase_locked = false;

    *out_timer = slot;
    pal_spinlock_unlock(&s_mcpwm_lock);
    return WINK_OK;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_mcpwm_new_oper(const pal_mcpwm_oper_cfg_t *cfg, pal_mcpwm_oper_handle_t *out_oper) {
    if (cfg == NULL || out_oper == NULL || cfg->timer == NULL || cfg->operator_id > 2) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_mcpwm_lock);
    struct pal_mcpwm_oper_s *slot = NULL;
    for (int i = 0; i < HOST_MCPWM_OPERS_MAX; i++) {
        if (!s_opers[i].in_use) {
            slot = &s_opers[i];
            break;
        }
    }
    if (slot == NULL) {
        pal_spinlock_unlock(&s_mcpwm_lock);
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    if (cfg->pin_pwm_a >= 0) {
        wink_status_t st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_pwm_a, "pal_mcpwm_host");
        if (st != WINK_OK) {
            pal_spinlock_unlock(&s_mcpwm_lock);
            return st;
        }
    }
    if (cfg->pin_pwm_b >= 0) {
        wink_status_t st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_pwm_b, "pal_mcpwm_host");
        if (st != WINK_OK) {
            if (cfg->pin_pwm_a >= 0) {
                pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_pwm_a, "pal_mcpwm_host");
            }
            pal_spinlock_unlock(&s_mcpwm_lock);
            return st;
        }
    }

    slot->in_use = true;
    slot->cfg = *cfg;
    *out_oper = slot;

    pal_spinlock_unlock(&s_mcpwm_lock);
    return WINK_OK;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_mcpwm_new_cmp(const pal_mcpwm_cmp_cfg_t *cfg, pal_mcpwm_cmp_handle_t *out_cmp) {
    if (cfg == NULL || out_cmp == NULL || cfg->oper == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_mcpwm_lock);
    struct pal_mcpwm_cmp_s *slot = NULL;
    for (int i = 0; i < HOST_MCPWM_CMPS_MAX; i++) {
        if (!s_cmps[i].in_use) {
            slot = &s_cmps[i];
            break;
        }
    }
    if (slot == NULL) {
        pal_spinlock_unlock(&s_mcpwm_lock);
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    slot->in_use = true;
    slot->cfg = *cfg;
    slot->duty_ticks = cfg->initial_duty_ticks;

    *out_cmp = slot;
    pal_spinlock_unlock(&s_mcpwm_lock);
    return WINK_OK;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_mcpwm_new_fault(const pal_mcpwm_fault_cfg_t *cfg, pal_mcpwm_fault_handle_t *out_fault) {
    if (cfg == NULL || out_fault == NULL || cfg->fault_id > 2) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_mcpwm_lock);
    struct pal_mcpwm_fault_s *slot = NULL;
    for (int i = 0; i < HOST_MCPWM_FAULTS_MAX; i++) {
        if (!s_faults[i].in_use) {
            slot = &s_faults[i];
            break;
        }
    }
    if (slot == NULL) {
        pal_spinlock_unlock(&s_mcpwm_lock);
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    if (cfg->fault_pin >= 0) {
        wink_status_t st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->fault_pin, "pal_mcpwm_host");
        if (st != WINK_OK) {
            pal_spinlock_unlock(&s_mcpwm_lock);
            return st;
        }
    }

    slot->in_use = true;
    slot->cfg = *cfg;
    slot->tripped = false;

    *out_fault = slot;
    pal_spinlock_unlock(&s_mcpwm_lock);
    return WINK_OK;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_mcpwm_new_capture(const pal_mcpwm_cap_cfg_t *cfg, pal_mcpwm_cap_handle_t *out_cap) {
    if (cfg == NULL || out_cap == NULL || cfg->cap_channel > 2) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_mcpwm_lock);
    struct pal_mcpwm_cap_s *slot = NULL;
    for (int i = 0; i < HOST_MCPWM_CAPS_MAX; i++) {
        if (!s_caps[i].in_use) {
            slot = &s_caps[i];
            break;
        }
    }
    if (slot == NULL) {
        pal_spinlock_unlock(&s_mcpwm_lock);
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    if (cfg->cap_pin >= 0) {
        wink_status_t st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->cap_pin, "pal_mcpwm_host");
        if (st != WINK_OK) {
            pal_spinlock_unlock(&s_mcpwm_lock);
            return st;
        }
    }

    slot->in_use = true;
    slot->cfg = *cfg;

    *out_cap = slot;
    pal_spinlock_unlock(&s_mcpwm_lock);
    return WINK_OK;
}

wink_status_t pal_mcpwm_timer_start(pal_mcpwm_timer_handle_t t) {
    if (t == NULL) return WINK_ERR_INVALID_ARG;
    pal_spinlock_lock(&s_mcpwm_lock);
    t->is_running = true;
    pal_spinlock_unlock(&s_mcpwm_lock);
    return WINK_OK;
}

wink_status_t pal_mcpwm_timer_stop(pal_mcpwm_timer_handle_t t) {
    if (t == NULL) return WINK_ERR_INVALID_ARG;
    pal_spinlock_lock(&s_mcpwm_lock);
    t->is_running = false;
    pal_spinlock_unlock(&s_mcpwm_lock);
    return WINK_OK;
}

wink_status_t pal_mcpwm_set_duty_ticks(pal_mcpwm_cmp_handle_t cmp, uint32_t duty_ticks) {
    if (cmp == NULL) return WINK_ERR_INVALID_ARG;
    pal_spinlock_lock(&s_mcpwm_lock);
    cmp->duty_ticks = duty_ticks;
    pal_spinlock_unlock(&s_mcpwm_lock);
    return WINK_OK;
}

wink_status_t pal_mcpwm_sync_gpio_config(wink_pin_t sync_gpio, bool active_level) {
    (void)active_level;
    if (sync_gpio < 0) return WINK_ERR_INVALID_ARG;
    return pal_resource_claim(PAL_RESOURCE_MCPWM_SYNC_GPIO, (uint32_t)sync_gpio, "pal_mcpwm_host");
}

wink_status_t pal_mcpwm_timer_enable_phase_lock(pal_mcpwm_timer_handle_t t, uint32_t phase_ticks) {
    if (t == NULL) return WINK_ERR_INVALID_ARG;
    pal_spinlock_lock(&s_mcpwm_lock);
    t->phase_ticks = phase_ticks;
    t->phase_locked = true;
    pal_spinlock_unlock(&s_mcpwm_lock);
    return WINK_OK;
}

wink_status_t pal_mcpwm_trigger_software_sync(void) {
    return WINK_OK;
}

wink_status_t pal_mcpwm_fault_clear(pal_mcpwm_fault_handle_t f) {
    if (f == NULL) return WINK_ERR_INVALID_ARG;
    pal_spinlock_lock(&s_mcpwm_lock);
    f->tripped = false;
    pal_spinlock_unlock(&s_mcpwm_lock);
    return WINK_OK;
}

void pal_mcpwm_del_timer(pal_mcpwm_timer_handle_t t) {
    if (t == NULL) return;
    pal_spinlock_lock(&s_mcpwm_lock);
    if (t->in_use) {
        pal_resource_release(PAL_RESOURCE_MCPWM_TIMER, (uint32_t)(t->cfg.mcpwm_unit * 3 + t->cfg.timer_id), "pal_mcpwm_host");
        t->in_use = false;
        t->is_running = false;
    }
    pal_spinlock_unlock(&s_mcpwm_lock);
}
