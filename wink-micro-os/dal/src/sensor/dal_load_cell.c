/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Wink AI Project
 */

#include "sensor/dal_load_cell.h"

#if defined(WINK_USE_LOAD_CELL) && WINK_USE_LOAD_CELL

#include "pal_osal.h"
#include "pal_resource.h"
#include "pal_log.h"
#include <string.h>
#include <math.h>

#define LOG_TAG "dal_load_cell"

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

/* --- Internal Helper Functions --- */

static bool dal_load_cell_is_pin_valid(wink_pin_t pin) {
    return pin >= 0;
}

static void dal_load_cell_cleanup_resources(dal_load_cell_t *dev) {
    if (!dev) return;
    const dal_load_cell_config_t *cfg = &dev->config;

    if (cfg->variant == DAL_LOAD_CELL_VARIANT_HX711_TWO_WIRE) {
        if (dal_load_cell_is_pin_valid(cfg->sck_pin)) {
            pal_gpio_write(cfg->sck_pin, PAL_GPIO_LEVEL_LOW); /* Force SCK low to avoid power-down state */
            pal_gpio_reset_pin(cfg->sck_pin);
            pal_resource_release(PAL_RESOURCE_GPIO_PIN, cfg->sck_pin, cfg->owner);
        }
        if (dal_load_cell_is_pin_valid(cfg->dt_pin)) {
            pal_gpio_reset_pin(cfg->dt_pin);
            pal_resource_release(PAL_RESOURCE_GPIO_PIN, cfg->dt_pin, cfg->owner);
        }
    }
}

/* --- Public Driver API Implementations --- */

wink_status_t dal_load_cell_init(dal_load_cell_t *dev, const dal_load_cell_config_t *config) {
    if (dev == NULL || config == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (dev->initialized) {
        return WINK_ERR_ALREADY_INITIALIZED;
    }

    memset(dev, 0, sizeof(*dev));
    memcpy(&dev->config, config, sizeof(*config));

    if (dev->config.owner == NULL) {
        dev->config.owner = "load_cell";
    }

    /* Set default timeout if unspecified */
    if (dev->config.timeout_us == 0) {
        dev->config.timeout_us = 150000;
    }

    /* Set default calibration factor if unspecified */
    if (dev->config.calibration_factor == 0.0f) {
        dev->config.calibration_factor = 1.0f;
    }

    /* Phase 1 Variant Check */
    if (dev->config.variant != DAL_LOAD_CELL_VARIANT_HX711_TWO_WIRE) {
        LOG_E(LOG_TAG, "Variant %d not supported in Phase 1", dev->config.variant);
        return WINK_ERR_UNSUPPORTED;
    }

    /* Validate HX711 Pinouts */
    if (!dal_load_cell_is_pin_valid(dev->config.dt_pin) || 
        !dal_load_cell_is_pin_valid(dev->config.sck_pin) ||
        dev->config.dt_pin == dev->config.sck_pin) {
        LOG_E(LOG_TAG, "Invalid dt_pin (%d) or sck_pin (%d)", dev->config.dt_pin, dev->config.sck_pin);
        return WINK_ERR_INVALID_ARG;
    }

    /* Claim DT pin (Input) */
    if (pal_resource_claim(PAL_RESOURCE_GPIO_PIN, dev->config.dt_pin, dev->config.owner) != WINK_OK) {
        LOG_E(LOG_TAG, "Failed to claim DT pin %d", dev->config.dt_pin);
        return WINK_ERR_BUSY;
    }
    if (pal_gpio_init(dev->config.dt_pin, PAL_GPIO_MODE_INPUT) != WINK_OK) {
        LOG_E(LOG_TAG, "Failed to init DT pin %d", dev->config.dt_pin);
        pal_resource_release(PAL_RESOURCE_GPIO_PIN, dev->config.dt_pin, dev->config.owner);
        return WINK_ERR_HARDWARE;
    }

    /* Claim SCK pin (Output Push-Pull) */
    if (pal_resource_claim(PAL_RESOURCE_GPIO_PIN, dev->config.sck_pin, dev->config.owner) != WINK_OK) {
        LOG_E(LOG_TAG, "Failed to claim SCK pin %d", dev->config.sck_pin);
        pal_gpio_reset_pin(dev->config.dt_pin);
        pal_resource_release(PAL_RESOURCE_GPIO_PIN, dev->config.dt_pin, dev->config.owner);
        return WINK_ERR_BUSY;
    }
    if (pal_gpio_init(dev->config.sck_pin, PAL_GPIO_MODE_OUTPUT_PUSH_PULL) != WINK_OK) {
        LOG_E(LOG_TAG, "Failed to init SCK pin %d", dev->config.sck_pin);
        pal_gpio_reset_pin(dev->config.dt_pin);
        pal_resource_release(PAL_RESOURCE_GPIO_PIN, dev->config.dt_pin, dev->config.owner);
        pal_resource_release(PAL_RESOURCE_GPIO_PIN, dev->config.sck_pin, dev->config.owner);
        return WINK_ERR_HARDWARE;
    }

    /* Set SCK to LOW initially */
    pal_gpio_write(dev->config.sck_pin, PAL_GPIO_LEVEL_LOW);
    dev->pending_gain = dev->config.gain;
    dev->initialized = true;

    LOG_I(LOG_TAG, "Load cell initialized successfully (variant: HX711)");
    return WINK_OK;
}

wink_status_t dal_load_cell_deinit(dal_load_cell_t *dev) {
    if (dev == NULL || !dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }

    dal_load_cell_cleanup_resources(dev);
    dev->initialized = false;

    LOG_I(LOG_TAG, "Load cell deinitialized");
    return WINK_OK;
}

wink_status_t dal_load_cell_is_data_ready(const dal_load_cell_t *dev, bool *out_ready) {
    if (dev == NULL || out_ready == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }

    /* HX711: DRDY is active LOW on DT pin */
    pal_gpio_level_t level = PAL_GPIO_LEVEL_HIGH;
    wink_status_t status = pal_gpio_read(dev->config.dt_pin, &level);
    if (status != WINK_OK) {
        return status;
    }

    *out_ready = (level == PAL_GPIO_LEVEL_LOW);
    return WINK_OK;
}

wink_status_t dal_load_cell_request_read(dal_load_cell_t *dev) {
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }

    bool ready = false;
    wink_status_t status = dal_load_cell_is_data_ready(dev, &ready);
    if (status != WINK_OK) {
        return status;
    }
    if (!ready) {
        return WINK_ERR_BUSY;
    }

    /* Bit-bang 24 bits DOUT on SCK pulses */
    uint32_t raw24 = 0;
    for (int i = 0; i < 24; i++) {
        pal_gpio_write(dev->config.sck_pin, PAL_GPIO_LEVEL_HIGH);
        pal_os_busy_wait_us(1);

        pal_gpio_level_t bit_level = PAL_GPIO_LEVEL_LOW;
        pal_gpio_read(dev->config.dt_pin, &bit_level);

        raw24 = (raw24 << 1) | (bit_level == PAL_GPIO_LEVEL_HIGH ? 1u : 0u);

        pal_gpio_write(dev->config.sck_pin, PAL_GPIO_LEVEL_LOW);
        pal_os_busy_wait_us(1);
    }

    /* Extra pulses for Gain / Channel setting on NEXT conversion:
     * 1 pulse  -> Gain 128 Channel A
     * 2 pulses -> Gain 32  Channel B
     * 3 pulses -> Gain 64  Channel A
     */
    int extra_pulses = 1;
    if (dev->pending_gain == DAL_LOAD_CELL_GAIN_32) {
        extra_pulses = 2;
    } else if (dev->pending_gain == DAL_LOAD_CELL_GAIN_64) {
        extra_pulses = 3;
    }

    for (int i = 0; i < extra_pulses; i++) {
        pal_gpio_write(dev->config.sck_pin, PAL_GPIO_LEVEL_HIGH);
        pal_os_busy_wait_us(1);
        pal_gpio_write(dev->config.sck_pin, PAL_GPIO_LEVEL_LOW);
        pal_os_busy_wait_us(1);
    }

    /* 24-bit Sign Extension to int32_t */
    int32_t signed_raw = (int32_t)raw24;
    if (raw24 & 0x800000u) {
        signed_raw |= (int32_t)0xFF000000u;
    }

    dev->last_raw = signed_raw;
    dev->last_weight_g = (float)(signed_raw - dev->config.zero_offset) / dev->config.calibration_factor;

    return WINK_OK;
}

wink_status_t dal_load_cell_get_cached_raw(const dal_load_cell_t *dev, int32_t *out_raw) {
    if (dev == NULL || out_raw == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }

    *out_raw = dev->last_raw;
    return WINK_OK;
}

wink_status_t dal_load_cell_get_cached_weight_g(const dal_load_cell_t *dev, float *out_g) {
    if (dev == NULL || out_g == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }

    *out_g = dev->last_weight_g;
    return WINK_OK;
}

#ifndef WINK_STRICT_NONBLOCKING
wink_status_t dal_load_cell_read_weight_g(dal_load_cell_t *dev, float *out_g) {
    if (dev == NULL || out_g == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }

    uint32_t elapsed_us = 0;
    const uint32_t step_us = 1000;
    bool ready = false;

    while (elapsed_us < dev->config.timeout_us) {
        wink_status_t st = dal_load_cell_is_data_ready(dev, &ready);
        if (st != WINK_OK) {
            return st;
        }
        if (ready) {
            st = dal_load_cell_request_read(dev);
            if (st == WINK_OK) {
                *out_g = dev->last_weight_g;
                return WINK_OK;
            }
        }
        pal_os_delay_ms(1);
        elapsed_us += step_us;
    }

    return WINK_ERR_TIMEOUT;
}
#endif

wink_status_t dal_load_cell_tare(dal_load_cell_t *dev) {
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }

    /* Median-Average Filter over 8 samples */
    #define TARE_SAMPLE_COUNT 8
    int32_t samples[TARE_SAMPLE_COUNT];
    int valid_count = 0;
    uint32_t elapsed_us = 0;

    while (valid_count < TARE_SAMPLE_COUNT && elapsed_us < dev->config.timeout_us * 2) {
        bool ready = false;
        if (dal_load_cell_is_data_ready(dev, &ready) == WINK_OK && ready) {
            if (dal_load_cell_request_read(dev) == WINK_OK) {
                samples[valid_count++] = dev->last_raw;
            }
        }
        pal_os_delay_ms(10);
        elapsed_us += 10000;
    }

    if (valid_count < TARE_SAMPLE_COUNT) {
        return WINK_ERR_TIMEOUT;
    }

    /* Simple Bubble Sort */
    for (int i = 0; i < TARE_SAMPLE_COUNT - 1; i++) {
        for (int j = 0; j < TARE_SAMPLE_COUNT - i - 1; j++) {
            if (samples[j] > samples[j + 1]) {
                int32_t tmp = samples[j];
                samples[j] = samples[j + 1];
                samples[j + 1] = tmp;
            }
        }
    }

    /* Discard 2 min and 2 max samples, average middle 4 samples */
    int64_t sum = 0;
    for (int i = 2; i < 6; i++) {
        sum += samples[i];
    }
    int32_t median_avg_zero = (int32_t)(sum / 4);

    dev->config.zero_offset = median_avg_zero;
    dev->last_weight_g = 0.0f;

    LOG_I(LOG_TAG, "Tare complete. New zero_offset = %d", dev->config.zero_offset);
    return WINK_OK;
}

wink_status_t dal_load_cell_set_calibration_factor(dal_load_cell_t *dev, float factor) {
    if (dev == NULL || factor == 0.0f) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }

    dev->config.calibration_factor = factor;
    dev->last_weight_g = (float)(dev->last_raw - dev->config.zero_offset) / dev->config.calibration_factor;
    return WINK_OK;
}

wink_status_t dal_load_cell_apply_override(void *dev_ptr, const uint8_t *params, uint16_t len) {
    dal_load_cell_t *dev = (dal_load_cell_t *)dev_ptr;
    if (dev == NULL || params == NULL || len == 0) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }

    /* ADR-0008 Parameter override (e.g. calibration_factor override) */
    if (len >= sizeof(float)) {
        float factor;
        memcpy(&factor, params, sizeof(float));
        if (factor != 0.0f) {
            dev->config.calibration_factor = factor;
        }
    }
    return WINK_OK;
}

#endif /* WINK_USE_LOAD_CELL */
