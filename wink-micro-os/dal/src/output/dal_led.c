#define LOG_TAG "dal_led"
#include "output/dal_led.h"
#include "pal_hal.h"
#include "pal_resource.h"
#include "pal_log.h"
#include <string.h> /* memcpy */

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
    /* pin is uint16_t (always >= 0); upper-bound validity is enforced by
     * pal_gpio_init / pal_resource_claim below (DAL-L-005 defense in depth). */
    if (dev->initialized) { return WINK_ERR_ALREADY_INITIALIZED; }

    /* Claim GPIO pin — two LEDs on the same pin with different owners yields BUSY. */
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

    /* DAL-L-006 zero-energy init: explicitly drive the OFF level for the
     * configured polarity. We must NOT rely on the GPIO output latch reset
     * default (LOW) — on an active_low LED that default would light it. */
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
    /* DAL-L-022: idempotent on uninitialized handles — invoked from
     * safe_off_all() on watchdog/panic/rollback paths where "nothing to
     * shut off" is success, not an error. */
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_OK; }
    /* Best-effort off; ignore write status on the emergency path. */
    WINK_IGNORE_UNUSED(dal_led_off(dev));
    return WINK_OK;
}

wink_status_t dal_led_deinit(dal_led_t *dev) {
    /* ADR-0024 §4 deinit: off → pal_gpio_reset_pin → release claim → memset. */
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_OK; }  /* idempotent no-op */

    /* 1. Best-effort turn LED off before releasing the pin (safe-off semantic). */
    WINK_IGNORE_UNUSED(dal_led_off(dev));

    /* Capture before memset so we can reset/release. */
    wink_pin_t pin = dev->config.pin;
    const char *owner = dev->config.owner;

    /* 2. Reset GPIO: disables leftover interrupt routing, reverts to Hi-Z INPUT,
     *    and clears the esp_gpio_reserve bitmap (ADR-0024 §4 #2). */
    pal_gpio_reset_pin(pin);

    /* 3. Release software resource claim so a subsequent init doesn't get BUSY.
     *    Failure is logged but never aborts teardown (DAL-L-014/015). */
    release_gpio_claim(pin, owner);

    /* 4. Clear the instance data completely to guarantee no residual state. */
    memset(dev, 0, sizeof(dal_led_t));

    return WINK_OK;
}
