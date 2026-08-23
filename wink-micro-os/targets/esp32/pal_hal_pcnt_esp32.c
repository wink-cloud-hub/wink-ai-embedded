// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_hal_pcnt_esp32.c
 * @brief ESP32 target PAL PCNT quadrature pulse counter hardware driver.
 *
 * Implements hardware-level counting with 64-bit software accumulator and glitch filter.
 * Complies with document E-001 (filter bounds and 16-bit hardware overflow handling).
 */
#include "pal_hal.h"
#include "hal/pal_pcnt.h"
#include "pal_resource.h"
#include "pal_spinlock.h"
#include "pal_log.h"

#include <string.h>

#if defined(ESP_PLATFORM)
#include "driver/pulse_cnt.h"
#include "esp_err.h"

#define LOG_TAG "pal_pcnt"

#define PCNT_DEFAULT_HIGH_LIMIT 32767
#define PCNT_DEFAULT_LOW_LIMIT  (-32768)

struct pal_pcnt_unit_s {
    bool                in_use;
    uint8_t             id;
    pal_pcnt_config_t   cfg;
    pcnt_unit_handle_t  unit_handle;
    pcnt_channel_handle_t chan_a;
    pcnt_channel_handle_t chan_b;
    volatile int64_t    accum_count;
    int16_t             high_limit;
    int16_t             low_limit;
};

static struct pal_pcnt_unit_s s_pcnt_units[PAL_PCNT_UNIT_MAX];
static pal_spinlock_t s_pcnt_lock = PAL_SPINLOCK_INITIALIZER;

static bool IRAM_ATTR esp32_pcnt_on_reach(pcnt_unit_handle_t unit,
                                          const pcnt_watch_event_data_t *edata,
                                          void *user_data) {
    (void)unit;
    struct pal_pcnt_unit_s *u = (struct pal_pcnt_unit_s *)user_data;
    if (u != NULL && edata != NULL) {
        if (edata->watch_point_value == u->high_limit) {
            u->accum_count += u->high_limit;
            pcnt_unit_clear_count(u->unit_handle);
        } else if (edata->watch_point_value == u->low_limit) {
            u->accum_count += u->low_limit;
            pcnt_unit_clear_count(u->unit_handle);
        }
    }
    return false;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_pcnt_init(const pal_pcnt_config_t *cfg,
                            pal_pcnt_unit_handle_t *out_handle) {
    if (cfg == NULL || out_handle == NULL || cfg->pin_a < 0) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_pcnt_lock);

    struct pal_pcnt_unit_s *slot = NULL;
    for (uint8_t i = 0; i < PAL_PCNT_UNIT_MAX; i++) {
        if (!s_pcnt_units[i].in_use) {
            slot = &s_pcnt_units[i];
            slot->id = i;
            break;
        }
    }
    if (slot == NULL) {
        pal_spinlock_unlock(&s_pcnt_lock);
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    wink_status_t st = pal_resource_claim(PAL_RESOURCE_PCNT_UNIT, slot->id, "pal_pcnt_esp32");
    if (st != WINK_OK) {
        pal_spinlock_unlock(&s_pcnt_lock);
        return st;
    }

    st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_a, "pal_pcnt_esp32");
    if (st != WINK_OK) {
        pal_resource_release(PAL_RESOURCE_PCNT_UNIT, slot->id, "pal_pcnt_esp32");
        pal_spinlock_unlock(&s_pcnt_lock);
        return st;
    }

    if (cfg->pin_b >= 0) {
        st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_b, "pal_pcnt_esp32");
        if (st != WINK_OK) {
            pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_a, "pal_pcnt_esp32");
            pal_resource_release(PAL_RESOURCE_PCNT_UNIT, slot->id, "pal_pcnt_esp32");
            pal_spinlock_unlock(&s_pcnt_lock);
            return st;
        }
    }

    slot->high_limit = (cfg->high_limit != 0) ? cfg->high_limit : PCNT_DEFAULT_HIGH_LIMIT;
    slot->low_limit = (cfg->low_limit != 0) ? cfg->low_limit : PCNT_DEFAULT_LOW_LIMIT;

    pcnt_unit_config_t unit_config = {
        .low_limit = slot->low_limit,
        .high_limit = slot->high_limit,
    };
    esp_err_t err = pcnt_new_unit(&unit_config, &slot->unit_handle);
    if (err != ESP_OK) {
        if (cfg->pin_b >= 0) {
            pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_b, "pal_pcnt_esp32");
        }
        pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_a, "pal_pcnt_esp32");
        pal_resource_release(PAL_RESOURCE_PCNT_UNIT, slot->id, "pal_pcnt_esp32");
        pal_spinlock_unlock(&s_pcnt_lock);
        return WINK_ERR_HARDWARE;
    }

    /* Glitch filter setup (E-001) */
    if (cfg->filter_ns > 0) {
        pcnt_glitch_filter_config_t filter_config = {
            .max_glitch_ns = cfg->filter_ns,
        };
        pcnt_unit_set_glitch_filter(slot->unit_handle, &filter_config);
    }

    /* Configure Channel A */
    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = cfg->pin_a,
        .level_gpio_num = cfg->pin_b,
    };
    err = pcnt_new_channel(slot->unit_handle, &chan_a_config, &slot->chan_a);
    if (err != ESP_OK) {
        pcnt_del_unit(slot->unit_handle);
        if (cfg->pin_b >= 0) {
            pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_b, "pal_pcnt_esp32");
        }
        pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_a, "pal_pcnt_esp32");
        pal_resource_release(PAL_RESOURCE_PCNT_UNIT, slot->id, "pal_pcnt_esp32");
        pal_spinlock_unlock(&s_pcnt_lock);
        return WINK_ERR_HARDWARE;
    }

    if (cfg->mode == PAL_PCNT_MODE_4X && cfg->pin_b >= 0) {
        /* Configure Channel B for 4X quadrature decoding */
        pcnt_chan_config_t chan_b_config = {
            .edge_gpio_num = cfg->pin_b,
            .level_gpio_num = cfg->pin_a,
        };
        pcnt_new_channel(slot->unit_handle, &chan_b_config, &slot->chan_b);
        pcnt_channel_set_edge_action(slot->chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
        pcnt_channel_set_level_action(slot->chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
        pcnt_channel_set_edge_action(slot->chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE);
        pcnt_channel_set_level_action(slot->chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
    } else if (cfg->mode == PAL_PCNT_MODE_2X) {
        pcnt_channel_set_edge_action(slot->chan_a, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
        pcnt_channel_set_level_action(slot->chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
        slot->chan_b = NULL;
    } else {
        /* 1X mode */
        pcnt_channel_set_edge_action(slot->chan_a, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD);
        pcnt_channel_set_level_action(slot->chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
        slot->chan_b = NULL;
    }

    /* Register watch points for 64-bit software accumulator */
    pcnt_unit_add_watch_point(slot->unit_handle, slot->high_limit);
    pcnt_unit_add_watch_point(slot->unit_handle, slot->low_limit);

    pcnt_event_callbacks_t cbs = {
        .on_reach = esp32_pcnt_on_reach,
    };
    pcnt_unit_register_event_callbacks(slot->unit_handle, &cbs, slot);

    pcnt_unit_enable(slot->unit_handle);
    pcnt_unit_clear_count(slot->unit_handle);
    pcnt_unit_start(slot->unit_handle);

    slot->in_use = true;
    slot->cfg = *cfg;
    slot->accum_count = 0;

    *out_handle = slot;
    pal_spinlock_unlock(&s_pcnt_lock);
    return WINK_OK;
}

wink_status_t pal_pcnt_deinit(pal_pcnt_unit_handle_t handle) {
    if (handle == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_pcnt_lock);
    if (!handle->in_use || handle->id >= PAL_PCNT_UNIT_MAX) {
        pal_spinlock_unlock(&s_pcnt_lock);
        return WINK_ERR_INVALID_ARG;
    }

    pcnt_unit_stop(handle->unit_handle);
    pcnt_unit_disable(handle->unit_handle);

    if (handle->chan_b != NULL) {
        pcnt_del_channel(handle->chan_b);
        handle->chan_b = NULL;
    }
    if (handle->chan_a != NULL) {
        pcnt_del_channel(handle->chan_a);
        handle->chan_a = NULL;
    }
    pcnt_del_unit(handle->unit_handle);
    handle->unit_handle = NULL;

    if (handle->cfg.pin_b >= 0) {
        pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)handle->cfg.pin_b, "pal_pcnt_esp32");
    }
    pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)handle->cfg.pin_a, "pal_pcnt_esp32");
    pal_resource_release(PAL_RESOURCE_PCNT_UNIT, handle->id, "pal_pcnt_esp32");

    handle->in_use = false;
    handle->accum_count = 0;

    pal_spinlock_unlock(&s_pcnt_lock);
    return WINK_OK;
}

wink_status_t pal_pcnt_get_count(pal_pcnt_unit_handle_t handle, int64_t *count_out) {
    if (handle == NULL || count_out == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_pcnt_lock);
    if (!handle->in_use) {
        pal_spinlock_unlock(&s_pcnt_lock);
        return WINK_ERR_INVALID_STATE;
    }

    int raw_val = 0;
    esp_err_t err = pcnt_unit_get_count(handle->unit_handle, &raw_val);
    if (err != ESP_OK) {
        pal_spinlock_unlock(&s_pcnt_lock);
        return WINK_ERR_HARDWARE;
    }

    *count_out = handle->accum_count + (int64_t)raw_val;
    pal_spinlock_unlock(&s_pcnt_lock);
    return WINK_OK;
}

wink_status_t pal_pcnt_clear(pal_pcnt_unit_handle_t handle) {
    if (handle == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_pcnt_lock);
    if (!handle->in_use) {
        pal_spinlock_unlock(&s_pcnt_lock);
        return WINK_ERR_INVALID_STATE;
    }

    handle->accum_count = 0;
    pcnt_unit_clear_count(handle->unit_handle);
    pal_spinlock_unlock(&s_pcnt_lock);
    return WINK_OK;
}

wink_status_t pal_pcnt_set_glitch_filter(pal_pcnt_unit_handle_t handle, uint32_t filter_ns) {
    if (handle == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_pcnt_lock);
    if (!handle->in_use) {
        pal_spinlock_unlock(&s_pcnt_lock);
        return WINK_ERR_INVALID_STATE;
    }

    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = filter_ns,
    };
    esp_err_t err = pcnt_unit_set_glitch_filter(handle->unit_handle, &filter_config);
    pal_spinlock_unlock(&s_pcnt_lock);
    return (err == ESP_OK) ? WINK_OK : WINK_ERR_HARDWARE;
}

#else

/* Non-ESP32 fallback stubs for cross-compilation static analysis */
WINK_WARN_UNUSED_RESULT wink_status_t pal_pcnt_init(const pal_pcnt_config_t *cfg, pal_pcnt_unit_handle_t *out_handle) { (void)cfg; (void)out_handle; return WINK_ERR_NOT_SUPPORTED; }
wink_status_t pal_pcnt_deinit(pal_pcnt_unit_handle_t handle) { (void)handle; return WINK_ERR_NOT_SUPPORTED; }
wink_status_t pal_pcnt_get_count(pal_pcnt_unit_handle_t handle, int64_t *count_out) { (void)handle; (void)count_out; return WINK_ERR_NOT_SUPPORTED; }
wink_status_t pal_pcnt_clear(pal_pcnt_unit_handle_t handle) { (void)handle; return WINK_ERR_NOT_SUPPORTED; }
wink_status_t pal_pcnt_set_glitch_filter(pal_pcnt_unit_handle_t handle, uint32_t filter_ns) { (void)handle; (void)filter_ns; return WINK_ERR_NOT_SUPPORTED; }

#endif
