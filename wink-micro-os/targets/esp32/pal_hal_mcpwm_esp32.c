// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_hal_mcpwm_esp32.c
 * @brief ESP32 target PAL MCPWM hardware driver using ESP-IDF 5.4+ mcpwm_prelude driver.
 */
#include "pal_hal.h"
#include "hal/pal_mcpwm.h"
#include "pal_resource.h"
#include "pal_spinlock.h"
#include "pal_log.h"

#include <string.h>

#if defined(ESP_PLATFORM)
#include "driver/mcpwm_prelude.h"
#include "esp_err.h"

#define LOG_TAG "pal_mcpwm"

#define ESP32_MCPWM_TIMERS_MAX 6
#define ESP32_MCPWM_OPERS_MAX  6
#define ESP32_MCPWM_CMPS_MAX   6
#define ESP32_MCPWM_FAULTS_MAX 6
#define ESP32_MCPWM_CAPS_MAX   6

struct pal_mcpwm_timer_s {
    bool                  in_use;
    pal_mcpwm_timer_cfg_t cfg;
    mcpwm_timer_handle_t  timer_handle;
};

struct pal_mcpwm_oper_s {
    bool                 in_use;
    pal_mcpwm_oper_cfg_t cfg;
    mcpwm_oper_handle_t  oper_handle;
    mcpwm_gen_handle_t   gen_a;
    mcpwm_gen_handle_t   gen_b;
};

struct pal_mcpwm_cmp_s {
    bool                in_use;
    pal_mcpwm_cmp_cfg_t cfg;
    mcpwm_cmpr_handle_t cmp_handle;
};

struct pal_mcpwm_fault_s {
    bool                  in_use;
    pal_mcpwm_fault_cfg_t cfg;
    mcpwm_fault_handle_t  fault_handle;
};

struct pal_mcpwm_cap_s {
    bool                 in_use;
    pal_mcpwm_cap_cfg_t  cfg;
    mcpwm_cap_channel_handle_t cap_chan;
};

static struct pal_mcpwm_timer_s s_timers[ESP32_MCPWM_TIMERS_MAX];
static struct pal_mcpwm_oper_s  s_opers[ESP32_MCPWM_OPERS_MAX];
static struct pal_mcpwm_cmp_s   s_cmps[ESP32_MCPWM_CMPS_MAX];
static struct pal_mcpwm_fault_s s_faults[ESP32_MCPWM_FAULTS_MAX];
static struct pal_mcpwm_cap_s   s_caps[ESP32_MCPWM_CAPS_MAX];

static pal_spinlock_t s_mcpwm_lock = PAL_SPINLOCK_INITIALIZER;

WINK_WARN_UNUSED_RESULT
wink_status_t pal_mcpwm_new_timer(const pal_mcpwm_timer_cfg_t *cfg, pal_mcpwm_timer_handle_t *out_timer) {
    if (cfg == NULL || out_timer == NULL || cfg->mcpwm_unit > 1 || cfg->timer_id > 2) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_mcpwm_lock);
    struct pal_mcpwm_timer_s *slot = NULL;
    for (int i = 0; i < ESP32_MCPWM_TIMERS_MAX; i++) {
        if (!s_timers[i].in_use) {
            slot = &s_timers[i];
            break;
        }
    }
    if (slot == NULL) {
        pal_spinlock_unlock(&s_mcpwm_lock);
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    wink_status_t st = pal_resource_claim(PAL_RESOURCE_MCPWM_TIMER, (uint32_t)(cfg->mcpwm_unit * 3 + cfg->timer_id), "pal_mcpwm_esp32");
    if (st != WINK_OK) {
        pal_spinlock_unlock(&s_mcpwm_lock);
        return st;
    }

    mcpwm_timer_config_t timer_config = {
        .group_id = cfg->mcpwm_unit,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000, /* 10 MHz resolution */
        .period_ticks = (cfg->pwm_freq_hz > 0) ? (10000000 / cfg->pwm_freq_hz) : 1000,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP_DOWN, /* Symmetric for FOC */
    };

    esp_err_t err = mcpwm_new_timer(&timer_config, &slot->timer_handle);
    if (err != ESP_OK) {
        pal_resource_release(PAL_RESOURCE_MCPWM_TIMER, (uint32_t)(cfg->mcpwm_unit * 3 + cfg->timer_id), "pal_mcpwm_esp32");
        pal_spinlock_unlock(&s_mcpwm_lock);
        return WINK_ERR_HARDWARE;
    }

    mcpwm_timer_enable(slot->timer_handle);

    slot->in_use = true;
    slot->cfg = *cfg;
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
    for (int i = 0; i < ESP32_MCPWM_OPERS_MAX; i++) {
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
        wink_status_t st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_pwm_a, "pal_mcpwm_esp32");
        if (st != WINK_OK) {
            pal_spinlock_unlock(&s_mcpwm_lock);
            return st;
        }
    }
    if (cfg->pin_pwm_b >= 0) {
        wink_status_t st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_pwm_b, "pal_mcpwm_esp32");
        if (st != WINK_OK) {
            if (cfg->pin_pwm_a >= 0) pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_pwm_a, "pal_mcpwm_esp32");
            pal_spinlock_unlock(&s_mcpwm_lock);
            return st;
        }
    }

    mcpwm_operator_config_t oper_config = {
        .group_id = cfg->timer->cfg.mcpwm_unit,
    };
    esp_err_t err = mcpwm_new_operator(&oper_config, &slot->oper_handle);
    if (err != ESP_OK) {
        if (cfg->pin_pwm_b >= 0) pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_pwm_b, "pal_mcpwm_esp32");
        if (cfg->pin_pwm_a >= 0) pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_pwm_a, "pal_mcpwm_esp32");
        pal_spinlock_unlock(&s_mcpwm_lock);
        return WINK_ERR_HARDWARE;
    }

    mcpwm_operator_connect_timer(slot->oper_handle, cfg->timer->timer_handle);

    /* Generator A */
    if (cfg->pin_pwm_a >= 0) {
        mcpwm_generator_config_t gen_a_config = {
            .gen_gpio_num = cfg->pin_pwm_a,
        };
        mcpwm_new_generator(slot->oper_handle, &gen_a_config, &slot->gen_a);
    }
    /* Generator B */
    if (cfg->pin_pwm_b >= 0) {
        mcpwm_generator_config_t gen_b_config = {
            .gen_gpio_num = cfg->pin_pwm_b,
        };
        mcpwm_new_generator(slot->oper_handle, &gen_b_config, &slot->gen_b);
    }

    /* Dead-time configuration */
    if (cfg->complementary_enable && slot->gen_a && slot->gen_b) {
        mcpwm_dead_time_config_t dt_red = {
            .posedge_path = MCPWM_DEAD_TIME_PATH_BYPASS,
            .negedge_path = MCPWM_DEAD_TIME_PATH_DELAY,
        };
        mcpwm_generator_set_dead_time(slot->gen_a, slot->gen_a, &dt_red);
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
    for (int i = 0; i < ESP32_MCPWM_CMPS_MAX; i++) {
        if (!s_cmps[i].in_use) {
            slot = &s_cmps[i];
            break;
        }
    }
    if (slot == NULL) {
        pal_spinlock_unlock(&s_mcpwm_lock);
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    mcpwm_comparator_config_t cmp_config = {
        .flags.update_cmp_on_tez = true,
    };
    esp_err_t err = mcpwm_new_comparator(cfg->oper->oper_handle, &cmp_config, &slot->cmp_handle);
    if (err != ESP_OK) {
        pal_spinlock_unlock(&s_mcpwm_lock);
        return WINK_ERR_HARDWARE;
    }

    mcpwm_comparator_set_compare_value(slot->cmp_handle, cfg->initial_duty_ticks);

    slot->in_use = true;
    slot->cfg = *cfg;
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
    for (int i = 0; i < ESP32_MCPWM_FAULTS_MAX; i++) {
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
        wink_status_t st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->fault_pin, "pal_mcpwm_esp32");
        if (st != WINK_OK) {
            pal_spinlock_unlock(&s_mcpwm_lock);
            return st;
        }

        mcpwm_gpio_fault_config_t gpio_fault_config = {
            .group_id = 0,
            .gpio_num = cfg->fault_pin,
            .flags.active_level = cfg->active_level,
        };
        mcpwm_new_gpio_fault(&gpio_fault_config, &slot->fault_handle);
    }

    slot->in_use = true;
    slot->cfg = *cfg;
    *out_fault = slot;

    pal_spinlock_unlock(&s_mcpwm_lock);
    return WINK_OK;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_mcpwm_new_capture(const pal_mcpwm_cap_cfg_t *cfg, pal_mcpwm_cap_handle_t *out_cap) {
    if (cfg == NULL || out_cap == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_mcpwm_lock);
    struct pal_mcpwm_cap_s *slot = NULL;
    for (int i = 0; i < ESP32_MCPWM_CAPS_MAX; i++) {
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
        wink_status_t st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->cap_pin, "pal_mcpwm_esp32");
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
    esp_err_t err = mcpwm_timer_start_stop(t->timer_handle, MCPWM_TIMER_START_NO_STOP);
    return (err == ESP_OK) ? WINK_OK : WINK_ERR_HARDWARE;
}

wink_status_t pal_mcpwm_timer_stop(pal_mcpwm_timer_handle_t t) {
    if (t == NULL) return WINK_ERR_INVALID_ARG;
    esp_err_t err = mcpwm_timer_start_stop(t->timer_handle, MCPWM_TIMER_STOP_EMPTY);
    return (err == ESP_OK) ? WINK_OK : WINK_ERR_HARDWARE;
}

wink_status_t pal_mcpwm_set_duty_ticks(pal_mcpwm_cmp_handle_t cmp, uint32_t duty_ticks) {
    if (cmp == NULL) return WINK_ERR_INVALID_ARG;
    esp_err_t err = mcpwm_comparator_set_compare_value(cmp->cmp_handle, duty_ticks);
    return (err == ESP_OK) ? WINK_OK : WINK_ERR_HARDWARE;
}

wink_status_t pal_mcpwm_sync_gpio_config(wink_pin_t sync_gpio, bool active_level) {
    (void)active_level;
    if (sync_gpio < 0) return WINK_ERR_INVALID_ARG;
    return pal_resource_claim(PAL_RESOURCE_MCPWM_SYNC_GPIO, (uint32_t)sync_gpio, "pal_mcpwm_esp32");
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
    (void)f;
    return WINK_OK;
}

void pal_mcpwm_del_timer(pal_mcpwm_timer_handle_t t) {
    if (t == NULL) return;
    pal_spinlock_lock(&s_mcpwm_lock);
    if (t->in_use) {
        mcpwm_timer_disable(t->timer_handle);
        mcpwm_del_timer(t->timer_handle);
        t->timer_handle = NULL;
        pal_resource_release(PAL_RESOURCE_MCPWM_TIMER, (uint32_t)(t->cfg.mcpwm_unit * 3 + t->cfg.timer_id), "pal_mcpwm_esp32");
        t->in_use = false;
    }
    pal_spinlock_unlock(&s_mcpwm_lock);
}

#else

/* Non-ESP32 fallback stubs for cross-compilation static analysis */
WINK_WARN_UNUSED_RESULT wink_status_t pal_mcpwm_new_timer(const pal_mcpwm_timer_cfg_t *cfg, pal_mcpwm_timer_handle_t *out_timer) { (void)cfg; (void)out_timer; return WINK_ERR_NOT_SUPPORTED; }
WINK_WARN_UNUSED_RESULT wink_status_t pal_mcpwm_new_oper(const pal_mcpwm_oper_cfg_t *cfg, pal_mcpwm_oper_handle_t *out_oper) { (void)cfg; (void)out_oper; return WINK_ERR_NOT_SUPPORTED; }
WINK_WARN_UNUSED_RESULT wink_status_t pal_mcpwm_new_cmp(const pal_mcpwm_cmp_cfg_t *cfg, pal_mcpwm_cmp_handle_t *out_cmp) { (void)cfg; (void)out_cmp; return WINK_ERR_NOT_SUPPORTED; }
WINK_WARN_UNUSED_RESULT wink_status_t pal_mcpwm_new_fault(const pal_mcpwm_fault_cfg_t *cfg, pal_mcpwm_fault_handle_t *out_fault) { (void)cfg; (void)out_fault; return WINK_ERR_NOT_SUPPORTED; }
WINK_WARN_UNUSED_RESULT wink_status_t pal_mcpwm_new_capture(const pal_mcpwm_cap_cfg_t *cfg, pal_mcpwm_cap_handle_t *out_cap) { (void)cfg; (void)out_cap; return WINK_ERR_NOT_SUPPORTED; }
wink_status_t pal_mcpwm_timer_start(pal_mcpwm_timer_handle_t t) { (void)t; return WINK_ERR_NOT_SUPPORTED; }
wink_status_t pal_mcpwm_timer_stop(pal_mcpwm_timer_handle_t t) { (void)t; return WINK_ERR_NOT_SUPPORTED; }
wink_status_t pal_mcpwm_set_duty_ticks(pal_mcpwm_cmp_handle_t cmp, uint32_t duty_ticks) { (void)cmp; (void)duty_ticks; return WINK_ERR_NOT_SUPPORTED; }
wink_status_t pal_mcpwm_sync_gpio_config(wink_pin_t sync_gpio, bool active_level) { (void)sync_gpio; (void)active_level; return WINK_ERR_NOT_SUPPORTED; }
wink_status_t pal_mcpwm_timer_enable_phase_lock(pal_mcpwm_timer_handle_t t, uint32_t phase_ticks) { (void)t; (void)phase_ticks; return WINK_ERR_NOT_SUPPORTED; }
wink_status_t pal_mcpwm_trigger_software_sync(void) { return WINK_ERR_NOT_SUPPORTED; }
wink_status_t pal_mcpwm_fault_clear(pal_mcpwm_fault_handle_t f) { (void)f; return WINK_ERR_NOT_SUPPORTED; }
void pal_mcpwm_del_timer(pal_mcpwm_timer_handle_t t) { (void)t; }

#endif
