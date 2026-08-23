// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_wasm_mcpwm.c
 * @brief Wasm target PAL MCPWM driver maintaining semantic duty/fault state.
 */
#include "hal/pal_mcpwm.h"
#include "wasm_bridge.h"
#include <string.h>

#define WASM_MCPWM_TIMERS_MAX 4
#define WASM_MCPWM_OPERS_MAX  4
#define WASM_MCPWM_CMPS_MAX   4
#define WASM_MCPWM_FAULTS_MAX 4
#define WASM_MCPWM_CAPS_MAX   4

struct pal_mcpwm_timer_s {
    bool                  in_use;
    bool                  is_running;
    pal_mcpwm_timer_cfg_t cfg;
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

static struct pal_mcpwm_timer_s s_timers[WASM_MCPWM_TIMERS_MAX];
static struct pal_mcpwm_oper_s  s_opers[WASM_MCPWM_OPERS_MAX];
static struct pal_mcpwm_cmp_s   s_cmps[WASM_MCPWM_CMPS_MAX];
static struct pal_mcpwm_fault_s s_faults[WASM_MCPWM_FAULTS_MAX];
static struct pal_mcpwm_cap_s   s_caps[WASM_MCPWM_CAPS_MAX];

WINK_WARN_UNUSED_RESULT
wink_status_t pal_mcpwm_new_timer(const pal_mcpwm_timer_cfg_t *cfg, pal_mcpwm_timer_handle_t *out_timer) {
    if (cfg == NULL || out_timer == NULL) return WINK_ERR_INVALID_ARG;
    for (int i = 0; i < WASM_MCPWM_TIMERS_MAX; i++) {
        if (!s_timers[i].in_use) {
            s_timers[i].in_use = true;
            s_timers[i].is_running = false;
            s_timers[i].cfg = *cfg;
            *out_timer = &s_timers[i];
            return WINK_OK;
        }
    }
    return WINK_ERR_RESOURCE_EXHAUSTED;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_mcpwm_new_oper(const pal_mcpwm_oper_cfg_t *cfg, pal_mcpwm_oper_handle_t *out_oper) {
    if (cfg == NULL || out_oper == NULL) return WINK_ERR_INVALID_ARG;
    for (int i = 0; i < WASM_MCPWM_OPERS_MAX; i++) {
        if (!s_opers[i].in_use) {
            s_opers[i].in_use = true;
            s_opers[i].cfg = *cfg;
            *out_oper = &s_opers[i];
            return WINK_OK;
        }
    }
    return WINK_ERR_RESOURCE_EXHAUSTED;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_mcpwm_new_cmp(const pal_mcpwm_cmp_cfg_t *cfg, pal_mcpwm_cmp_handle_t *out_cmp) {
    if (cfg == NULL || out_cmp == NULL) return WINK_ERR_INVALID_ARG;
    for (int i = 0; i < WASM_MCPWM_CMPS_MAX; i++) {
        if (!s_cmps[i].in_use) {
            s_cmps[i].in_use = true;
            s_cmps[i].cfg = *cfg;
            s_cmps[i].duty_ticks = cfg->initial_duty_ticks;
            *out_cmp = &s_cmps[i];
            return WINK_OK;
        }
    }
    return WINK_ERR_RESOURCE_EXHAUSTED;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_mcpwm_new_fault(const pal_mcpwm_fault_cfg_t *cfg, pal_mcpwm_fault_handle_t *out_fault) {
    if (cfg == NULL || out_fault == NULL) return WINK_ERR_INVALID_ARG;
    for (int i = 0; i < WASM_MCPWM_FAULTS_MAX; i++) {
        if (!s_faults[i].in_use) {
            s_faults[i].in_use = true;
            s_faults[i].cfg = *cfg;
            s_faults[i].tripped = false;
            *out_fault = &s_faults[i];
            return WINK_OK;
        }
    }
    return WINK_ERR_RESOURCE_EXHAUSTED;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_mcpwm_new_capture(const pal_mcpwm_cap_cfg_t *cfg, pal_mcpwm_cap_handle_t *out_cap) {
    if (cfg == NULL || out_cap == NULL) return WINK_ERR_INVALID_ARG;
    for (int i = 0; i < WASM_MCPWM_CAPS_MAX; i++) {
        if (!s_caps[i].in_use) {
            s_caps[i].in_use = true;
            s_caps[i].cfg = *cfg;
            *out_cap = &s_caps[i];
            return WINK_OK;
        }
    }
    return WINK_ERR_RESOURCE_EXHAUSTED;
}

wink_status_t pal_mcpwm_timer_start(pal_mcpwm_timer_handle_t t) {
    if (t == NULL) return WINK_ERR_INVALID_ARG;
    t->is_running = true;
    return WINK_OK;
}

wink_status_t pal_mcpwm_timer_stop(pal_mcpwm_timer_handle_t t) {
    if (t == NULL) return WINK_ERR_INVALID_ARG;
    t->is_running = false;
    return WINK_OK;
}

wink_status_t pal_mcpwm_set_duty_ticks(pal_mcpwm_cmp_handle_t cmp, uint32_t duty_ticks) {
    if (cmp == NULL) return WINK_ERR_INVALID_ARG;
    cmp->duty_ticks = duty_ticks;
    return WINK_OK;
}

wink_status_t pal_mcpwm_sync_gpio_config(wink_pin_t sync_gpio, bool active_level) {
    (void)sync_gpio;
    (void)active_level;
    return WINK_OK;
}

wink_status_t pal_mcpwm_timer_enable_phase_lock(pal_mcpwm_timer_handle_t t, uint32_t phase_ticks) {
    (void)t;
    (void)phase_ticks;
    return WINK_OK;
}

wink_status_t pal_mcpwm_trigger_software_sync(void) {
    return WINK_OK;
}

wink_status_t pal_mcpwm_fault_clear(pal_mcpwm_fault_handle_t f) {
    if (f == NULL) return WINK_ERR_INVALID_ARG;
    f->tripped = false;
    return WINK_OK;
}

void pal_mcpwm_del_timer(pal_mcpwm_timer_handle_t t) {
    if (t == NULL) return;
    t->in_use = false;
    t->is_running = false;
}

uint32_t js_pal_mcpwm_get_duty_ticks(uint8_t mcpwm_unit, uint8_t cmp_id) {
    (void)mcpwm_unit;
    if (cmp_id < WASM_MCPWM_CMPS_MAX && s_cmps[cmp_id].in_use) {
        return s_cmps[cmp_id].duty_ticks;
    }
    return 0;
}
