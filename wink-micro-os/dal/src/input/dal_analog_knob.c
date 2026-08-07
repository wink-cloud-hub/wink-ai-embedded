// SPDX-License-Identifier: Apache-2.0
#include "input/dal_analog_knob.h"
#include "hal/pal_adc.h"
#include "hal/pal_hal.h"
#include "pal_resource.h"
#include <string.h>

/* Fixed-point integer square root linearization for Audio Logarithmic A-taper */
static inline uint16_t dal_analog_knob_log_to_linear(uint16_t promille) {
    uint32_t val32 = (uint32_t)promille * 1000u;
    uint32_t root = 0;
    uint32_t bit = 1u << 30;
    while (bit > val32) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (val32 >= root + bit) {
            val32 -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    return (root > 1000u) ? 1000u : (uint16_t)root;
}

/* Quadratic mapping for Anti-Logarithmic C-taper */
static inline uint16_t dal_analog_knob_antilog_to_linear(uint16_t promille) {
    uint32_t sq = ((uint32_t)promille * (uint32_t)promille) / 1000u;
    return (sq > 1000u) ? 1000u : (uint16_t)sq;
}

wink_status_t dal_analog_knob_init(dal_analog_knob_t *dev, const dal_analog_knob_config_t *cfg) {
    if (dev == NULL || cfg == NULL || cfg->owner == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (dev->initialized) {
        return WINK_ERR_ALREADY_INITIALIZED;
    }

    pal_adc_config_t pal_cfg = {
        .pin = (wink_pin_t)cfg->pin,
        .full_scale_mv = 0, /* 0 = use target platform default full scale */
        .resolution_bits = 0,
    };

    pal_adc_channel_t ch = 0;
    wink_status_t st = pal_adc_acquire((wink_pin_t)cfg->pin, &pal_cfg, &ch);
    if (wink_status_is_error(st)) {
        return st;
    }

    /* Dual resource claim: claim ADC channel and GPIO pin for owner */
    st = pal_resource_claim(PAL_RESOURCE_ADC_CHANNEL, ch, cfg->owner);
    if (wink_status_is_error(st)) {
        WINK_IGNORE_UNUSED(pal_adc_release(ch));
        return st;
    }

    st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, cfg->pin, cfg->owner);
    if (wink_status_is_error(st)) {
        WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_ADC_CHANNEL, ch, cfg->owner));
        WINK_IGNORE_UNUSED(pal_adc_release(ch));
        return st;
    }

    if (cfg->enable_pin >= 0) {
        st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint16_t)cfg->enable_pin, cfg->owner);
        if (wink_status_is_error(st)) {
            WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, cfg->pin, cfg->owner));
            WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_ADC_CHANNEL, ch, cfg->owner));
            WINK_IGNORE_UNUSED(pal_adc_release(ch));
            return st;
        }
    }

    /* Guard A: Turn on power enable pin if specified */
    if (cfg->enable_pin >= 0) {
        WINK_IGNORE_UNUSED(pal_gpio_init((wink_pin_t)cfg->enable_pin, PAL_GPIO_OUTPUT_PUSH_PULL));
        WINK_IGNORE_UNUSED(pal_gpio_write((wink_pin_t)cfg->enable_pin, true));
    }

    memcpy(&dev->config, cfg, sizeof(dal_analog_knob_config_t));
    dev->last_knob_promille = 0;
    dev->last_raw = 0;
    dev->last_status = WINK_OK;
    dev->initialized = true;

    return WINK_OK;
}

wink_status_t dal_analog_knob_deinit(dal_analog_knob_t *dev) {
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_OK;
    }

    pal_adc_channel_t ch = 0;
    if (wink_status_is_success(pal_adc_pin_channel((wink_pin_t)dev->config.pin, &ch))) {
        WINK_IGNORE_UNUSED(pal_adc_release(ch));
        WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_ADC_CHANNEL, ch, dev->config.owner));
    }
    WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, dev->config.pin, dev->config.owner));

    if (dev->config.enable_pin >= 0) {
        WINK_IGNORE_UNUSED(pal_gpio_write((wink_pin_t)dev->config.enable_pin, false));
        WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint16_t)dev->config.enable_pin, dev->config.owner));
    }

    memset(dev, 0, sizeof(dal_analog_knob_t));
    return WINK_OK;
}

wink_status_t dal_analog_knob_read_mv(dal_analog_knob_t *dev, uint16_t *out_mv) {
    if (dev == NULL || out_mv == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }

    pal_adc_channel_t ch = 0;
    wink_status_t st = pal_adc_pin_channel((wink_pin_t)dev->config.pin, &ch);
    if (wink_status_is_error(st)) {
        dev->last_status = st;
        return st;
    }

    st = pal_adc_read_mv(ch, out_mv);
    dev->last_status = st;
    return st;
}

wink_status_t dal_analog_knob_read_promille(dal_analog_knob_t *dev, uint16_t *out_knob_promille) {
    if (dev == NULL || out_knob_promille == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }

    pal_adc_channel_t ch = 0;
    wink_status_t st = pal_adc_pin_channel((wink_pin_t)dev->config.pin, &ch);
    if (wink_status_is_error(st)) {
        dev->last_status = st;
        return st;
    }

    uint16_t raw_mv = 0;
    st = pal_adc_read_mv(ch, &raw_mv);
    if (wink_status_is_error(st)) {
        dev->last_status = st;
        return st;
    }

    uint16_t raw_val = 0;
    if (wink_status_is_success(pal_adc_read_raw(ch, &raw_val))) {
        dev->last_raw = raw_val;
    }

    /* Guard C: Zero-as-default range resolution */
    uint16_t min_mv = dev->config.min_mv;
    uint16_t max_mv = dev->config.max_mv;
    if (min_mv == 0 && max_mv == 0) {
        uint16_t fs_mv = 0;
        if (wink_status_is_success(pal_adc_full_scale_mv(ch, &fs_mv)) && fs_mv > 0) {
            max_mv = fs_mv;
        } else {
            max_mv = 3300;
        }
    }

    /* Divide-by-zero & boundary protection */
    uint16_t effective_mv = raw_mv;
    if (min_mv >= max_mv) {
        effective_mv = min_mv;
    } else if (effective_mv < min_mv) {
        effective_mv = min_mv;
    } else if (effective_mv > max_mv) {
        effective_mv = max_mv;
    }

    uint32_t span = (max_mv > min_mv) ? (uint32_t)(max_mv - min_mv) : 1u;
    uint32_t offset = (uint32_t)(effective_mv - min_mv);

    /* DAL-U-029: Explicit uint32_t promotion before division */
    uint32_t promille_32 = (offset * 1000u) / span;
    if (promille_32 > 1000u) {
        promille_32 = 1000u;
    }

    uint16_t promille = (uint16_t)promille_32;

    /* Topology variant processing: curve linearization & center detent clamping */
    switch (dev->config.variant) {
        case DAL_ANALOG_KNOB_VARIANT_CENTER_DETENT:
            /* Center detent deadzone: 480~520 promille (48%~52%) clamps to exact 500 (50%) */
            if (promille >= 480u && promille <= 520u) {
                promille = 500u;
            }
            break;

        case DAL_ANALOG_KNOB_VARIANT_LOGARITHMIC:
            /* Audio A-taper logarithmic to linear linearization */
            promille = dal_analog_knob_log_to_linear(promille);
            break;

        case DAL_ANALOG_KNOB_VARIANT_ANTI_LOGARITHMIC:
            /* Anti-log C-taper to linear linearization */
            promille = dal_analog_knob_antilog_to_linear(promille);
            break;

        case DAL_ANALOG_KNOB_VARIANT_STANDARD:
        default:
            break;
    }

    if (dev->config.inverted) {
        promille = (uint16_t)(1000u - promille);
    }

    /* Endpoint clamping: 1% (10 promille) deadzone for reliable 0 and 1000 bounds */
    if (promille <= 10u) {
        promille = 0u;
    } else if (promille >= 990u) {
        promille = 1000u;
    }

    dev->last_knob_promille = promille;
    dev->last_status = WINK_OK;
    *out_knob_promille = promille;

    return WINK_OK;
}

wink_status_t dal_analog_knob_poll(dal_analog_knob_t *dev, bool *out_changed, uint16_t *out_knob_promille) {
    if (dev == NULL || out_changed == NULL || out_knob_promille == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }

    /* Take snapshot of previous stable promille baseline BEFORE reading current sample */
    uint16_t prev_promille = dev->last_knob_promille;

    uint16_t current_promille = 0;
    wink_status_t st = dal_analog_knob_read_promille(dev, &current_promille);
    if (wink_status_is_error(st)) {
        dev->last_status = st;
        *out_changed = false;
        return st;
    }

    uint16_t diff = (current_promille >= prev_promille) ?
                    (current_promille - prev_promille) :
                    (prev_promille - current_promille);

    bool changed = (diff >= dev->config.hysteresis_promille);
    if (!changed) {
        /* Sub-threshold fluctuation: restore previous stable baseline to prevent integration drift */
        dev->last_knob_promille = prev_promille;
        *out_knob_promille = prev_promille;
    } else {
        *out_knob_promille = current_promille;
    }

    *out_changed = changed;
    dev->last_status = WINK_OK;

    return WINK_OK;
}

wink_status_t dal_analog_knob_get_status(const dal_analog_knob_t *dev, wink_status_t *out_status) {
    if (dev == NULL || out_status == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }

    *out_status = dev->last_status;
    return WINK_OK;
}
