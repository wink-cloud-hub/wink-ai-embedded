#include "dal_button.h"
#include "pal_hal.h"
#include <string.h> /* memcpy */

static bool button_raw_pressed(bool raw_level, bool active_low) {
    /* active_low: 按下=LOW(raw=false) → pressed=true */
    return raw_level != active_low;
}

wink_status_t dal_button_init(dal_button_t *dev, const dal_button_config_t *cfg) {
    if (dev == NULL || cfg == NULL) { return WINK_ERR_INVALID_ARG; }
    pal_gpio_mode_t mode = cfg->active_low ? PAL_GPIO_INPUT_PULLUP : PAL_GPIO_INPUT_PULLDOWN;
    wink_status_t status = pal_gpio_init(cfg->pin, mode);
    if (wink_status_is_error(status)) { return status; }
    /* 深拷贝配置到实例（支持 ADR-0008 Flash 动态覆写） */
    memcpy(&dev->config, cfg, sizeof(dal_button_config_t));
    dev->stable_pressed   = false;
    dev->last_reported    = false;
    dev->initialized      = true;
    dev->debounce_counter = 0;
    return WINK_OK;
}

wink_status_t dal_button_poll(dal_button_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }

    bool raw = pal_gpio_read(dev->config.pin);
    bool now_pressed = button_raw_pressed(raw, dev->config.active_low);

    if (now_pressed == dev->stable_pressed) {
        dev->debounce_counter = 0;
    } else {
        dev->debounce_counter++;
        if (dev->debounce_counter >= DAL_BUTTON_DEBOUNCE_THRESHOLD) {
            dev->stable_pressed = now_pressed;
            dev->debounce_counter = 0;
        }
    }
    return WINK_OK;
}

wink_status_t dal_button_is_pressed(const dal_button_t *dev, bool *out_pressed) {
    if (dev == NULL || out_pressed == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    *out_pressed = dev->stable_pressed;
    return WINK_OK;
}

wink_status_t dal_button_was_pressed(dal_button_t *dev, bool *out_was_pressed) {
    if (dev == NULL || out_was_pressed == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }

    bool event = (dev->stable_pressed && !dev->last_reported);
    dev->last_reported = dev->stable_pressed;
    *out_was_pressed = event;
    return WINK_OK;
}
