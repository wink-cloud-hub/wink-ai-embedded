#define LOG_TAG "dal_relay"
#include "output/dal_relay.h"
#include "pal_hal.h"
#include "pal_resource.h"
#include "pal_log.h"
#include "osal/pal_osal.h"
#include <string.h> /* memcpy, memset */

static void release_relay_claims(const dal_relay_config_t *cfg)
{
    if (cfg == NULL || cfg->owner == NULL) {
        return;
    }
    if (cfg->pin != (uint16_t)-1) {
        wink_status_t rs = pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin, cfg->owner);
        if (wink_status_is_error(rs)) {
            LOG_W("release main GPIO pin %d for '%s' failed: %d", (int)cfg->pin, cfg->owner, (int)rs);
        }
    }
    if (cfg->reset_pin >= 0) {
        wink_status_t rs = pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->reset_pin, cfg->owner);
        if (wink_status_is_error(rs)) {
            LOG_W("release reset GPIO pin %d for '%s' failed: %d", (int)cfg->reset_pin, cfg->owner, (int)rs);
        }
    }
}

static bool get_inactive_level(bool active_low)
{
    return active_low ? true : false;
}

static bool get_active_level(bool active_low)
{
    return active_low ? false : true;
}

wink_status_t dal_relay_init(dal_relay_t *dev, const dal_relay_config_t *cfg)
{
    if (dev == NULL || cfg == NULL || cfg->owner == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (dev->initialized) {
        return WINK_ERR_ALREADY_INITIALIZED;
    }

    /* Validate latching variant requirements */
    if (cfg->variant == DAL_RELAY_VARIANT_LATCHING_DUAL_PIN ||
        cfg->variant == DAL_RELAY_VARIANT_LATCHING_SINGLE_PIN) {
        if (cfg->reset_pin < 0) {
            LOG_E("init: '%s' latching variant requires valid reset_pin", cfg->owner);
            return WINK_ERR_INVALID_ARG;
        }
    }

    /* Claim main GPIO pin */
    wink_status_t rs = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, cfg->pin, cfg->owner);
    if (wink_status_is_error(rs)) {
        LOG_W("init: claim GPIO pin %d for '%s' failed: %d", (int)cfg->pin, cfg->owner, (int)rs);
        return rs;
    }

    /* Claim reset GPIO pin if applicable */
    if (cfg->reset_pin >= 0) {
        rs = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->reset_pin, cfg->owner);
        if (wink_status_is_error(rs)) {
            LOG_W("init: claim reset GPIO pin %d for '%s' failed: %d", (int)cfg->reset_pin, cfg->owner, (int)rs);
            WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, cfg->pin, cfg->owner));
            return rs;
        }
    }

    /* Configure main GPIO pin */
    wink_status_t status = pal_gpio_init(cfg->pin, PAL_GPIO_OUTPUT_PUSH_PULL);
    if (wink_status_is_error(status)) {
        LOG_W("init: pal_gpio_init pin %d for '%s' failed: %d", (int)cfg->pin, cfg->owner, (int)status);
        release_relay_claims(cfg);
        return status;
    }

    /* Configure reset GPIO pin if applicable */
    if (cfg->reset_pin >= 0) {
        status = pal_gpio_init((wink_pin_t)cfg->reset_pin, PAL_GPIO_OUTPUT_PUSH_PULL);
        if (wink_status_is_error(status)) {
            LOG_W("init: pal_gpio_init reset pin %d for '%s' failed: %d", (int)cfg->reset_pin, cfg->owner, (int)status);
            pal_gpio_reset_pin((wink_pin_t)cfg->pin);
            release_relay_claims(cfg);
            return status;
        }
    }

    /* Store config copy */
    memcpy(&dev->config, cfg, sizeof(dal_relay_config_t));
    if (dev->config.pulse_duration_ms == 0) {
        dev->config.pulse_duration_ms = 50; /* Guard C: zero-as-default fallback */
    }

    dev->pulse_start_ms = 0;
    dev->pulse_active = false;
    dev->is_on = cfg->initial_state;

    /* Set initial hardware pin outputs */
    bool inactive = get_inactive_level(dev->config.active_low);
    bool active = get_active_level(dev->config.active_low);

    if (dev->config.variant == DAL_RELAY_VARIANT_DIRECT_GPIO ||
        dev->config.variant == DAL_RELAY_VARIANT_SSR) {
        bool target_level = dev->is_on ? active : inactive;
        status = pal_gpio_write((wink_pin_t)dev->config.pin, target_level);
        if (wink_status_is_error(status)) {
            if (dev->config.reset_pin >= 0) {
                pal_gpio_reset_pin((wink_pin_t)dev->config.reset_pin);
            }
            pal_gpio_reset_pin((wink_pin_t)dev->config.pin);
            release_relay_claims(cfg);
            return status;
        }
    } else {
        /* Latching variants: start with pulse pins inactive (zero static power) */
        WINK_IGNORE_UNUSED(pal_gpio_write((wink_pin_t)dev->config.pin, inactive));
        if (dev->config.reset_pin >= 0) {
            WINK_IGNORE_UNUSED(pal_gpio_write((wink_pin_t)dev->config.reset_pin, inactive));
        }
    }

    dev->initialized = true;
    dev->last_status = WINK_OK;

    LOG_I("init: '%s' ready (pin %d, reset_pin %d, variant %d)",
          cfg->owner, (int)cfg->pin, (int)cfg->reset_pin, (int)cfg->variant);
    return WINK_OK;
}

wink_status_t dal_relay_set_state(dal_relay_t *dev, bool on)
{
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        dev->last_status = WINK_ERR_NOT_INITIALIZED;
        return WINK_ERR_NOT_INITIALIZED;
    }

    bool inactive = get_inactive_level(dev->config.active_low);
    bool active = get_active_level(dev->config.active_low);
    wink_status_t status = WINK_OK;

    switch (dev->config.variant) {
    case DAL_RELAY_VARIANT_DIRECT_GPIO:
    case DAL_RELAY_VARIANT_SSR: {
        bool target_level = on ? active : inactive;
        status = pal_gpio_write((wink_pin_t)dev->config.pin, target_level);
        break;
    }

    case DAL_RELAY_VARIANT_LATCHING_DUAL_PIN:
    case DAL_RELAY_VARIANT_LATCHING_SINGLE_PIN: {
        wink_pin_t set_pin = (wink_pin_t)dev->config.pin;
        wink_pin_t reset_pin = (wink_pin_t)dev->config.reset_pin;

        if (on) {
            /* Pulse Set pin */
            WINK_IGNORE_UNUSED(pal_gpio_write(reset_pin, inactive));
            status = pal_gpio_write(set_pin, active);
        } else {
            /* Pulse Reset pin */
            WINK_IGNORE_UNUSED(pal_gpio_write(set_pin, inactive));
            status = pal_gpio_write(reset_pin, active);
        }

        if (!wink_status_is_error(status)) {
            dev->pulse_start_ms = (uint32_t)pal_os_get_ms();
            dev->pulse_active = true;
        }
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

wink_status_t dal_relay_turn_on(dal_relay_t *dev)
{
    return dal_relay_set_state(dev, true);
}

wink_status_t dal_relay_turn_off(dal_relay_t *dev)
{
    return dal_relay_set_state(dev, false);
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
    return dal_relay_set_state(dev, !dev->is_on);
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

wink_status_t dal_relay_poll(dal_relay_t *dev)
{
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_OK; /* Idempotent no-op when uninitialized */
    }

    if (dev->pulse_active) {
        uint32_t now = (uint32_t)pal_os_get_ms();
        uint32_t elapsed = now - dev->pulse_start_ms;
        if (elapsed >= dev->config.pulse_duration_ms) {
            bool inactive = get_inactive_level(dev->config.active_low);
            WINK_IGNORE_UNUSED(pal_gpio_write((wink_pin_t)dev->config.pin, inactive));
            if (dev->config.reset_pin >= 0) {
                WINK_IGNORE_UNUSED(pal_gpio_write((wink_pin_t)dev->config.reset_pin, inactive));
            }
            dev->pulse_active = false;
        }
    }
    return WINK_OK;
}

wink_status_t dal_relay_deinit(dal_relay_t *dev)
{
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_OK; /* DAL-L-010 idempotent no-op */
    }

    /* Guard A: Turn off relay coil / clear pulses before releasing resources */
    WINK_IGNORE_UNUSED(dal_relay_turn_off(dev));

    bool inactive = get_inactive_level(dev->config.active_low);
    WINK_IGNORE_UNUSED(pal_gpio_write((wink_pin_t)dev->config.pin, inactive));
    pal_gpio_reset_pin((wink_pin_t)dev->config.pin);

    if (dev->config.reset_pin >= 0) {
        WINK_IGNORE_UNUSED(pal_gpio_write((wink_pin_t)dev->config.reset_pin, inactive));
        pal_gpio_reset_pin((wink_pin_t)dev->config.reset_pin);
    }

    release_relay_claims(&dev->config);
    memset(dev, 0, sizeof(dal_relay_t));
    return WINK_OK;
}
