// SPDX-License-Identifier: Apache-2.0
#define LOG_TAG "dal_encoder"
#include "sensor/dal_encoder.h"
#include "pal_resource.h"
#include "pal_irq.h"
#include "pal_log.h"
#include <string.h>

/** Map DAL pull semantics -> PAL GPIO input mode (no pal_* types in public header). */
static wink_status_t encoder_map_pull(dal_encoder_pull_t pull, pal_gpio_mode_t *out_mode)
{
    if (out_mode == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    switch (pull) {
        case DAL_ENCODER_PULL_UP:   *out_mode = PAL_GPIO_INPUT_PULLUP;   return WINK_OK;
        case DAL_ENCODER_PULL_DOWN: *out_mode = PAL_GPIO_INPUT_PULLDOWN; return WINK_OK;
        case DAL_ENCODER_PULL_NONE: *out_mode = PAL_GPIO_INPUT;          return WINK_OK;
        default:                    return WINK_ERR_INVALID_ARG;
    }
}

/* Best-effort GPIO claim release for init-rollback/deinit: releases the pin and
 * logs on failure, but never aborts the remaining teardown (DAL-L-014/015).
 * Pins < 0 are unused and treated as success. */
static void release_gpio_claim_logged(wink_pin_t pin, const char *owner)
{
    if (pin < 0) {
        return;
    }
    wink_status_t rs = pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)pin, owner);
    if (wink_status_is_error(rs)) {
        LOG_W("release GPIO pin %d for '%s' failed: %d",
              (int)pin, owner ? owner : "(null)", (int)rs);
    }
}

static int32_t dal_encoder_x1_delta(bool val_b, bool invert)
{
    int32_t delta = val_b ? 1 : -1;
    if (invert) {
        delta = -delta;
    }
    return delta;
}

PAL_DEFINE_ISR(dal_encoder_gpio_isr, dal_encoder_t, dev)
{
    if (dev->config.pin_b >= 0) {
        bool val_b = false;
        /* x1: sample B on A rising edge */
        if (pal_gpio_read(dev->config.pin_b, &val_b) == WINK_OK) {
            dev->count += dal_encoder_x1_delta(val_b, dev->config.invert);
        }
    } else {
        /* Single-phase: increment only (invert N/A without B) */
        dev->count++;
    }
}

wink_status_t dal_encoder_init(dal_encoder_t *dev, const dal_encoder_config_t *cfg)
{
    if (dev == NULL || cfg == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (cfg->owner == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (cfg->pin_a < 0) {
        return WINK_ERR_INVALID_ARG;
    }
    if (cfg->variant != DAL_ENCODER_VARIANT_X1_RISING) {
        return WINK_ERR_UNSUPPORTED;
    }
    if (dev->initialized) {
        return WINK_ERR_ALREADY_INITIALIZED;
    }

    pal_gpio_mode_t pull_mode;
    wink_status_t map_st = encoder_map_pull(cfg->pull, &pull_mode);
    if (wink_status_is_error(map_st)) {
        return map_st;
    }

    /* 1. Claim resources (GPIO pin conflict detection) */
    wink_status_t rs = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_a, cfg->owner);
    if (wink_status_is_error(rs)) {
        LOG_W("init: claim pin_a %d for '%s' failed: %d",
              (int)cfg->pin_a, cfg->owner, (int)rs);
        return rs;
    }
    if (cfg->pin_b >= 0) {
        rs = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_b, cfg->owner);
        if (wink_status_is_error(rs)) {
            LOG_W("init: claim pin_b %d for '%s' failed: %d",
                  (int)cfg->pin_b, cfg->owner, (int)rs);
            release_gpio_claim_logged(cfg->pin_a, cfg->owner);
            return rs;
        }
    }

    /* 2. Configure GPIO inputs */
    wink_status_t status = pal_gpio_init(cfg->pin_a, pull_mode);
    if (wink_status_is_error(status)) {
        LOG_W("init: pal_gpio_init pin_a %d for '%s' failed: %d",
              (int)cfg->pin_a, cfg->owner, (int)status);
        goto err_release;
    }

    if (cfg->pin_b >= 0) {
        status = pal_gpio_init(cfg->pin_b, pull_mode);
        if (wink_status_is_error(status)) {
            LOG_W("init: pal_gpio_init pin_b %d for '%s' failed: %d",
                  (int)cfg->pin_b, cfg->owner, (int)status);
            pal_gpio_reset_pin(cfg->pin_a);
            goto err_release;
        }
    }

    /* 3. Stage config + counter */
    memcpy(&dev->config, cfg, sizeof(dal_encoder_config_t));
    dev->count = 0;
    dev->isr_registered = false;

    /* 4. Register rising-edge interrupt on pin A */
    status = pal_gpio_enable_interrupt(
        cfg->pin_a,
        PAL_GPIO_INTR_RISING_EDGE,
        dal_encoder_gpio_isr,
        dev);
    if (wink_status_is_error(status)) {
        LOG_W("init: enable interrupt pin_a %d for '%s' failed: %d",
              (int)cfg->pin_a, cfg->owner, (int)status);
        pal_gpio_reset_pin(cfg->pin_a);
        if (cfg->pin_b >= 0) {
            pal_gpio_reset_pin(cfg->pin_b);
        }
        memset(dev, 0, sizeof(dal_encoder_t));
        goto err_release;
    }

    dev->isr_registered = true;
    dev->initialized = true;

    LOG_I("init: '%s' ready (pin_a=%d pin_b=%d pull=%d%s)",
          cfg->owner, (int)cfg->pin_a, (int)cfg->pin_b, (int)cfg->pull,
          cfg->invert ? ", inverted" : "");
    return WINK_OK;

err_release:
    release_gpio_claim_logged(cfg->pin_b, cfg->owner);
    release_gpio_claim_logged(cfg->pin_a, cfg->owner);
    return status;
}

wink_status_t dal_encoder_get_count(const dal_encoder_t *dev, int32_t *out_count)
{
    if (dev == NULL || out_count == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }

    *out_count = dev->count;
    return WINK_OK;
}

wink_status_t dal_encoder_reset(dal_encoder_t *dev)
{
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }

    PAL_CRITICAL_SECTION({
        dev->count = 0;
    });

    return WINK_OK;
}

wink_status_t dal_encoder_deinit(dal_encoder_t *dev)
{
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_OK;
    }

    if (dev->isr_registered) {
        WINK_IGNORE_UNUSED(pal_gpio_disable_interrupt(dev->config.pin_a));
        WINK_IGNORE_UNUSED(pal_gpio_synchronize_interrupt(dev->config.pin_a));
    }

    pal_gpio_reset_pin(dev->config.pin_a);
    if (dev->config.pin_b >= 0) {
        pal_gpio_reset_pin(dev->config.pin_b);
    }

    wink_pin_t pin_a = dev->config.pin_a;
    wink_pin_t pin_b = dev->config.pin_b;
    const char *owner = dev->config.owner;

    release_gpio_claim_logged(pin_a, owner);
    release_gpio_claim_logged(pin_b, owner);

    memset(dev, 0, sizeof(dal_encoder_t));
    return WINK_OK;
}
