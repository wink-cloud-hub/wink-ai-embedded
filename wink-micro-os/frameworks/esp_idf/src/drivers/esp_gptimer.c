// SPDX-License-Identifier: LGPL-3.0-only
#include "driver/gptimer.h"
#include "hal/pal_hwtimer.h"
#include "osal/pal_osal.h"
#include <string.h>

struct gptimer_t {
    bool in_use;
    bool enabled;
    bool running;
    bool alarm_configured;
    uint8_t id;
    uint32_t resolution_hz;
    gptimer_alarm_cb_t alarm_cb;
    void *user_data;
    uint64_t alarm_count;
    bool auto_reload;
    uint64_t raw_count_offset;
};

static struct gptimer_t s_gptimers[PAL_HWTIMERS_MAX];

static void on_hwtimer_isr(void *arg) {
    struct gptimer_t *t = (struct gptimer_t *)arg;
    if (t && t->alarm_cb) {
        uint64_t now_val = 0;
        gptimer_get_raw_count(t, &now_val);
        gptimer_alarm_event_data_t edata = {
            .count_value = now_val,
            .alarm_value = t->alarm_count
        };
        (void)t->alarm_cb((gptimer_handle_t)t, &edata, t->user_data);
    }
}

esp_err_t gptimer_new_timer(const gptimer_config_t *config, gptimer_handle_t *ret_timer) {
    if (!config || !ret_timer || config->resolution_hz == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    for (int i = 0; i < PAL_HWTIMERS_MAX; i++) {
        if (!s_gptimers[i].in_use) {
            s_gptimers[i].in_use = true;
            s_gptimers[i].enabled = false;
            s_gptimers[i].running = false;
            s_gptimers[i].alarm_configured = false;
            s_gptimers[i].id = (uint8_t)i;
            s_gptimers[i].resolution_hz = config->resolution_hz;
            s_gptimers[i].alarm_cb = NULL;
            s_gptimers[i].user_data = NULL;
            s_gptimers[i].alarm_count = 0;
            s_gptimers[i].auto_reload = false;
            s_gptimers[i].raw_count_offset = 0;
            *ret_timer = &s_gptimers[i];
            return ESP_OK;
        }
    }
    return ESP_ERR_NO_MEM;
}

esp_err_t gptimer_register_event_callbacks(gptimer_handle_t timer, const gptimer_event_callbacks_t *cbs, void *user_data) {
    if (!timer || !timer->in_use || !cbs) {
        return ESP_ERR_INVALID_ARG;
    }
    timer->alarm_cb = cbs->on_alarm;
    timer->user_data = user_data;
    return ESP_OK;
}

esp_err_t gptimer_get_raw_count(gptimer_handle_t timer, uint64_t *value) {
    if (!timer || !timer->in_use || !value) {
        return ESP_ERR_INVALID_ARG;
    }
    uint64_t now_us = pal_os_get_us();
    if (timer->resolution_hz == 1000000ULL) {
        *value = now_us + timer->raw_count_offset;
    } else {
        *value = ((now_us * (uint64_t)timer->resolution_hz) / 1000000ULL) + timer->raw_count_offset;
    }
    return ESP_OK;
}

esp_err_t gptimer_set_raw_count(gptimer_handle_t timer, uint64_t value) {
    if (!timer || !timer->in_use) {
        return ESP_ERR_INVALID_ARG;
    }
    uint64_t now_us = pal_os_get_us();
    uint64_t base_count = (timer->resolution_hz == 1000000ULL)
        ? now_us
        : ((now_us * (uint64_t)timer->resolution_hz) / 1000000ULL);
    timer->raw_count_offset = value - base_count;
    return ESP_OK;
}

esp_err_t gptimer_set_alarm_action(gptimer_handle_t timer, const gptimer_alarm_config_t *config) {
    if (!timer || !timer->in_use || !config) {
        return ESP_ERR_INVALID_ARG;
    }
    timer->alarm_count = config->alarm_count;
    timer->auto_reload = config->flags.auto_reload_on_alarm ? true : false;

    uint64_t us = (config->alarm_count * 1000000ULL) / timer->resolution_hz;
    if (us < 10000ULL) {
        us = 10000ULL;
    }

    if (timer->alarm_configured) {
        pal_hwtimer_deinit(timer->id);
        timer->alarm_configured = false;
    }

    pal_hwtimer_cfg_t pcfg = {
        .timer_id = timer->id,
        .period_us = (uint32_t)us,
        .oneshot = !timer->auto_reload,
        .auto_start = false,
        .core_affinity = PAL_OS_CORE_0,
        .isr_priority = 1,
        .uses_fpu = false,
        .callback = on_hwtimer_isr,
        .callback_arg = timer
    };
    wink_status_t st = pal_hwtimer_init(&pcfg);
    if (st != WINK_OK) {
        return ESP_FAIL;
    }
    timer->alarm_configured = true;
    if (timer->running) {
        pal_hwtimer_start(timer->id);
    }
    return ESP_OK;
}

esp_err_t gptimer_enable(gptimer_handle_t timer) {
    if (!timer || !timer->in_use) {
        return ESP_ERR_INVALID_ARG;
    }
    timer->enabled = true;
    return ESP_OK;
}

esp_err_t gptimer_disable(gptimer_handle_t timer) {
    if (!timer || !timer->in_use) {
        return ESP_ERR_INVALID_ARG;
    }
    timer->enabled = false;
    return ESP_OK;
}

esp_err_t gptimer_start(gptimer_handle_t timer) {
    if (!timer || !timer->in_use) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!timer->enabled) {
        return ESP_ERR_INVALID_STATE;
    }
    timer->running = true;
    if (timer->alarm_configured) {
        wink_status_t st = pal_hwtimer_start(timer->id);
        if (st != WINK_OK) {
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

esp_err_t gptimer_stop(gptimer_handle_t timer) {
    if (!timer || !timer->in_use) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!timer->enabled) {
        return ESP_ERR_INVALID_STATE;
    }
    timer->running = false;
    if (timer->alarm_configured) {
        wink_status_t st = pal_hwtimer_stop(timer->id);
        if (st != WINK_OK) {
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

esp_err_t gptimer_del_timer(gptimer_handle_t timer) {
    if (!timer || !timer->in_use) {
        return ESP_ERR_INVALID_ARG;
    }
    if (timer->alarm_configured) {
        pal_hwtimer_deinit(timer->id);
        timer->alarm_configured = false;
    }
    timer->in_use = false;
    return ESP_OK;
}

void esp_gptimer_reset(void) {
    for (int i = 0; i < PAL_HWTIMERS_MAX; i++) {
        if (s_gptimers[i].alarm_configured) {
            pal_hwtimer_deinit(s_gptimers[i].id);
        }
    }
    memset(s_gptimers, 0, sizeof(s_gptimers));
}
