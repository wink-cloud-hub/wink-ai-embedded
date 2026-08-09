// SPDX-License-Identifier: Apache-2.0
/**
 * @file selftest_rmt_loopback.c
 * @brief RMT pulse capture hardware self-loopback selftest.
 */
#define LOG_TAG "selftest.rmt"

#include "wink_selftest.h"
#include "wink_selftest_internal.h"
#include "wink_status.h"
#include "wink_log.h"
#include "pal_hal.h"
#include "pal_osal.h"
#include "pal_pwm_router.h"
#include "pal_resource.h"
#include "internal/pal_test_loopback.h"
#include "hal/pal_rmt.h"

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#  pragma warning(disable: 4996)
#endif

#define RMT_TEST_PIN   4u
#define RMT_PWM_CH     1u
#define RMT_PULSE_US   100u
#define RMT_PULSE_MIN  90u
#define RMT_PULSE_MAX  110u
#define RMT_TIMEOUT_US 30000u

wink_status_t wink_selftest_rmt_self_loopback(wink_selftest_result_t *r)
{
    r->note = "RMT 100us pulse self-loopback";
    r->metric = 0;

    wink_status_t final_st = WINK_OK;
    bool skip = false;

    bool pwm_was_up = pal_pwm_router_channel_ready(RMT_PWM_CH);
    if (pwm_was_up) {
        pal_pwm_deinit(RMT_PWM_CH);
    }

    wink_status_t st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, RMT_TEST_PIN, "selftest_rmt");
    if (wink_status_is_error(st) && st != WINK_ERR_BUSY) {
        r->note = "resource claim failed";
        goto restore_pwm;
    }

    st = pal_gpio_init(RMT_TEST_PIN, PAL_GPIO_OUTPUT_PUSH_PULL);
    if (wink_status_is_error(st)) {
        r->note = "pal_gpio_init failed";
        goto release;
    }
    WINK_IGNORE_RESULT(pal_gpio_write(RMT_TEST_PIN, false));

    st = pal_rmt_pulse_capture_init(RMT_TEST_PIN, PAL_RMT_EDGE_RISING);
    if (st == WINK_ERR_UNSUPPORTED) {
        r->note = "RMT not supported on this target";
        r->metric = 0;
        WINK_IGNORE_RESULT(pal_gpio_init(RMT_TEST_PIN, PAL_GPIO_INPUT_PULLUP));
        skip = true;
        goto release;
    }
    if (wink_status_is_error(st)) {
        r->note = "pal_rmt_pulse_capture_init failed";
        final_st = st;
        goto release;
    }

    st = pal_test_enable_hardware_loopback(RMT_TEST_PIN, RMT_TEST_PIN);
    if (wink_status_is_error(st)) {
        r->note = "loopback enable failed";
        final_st = st;
        pal_rmt_pulse_capture_deinit();
        goto release;
    }

    st = pal_rmt_pulse_capture_arm();
    if (wink_status_is_error(st)) {
        r->note = "pal_rmt_pulse_capture_arm failed";
        final_st = st;
        pal_test_disable_hardware_loopback(RMT_TEST_PIN, RMT_TEST_PIN);
        pal_rmt_pulse_capture_deinit();
        goto release;
    }

    pal_os_busy_wait_us(50);
    WINK_IGNORE_RESULT(pal_gpio_write(RMT_TEST_PIN, true));
    pal_os_busy_wait_us(RMT_PULSE_US);
    WINK_IGNORE_RESULT(pal_gpio_write(RMT_TEST_PIN, false));

    uint32_t pulse_us = 0;
    st = pal_rmt_pulse_capture_wait_armed(RMT_TIMEOUT_US, &pulse_us);

    pal_test_disable_hardware_loopback(RMT_TEST_PIN, RMT_TEST_PIN);
    pal_rmt_pulse_capture_deinit();

    if (wink_status_is_error(st)) {
        r->note = "capture wait failed (no edge / timeout)";
        r->metric = 0;
        final_st = st;
        goto release;
    }

    r->metric = pulse_us;
    if (pulse_us < RMT_PULSE_MIN || pulse_us > RMT_PULSE_MAX) {
        r->note = "pulse width out of 90..110us range";
        final_st = WINK_ERR_HARDWARE;
        goto release;
    }

    r->note = "100us pulse captured OK";

release:
    WINK_IGNORE_RESULT(pal_resource_release(PAL_RESOURCE_GPIO_PIN, RMT_TEST_PIN, "selftest_rmt"));
    WINK_IGNORE_RESULT(pal_gpio_init(RMT_TEST_PIN, PAL_GPIO_INPUT_PULLUP));

restore_pwm:
    if (pwm_was_up) {
        if (pal_pwm_init(RMT_PWM_CH, 50u) == WINK_OK) {
            WINK_IGNORE_RESULT(pal_pwm_set_duty(RMT_PWM_CH, 50.0f));
        }
    }

    if (skip) return WINK_ERR_UNSUPPORTED;
    return final_st;
}
