// SPDX-License-Identifier: Apache-2.0
#define LOG_TAG "dal_dc_motor"
#include "actuator/dal_dc_motor.h"
#include "pal_resource.h"
#include "pal_log.h"
#include <string.h>

static wink_status_t write_enable_if_present(const dal_dc_motor_t *dev,
                                             bool level)
{
    if (dev->config.enable_pin < 0) {
        return WINK_OK;
    }
    return pal_gpio_write(dev->config.enable_pin, level);
}

static wink_status_t apply_dir_and_duty(dal_dc_motor_t *dev,
                                        bool pin_a_level,
                                        bool pin_b_level,
                                        uint16_t abs_promille)
{
    wink_status_t s = pal_gpio_write(dev->config.dir_pin_a, pin_a_level);
    if (wink_status_is_error(s)) {
        return s;
    }

    if (dev->config.dir_pin_b >= 0) {
        s = pal_gpio_write(dev->config.dir_pin_b, pin_b_level);
        if (wink_status_is_error(s)) {
            return s;
        }
    }

    if (abs_promille > 1000) {
        abs_promille = 1000;
    }
    float duty_percent = ((float)abs_promille) / 10.0f;
    return pal_pwm_set_duty(dev->config.pwm_channel, duty_percent);
}

/* Best-effort GPIO claim release for init-rollback/deinit: releases the pin
 * and logs on failure, but never aborts the remaining teardown (DAL-L-008
 * rollback, DAL-L-014/L-015). Pins < 0 are unused and treated as success. */
static void release_gpio_claim(wink_pin_t pin, const char *owner)
{
    if (pin < 0) {
        return;
    }
    wink_status_t rs = pal_resource_release(
        PAL_RESOURCE_GPIO_PIN, (uint32_t)pin, owner);
    if (wink_status_is_error(rs)) {
        LOG_W("release GPIO pin %d for '%s' failed: %d",
              (int)pin, owner ? owner : "(null)", (int)rs);
    }
}

/* Best-effort resource release for deinit: release and log on failure, but
 * never abort the remaining teardown (DAL-L-014/L-015). */
static void release_resource_logged(pal_resource_type_t type, uint32_t id,
                                    const char *owner)
{
    wink_status_t rs = pal_resource_release(type, id, owner);
    if (wink_status_is_error(rs)) {
        LOG_W("deinit: release resource type=%d id=%u for '%s' failed: %d",
              (int)type, (unsigned)id, owner ? owner : "(null)", (int)rs);
    }
}

wink_status_t dal_dc_motor_init(dal_dc_motor_t *dev,
                                const dal_dc_motor_config_t *cfg)
{
    if (dev == NULL || cfg == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (cfg->owner == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (cfg->pwm_channel >= PAL_PWM_CHANNELS) {
        return WINK_ERR_INVALID_ARG;
    }
    if (cfg->dir_pin_a < 0) {
        return WINK_ERR_INVALID_ARG;
    }
    if (cfg->variant != DAL_DC_MOTOR_VARIANT_IN_IN) {
        return WINK_ERR_UNSUPPORTED;
    }
    if (dev->initialized) {
        return WINK_ERR_ALREADY_INITIALIZED;
    }

    wink_pin_t enable_pin = cfg->enable_pin;
    if (enable_pin == 0) {
        /* Zero-init / omitted optional field; -1 = unused (not GPIO 0). */
        enable_pin = -1;
    }

    /* 1. Claim resources */
    wink_status_t rs = pal_resource_claim(
        PAL_RESOURCE_PWM_CHANNEL, (uint32_t)cfg->pwm_channel, cfg->owner);
    if (wink_status_is_error(rs)) {
        LOG_W("init: claim PWM ch%u for '%s' failed: %d",
              (unsigned)cfg->pwm_channel, cfg->owner, (int)rs);
        return rs;
    }
    rs = pal_resource_claim(
        PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->dir_pin_a, cfg->owner);
    if (wink_status_is_error(rs)) {
        LOG_W("init: claim dir_pin_a %d for '%s' failed: %d",
              (int)cfg->dir_pin_a, cfg->owner, (int)rs);
        release_resource_logged(
            PAL_RESOURCE_PWM_CHANNEL, (uint32_t)cfg->pwm_channel, cfg->owner);
        return rs;
    }
    if (cfg->dir_pin_b >= 0) {
        rs = pal_resource_claim(
            PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->dir_pin_b, cfg->owner);
        if (wink_status_is_error(rs)) {
            LOG_W("init: claim dir_pin_b %d for '%s' failed: %d",
                  (int)cfg->dir_pin_b, cfg->owner, (int)rs);
            release_gpio_claim(cfg->dir_pin_a, cfg->owner);
            release_resource_logged(
                PAL_RESOURCE_PWM_CHANNEL,
                (uint32_t)cfg->pwm_channel,
                cfg->owner);
            return rs;
        }
    }
    if (enable_pin >= 0) {
        rs = pal_resource_claim(
            PAL_RESOURCE_GPIO_PIN, (uint32_t)enable_pin, cfg->owner);
        if (wink_status_is_error(rs)) {
            LOG_W("init: claim enable_pin %d for '%s' failed: %d",
                  (int)enable_pin, cfg->owner, (int)rs);
            release_gpio_claim(cfg->dir_pin_b, cfg->owner);
            release_gpio_claim(cfg->dir_pin_a, cfg->owner);
            release_resource_logged(
                PAL_RESOURCE_PWM_CHANNEL,
                (uint32_t)cfg->pwm_channel,
                cfg->owner);
            return rs;
        }
    }

    /* 2. Init PWM + GPIO */
    wink_status_t status = pal_pwm_init(
        cfg->pwm_channel, cfg->pwm_freq_hz > 0 ? cfg->pwm_freq_hz : 20000);
    if (wink_status_is_error(status)) {
        goto err_release;
    }

    status = pal_gpio_init(cfg->dir_pin_a, PAL_GPIO_OUTPUT_PUSH_PULL);
    if (wink_status_is_error(status)) {
        goto err_pwm_deinit;
    }

    if (cfg->dir_pin_b >= 0) {
        status = pal_gpio_init(cfg->dir_pin_b, PAL_GPIO_OUTPUT_PUSH_PULL);
        if (wink_status_is_error(status)) {
            pal_gpio_reset_pin(cfg->dir_pin_a);
            goto err_pwm_deinit;
        }
    }

    if (enable_pin >= 0) {
        status = pal_gpio_init(enable_pin, PAL_GPIO_OUTPUT_PUSH_PULL);
        if (wink_status_is_error(status)) {
            if (cfg->dir_pin_b >= 0) {
                pal_gpio_reset_pin(cfg->dir_pin_b);
            }
            pal_gpio_reset_pin(cfg->dir_pin_a);
            goto err_pwm_deinit;
        }
        status = pal_gpio_write(enable_pin, false);
        if (wink_status_is_error(status)) {
            pal_gpio_reset_pin(enable_pin);
            if (cfg->dir_pin_b >= 0) {
                pal_gpio_reset_pin(cfg->dir_pin_b);
            }
            pal_gpio_reset_pin(cfg->dir_pin_a);
            goto err_pwm_deinit;
        }
    }

    /* 3. Save config; start in coast */
    memcpy(&dev->config, cfg, sizeof(dal_dc_motor_config_t));
    dev->config.enable_pin = enable_pin;
    dev->current_speed_promille = 0;
    dev->initialized = true;

    WINK_IGNORE_UNUSED(dal_dc_motor_coast(dev));

    LOG_I("init: '%s' ready (ch%u, dir_a=%d dir_b=%d en=%d, %lu Hz)",
          cfg->owner, (unsigned)cfg->pwm_channel, (int)cfg->dir_pin_a,
          (int)cfg->dir_pin_b, (int)enable_pin,
          (unsigned long)(cfg->pwm_freq_hz > 0 ? cfg->pwm_freq_hz : 20000));
    return WINK_OK;

err_pwm_deinit:
    pal_pwm_deinit(cfg->pwm_channel);
err_release:
    LOG_W("init: hardware setup failed for '%s' (ch%u): %d; rolled back claims",
          cfg->owner, (unsigned)cfg->pwm_channel, (int)status);
    release_gpio_claim(enable_pin, cfg->owner);
    release_gpio_claim(cfg->dir_pin_b, cfg->owner);
    release_gpio_claim(cfg->dir_pin_a, cfg->owner);
    release_resource_logged(
        PAL_RESOURCE_PWM_CHANNEL, (uint32_t)cfg->pwm_channel, cfg->owner);
    return status;
}

wink_status_t dal_dc_motor_set_speed_promille(dal_dc_motor_t *dev, int16_t speed_promille)
{
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }

    if (speed_promille > 1000) {
        speed_promille = 1000;
    } else if (speed_promille < -1000) {
        speed_promille = -1000;
    }

    if (speed_promille == 0) {
        return dal_dc_motor_coast(dev);
    }

    wink_status_t s = write_enable_if_present(dev, true);
    if (wink_status_is_error(s)) {
        return s;
    }

    /* Apply invert: swap direction sense when config.invert == true */
    int16_t effective_speed = dev->config.invert ? -speed_promille : speed_promille;

    bool pin_a_level = false;
    bool pin_b_level = false;
    if (effective_speed > 0) {
        pin_a_level = true;
        pin_b_level = false;
    } else {
        pin_a_level = false;
        pin_b_level = true;
    }

    uint16_t abs_promille = (uint16_t)(effective_speed >= 0 ? effective_speed : -effective_speed);
    s = apply_dir_and_duty(dev, pin_a_level, pin_b_level, abs_promille);
    if (wink_status_is_error(s)) {
        return s;
    }

    dev->current_speed_promille = speed_promille;
    return WINK_OK;
}

wink_status_t dal_dc_motor_get_speed_promille(const dal_dc_motor_t *dev, int16_t *out_speed_promille)
{
    if (dev == NULL || out_speed_promille == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }
    *out_speed_promille = dev->current_speed_promille;
    return WINK_OK;
}

wink_status_t dal_dc_motor_coast(dal_dc_motor_t *dev)
{
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }

    wink_status_t s = write_enable_if_present(dev, true);
    if (wink_status_is_error(s)) {
        return s;
    }

    s = apply_dir_and_duty(dev, false, false, 0);
    if (wink_status_is_error(s)) {
        return s;
    }

    dev->current_speed_promille = 0;
    return WINK_OK;
}

wink_status_t dal_dc_motor_brake(dal_dc_motor_t *dev)
{
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }
    /* Short-brake needs both half-bridges; no silent coast. */
    if (dev->config.dir_pin_b < 0) {
        return WINK_ERR_UNSUPPORTED;
    }

    wink_status_t s = write_enable_if_present(dev, true);
    if (wink_status_is_error(s)) {
        return s;
    }

    s = apply_dir_and_duty(dev, true, true, 0);
    if (wink_status_is_error(s)) {
        return s;
    }

    dev->current_speed_promille = 0;
    return WINK_OK;
}

wink_status_t dal_dc_motor_safe_off(dal_dc_motor_t *dev)
{
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    /* DAL-L-022: idempotent on uninitialized handles — invoked from
     * safe_off_all() on watchdog/panic/rollback paths where "nothing to
     * shut off" is success, not an error. */
    if (!dev->initialized) {
        return WINK_OK;
    }

    if (dev->config.enable_pin >= 0) {
        if (dev->config.dir_pin_b >= 0) {
            WINK_IGNORE_UNUSED(dal_dc_motor_brake(dev));
        } else {
            WINK_IGNORE_UNUSED(dal_dc_motor_coast(dev));
        }
        wink_status_t s = write_enable_if_present(dev, false);
        if (wink_status_is_error(s)) {
            return s;
        }
        dev->current_speed_promille = 0;
        return WINK_OK;
    }

    return dal_dc_motor_brake(dev);
}

wink_status_t dal_dc_motor_deinit(dal_dc_motor_t *dev)
{
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_OK;
    }

    /* Prefer safe_off; ignore unsupported single-dir without enable. */
    WINK_IGNORE_UNUSED(dal_dc_motor_safe_off(dev));

    pal_pwm_deinit(dev->config.pwm_channel);
    pal_gpio_reset_pin(dev->config.dir_pin_a);
    if (dev->config.dir_pin_b >= 0) {
        pal_gpio_reset_pin(dev->config.dir_pin_b);
    }
    if (dev->config.enable_pin >= 0) {
        pal_gpio_reset_pin(dev->config.enable_pin);
    }

    uint8_t channel = dev->config.pwm_channel;
    wink_pin_t pin_a = dev->config.dir_pin_a;
    wink_pin_t pin_b = dev->config.dir_pin_b;
    wink_pin_t enable = dev->config.enable_pin;
    const char *owner = dev->config.owner;

    release_resource_logged(PAL_RESOURCE_PWM_CHANNEL, (uint32_t)channel, owner);
    release_resource_logged(PAL_RESOURCE_GPIO_PIN, (uint32_t)pin_a, owner);
    if (pin_b >= 0) {
        release_resource_logged(PAL_RESOURCE_GPIO_PIN, (uint32_t)pin_b, owner);
    }
    if (enable >= 0) {
        release_resource_logged(PAL_RESOURCE_GPIO_PIN, (uint32_t)enable, owner);
    }

    memset(dev, 0, sizeof(dal_dc_motor_t));
    return WINK_OK;
}
