/**
 * @file app_callbacks.c
 * @brief determinism_fixture WASM app — exercises GPIO, ADC, PWM, UART & faults.
 */
#include "device_tree.h"
#include "wink_app.h"
#include "wink_runtime.h"
#include "wink_status.h"
#include "pal_log.h"
#include "pal_hal.h"
#include "hal/pal_uart.h"
#include "pal_resource.h"

static void app_on_boot(const wink_boot_info_t *info)
{
    (void)info;
}

static wink_status_t app_init_status(void)
{
    (void)pal_gpio_init(FIXTURE_LED_PIN, PAL_GPIO_OUTPUT_PUSH_PULL);
    (void)pal_gpio_write(FIXTURE_LED_PIN, true);
    (void)pal_gpio_init(FIXTURE_BTN_PIN, PAL_GPIO_INPUT_PULLUP);
    return WINK_OK;
}

static void app_loop(void)
{
    bool btn = false;
    (void)pal_gpio_read(FIXTURE_BTN_PIN, &btn);
    (void)pal_gpio_write(FIXTURE_LED_PIN, !btn);
}

static const wink_app_callbacks_t s_callbacks = {
    .on_boot = app_on_boot,
    .init_status = app_init_status,
    .loop = app_loop,
};

const wink_app_callbacks_t *wink_app_get_callbacks(void)
{
    return &s_callbacks;
}
