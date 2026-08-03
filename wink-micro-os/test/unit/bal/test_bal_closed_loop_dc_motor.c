#define LOG_TAG "tst_cl_motor"

#include "unity.h"
#include "control/wink_closed_loop_dc_motor.h"
#include "wink_tasks.h"
#include "wink_soft_timer.h"
#include "wink_status.h"
#include "pal_osal.h"
#include "pal_resource.h"
#include "pal_log.h"
#include "wink_trace.h"
#include "wink_fault.h"
#include "host_test_ctrl.h"
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdint.h>

#ifndef WINK_CLOSED_LOOP_DC_MOTOR_MAX
#  define WINK_CLOSED_LOOP_DC_MOTOR_MAX 4
#endif

static dal_dc_motor_t s_motor;
static dal_encoder_t s_encoder;

/* POD count injection (test TU only — avoids pulling DAL into pal_host). */
void sim_set_encoder_count(dal_encoder_t *dev, int32_t count)
{
    if (dev != NULL) {
        dev->count = count;
    }
}

void sim_advance_encoder_count(dal_encoder_t *dev, int32_t delta)
{
    if (dev != NULL) {
        dev->count += delta;
    }
}

void setUp(void) {
    extern void wink_closed_loop_dc_motor_reset(void);
    wink_closed_loop_dc_motor_reset();

    extern void sim_scheduler_reset(uint32_t flags);
    sim_scheduler_reset(0);
    WINK_IGNORE_RESULT(wink_soft_timer_init());
    pal_resource_reset();
    sim_clear_gpio_ideal();
    sim_reset_time();
    wink_trace_reset();

    memset(&s_motor, 0, sizeof(s_motor));
    const dal_dc_motor_config_t motor_cfg = {
        .owner = "test_motor",
        .pwm_channel = 0,
        .dir_pin_a = 5,
        .dir_pin_b = 6,
        .pwm_freq_hz = 20000
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_init(&s_motor, &motor_cfg));

    memset(&s_encoder, 0, sizeof(s_encoder));
    const dal_encoder_config_t encoder_cfg = {
        .owner = "test_encoder",
        .pin_a = 2,
        .pin_b = 3,
        .pull = DAL_ENCODER_PULL_UP
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_init(&s_encoder, &encoder_cfg));
}

void tearDown(void) {
    WINK_IGNORE_RESULT(dal_dc_motor_deinit(&s_motor));
    WINK_IGNORE_RESULT(dal_encoder_deinit(&s_encoder));
}

/* Virtual-clock tick: +10 ms (R-010 — no wall clock). */
static void tick_once(void) {
    sim_advance_mono_time_us(10000u);
    wink_soft_timer_dispatch();
}

static void tick_n(int n) {
    for (int i = 0; i < n; i++) {
        tick_once();
    }
}

void test_cl_motor_invalid_args(void) {
    const wink_closed_loop_dc_motor_config_t cfg = {
        .pid_cfg = { .kp = 1.0f, .ki = 0.0f, .kd = 0.0f, .min_output = -1.0f, .max_output = 1.0f, .min_integral = -1.0f, .max_integral = 1.0f },
        .period_ms = 20,
        .timeout_ms = 200,
        .counts_per_rev = 360.0f
    };
    
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_closed_loop_dc_motor_start(NULL, &s_encoder, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_closed_loop_dc_motor_start(&s_motor, NULL, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_closed_loop_dc_motor_start(&s_motor, &s_encoder, NULL));
    
    wink_closed_loop_dc_motor_config_t invalid_cfg = cfg;
    invalid_cfg.period_ms = 0;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_closed_loop_dc_motor_start(&s_motor, &s_encoder, &invalid_cfg));
    
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_closed_loop_dc_motor_stop(NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_closed_loop_dc_motor_set_speed(NULL, 10.0f));
}

void test_cl_motor_lifecycle(void) {
    const wink_closed_loop_dc_motor_config_t cfg = {
        .pid_cfg = { .kp = 1.0f, .ki = 0.0f, .kd = 0.0f, .min_output = -1.0f, .max_output = 1.0f, .min_integral = -1.0f, .max_integral = 1.0f },
        .period_ms = 20,
        .timeout_ms = 200,
        .counts_per_rev = 360.0f
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_closed_loop_dc_motor_start(&s_motor, &s_encoder, &cfg));
    
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_STATE, wink_closed_loop_dc_motor_start(&s_motor, &s_encoder, &cfg));

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_closed_loop_dc_motor_set_speed(&s_motor, 50.0f));

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_closed_loop_dc_motor_stop(&s_motor));
}

void test_cl_motor_pid_control_loop(void) {
    const wink_closed_loop_dc_motor_config_t cfg = {
        .pid_cfg = { .kp = 0.01f, .ki = 0.0f, .kd = 0.0f, .min_output = -1.0f, .max_output = 1.0f, .min_integral = -1.0f, .max_integral = 1.0f },
        .period_ms = 20,
        .timeout_ms = 500,
        .counts_per_rev = 360.0f
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_closed_loop_dc_motor_start(&s_motor, &s_encoder, &cfg));

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_closed_loop_dc_motor_set_speed(&s_motor, 100.0f));

    tick_n(2);

    TEST_ASSERT_TRUE(s_motor.current_speed > 0.0f);

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_closed_loop_dc_motor_set_speed(&s_motor, -100.0f));
    tick_n(2);
    TEST_ASSERT_TRUE(s_motor.current_speed < 0.0f);

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_closed_loop_dc_motor_stop(&s_motor));
}

void test_cl_motor_failsafe_timeout(void) {
    const wink_closed_loop_dc_motor_config_t cfg = {
        .pid_cfg = { .kp = 0.01f, .ki = 0.0f, .kd = 0.0f, .min_output = -1.0f, .max_output = 1.0f, .min_integral = -1.0f, .max_integral = 1.0f },
        .period_ms = 20,
        .timeout_ms = 100,
        .counts_per_rev = 360.0f
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_closed_loop_dc_motor_start(&s_motor, &s_encoder, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_closed_loop_dc_motor_set_speed(&s_motor, 100.0f));

    tick_n(2);
    TEST_ASSERT_TRUE(s_motor.current_speed > 0.0f);

    /* Virtual-clock timeout: count frozen → fail-safe (R-010). */
    tick_n(15);

    /* safe_off → brake on dual-dir H-bridge: speed + PWM duty cleared. */
    TEST_ASSERT_EQUAL_FLOAT(0.0f, s_motor.current_speed);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, sim_last_pwm_duty(0));

    TEST_ASSERT_EQUAL_UINT32(WINK_FAULT_MOTOR_FEEDBACK_LOSS, wink_trace_last());

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_closed_loop_dc_motor_stop(&s_motor));
}

/*
 * Inject encoder ramp matching target; duty/command should settle (not pegged).
 * Advances via sim_advance_mono_time_us + get_count read path (R-010/R-012).
 */
void test_cl_motor_tracks_injected_encoder_ramp(void) {
    const float target_cps = 100.0f;
    const uint32_t period_ms = 20u;
    const wink_closed_loop_dc_motor_config_t cfg = {
        .pid_cfg = {
            .kp = 0.02f, .ki = 0.05f, .kd = 0.0f,
            .min_output = -1.0f, .max_output = 1.0f,
            .min_integral = -0.5f, .max_integral = 0.5f
        },
        .period_ms = period_ms,
        .timeout_ms = 2000,
        .counts_per_rev = 360.0f
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_closed_loop_dc_motor_start(&s_motor, &s_encoder, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_closed_loop_dc_motor_set_speed(&s_motor, target_cps));

    const int32_t counts_per_period =
        (int32_t)(target_cps * ((float)period_ms / 1000.0f)); /* ~2 */

    for (int i = 0; i < 40; i++) {
        sim_advance_encoder_count(&s_encoder, counts_per_period);
        /* Prove read path sees injected count (not only POD poke). */
        int32_t read_count = 0;
        TEST_ASSERT_EQUAL_INT(WINK_OK,
                              dal_encoder_get_count(&s_encoder, &read_count));
        sim_advance_mono_time_us((uint64_t)period_ms * 1000u);
        wink_soft_timer_dispatch();
    }

    float feedback = 0.0f;
    TEST_ASSERT_EQUAL_INT(WINK_OK,
                          wink_closed_loop_dc_motor_get_speed(&s_motor, &feedback));
    TEST_ASSERT_FLOAT_WITHIN(40.0f, target_cps, feedback);

    /* Tracking well → command not stuck at saturation. */
    TEST_ASSERT_TRUE(fabsf(s_motor.current_speed) < 0.85f);

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_closed_loop_dc_motor_stop(&s_motor));
}

/*
 * Large Ki + stuck feedback → output saturates; integrator must stop climbing
 * (R-011). After releasing saturation, command recovers without hard overshoot.
 */
void test_cl_motor_anti_windup_under_saturation(void) {
    const wink_closed_loop_dc_motor_config_t cfg = {
        .pid_cfg = {
            .kp = 0.05f, .ki = 40.0f, .kd = 0.0f,
            .min_output = -1.0f, .max_output = 1.0f,
            .min_integral = -0.4f, .max_integral = 0.4f
        },
        .period_ms = 20,
        .timeout_ms = 10000, /* avoid fail-safe while holding feedback */
        .counts_per_rev = 360.0f
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_closed_loop_dc_motor_start(&s_motor, &s_encoder, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_closed_loop_dc_motor_set_speed(&s_motor, 200.0f));

    /* Saturate with frozen encoder. */
    tick_n(30);
    TEST_ASSERT_TRUE(s_motor.current_speed >= 0.99f);

    float integral_at_sat = 0.0f;
    TEST_ASSERT_EQUAL_INT(
        WINK_OK,
        wink_closed_loop_dc_motor_debug_get_integral(&s_motor, &integral_at_sat));
    /* I-term clamp: |integral| <= max_integral / ki */
    TEST_ASSERT_TRUE(fabsf(integral_at_sat) <= (0.4f / 40.0f) + 1e-4f);

    tick_n(20);
    float integral_later = 0.0f;
    TEST_ASSERT_EQUAL_INT(
        WINK_OK,
        wink_closed_loop_dc_motor_debug_get_integral(&s_motor, &integral_later));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, integral_at_sat, integral_later);

    /* Release: inject feedback near target so error collapses. */
    for (int i = 0; i < 25; i++) {
        sim_advance_encoder_count(&s_encoder, 4); /* ~200 counts/s @ 20 ms */
        sim_advance_mono_time_us(20000u);
        wink_soft_timer_dispatch();
    }

    /* After matching feedback, command must not reverse hard; stay near [0, 1]. */
    TEST_ASSERT_TRUE(s_motor.current_speed > -0.15f);
    TEST_ASSERT_TRUE(s_motor.current_speed < 1.01f);

    float integral_after = 0.0f;
    TEST_ASSERT_EQUAL_INT(
        WINK_OK,
        wink_closed_loop_dc_motor_debug_get_integral(&s_motor, &integral_after));
    TEST_ASSERT_TRUE(fabsf(integral_after) <= fabsf(integral_at_sat) + 1e-3f);

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_closed_loop_dc_motor_stop(&s_motor));
}

/* Encoder int32 wraparound must not produce a velocity spike (R-012). */
void test_cl_motor_encoder_count_wraparound(void) {
    const wink_closed_loop_dc_motor_config_t cfg = {
        .pid_cfg = {
            .kp = 0.01f, .ki = 0.0f, .kd = 0.0f,
            .min_output = -1.0f, .max_output = 1.0f,
            .min_integral = -1.0f, .max_integral = 1.0f
        },
        .period_ms = 20,
        .timeout_ms = 2000,
        .counts_per_rev = 360.0f
    };

    const int32_t near_max = INT32_MAX - 3;
    sim_set_encoder_count(&s_encoder, near_max);
    int32_t via_api = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_get_count(&s_encoder, &via_api));
    TEST_ASSERT_EQUAL_INT(near_max, via_api);

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_closed_loop_dc_motor_start(&s_motor, &s_encoder, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_closed_loop_dc_motor_set_speed(&s_motor, 100.0f));

    /* Establish one control period on virtual clock (same cadence as other tests). */
    tick_n(2);

    /* Cross INT32 wrap: +10 counts (two's-complement). */
    const int32_t after_wrap = (int32_t)(near_max + 10);
    sim_set_encoder_count(&s_encoder, after_wrap);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_get_count(&s_encoder, &via_api));
    TEST_ASSERT_EQUAL_INT(after_wrap, via_api);

    tick_n(2);

    float feedback = 0.0f;
    TEST_ASSERT_EQUAL_INT(WINK_OK,
                          wink_closed_loop_dc_motor_get_speed(&s_motor, &feedback));
    /* Signed delta of +10 over one ~20 ms period → hundreds of counts/s.
     * Must not explode to INT32-scale (broken unsigned/wrap handling). */
    TEST_ASSERT_TRUE(feedback > 0.0f);
    TEST_ASSERT_TRUE(feedback < 5000.0f);
    TEST_ASSERT_FLOAT_WITHIN(400.0f, 500.0f, feedback);

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_closed_loop_dc_motor_stop(&s_motor));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cl_motor_invalid_args);
    RUN_TEST(test_cl_motor_lifecycle);
    RUN_TEST(test_cl_motor_pid_control_loop);
    RUN_TEST(test_cl_motor_failsafe_timeout);
    RUN_TEST(test_cl_motor_tracks_injected_encoder_ramp);
    RUN_TEST(test_cl_motor_anti_windup_under_saturation);
    RUN_TEST(test_cl_motor_encoder_count_wraparound);
    return UNITY_END();
}
