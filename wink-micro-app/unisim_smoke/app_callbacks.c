/**
 * @file app_callbacks.c
 * @brief unisim_smoke wasm fixture — reaches all 13 js_* imports.
 *
 * The Node smoke test (simulator/src/unisim/bridge/__tests__/nodeSmoke.test.ts)
 * loads the compiled wasm and, over its lifetime, drives every js_* import
 * at least once. This app's job is only to CALL the C side of each import so
 * emcc doesn't tree-shake the symbol out.
 *
 * All calls live in app_init_status() so the Node test can drive one wasm boot
 * and observe end state without spinning app_loop.
 *
 * Note: device_tree.h is hand-written (not codegen) because this sample
 * exercises raw PAL bridge imports with explicit pin/channel constants, not
 * DAL-level device init. Codegen would claim resources that conflict with
 * the raw PAL calls below.
 */
#define LOG_TAG "unisim_smoke"

#include "device_tree.h"
#include "wink_app.h"
#include "wink_runtime.h"
#include "wink_status.h"
#include "wink_blocking_region.h"
#include "pal_log.h"
#include "pal_hal.h"
#include "hal/pal_i2c.h"
#include "pal_resource.h"
#include "pal_osal.h"
#include "pal_irq.h"

static PAL_ISR void smoke_isr(void *arg)
{
    (void)arg;
}

static void app_on_boot(const wink_boot_info_t *info)
{
    (void)info;
}

static wink_status_t app_init_status(void)
{
    /* --- js_pal_gpio_write / js_pal_gpio_read --- */
    (void)pal_gpio_init(SMOKE_LED_PIN, PAL_GPIO_OUTPUT_PUSH_PULL);
    (void)pal_resource_claim(PAL_RESOURCE_GPIO_PIN, SMOKE_LED_PIN, "smoke_led");
    (void)pal_gpio_write(SMOKE_LED_PIN, true);
    bool level = false;
    (void)pal_gpio_read(SMOKE_LED_PIN, &level);

    /* --- js_pal_pwm_set_duty --- */
    if (!wink_status_is_error(pal_pwm_init(SMOKE_PWM_CHANNEL, SMOKE_PWM_FREQ_HZ))) {
        (void)pal_pwm_set_duty(SMOKE_PWM_CHANNEL, 50.0f);
    }

    /* --- js_pal_i2c_transfer --- */
    (void)pal_i2c_bus_init(SMOKE_I2C_PORT, 0, 0, 100000);
    uint8_t wbuf[2] = { 0xAA, 0xBB };
    uint8_t rbuf[2] = { 0 };
    (void)pal_i2c_transfer(SMOKE_I2C_PORT, SMOKE_I2C_ADDR, wbuf, sizeof(wbuf), rbuf, sizeof(rbuf));

    /* --- js_pal_register_interrupt / js_pal_deregister_interrupt --- */
    (void)pal_gpio_init(SMOKE_ISR_PIN, PAL_GPIO_INPUT);
    (void)pal_resource_claim(PAL_RESOURCE_GPIO_PIN, SMOKE_ISR_PIN, "smoke_isr");
    (void)pal_gpio_enable_interrupt(SMOKE_ISR_PIN, PAL_GPIO_INTR_RISING_EDGE, smoke_isr, NULL);
    (void)pal_gpio_disable_interrupt(SMOKE_ISR_PIN);
    (void)pal_gpio_enable_interrupt(SMOKE_ISR_PIN, PAL_GPIO_INTR_RISING_EDGE, smoke_isr, NULL);

    /* --- HC-SR04 ultrasonic timing via generic PAL primitives --- */
    (void)pal_gpio_init(SMOKE_ULTRASONIC_TRIG, PAL_GPIO_OUTPUT_PUSH_PULL);
    (void)pal_gpio_init(SMOKE_ULTRASONIC_ECHO, PAL_GPIO_INPUT);
    (void)pal_resource_claim(PAL_RESOURCE_GPIO_PIN, SMOKE_ULTRASONIC_TRIG, "smoke_us_trig");
    (void)pal_resource_claim(PAL_RESOURCE_GPIO_PIN, SMOKE_ULTRASONIC_ECHO, "smoke_us_echo");
    (void)pal_gpio_write(SMOKE_ULTRASONIC_TRIG, true);
    pal_os_busy_wait_us(10);
    (void)pal_gpio_write(SMOKE_ULTRASONIC_TRIG, false);
    uint32_t pulse_us = 0;
    (void)pal_gpio_pulse_in(SMOKE_ULTRASONIC_ECHO, true, 0, &pulse_us);

    /* --- js_pal_os_get_ms / js_pal_os_get_us / js_pal_os_sleep_ms / js_pal_os_busy_wait_us --- */
    uint64_t t0_us = pal_os_get_us();
    uint64_t t0_ms = pal_os_get_ms();
#ifndef WINK_STRICT_NONBLOCKING
    WINK_INIT_BLOCKING_REGION_BEGIN
    pal_os_sleep_ms(5);
    WINK_INIT_BLOCKING_REGION_END
#endif
    pal_os_busy_wait_us(100);
    uint64_t t1_us = pal_os_get_us();
    uint64_t t1_ms = pal_os_get_ms();
    LOG_I("init complete t0_us=%llu t0_ms=%llu t1_us=%llu t1_ms=%llu",
          (unsigned long long)t0_us, (unsigned long long)t0_ms,
          (unsigned long long)t1_us, (unsigned long long)t1_ms);

    return WINK_OK;
}

static void app_loop(void)
{
}

static wink_status_t app_on_fault_status(uint32_t code)
{
    (void)code;
    return WINK_OK;
}

const wink_app_callbacks_t *wink_app_get_callbacks(void)
{
    static const wink_app_callbacks_t cb = {
        .init_status     = app_init_status,
        .loop            = app_loop,
        .on_fault_status = app_on_fault_status,
        .on_boot         = app_on_boot,
    };
    return &cb;
}
