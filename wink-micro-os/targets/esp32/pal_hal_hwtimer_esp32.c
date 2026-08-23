// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_hal_hwtimer_esp32.c
 * @brief ESP32 target PAL HWTIMER driver using ESP-IDF gptimer (ESP-IDF 5.4+).
 */
#include "pal_hal.h"
#include "hal/pal_hwtimer.h"
#include "pal_resource.h"
#include "pal_spinlock.h"
#include "pal_log.h"

#include <string.h>

#if defined(ESP_PLATFORM)
#include "driver/gptimer.h"
#include "esp_err.h"
#include "esp_intr_alloc.h"

#define LOG_TAG "pal_hwtimer"

typedef struct {
    bool              in_use;
    bool              is_running;
    pal_hwtimer_cfg_t cfg;
    gptimer_handle_t  gptimer;
} esp32_hwtimer_slot_t;

static esp32_hwtimer_slot_t s_timers[PAL_HWTIMERS_MAX];
static pal_spinlock_t s_hwtimer_lock = PAL_SPINLOCK_INITIALIZER;

static bool IRAM_ATTR esp32_gptimer_on_alarm(gptimer_handle_t timer,
                                            const gptimer_alarm_event_data_t *edata,
                                            void *user_ctx) {
    (void)timer;
    (void)edata;
    esp32_hwtimer_slot_t *slot = (esp32_hwtimer_slot_t *)user_ctx;
    if (slot != NULL && slot->cfg.callback != NULL) {
        if (slot->cfg.oneshot) {
            slot->is_running = false;
        }
        slot->cfg.callback(slot->cfg.callback_arg);
    }
    return false;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_hwtimer_init(const pal_hwtimer_cfg_t *cfg) {
    if (cfg == NULL || cfg->timer_id >= PAL_HWTIMERS_MAX || cfg->period_us == 0) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_hwtimer_lock);
    esp32_hwtimer_slot_t *slot = &s_timers[cfg->timer_id];
    if (slot->in_use) {
        pal_spinlock_unlock(&s_hwtimer_lock);
        return WINK_ERR_BUSY;
    }

    wink_status_t st = pal_resource_claim(PAL_RESOURCE_HWTIMER, cfg->timer_id, "pal_hwtimer_esp32");
    if (st != WINK_OK) {
        pal_spinlock_unlock(&s_hwtimer_lock);
        return st;
    }

    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, /* 1 MHz resolution -> 1 tick = 1 us */
    };

    esp_err_t err = gptimer_new_timer(&timer_config, &slot->gptimer);
    if (err != ESP_OK) {
        pal_resource_release(PAL_RESOURCE_HWTIMER, cfg->timer_id, "pal_hwtimer_esp32");
        pal_spinlock_unlock(&s_hwtimer_lock);
        return WINK_ERR_HARDWARE;
    }

    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        .alarm_count = cfg->period_us,
        .flags.auto_reload_on_alarm = !cfg->oneshot,
    };

    err = gptimer_set_alarm_action(slot->gptimer, &alarm_config);
    if (err != ESP_OK) {
        gptimer_del_timer(slot->gptimer);
        pal_resource_release(PAL_RESOURCE_HWTIMER, cfg->timer_id, "pal_hwtimer_esp32");
        pal_spinlock_unlock(&s_hwtimer_lock);
        return WINK_ERR_HARDWARE;
    }

    gptimer_event_callbacks_t cbs = {
        .on_alarm = esp32_gptimer_on_alarm,
    };
    gptimer_register_event_callbacks(slot->gptimer, &cbs, slot);

    gptimer_enable(slot->gptimer);

    slot->in_use = true;
    slot->cfg = *cfg;
    slot->is_running = false;

    if (cfg->auto_start) {
        gptimer_start(slot->gptimer);
        slot->is_running = true;
    }

    pal_spinlock_unlock(&s_hwtimer_lock);
    return WINK_OK;
}

wink_status_t pal_hwtimer_start(uint8_t timer_id) {
    if (timer_id >= PAL_HWTIMERS_MAX) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_hwtimer_lock);
    esp32_hwtimer_slot_t *slot = &s_timers[timer_id];
    if (!slot->in_use) {
        pal_spinlock_unlock(&s_hwtimer_lock);
        return WINK_ERR_INVALID_STATE;
    }

    esp_err_t err = gptimer_start(slot->gptimer);
    if (err == ESP_OK) {
        slot->is_running = true;
    }
    pal_spinlock_unlock(&s_hwtimer_lock);
    return (err == ESP_OK) ? WINK_OK : WINK_ERR_HARDWARE;
}

wink_status_t pal_hwtimer_stop(uint8_t timer_id) {
    if (timer_id >= PAL_HWTIMERS_MAX) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_hwtimer_lock);
    esp32_hwtimer_slot_t *slot = &s_timers[timer_id];
    if (!slot->in_use) {
        pal_spinlock_unlock(&s_hwtimer_lock);
        return WINK_ERR_INVALID_STATE;
    }

    esp_err_t err = gptimer_stop(slot->gptimer);
    if (err == ESP_OK) {
        slot->is_running = false;
    }
    pal_spinlock_unlock(&s_hwtimer_lock);
    return (err == ESP_OK) ? WINK_OK : WINK_ERR_HARDWARE;
}

wink_status_t pal_hwtimer_change_period(uint8_t timer_id, uint32_t new_period_us) {
    if (timer_id >= PAL_HWTIMERS_MAX || new_period_us == 0) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_hwtimer_lock);
    esp32_hwtimer_slot_t *slot = &s_timers[timer_id];
    if (!slot->in_use) {
        pal_spinlock_unlock(&s_hwtimer_lock);
        return WINK_ERR_INVALID_STATE;
    }

    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        .alarm_count = new_period_us,
        .flags.auto_reload_on_alarm = !slot->cfg.oneshot,
    };

    esp_err_t err = gptimer_set_alarm_action(slot->gptimer, &alarm_config);
    if (err == ESP_OK) {
        slot->cfg.period_us = new_period_us;
    }
    pal_spinlock_unlock(&s_hwtimer_lock);
    return (err == ESP_OK) ? WINK_OK : WINK_ERR_HARDWARE;
}

void pal_hwtimer_deinit(uint8_t timer_id) {
    if (timer_id >= PAL_HWTIMERS_MAX) {
        return;
    }

    pal_spinlock_lock(&s_hwtimer_lock);
    esp32_hwtimer_slot_t *slot = &s_timers[timer_id];
    if (!slot->in_use) {
        pal_spinlock_unlock(&s_hwtimer_lock);
        return;
    }

    gptimer_stop(slot->gptimer);
    gptimer_disable(slot->gptimer);
    gptimer_del_timer(slot->gptimer);
    slot->gptimer = NULL;

    pal_resource_release(PAL_RESOURCE_HWTIMER, timer_id, "pal_hwtimer_esp32");
    slot->in_use = false;
    slot->is_running = false;
    pal_spinlock_unlock(&s_hwtimer_lock);
}

wink_status_t pal_hwtimer_fire_soft(uint8_t timer_id) {
    (void)timer_id;
    return WINK_ERR_NOT_SUPPORTED;
}

#else

/* Non-ESP32 fallback stubs for cross-compilation static analysis */
WINK_WARN_UNUSED_RESULT wink_status_t pal_hwtimer_init(const pal_hwtimer_cfg_t *cfg) { (void)cfg; return WINK_ERR_NOT_SUPPORTED; }
wink_status_t pal_hwtimer_start(uint8_t timer_id) { (void)timer_id; return WINK_ERR_NOT_SUPPORTED; }
wink_status_t pal_hwtimer_stop(uint8_t timer_id) { (void)timer_id; return WINK_ERR_NOT_SUPPORTED; }
wink_status_t pal_hwtimer_change_period(uint8_t timer_id, uint32_t new_period_us) { (void)timer_id; (void)new_period_us; return WINK_ERR_NOT_SUPPORTED; }
void pal_hwtimer_deinit(uint8_t timer_id) { (void)timer_id; }
wink_status_t pal_hwtimer_fire_soft(uint8_t timer_id) { (void)timer_id; return WINK_ERR_NOT_SUPPORTED; }

#endif
