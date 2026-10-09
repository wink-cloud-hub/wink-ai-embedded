// SPDX-License-Identifier: LGPL-3.0-only
#include "driver/gptimer.h"
#include "hal/pal_hwtimer.h"
#include "osal/pal_osal.h"
#include "esp_sim_handle.h"
#include "esp_idf_wink.h"
#include <string.h>

#if defined(__EMSCRIPTEN__)
#  include <emscripten.h>
#  define WINK_SIM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#  define WINK_SIM_EXPORT
#endif

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
    uint64_t reload_count;
    uint64_t stopped_count;
    uint64_t raw_count_offset;
    uint32_t token;
};

static struct gptimer_t s_gptimers[PAL_HWTIMERS_MAX];
_Static_assert(PAL_HWTIMERS_MAX <= 64, "PAL_HWTIMERS_MAX must not exceed 64 (handle encoding limit)");

static struct gptimer_t *resolve_gptimer(gptimer_handle_t timer) {
    if (!timer) {
        return NULL;
    }
    uint32_t slot = 0;
    if (!esp_sim_handle_decode(timer, ESP_SIM_HANDLE_GPTIMER, PAL_HWTIMERS_MAX, &slot)) {
        return NULL;
    }
    struct gptimer_t *t = &s_gptimers[slot];
    if (!t->in_use || t->token != (uint32_t)(uintptr_t)timer || t->id != slot) {
        return NULL;
    }
    return t;
}

static void on_hwtimer_isr(void *arg) {
    struct gptimer_t *t = (struct gptimer_t *)arg;
    if (t && t->in_use && t->running && t->alarm_cb) {
        gptimer_handle_t timer_handle = (gptimer_handle_t)(uintptr_t)t->token;
        if (resolve_gptimer(timer_handle) != t) {
            return;
        }
        uint64_t now_val = 0;
        gptimer_get_raw_count(timer_handle, &now_val);
        gptimer_alarm_event_data_t edata = {
            .count_value = now_val,
            .alarm_value = t->alarm_count
        };
        (void)t->alarm_cb(timer_handle, &edata, t->user_data);
        if (t->auto_reload) {
            gptimer_set_raw_count(timer_handle, t->reload_count);
        }
    }
}

esp_err_t gptimer_new_timer(const gptimer_config_t *config, gptimer_handle_t *ret_timer) {
    if (!config || !ret_timer || config->resolution_hz == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    for (int i = 0; i < PAL_HWTIMERS_MAX; i++) {
        if (!s_gptimers[i].in_use) {
            uint32_t token = esp_sim_handle_issue(ESP_SIM_HANDLE_GPTIMER, (uint8_t)i);
            if (!token) {
                return ESP_ERR_NO_MEM;
            }
            s_gptimers[i].in_use = true;
            s_gptimers[i].token = token;
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
            *ret_timer = (gptimer_handle_t)(uintptr_t)token;
            return ESP_OK;
        }
    }
    return ESP_ERR_NO_MEM;
}

esp_err_t gptimer_register_event_callbacks(gptimer_handle_t timer, const gptimer_event_callbacks_t *cbs, void *user_data) {
    struct gptimer_t *t = resolve_gptimer(timer);
    if (!t || !cbs) {
        return ESP_ERR_INVALID_ARG;
    }
    t->alarm_cb = cbs->on_alarm;
    t->user_data = user_data;
    return ESP_OK;
}

esp_err_t gptimer_get_raw_count(gptimer_handle_t timer, uint64_t *value) {
    struct gptimer_t *t = resolve_gptimer(timer);
    if (!t || !value) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!t->running) {
        *value = t->stopped_count;
        return ESP_OK;
    }
    uint64_t now_us = pal_os_get_us();
    if (t->resolution_hz == 1000000ULL) {
        *value = now_us + t->raw_count_offset;
    } else {
        *value = ((now_us * (uint64_t)t->resolution_hz) / 1000000ULL) + t->raw_count_offset;
    }
    return ESP_OK;
}

esp_err_t gptimer_get_captured_count(gptimer_handle_t timer, uint64_t *value) {
    return gptimer_get_raw_count(timer, value);
}

esp_err_t gptimer_get_resolution(gptimer_handle_t timer, uint32_t *out_resolution) {
    struct gptimer_t *t = resolve_gptimer(timer);
    if (!t || !out_resolution) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_resolution = t->resolution_hz;
    return ESP_OK;
}

esp_err_t gptimer_set_raw_count(gptimer_handle_t timer, uint64_t value) {
    struct gptimer_t *t = resolve_gptimer(timer);
    if (!t) {
        return ESP_ERR_INVALID_ARG;
    }
    t->stopped_count = value;
    uint64_t now_us = pal_os_get_us();
    uint64_t base_count = (t->resolution_hz == 1000000ULL)
        ? now_us
        : ((now_us * (uint64_t)t->resolution_hz) / 1000000ULL);
    t->raw_count_offset = value - base_count;
    return ESP_OK;
}

esp_err_t gptimer_set_alarm_action(gptimer_handle_t timer, const gptimer_alarm_config_t *config) {
    struct gptimer_t *t = resolve_gptimer(timer);
    if (!t || !config) {
        return ESP_ERR_INVALID_ARG;
    }
    bool was_running = t->running;
    if (was_running && t->alarm_configured) {
        pal_hwtimer_stop(t->id);
    }
    if (t->alarm_configured) {
        pal_hwtimer_deinit(t->id);
        t->alarm_configured = false;
    }

    t->alarm_count = config->alarm_count;
    t->auto_reload = config->flags.auto_reload_on_alarm ? true : false;
    t->reload_count = config->reload_count;

    uint64_t us = (config->alarm_count * 1000000ULL) / t->resolution_hz;
    if (us < 10000ULL) {
        us = 10000ULL;
    }

    pal_hwtimer_cfg_t pcfg = {
        .timer_id = t->id,
        .period_us = (uint32_t)us,
        .oneshot = !t->auto_reload,
        .auto_start = false,
        .core_affinity = PAL_OS_CORE_0,
        .isr_priority = 1,
        .uses_fpu = false,
        .callback = on_hwtimer_isr,
        .callback_arg = t
    };
    wink_status_t st = pal_hwtimer_init(&pcfg);
    if (st != WINK_OK) {
        t->running = false;
        return esp_err_from_wink(st);
    }
    t->alarm_configured = true;
    if (was_running) {
        wink_status_t sst = pal_hwtimer_start(t->id);
        if (sst != WINK_OK) {
            t->running = false;
            return esp_err_from_wink(sst);
        }
    }
    return ESP_OK;
}

esp_err_t gptimer_enable(gptimer_handle_t timer) {
    struct gptimer_t *t = resolve_gptimer(timer);
    if (!t) {
        return ESP_ERR_INVALID_ARG;
    }
    t->enabled = true;
    return ESP_OK;
}

esp_err_t gptimer_disable(gptimer_handle_t timer) {
    struct gptimer_t *t = resolve_gptimer(timer);
    if (!t) {
        return ESP_ERR_INVALID_ARG;
    }
    t->enabled = false;
    return ESP_OK;
}

esp_err_t gptimer_start(gptimer_handle_t timer) {
    struct gptimer_t *t = resolve_gptimer(timer);
    if (!t) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!t->enabled) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!t->running) {
        uint64_t now_us = pal_os_get_us();
        uint64_t base_count = (t->resolution_hz == 1000000ULL)
            ? now_us
            : ((now_us * (uint64_t)t->resolution_hz) / 1000000ULL);
        t->raw_count_offset = t->stopped_count - base_count;
        t->running = true;
    }
    if (t->alarm_configured) {
        wink_status_t st = pal_hwtimer_start(t->id);
        if (st != WINK_OK) {
            t->running = false;
            return esp_err_from_wink(st);
        }
    }
    return ESP_OK;
}

esp_err_t gptimer_stop(gptimer_handle_t timer) {
    struct gptimer_t *t = resolve_gptimer(timer);
    if (!t) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!t->enabled) {
        return ESP_ERR_INVALID_STATE;
    }
    if (t->running) {
        gptimer_get_raw_count(timer, &t->stopped_count);
        t->running = false;
    }
    if (t->alarm_configured) {
        wink_status_t st = pal_hwtimer_stop(t->id);
        if (st != WINK_OK) {
            return esp_err_from_wink(st);
        }
    }
    return ESP_OK;
}

esp_err_t gptimer_del_timer(gptimer_handle_t timer) {
    struct gptimer_t *t = resolve_gptimer(timer);
    if (!t) {
        return ESP_ERR_INVALID_ARG;
    }
    if (t->running || t->alarm_configured) {
        pal_hwtimer_stop(t->id);
    }
    if (t->alarm_configured) {
        pal_hwtimer_deinit(t->id);
        t->alarm_configured = false;
    }
    t->enabled = false;
    t->running = false;
    t->alarm_cb = NULL;
    t->user_data = NULL;
    t->in_use = false;
    t->token = 0;
    return ESP_OK;
}

void esp_gptimer_reset(void) {
    for (int i = 0; i < PAL_HWTIMERS_MAX; i++) {
        if (s_gptimers[i].alarm_configured) {
            pal_hwtimer_stop(s_gptimers[i].id);
            pal_hwtimer_deinit(s_gptimers[i].id);
        }
    }
    memset(s_gptimers, 0, sizeof(s_gptimers));
}

WINK_SIM_EXPORT double sim_timer_get_counter(uint32_t timer_id) {
    if (timer_id >= PAL_HWTIMERS_MAX) {
        return 0;
    }
    struct gptimer_t *t = &s_gptimers[timer_id];
    if (!t->in_use) {
        return 0;
    }
    if (!t->running) {
        return t->stopped_count;
    }
    uint64_t now_us = pal_os_get_us();
    if (t->resolution_hz == 1000000ULL) {
        return now_us + t->raw_count_offset;
    } else {
        return ((now_us * (uint64_t)t->resolution_hz) / 1000000ULL) + t->raw_count_offset;
    }
}

