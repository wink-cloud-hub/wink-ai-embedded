// SPDX-License-Identifier: Apache-2.0
/**
 * @file dal_buzzer.c
 * @brief DAL buzzer driver implementation (PASSIVE_PWM & ACTIVE_GPIO variants).
 */
#define LOG_TAG "dal_buzzer"
#include "output/dal_buzzer.h"
#include "pal_hal.h"
#include "pal_resource.h"
#include "pal_log.h"
#include <string.h>

wink_status_t dal_buzzer_init(dal_buzzer_t *dev, const dal_buzzer_config_t *cfg)
{
    if (dev == NULL || cfg == NULL || cfg->owner == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (dev->initialized) {
        return WINK_ERR_ALREADY_INITIALIZED;
    }

    dev->config = *cfg;
    if (dev->config.default_freq_hz == 0u) {
        dev->config.default_freq_hz = DAL_BUZZER_DEFAULT_FREQ_HZ;
    }

    if (dev->config.variant == DAL_BUZZER_VARIANT_PASSIVE_PWM) {
        if (dev->config.pwm_channel >= PAL_PWM_CHANNELS) {
            return WINK_ERR_INVALID_ARG;
        }

        wink_status_t st = pal_resource_claim(PAL_RESOURCE_PWM_CHANNEL,
                                             dev->config.pwm_channel,
                                             dev->config.owner);
        if (wink_status_is_error(st)) {
            return st;
        }

        if (dev->config.enable_pin >= 0) {
            st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN,
                                    (uint32_t)dev->config.enable_pin,
                                    dev->config.owner);
            if (wink_status_is_error(st)) {
                WINK_IGNORE_RESULT(pal_resource_release(PAL_RESOURCE_PWM_CHANNEL,
                                                        dev->config.pwm_channel,
                                                        dev->config.owner));
                return st;
            }
            WINK_IGNORE_RESULT(pal_gpio_init(dev->config.enable_pin, PAL_GPIO_OUTPUT_PUSH_PULL));
            WINK_IGNORE_RESULT(pal_gpio_write(dev->config.enable_pin, !dev->config.enable_active_high));
        }

        pal_pwm_config_t pwm_cfg = {
            .freq_hz = dev->config.default_freq_hz,
            .resolution_bits = 0u,
            .clock_requirement = PAL_PWM_CLOCK_AUTO,
        };
        st = pal_pwm_init_ex(dev->config.pwm_channel, &pwm_cfg);
        if (wink_status_is_error(st)) {
            if (dev->config.enable_pin >= 0) {
                pal_gpio_reset_pin(dev->config.enable_pin);
                WINK_IGNORE_RESULT(pal_resource_release(PAL_RESOURCE_GPIO_PIN,
                                                        (uint32_t)dev->config.enable_pin,
                                                        dev->config.owner));
            }
            WINK_IGNORE_RESULT(pal_resource_release(PAL_RESOURCE_PWM_CHANNEL,
                                                    dev->config.pwm_channel,
                                                    dev->config.owner));
            return st;
        }

        WINK_IGNORE_RESULT(pal_pwm_set_duty(dev->config.pwm_channel, 0.0f));

        if (dev->config.enable_pin >= 0) {
            WINK_IGNORE_RESULT(pal_gpio_write(dev->config.enable_pin, dev->config.enable_active_high));
        }
    } else if (dev->config.variant == DAL_BUZZER_VARIANT_ACTIVE_GPIO) {
        wink_status_t st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN,
                                             (uint32_t)dev->config.pin,
                                             dev->config.owner);
        if (wink_status_is_error(st)) {
            return st;
        }

        if (dev->config.enable_pin >= 0) {
            st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN,
                                    (uint32_t)dev->config.enable_pin,
                                    dev->config.owner);
            if (wink_status_is_error(st)) {
                WINK_IGNORE_RESULT(pal_resource_release(PAL_RESOURCE_GPIO_PIN,
                                                        (uint32_t)dev->config.pin,
                                                        dev->config.owner));
                return st;
            }
            WINK_IGNORE_RESULT(pal_gpio_init(dev->config.enable_pin, PAL_GPIO_OUTPUT_PUSH_PULL));
            WINK_IGNORE_RESULT(pal_gpio_write(dev->config.enable_pin, !dev->config.enable_active_high));
        }

        st = pal_gpio_init(dev->config.pin, PAL_GPIO_OUTPUT_PUSH_PULL);
        if (wink_status_is_error(st)) {
            if (dev->config.enable_pin >= 0) {
                pal_gpio_reset_pin(dev->config.enable_pin);
                WINK_IGNORE_RESULT(pal_resource_release(PAL_RESOURCE_GPIO_PIN,
                                                        (uint32_t)dev->config.enable_pin,
                                                        dev->config.owner));
            }
            WINK_IGNORE_RESULT(pal_resource_release(PAL_RESOURCE_GPIO_PIN,
                                                    (uint32_t)dev->config.pin,
                                                    dev->config.owner));
            return st;
        }

        WINK_IGNORE_RESULT(pal_gpio_write(dev->config.pin, !dev->config.active_high));

        if (dev->config.enable_pin >= 0) {
            WINK_IGNORE_RESULT(pal_gpio_write(dev->config.enable_pin, dev->config.enable_active_high));
        }
    } else {
        return WINK_ERR_INVALID_ARG;
    }

    dev->current_freq_hz = 0u;
    dev->is_on = false;
    dev->initialized = true;
    return WINK_OK;
}

wink_status_t dal_buzzer_play_tone(dal_buzzer_t *dev, uint32_t freq_hz)
{
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }

    if (freq_hz == 0u) {
        return dal_buzzer_stop_tone(dev);
    }

    if (dev->config.variant == DAL_BUZZER_VARIANT_PASSIVE_PWM) {
        if (freq_hz < DAL_BUZZER_MIN_FREQ_HZ || freq_hz > DAL_BUZZER_MAX_FREQ_HZ) {
            return WINK_ERR_OUT_OF_RANGE;
        }

        wink_status_t st = pal_pwm_set_freq(dev->config.pwm_channel, freq_hz);
        if (wink_status_is_error(st)) {
            return st;
        }

        st = pal_pwm_set_duty(dev->config.pwm_channel, 50.0f);
        if (wink_status_is_error(st)) {
            return st;
        }

        dev->current_freq_hz = freq_hz;
        dev->is_on = true;
        return WINK_OK;
    } else if (dev->config.variant == DAL_BUZZER_VARIANT_ACTIVE_GPIO) {
        wink_status_t st = pal_gpio_write(dev->config.pin, dev->config.active_high);
        if (wink_status_is_error(st)) {
            return st;
        }

        dev->current_freq_hz = dev->config.default_freq_hz;
        dev->is_on = true;
        return WINK_OK;
    }

    return WINK_ERR_INVALID_STATE;
}

wink_status_t dal_buzzer_stop_tone(dal_buzzer_t *dev)
{
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }

    if (dev->config.variant == DAL_BUZZER_VARIANT_PASSIVE_PWM) {
        WINK_IGNORE_RESULT(pal_pwm_set_duty(dev->config.pwm_channel, 0.0f));
    } else if (dev->config.variant == DAL_BUZZER_VARIANT_ACTIVE_GPIO) {
        WINK_IGNORE_RESULT(pal_gpio_write(dev->config.pin, !dev->config.active_high));
    }

    dev->current_freq_hz = 0u;
    dev->is_on = false;
    return WINK_OK;
}

wink_status_t dal_buzzer_on(dal_buzzer_t *dev)
{
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }
    return dal_buzzer_play_tone(dev, dev->config.default_freq_hz);
}

wink_status_t dal_buzzer_off(dal_buzzer_t *dev)
{
    return dal_buzzer_stop_tone(dev);
}

wink_status_t dal_buzzer_set(dal_buzzer_t *dev, bool on)
{
    return on ? dal_buzzer_on(dev) : dal_buzzer_off(dev);
}

wink_status_t dal_buzzer_toggle(dal_buzzer_t *dev)
{
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }
    return dev->is_on ? dal_buzzer_off(dev) : dal_buzzer_on(dev);
}

wink_status_t dal_buzzer_is_on(const dal_buzzer_t *dev, bool *out_on)
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

wink_status_t dal_buzzer_safe_off(dal_buzzer_t *dev)
{
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_OK;
    }

    WINK_IGNORE_RESULT(dal_buzzer_off(dev));

    if (dev->config.enable_pin >= 0) {
        WINK_IGNORE_RESULT(pal_gpio_write(dev->config.enable_pin, !dev->config.enable_active_high));
    }
    return WINK_OK;
}

wink_status_t dal_buzzer_deinit(dal_buzzer_t *dev)
{
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_OK;
    }

    WINK_IGNORE_RESULT(dal_buzzer_safe_off(dev));

    if (dev->config.variant == DAL_BUZZER_VARIANT_PASSIVE_PWM) {
        pal_pwm_deinit(dev->config.pwm_channel);
        wink_status_t st = pal_resource_release(PAL_RESOURCE_PWM_CHANNEL,
                                               dev->config.pwm_channel,
                                               dev->config.owner);
        if (wink_status_is_error(st)) {
            LOG_W("Failed to release PWM channel %u for owner %s: %d",
                  dev->config.pwm_channel, dev->config.owner, (int)st);
        }
    } else if (dev->config.variant == DAL_BUZZER_VARIANT_ACTIVE_GPIO) {
        pal_gpio_reset_pin(dev->config.pin);
        wink_status_t st = pal_resource_release(PAL_RESOURCE_GPIO_PIN,
                                               (uint32_t)dev->config.pin,
                                               dev->config.owner);
        if (wink_status_is_error(st)) {
            LOG_W("Failed to release GPIO pin %u for owner %s: %d",
                  dev->config.pin, dev->config.owner, (int)st);
        }
    }

    if (dev->config.enable_pin >= 0) {
        pal_gpio_reset_pin(dev->config.enable_pin);
        wink_status_t st = pal_resource_release(PAL_RESOURCE_GPIO_PIN,
                                               (uint32_t)dev->config.enable_pin,
                                               dev->config.owner);
        if (wink_status_is_error(st)) {
            LOG_W("Failed to release enable GPIO pin %d for owner %s: %d",
                  dev->config.enable_pin, dev->config.owner, (int)st);
        }
    }

    memset(dev, 0, sizeof(*dev));
    return WINK_OK;
}
