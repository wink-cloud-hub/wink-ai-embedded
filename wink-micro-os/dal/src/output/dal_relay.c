// SPDX-License-Identifier: Apache-2.0
#define LOG_TAG "dal_relay"
#include "output/dal_relay.h"
#include "pal_hal.h"
#include "pal_resource.h"
#include "pal_log.h"
#include "osal/pal_osal.h"
#include <string.h>

/* Release the GPIO resource claims best-effort; logs but never aborts teardown
 * (DAL-L-014). The main pin is uint16_t and is always claimed on success;
 * reset_pin is optional (int16_t, -1 = unbound). */
static void release_relay_claims(const dal_relay_config_t *cfg)
{
    if (cfg == NULL || cfg->owner == NULL) {
        return;
    }
    wink_status_t rs = pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin, cfg->owner);
    if (wink_status_is_error(rs)) {
        LOG_W("release main GPIO pin %d for '%s' failed: %d", (int)cfg->pin, cfg->owner, (int)rs);
    }
    if (cfg->reset_pin >= 0) {
        rs = pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->reset_pin, cfg->owner);
        if (wink_status_is_error(rs)) {
            LOG_W("release reset GPIO pin %d for '%s' failed: %d", (int)cfg->reset_pin, cfg->owner, (int)rs);
        }
    }
}

/* Drive both coil pins to the inactive level. */
static void relay_write_both_inactive(const dal_relay_config_t *cfg)
{
    bool inactive = cfg->active_low;
    WINK_IGNORE_UNUSED(pal_gpio_write((wink_pin_t)cfg->pin, inactive));
    if (cfg->reset_pin >= 0) {
        WINK_IGNORE_UNUSED(pal_gpio_write((wink_pin_t)cfg->reset_pin, inactive));
    }
}

/* Start a set/reset pulse on the appropriate coil pin. */
static wink_status_t relay_start_pulse(dal_relay_t *dev, bool on)
{
    const dal_relay_config_t *cfg = &dev->config;
    bool active = !cfg->active_low;
    wink_pin_t target = on ? (wink_pin_t)cfg->pin : (wink_pin_t)cfg->reset_pin;

    wink_status_t status = pal_gpio_write(target, active);
    if (wink_status_is_error(status)) {
        return status;
    }
    dev->pulse_start_ms = (uint32_t)pal_os_get_ms();
    dev->pulse_active = true;
    return WINK_OK;
}

wink_status_t dal_relay_init(dal_relay_t *dev, const dal_relay_config_t *cfg)
{
    if (dev == NULL || cfg == NULL || cfg->owner == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (dev->initialized) {
        return WINK_ERR_ALREADY_INITIALIZED;
    }

    if (cfg->variant != DAL_RELAY_VARIANT_DIRECT_GPIO &&
        cfg->variant != DAL_RELAY_VARIANT_LATCHING_DUAL_PIN) {
        LOG_E("init: '%s' invalid variant %d", cfg->owner, (int)cfg->variant);
        return WINK_ERR_INVALID_ARG;
    }

    if (cfg->variant == DAL_RELAY_VARIANT_LATCHING_DUAL_PIN && cfg->reset_pin < 0) {
        LOG_E("init: '%s' latching variant requires valid reset_pin", cfg->owner);
        return WINK_ERR_INVALID_ARG;
    }

    uint16_t pulse_ms = cfg->pulse_duration_ms;
    if (pulse_ms == 0) {
        pulse_ms = DAL_RELAY_DEFAULT_PULSE_MS;
    } else if (pulse_ms > DAL_RELAY_MAX_PULSE_MS) {
        LOG_E("init: '%s' pulse_duration_ms %u exceeds max %u",
              cfg->owner, (unsigned)pulse_ms, (unsigned)DAL_RELAY_MAX_PULSE_MS);
        return WINK_ERR_INVALID_ARG;
    }

    wink_status_t rs = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, cfg->pin, cfg->owner);
    if (wink_status_is_error(rs)) {
        LOG_W("init: claim GPIO pin %d for '%s' failed: %d", (int)cfg->pin, cfg->owner, (int)rs);
        return rs;
    }

    if (cfg->reset_pin >= 0) {
        rs = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->reset_pin, cfg->owner);
        if (wink_status_is_error(rs)) {
            LOG_W("init: claim reset GPIO pin %d for '%s' failed: %d", (int)cfg->reset_pin, cfg->owner, (int)rs);
            WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, cfg->pin, cfg->owner));
            return rs;
        }
    }

    wink_status_t status = pal_gpio_init((wink_pin_t)cfg->pin, PAL_GPIO_OUTPUT_PUSH_PULL);
    if (wink_status_is_error(status)) {
        LOG_W("init: pal_gpio_init pin %d for '%s' failed: %d", (int)cfg->pin, cfg->owner, (int)status);
        release_relay_claims(cfg);
        return status;
    }

    if (cfg->reset_pin >= 0) {
        status = pal_gpio_init((wink_pin_t)cfg->reset_pin, PAL_GPIO_OUTPUT_PUSH_PULL);
        if (wink_status_is_error(status)) {
            LOG_W("init: pal_gpio_init reset pin %d for '%s' failed: %d", (int)cfg->reset_pin, cfg->owner, (int)status);
            pal_gpio_reset_pin((wink_pin_t)cfg->pin);
            release_relay_claims(cfg);
            return status;
        }
    }

    memcpy(&dev->config, cfg, sizeof(dal_relay_config_t));
    dev->config.pulse_duration_ms = pulse_ms;

    dev->pulse_start_ms = 0;
    dev->pulse_active = false;
    dev->is_on = cfg->initial_state;

    bool is_latching = (dev->config.variant == DAL_RELAY_VARIANT_LATCHING_DUAL_PIN);

    if (!is_latching) {
        bool active = !dev->config.active_low;
        bool target_level = dev->is_on ? active : !active;
        status = pal_gpio_write((wink_pin_t)dev->config.pin, target_level);
        if (wink_status_is_error(status)) {
            pal_gpio_reset_pin((wink_pin_t)dev->config.pin);
            if (dev->config.reset_pin >= 0) {
                pal_gpio_reset_pin((wink_pin_t)dev->config.reset_pin);
            }
            release_relay_claims(cfg);
            return status;
        }
    } else {
        relay_write_both_inactive(&dev->config);
        status = relay_start_pulse(dev, dev->is_on);
        if (wink_status_is_error(status)) {
            pal_gpio_reset_pin((wink_pin_t)dev->config.pin);
            if (dev->config.reset_pin >= 0) {
                pal_gpio_reset_pin((wink_pin_t)dev->config.reset_pin);
            }
            release_relay_claims(cfg);
            return status;
        }
    }

    dev->initialized = true;
    dev->last_status = WINK_OK;

    LOG_I("init: '%s' ready (pin %d, reset_pin %d, variant %d)",
          cfg->owner, (int)cfg->pin, (int)cfg->reset_pin, (int)cfg->variant);
    return WINK_OK;
}

wink_status_t dal_relay_set(dal_relay_t *dev, bool on)
{
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        dev->last_status = WINK_ERR_NOT_INITIALIZED;
        return WINK_ERR_NOT_INITIALIZED;
    }

    wink_status_t status = WINK_OK;

    switch (dev->config.variant) {
    case DAL_RELAY_VARIANT_DIRECT_GPIO: {
        bool active = !dev->config.active_low;
        bool target_level = on ? active : !active;
        status = pal_gpio_write((wink_pin_t)dev->config.pin, target_level);
        break;
    }

    case DAL_RELAY_VARIANT_LATCHING_DUAL_PIN: {
        relay_write_both_inactive(&dev->config);
        status = relay_start_pulse(dev, on);
        break;
    }

    default:
        status = WINK_ERR_INVALID_ARG;
        break;
    }

    if (!wink_status_is_error(status)) {
        dev->is_on = on;
    }
    dev->last_status = status;
    return status;
}

wink_status_t dal_relay_on(dal_relay_t *dev)
{
    return dal_relay_set(dev, true);
}

wink_status_t dal_relay_off(dal_relay_t *dev)
{
    return dal_relay_set(dev, false);
}

wink_status_t dal_relay_toggle(dal_relay_t *dev)
{
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        dev->last_status = WINK_ERR_NOT_INITIALIZED;
        return WINK_ERR_NOT_INITIALIZED;
    }
    return dal_relay_set(dev, !dev->is_on);
}

wink_status_t dal_relay_is_on(const dal_relay_t *dev, bool *out_on)
{
    if (dev == NULL || out_on == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }
    *out_on = dev->is_on;
    return WINK_OK;
}

wink_status_t dal_relay_get_last_status(const dal_relay_t *dev)
{
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }
    return dev->last_status;
}

wink_status_t dal_relay_poll(dal_relay_t *dev)
{
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_OK;
    }

    if (dev->pulse_active) {
        uint32_t now = (uint32_t)pal_os_get_ms();
        uint32_t elapsed = now - dev->pulse_start_ms;
        if (elapsed >= dev->config.pulse_duration_ms) {
            relay_write_both_inactive(&dev->config);
            dev->pulse_active = false;
        }
    }
    return WINK_OK;
}

wink_status_t dal_relay_safe_off(dal_relay_t *dev)
{
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_OK;
    }
    WINK_IGNORE_UNUSED(dal_relay_off(dev));
    return WINK_OK;
}

wink_status_t dal_relay_deinit(dal_relay_t *dev)
{
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_OK;
    }

    WINK_IGNORE_UNUSED(dal_relay_off(dev));

    relay_write_both_inactive(&dev->config);
    pal_gpio_reset_pin((wink_pin_t)dev->config.pin);
    if (dev->config.reset_pin >= 0) {
        pal_gpio_reset_pin((wink_pin_t)dev->config.reset_pin);
    }

    release_relay_claims(&dev->config);
    memset(dev, 0, sizeof(dal_relay_t));
    return WINK_OK;
}
