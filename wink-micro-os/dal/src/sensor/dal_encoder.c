#include "sensor/dal_encoder.h"
#include "pal_resource.h"
#include "pal_irq.h"
#include <string.h>

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

    /* 1. 声明占用资源 */
    wink_status_t rs = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_a, cfg->owner);
    if (wink_status_is_error(rs)) {
        return rs;
    }
    if (cfg->pin_b >= 0) {
        rs = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_b, cfg->owner);
        if (wink_status_is_error(rs)) {
            WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_a, cfg->owner));
            return rs;
        }
    }

    /* 2. 初始化底层 GPIO */
    wink_status_t status = pal_gpio_init(cfg->pin_a, cfg->pull);
    if (wink_status_is_error(status)) {
        goto err_release;
    }

    if (cfg->pin_b >= 0) {
        status = pal_gpio_init(cfg->pin_b, cfg->pull);
        if (wink_status_is_error(status)) {
            pal_gpio_reset_pin(cfg->pin_a);
            goto err_release;
        }
    }

    /* 保存配置 */
    memcpy(&dev->config, cfg, sizeof(dal_encoder_config_t));
    dev->count = 0;
    dev->initialized = true;
    dev->isr_registered = false;

    /* 3. 注册上升沿中断 */
    status = pal_gpio_enable_interrupt(
        cfg->pin_a,
        PAL_GPIO_INTR_RISING_EDGE,
        dal_encoder_gpio_isr,
        dev);
    if (wink_status_is_error(status)) {
        pal_gpio_reset_pin(cfg->pin_a);
        if (cfg->pin_b >= 0) {
            pal_gpio_reset_pin(cfg->pin_b);
        }
        goto err_release;
    }
    dev->isr_registered = true;

    return WINK_OK;

err_release:
    if (cfg->pin_b >= 0) {
        WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_b, cfg->owner));
    }
    WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_a, cfg->owner));
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

    /* 禁用中断 */
    if (dev->isr_registered) {
        WINK_IGNORE_UNUSED(pal_gpio_disable_interrupt(dev->config.pin_a));
        WINK_IGNORE_UNUSED(pal_gpio_synchronize_interrupt(dev->config.pin_a));
    }

    /* 重置引脚 */
    pal_gpio_reset_pin(dev->config.pin_a);
    if (dev->config.pin_b >= 0) {
        pal_gpio_reset_pin(dev->config.pin_b);
    }

    /* 释放资源 */
    wink_pin_t pin_a = dev->config.pin_a;
    wink_pin_t pin_b = dev->config.pin_b;
    const char *owner = dev->config.owner;

    WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)pin_a, owner));
    if (pin_b >= 0) {
        WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)pin_b, owner));
    }

    memset(dev, 0, sizeof(dal_encoder_t));
    return WINK_OK;
}
