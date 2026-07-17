#include "actuator/dal_motor.h"
#include "pal_resource.h"
#include <string.h>

wink_status_t dal_motor_init(dal_motor_t *dev, const dal_motor_config_t *cfg)
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

    /* 1. 声明占用资源 */
    wink_status_t rs = pal_resource_claim(PAL_RESOURCE_PWM_CHANNEL, (uint32_t)cfg->pwm_channel, cfg->owner);
    if (wink_status_is_error(rs)) {
        return rs;
    }
    rs = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->dir_pin_a, cfg->owner);
    if (wink_status_is_error(rs)) {
        WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_PWM_CHANNEL, (uint32_t)cfg->pwm_channel, cfg->owner));
        return rs;
    }
    if (cfg->dir_pin_b >= 0) {
        rs = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->dir_pin_b, cfg->owner);
        if (wink_status_is_error(rs)) {
            WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->dir_pin_a, cfg->owner));
            WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_PWM_CHANNEL, (uint32_t)cfg->pwm_channel, cfg->owner));
            return rs;
        }
    }

    /* 2. 初始化底层 PWM 和 GPIO */
    wink_status_t status = pal_pwm_init(cfg->pwm_channel, cfg->pwm_freq_hz > 0 ? cfg->pwm_freq_hz : 20000);
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

    /* 3. 保存配置 */
    memcpy(&dev->config, cfg, sizeof(dal_motor_config_t));
    dev->current_speed = 0.0f;
    dev->initialized = true;

    /* 初始状态停止 */
    WINK_IGNORE_UNUSED(dal_motor_set_speed(dev, 0.0f));

    return WINK_OK;

err_pwm_deinit:
    pal_pwm_deinit(cfg->pwm_channel);
err_release:
    if (cfg->dir_pin_b >= 0) {
        WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->dir_pin_b, cfg->owner));
    }
    WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->dir_pin_a, cfg->owner));
    WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_PWM_CHANNEL, (uint32_t)cfg->pwm_channel, cfg->owner));
    return status;
}

wink_status_t dal_motor_set_speed(dal_motor_t *dev, float speed)
{
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }

    /* 速度限幅 [-1.0, 1.0] */
    if (speed > 1.0f) {
        speed = 1.0f;
    } else if (speed < -1.0f) {
        speed = -1.0f;
    }

    dev->current_speed = speed;

    /* 正反向引脚电平控制 */
    bool pin_a_level = false;
    bool pin_b_level = false;

    if (speed > 0.0f) {
        pin_a_level = true;
        pin_b_level = false;
    } else if (speed < 0.0f) {
        pin_a_level = false;
        pin_b_level = true;
    } else {
        /* 停止 */
        pin_a_level = false;
        pin_b_level = false;
    }

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

    /* 计算占空比绝对值百分比 */
    float abs_speed = speed >= 0.0f ? speed : -speed;
    float duty_percent = abs_speed * 100.0f;

    return pal_pwm_set_duty(dev->config.pwm_channel, duty_percent);
}

wink_status_t dal_motor_safe_off(dal_motor_t *dev)
{
    return dal_motor_set_speed(dev, 0.0f);
}

wink_status_t dal_motor_deinit(dal_motor_t *dev)
{
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_OK;
    }

    /* 停止电机 */
    WINK_IGNORE_UNUSED(dal_motor_safe_off(dev));

    /* 释放底层资源 */
    pal_pwm_deinit(dev->config.pwm_channel);
    pal_gpio_reset_pin(dev->config.dir_pin_a);
    if (dev->config.dir_pin_b >= 0) {
        pal_gpio_reset_pin(dev->config.dir_pin_b);
    }

    /* 释放资源声明 */
    uint8_t channel = dev->config.pwm_channel;
    wink_pin_t pin_a = dev->config.dir_pin_a;
    wink_pin_t pin_b = dev->config.dir_pin_b;
    const char *owner = dev->config.owner;

    WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_PWM_CHANNEL, (uint32_t)channel, owner));
    WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)pin_a, owner));
    if (pin_b >= 0) {
        WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)pin_b, owner));
    }

    memset(dev, 0, sizeof(dal_motor_t));
    return WINK_OK;
}
