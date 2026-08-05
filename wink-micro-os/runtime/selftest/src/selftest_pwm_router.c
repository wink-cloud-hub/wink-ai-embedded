// SPDX-License-Identifier: Apache-2.0
/**
 * @file selftest_pwm_router.c
 * @brief PWM router frequency isolation and timer reuse selftest.
 */
#define LOG_TAG "selftest.pwm"

#include "wink_selftest.h"
#include "wink_selftest_internal.h"
#include "wink_status.h"
#include "wink_log.h"
#include "pal_pwm_router.h"
#include "pal_hal.h"

#define SMOKE_PWM_CH_LO  1u
#define SMOKE_PWM_CH_HI  2u
#define SMOKE_PWM_CH_SAME 7u

wink_status_t wink_selftest_pwm_router_freq_isolation(wink_selftest_result_t *r)
{
    r->note = "50Hz vs 1kHz -> different timer";

    pal_pwm_deinit(SMOKE_PWM_CH_LO);
    pal_pwm_deinit(SMOKE_PWM_CH_HI);
    pal_pwm_deinit(SMOKE_PWM_CH_SAME);

    wink_status_t st = pal_pwm_init(SMOKE_PWM_CH_LO, 50u);
    if (wink_status_is_error(st)) {
        r->note = "pal_pwm_init(ch_lo) failed";
        return st;
    }
    st = pal_pwm_init(SMOKE_PWM_CH_HI, 1000u);
    if (wink_status_is_error(st)) {
        pal_pwm_deinit(SMOKE_PWM_CH_LO);
        r->note = "pal_pwm_init(ch_hi) failed";
        return st;
    }
    st = pal_pwm_init(SMOKE_PWM_CH_SAME, 50u);
    if (wink_status_is_error(st)) {
        pal_pwm_deinit(SMOKE_PWM_CH_HI);
        pal_pwm_deinit(SMOKE_PWM_CH_LO);
        r->note = "pal_pwm_init(ch_same) failed";
        return st;
    }

    uint8_t t_lo   = pal_pwm_router_channel_timer(SMOKE_PWM_CH_LO);
    uint8_t t_hi   = pal_pwm_router_channel_timer(SMOKE_PWM_CH_HI);
    uint8_t t_same = pal_pwm_router_channel_timer(SMOKE_PWM_CH_SAME);

    WINK_IGNORE_RESULT(pal_pwm_set_duty(SMOKE_PWM_CH_LO, 50.0f));
    WINK_IGNORE_RESULT(pal_pwm_set_duty(SMOKE_PWM_CH_HI, 50.0f));

    r->metric = ((uint32_t)t_lo & 0xFu)
              | (((uint32_t)t_hi   & 0xFu) << 4)
              | (((uint32_t)t_same & 0xFu) << 8);

    bool pass = true;
    if (t_lo >= 4u || t_hi >= 4u || t_same >= 4u) pass = false;
    if (t_lo == t_hi)                             pass = false;
    if (t_lo != t_same)                           pass = false;

    pal_pwm_deinit(SMOKE_PWM_CH_SAME);

    (void)pass;
    if (!pass) {
        r->note = "freq isolation or reuse violated";
        return WINK_ERR_HARDWARE;
    }
    return WINK_OK;
}
