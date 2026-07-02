#include "dal_led.h"
#include "pal_hal.h"
#include "pal_resource.h"
#include <string.h> /* memcpy */

wink_status_t dal_led_init(dal_led_t *dev, const dal_led_config_t *cfg) {
    if (dev == NULL || cfg == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->owner == NULL) { return WINK_ERR_INVALID_ARG; }

    /* Track A（M1）：GPIO 引脚冲突治理——两 LED 若配同 pin 不同 owner，此处 BUSY。 */
    wink_status_t rs = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, cfg->pin, cfg->owner);
    if (wink_status_is_error(rs)) { return rs; }

    wink_status_t status = pal_gpio_init(cfg->pin, PAL_GPIO_OUTPUT_PUSH_PULL);
    if (wink_status_is_error(status)) {
        /* gcc16 不因 (void) 抑制 warn_unused_result：先赋值再丢弃，best-effort 回滚。 */
        wink_status_t _rb = pal_resource_release(PAL_RESOURCE_GPIO_PIN, cfg->pin, cfg->owner);
        (void)_rb;
        return status;
    }
    /* 深拷贝配置到实例（支持 ADR-0008 Flash 动态覆写） */
    memcpy(&dev->config, cfg, sizeof(dal_led_config_t));
    dev->is_on       = false;
    dev->initialized = true;
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
    pal_gpio_write(dev->config.pin, level);
    dev->is_on = on;
    return WINK_OK;
}

wink_status_t dal_led_toggle(dal_led_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    return dal_led_set(dev, !dev->is_on);
}
