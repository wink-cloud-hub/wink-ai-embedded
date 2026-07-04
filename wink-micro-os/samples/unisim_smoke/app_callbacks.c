/**
 * @file app_callbacks.c
 * @brief unisim_smoke wasm fixture — reaches all 13 js_* imports.
 *
 * The Node smoke test (simulator/src/unisim/bridge/__tests__/nodeSmoke.test.ts)
 * loads the compiled wasm and, over its lifetime, drives every js_* import
 * at least once. This app's job is only to CALL the C side of each import so
 * emcc doesn't tree-shake the symbol out.
 *
 * All calls live in app_init() so the Node test can drive one wasm boot and
 * observe end state without spinning app_loop.
 */
#include "device_tree.h"
#include "wink_app.h"
#include "wink_runtime.h"
#include "wink_status.h"
#include "pal_hal.h"      /* pal_gpio_write/read, pal_pwm_init/set_duty, pal_i2c_transfer,


/* ADR-0017 层 1 例外：本 TU 合法调用 WINK_BLOCKING API。抑制
 * -Wdeprecated-declarations 使 -Werror 下仍能编译；严格模式
 * (-DWINK_STRICT_NONBLOCKING=1) 下相关 API 声明直接消失，本 TU 会链接失败——那是设计意图。 */
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
                             pal_gpio_enable_interrupt/disable_interrupt, pal_gpio_pulse_in */
#include "pal_resource.h" /* pal_resource_claim */
#include "pal_osal.h"     /* pal_os_sleep_ms, pal_os_busy_wait_us, pal_os_get_ms/us */
#include "pal_irq.h"      /* PAL_ISR macro */
#include "pal_debug.h"    /* pal_debug_printf */

static PAL_ISR void smoke_isr(void *arg)
{
    (void)arg;
    /* No-op — the presence of the registered ISR is what we care about;
     * the Node test observes the poll pump via wasm ticks. */
}

static void app_init(void)
{
    /* --- js_pal_gpio_write / js_pal_gpio_read ---
     * PAL requires the pin to be claimed via pal_resource_claim before
     * write/read will route to js_pal_gpio_*. pal_gpio_init is a no-op on
     * the wasm target (placeholder for hardware init). */
    (void)pal_gpio_init(SMOKE_LED_PIN, PAL_GPIO_OUTPUT_PUSH_PULL);
    (void)pal_resource_claim(PAL_RESOURCE_GPIO_PIN, SMOKE_LED_PIN, "smoke_led");
    (void)pal_gpio_write(SMOKE_LED_PIN, true);
    bool level = false;
    (void)pal_gpio_read(SMOKE_LED_PIN, &level);

    /* --- js_pal_pwm_set_duty ---
     * pal_pwm_init claims the channel internally. */
    if (!wink_status_is_error(pal_pwm_init(SMOKE_PWM_CHANNEL, SMOKE_PWM_FREQ_HZ))) {
        (void)pal_pwm_set_duty(SMOKE_PWM_CHANNEL, 50.0f);
    }

    /* --- js_pal_i2c_transfer --- */
    uint8_t wbuf[2] = { 0xAA, 0xBB };
    uint8_t rbuf[2] = { 0 };
    (void)pal_i2c_transfer(SMOKE_I2C_PORT, SMOKE_I2C_ADDR, wbuf, sizeof(wbuf), rbuf, sizeof(rbuf));

    /* --- js_pal_register_interrupt / js_pal_deregister_interrupt ---
     * ISR pin must also be claimed for read-back; enable_interrupt handles
     * its own service install but the GPIO resource claim is still required. */
    (void)pal_gpio_init(SMOKE_ISR_PIN, PAL_GPIO_INPUT);
    (void)pal_resource_claim(PAL_RESOURCE_GPIO_PIN, SMOKE_ISR_PIN, "smoke_isr");
    (void)pal_gpio_enable_interrupt(SMOKE_ISR_PIN, PAL_GPIO_INTR_RISING_EDGE, smoke_isr, NULL);
    /* Then disable to hit the deregister path in the same run: */
    (void)pal_gpio_disable_interrupt(SMOKE_ISR_PIN);
    /* Re-enable so the Node test can inject one via irqQueue.push and see
     * the poll pump deliver it. */
    (void)pal_gpio_enable_interrupt(SMOKE_ISR_PIN, PAL_GPIO_INTR_RISING_EDGE, smoke_isr, NULL);

    /* --- HC-SR04 ultrasonic timing via generic PAL primitives ---
     *     ADR-0017: no dedicated js_sim_*_ultrasonic bridge hooks exist. The
     *     ultrasonic simulation is driven end-to-end through generic PAL APIs:
     *     pal_gpio_write for the TRIG pulse and pal_gpio_pulse_in for ECHO
     *     capture — both routed to the standard js_pal_gpio_* / js_pal_rmt_*
     *     bridge imports. Both TRIG and ECHO pins must be claimed for
     *     pal_gpio_pulse_in to not short-circuit on the resource check. */
    (void)pal_gpio_init(SMOKE_ULTRASONIC_TRIG, PAL_GPIO_OUTPUT_PUSH_PULL);
    (void)pal_gpio_init(SMOKE_ULTRASONIC_ECHO, PAL_GPIO_INPUT);
    (void)pal_resource_claim(PAL_RESOURCE_GPIO_PIN, SMOKE_ULTRASONIC_TRIG, "smoke_us_trig");
    (void)pal_resource_claim(PAL_RESOURCE_GPIO_PIN, SMOKE_ULTRASONIC_ECHO, "smoke_us_echo");
    (void)pal_gpio_write(SMOKE_ULTRASONIC_TRIG, true);
    pal_os_busy_wait_us(10);
    (void)pal_gpio_write(SMOKE_ULTRASONIC_TRIG, false);
    uint32_t pulse_us = 0;
    (void)pal_gpio_pulse_in(SMOKE_ULTRASONIC_ECHO, true, 0, &pulse_us);

    /* --- js_pal_os_get_ms / js_pal_os_get_us / js_pal_os_sleep_ms / js_pal_os_busy_wait_us ---
     * Values printed via pal_debug_printf so the calls survive LTO. */
    uint64_t t0_us = pal_os_get_us();
    uint64_t t0_ms = pal_os_get_ms();
    pal_os_sleep_ms(5);
    pal_os_busy_wait_us(100);
    uint64_t t1_us = pal_os_get_us();
    uint64_t t1_ms = pal_os_get_ms();
    pal_debug_printf("[smoke] init complete t0=%llu/%llu t1=%llu/%llu\n",
                     (unsigned long long)t0_us, (unsigned long long)t0_ms,
                     (unsigned long long)t1_us, (unsigned long long)t1_ms);
}

static void app_loop(void)
{
    /* Idle loop; Node smoke drives progression via advance() on the JS clock,
     * which resolves the sleep(5) promise on the next tick boundary. */
}

static void app_on_fault(uint32_t code)
{
    (void)code;
}

const wink_app_callbacks_t *wink_app_get_callbacks(void)
{
    static const wink_app_callbacks_t cb = {
        .init     = app_init,
        .loop     = app_loop,
        .on_fault = app_on_fault,
    };
    return &cb;
}
