// SPDX-License-Identifier: Apache-2.0
#define LOG_TAG "dal_led"
#include "output/dal_led.h"
#include "pal_hal.h"
#include "pal_resource.h"
#include "pal_log.h"
#include <string.h>

/* Best-effort GPIO claim release for init-rollback/deinit: releases the pin
 * and logs on failure, but never aborts the remaining teardown (DAL-L-014).
 * Pins < 0 are unused and treated as success. */
static void release_gpio_claim(wink_pin_t pin, const char *owner)
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

wink_status_t dal_led_init(dal_led_t *dev, const dal_led_config_t *cfg) {
    if (dev == NULL || cfg == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->owner == NULL) { return WINK_ERR_INVALID_ARG; }
    if (dev->initialized) { return WINK_ERR_ALREADY_INITIALIZED; }

    /* Claim GPIO pin - two LEDs on the same pin with different owners yields BUSY. */
    wink_status_t rs = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, cfg->pin, cfg->owner);
    if (wink_status_is_error(rs)) {
        LOG_W("init: claim GPIO pin %d for '%s' failed: %d",
              (int)cfg->pin, cfg->owner, (int)rs);
        return rs;
    }

    wink_status_t status = pal_gpio_init(cfg->pin, PAL_GPIO_OUTPUT_PUSH_PULL);
    if (wink_status_is_error(status)) {
        LOG_W("init: pal_gpio_init pin %d for '%s' failed: %d",
              (int)cfg->pin, cfg->owner, (int)status);
        release_gpio_claim(cfg->pin, cfg->owner);
        return status;
    }

    /* Deep-copy config before driving the pin so rollback/error paths read it. */
    memcpy(&dev->config, cfg, sizeof(dal_led_config_t));
    dev->is_on = false;

    /* DAL-L-006 zero-energy init: explicitly drive the OFF level. */
    bool off_level = dev->config.active_high ? false : true;
    status = pal_gpio_write(dev->config.pin, off_level);
    if (wink_status_is_error(status)) {
        LOG_W("init: write off-level pin %d for '%s' failed: %d; rolling back",
              (int)cfg->pin, cfg->owner, (int)status);
        pal_gpio_reset_pin(cfg->pin);
        release_gpio_claim(cfg->pin, cfg->owner);
        return status;
    }

    dev->initialized = true;

    LOG_I("init: '%s' ready (pin %d, active_%s)",
          cfg->owner, (int)cfg->pin, cfg->active_high ? "high" : "low");
    return WINK_OK;
}

wink_status_t dal_led_on(dal_led_t *dev) {
    return dal_led_set(dev, true);
}

wink_status_t dal_led_off(dal_led_t *dev) {
    return dal_led_set(dev, false);
}

wink_status_t dal_led_set(dal_led_t *dev, bool on) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    bool level = on ? dev->config.active_high : (!dev->config.active_high);
    wink_status_t s = pal_gpio_write(dev->config.pin, level);
    if (wink_status_is_error(s)) { return s; }
    dev->is_on = on;
    return WINK_OK;
}

wink_status_t dal_led_toggle(dal_led_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    return dal_led_set(dev, !dev->is_on);
}

wink_status_t dal_led_safe_off(dal_led_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_OK; }
    WINK_IGNORE_UNUSED(dal_led_off(dev));
    return WINK_OK;
}

wink_status_t dal_led_deinit(dal_led_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_OK; }

    WINK_IGNORE_UNUSED(dal_led_off(dev));

    wink_pin_t pin = dev->config.pin;
    const char *owner = dev->config.owner;

    pal_gpio_reset_pin(pin);
    release_gpio_claim(pin, owner);
    memset(dev, 0, sizeof(dal_led_t));

    return WINK_OK;
}
