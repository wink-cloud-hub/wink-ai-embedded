#include "actuator/dal_dc_motor.h"
#include "pal_resource.h"
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
                                        float abs_speed)
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

    float duty_percent = abs_speed * 100.0f;
    return pal_pwm_set_duty(dev->config.pwm_channel, duty_percent);
}

static wink_status_t release_gpio_claim(wink_pin_t pin, const char *owner)
{
    if (pin < 0) {
        return WINK_OK;
    }
    return pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)pin, owner);
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
        return rs;
    }
    rs = pal_resource_claim(
        PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->dir_pin_a, cfg->owner);
    if (wink_status_is_error(rs)) {
        WINK_IGNORE_UNUSED(pal_resource_release(
            PAL_RESOURCE_PWM_CHANNEL, (uint32_t)cfg->pwm_channel, cfg->owner));
        return rs;
    }
    if (cfg->dir_pin_b >= 0) {
        rs = pal_resource_claim(
            PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->dir_pin_b, cfg->owner);
        if (wink_status_is_error(rs)) {
            WINK_IGNORE_UNUSED(release_gpio_claim(cfg->dir_pin_a, cfg->owner));
            WINK_IGNORE_UNUSED(pal_resource_release(
                PAL_RESOURCE_PWM_CHANNEL,
                (uint32_t)cfg->pwm_channel,
                cfg->owner));
            return rs;
        }
    }
    if (enable_pin >= 0) {
        rs = pal_resource_claim(
            PAL_RESOURCE_GPIO_PIN, (uint32_t)enable_pin, cfg->owner);
        if (wink_status_is_error(rs)) {
            WINK_IGNORE_UNUSED(release_gpio_claim(cfg->dir_pin_b, cfg->owner));
            WINK_IGNORE_UNUSED(release_gpio_claim(cfg->dir_pin_a, cfg->owner));
            WINK_IGNORE_UNUSED(pal_resource_release(
                PAL_RESOURCE_PWM_CHANNEL,
                (uint32_t)cfg->pwm_channel,
                cfg->owner));
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
    dev->current_speed = 0.0f;
    dev->initialized = true;

    WINK_IGNORE_UNUSED(dal_dc_motor_coast(dev));

    return WINK_OK;

err_pwm_deinit:
    pal_pwm_deinit(cfg->pwm_channel);
err_release:
    WINK_IGNORE_UNUSED(release_gpio_claim(enable_pin, cfg->owner));
    WINK_IGNORE_UNUSED(release_gpio_claim(cfg->dir_pin_b, cfg->owner));
    WINK_IGNORE_UNUSED(release_gpio_claim(cfg->dir_pin_a, cfg->owner));
    WINK_IGNORE_UNUSED(pal_resource_release(
        PAL_RESOURCE_PWM_CHANNEL, (uint32_t)cfg->pwm_channel, cfg->owner));
    return status;
}

wink_status_t dal_dc_motor_set_speed(dal_dc_motor_t *dev, float speed)
{
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }

    if (speed > 1.0f) {
        speed = 1.0f;
    } else if (speed < -1.0f) {
        speed = -1.0f;
    }

    if (speed == 0.0f) {
        return dal_dc_motor_coast(dev);
    }

    wink_status_t s = write_enable_if_present(dev, true);
    if (wink_status_is_error(s)) {
        return s;
    }

    /* Apply invert: swap direction sense when config.invert == true */
    float effective_speed = dev->config.invert ? -speed : speed;

    bool pin_a_level = false;
    bool pin_b_level = false;
    if (effective_speed > 0.0f) {
        pin_a_level = true;
        pin_b_level = false;
    } else {
        pin_a_level = false;
        pin_b_level = true;
    }

    float abs_speed = effective_speed >= 0.0f ? effective_speed : -effective_speed;
    s = apply_dir_and_duty(dev, pin_a_level, pin_b_level, abs_speed);
    if (wink_status_is_error(s)) {
        return s;
    }

    dev->current_speed = speed;
    return WINK_OK;
}

wink_status_t dal_dc_motor_get_speed(const dal_dc_motor_t *dev, float *out_speed)
{
    if (dev == NULL || out_speed == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }
    *out_speed = dev->current_speed;
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

    s = apply_dir_and_duty(dev, false, false, 0.0f);
    if (wink_status_is_error(s)) {
        return s;
    }

    dev->current_speed = 0.0f;
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

    s = apply_dir_and_duty(dev, true, true, 0.0f);
    if (wink_status_is_error(s)) {
        return s;
    }

    dev->current_speed = 0.0f;
    return WINK_OK;
}

wink_status_t dal_dc_motor_safe_off(dal_dc_motor_t *dev)
{
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
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
        dev->current_speed = 0.0f;
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

    WINK_IGNORE_UNUSED(pal_resource_release(
        PAL_RESOURCE_PWM_CHANNEL, (uint32_t)channel, owner));
    WINK_IGNORE_UNUSED(pal_resource_release(
        PAL_RESOURCE_GPIO_PIN, (uint32_t)pin_a, owner));
    if (pin_b >= 0) {
        WINK_IGNORE_UNUSED(pal_resource_release(
            PAL_RESOURCE_GPIO_PIN, (uint32_t)pin_b, owner));
    }
    if (enable >= 0) {
        WINK_IGNORE_UNUSED(pal_resource_release(
            PAL_RESOURCE_GPIO_PIN, (uint32_t)enable, owner));
    }

    memset(dev, 0, sizeof(dal_dc_motor_t));
    return WINK_OK;
}
