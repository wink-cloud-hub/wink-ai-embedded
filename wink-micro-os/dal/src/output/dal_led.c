#include "dal_led.h"
#include "pal_hal.h"
#include "pal_resource.h"
#include <string.h> /* memcpy */

wink_status_t dal_led_init(dal_led_t *dev, const dal_led_config_t *cfg) {
    if (dev == NULL || cfg == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->owner == NULL) { return WINK_ERR_INVALID_ARG; }
    if (dev->initialized) { return WINK_ERR_ALREADY_INITIALIZED; }

    /* Track A（M1）：GPIO 引脚冲突治理——两 LED 若配同 pin 不同 owner，此处 BUSY。 */
    wink_status_t rs = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, cfg->pin, cfg->owner);
    if (wink_status_is_error(rs)) { return rs; }

    wink_status_t status = pal_gpio_init(cfg->pin, PAL_GPIO_OUTPUT_PUSH_PULL);
    if (wink_status_is_error(status)) {
        WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, cfg->pin, cfg->owner));
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

wink_status_t dal_led_deinit(dal_led_t *dev) {
    /* ADR-0024 §4 deinit — checked: 1(off)/2(pal_gpio_reset_pin)/3(N/A: no ISR)/
     *   4(N/A: no DMA)/5(N/A: bus-owner only)/6(N/A: not on shared bus)/7(memset)/
     *   8(NULL+uninit idempotent)/9(<10µs, no waits)/10(signature unified) */
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_OK; }  /* idempotent no-op on un-init dev */

    /* 1. Best-effort turn LED off before releasing the pin (safe-off semantic, ≤1µs). */
    WINK_IGNORE_UNUSED(dal_led_off(dev));

    /* Keep pin for resource release and GPIO reset */
    uint16_t pin = dev->config.pin;
    const char *owner = dev->config.owner;

    /* 2. Reset GPIO: disables any leftover interrupt routing, reverts to
     *    Hi-Z INPUT, and clears the esp_gpio_reserve bitmap (ADR-0024 §4 #2).
     *    On host/wasm this is a no-op; pal_resource_release handles SW claim. */
    pal_gpio_reset_pin(pin);

    /* 3. Release software resource claim so a subsequent init does not fail with BUSY. */
    WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, pin, owner));

    /* 7. Clear the instance data completely to guarantee no residual state */
    memset(dev, 0, sizeof(dal_led_t));

    return WINK_OK;
}
