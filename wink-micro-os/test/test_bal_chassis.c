/**
 * @file test_bal_chassis.c
 * @brief Unit tests for BAL chassis + diff drive kinematics (host).
 *
 * Covers:
 *   1. Diff-drive kinematics inverse/forward correctness
 *   2. Speed ↔ counts unit conversion round-trip
 *   3. Chassis lifecycle (start → set_velocity → stop)
 *   4. Argument validation
 *   5. Pool exhaustion
 */
#define LOG_TAG "tst_chassis"

#include "unity.h"
#include "control/wink_chassis.h"
#include "math/wink_diff_drive_kinematics.h"
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

#ifndef M_PI
#  define M_PI 3.14159265358979323846f
#endif

extern void host_sim_advance_to(uint64_t us);

static dal_motor_t   s_left_motor;
static dal_encoder_t s_left_encoder;
static dal_motor_t   s_right_motor;
static dal_encoder_t s_right_encoder;

void setUp(void) {
    extern void wink_chassis_reset(void);
    extern void wink_closed_loop_motor_reset(void);
    wink_chassis_reset();
    wink_closed_loop_motor_reset();

    extern void sim_scheduler_reset(uint32_t flags);
    sim_scheduler_reset(0);
    WINK_IGNORE_RESULT(wink_soft_timer_init());
    pal_resource_reset();
    sim_clear_gpio_ideal();
    sim_reset_time();
    wink_trace_reset();

    memset(&s_left_motor, 0, sizeof(s_left_motor));
    const dal_motor_config_t lm_cfg = {
        .owner = "left_motor", .pwm_channel = 0,
        .dir_pin_a = 5, .dir_pin_b = 6, .pwm_freq_hz = 20000
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_motor_init(&s_left_motor, &lm_cfg));

    memset(&s_left_encoder, 0, sizeof(s_left_encoder));
    const dal_encoder_config_t le_cfg = {
        .owner = "left_enc", .pin_a = 2, .pin_b = 3,
        .pull = PAL_GPIO_INPUT_PULLUP
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_init(&s_left_encoder, &le_cfg));

    memset(&s_right_motor, 0, sizeof(s_right_motor));
    const dal_motor_config_t rm_cfg = {
        .owner = "right_motor", .pwm_channel = 1,
        .dir_pin_a = 7, .dir_pin_b = 8, .pwm_freq_hz = 20000
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_motor_init(&s_right_motor, &rm_cfg));

    memset(&s_right_encoder, 0, sizeof(s_right_encoder));
    const dal_encoder_config_t re_cfg = {
        .owner = "right_enc", .pin_a = 10, .pin_b = 11,
        .pull = PAL_GPIO_INPUT_PULLUP
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_init(&s_right_encoder, &re_cfg));
}

void tearDown(void) {
    WINK_IGNORE_RESULT(dal_motor_deinit(&s_left_motor));
    WINK_IGNORE_RESULT(dal_encoder_deinit(&s_left_encoder));
    WINK_IGNORE_RESULT(dal_motor_deinit(&s_right_motor));
    WINK_IGNORE_RESULT(dal_encoder_deinit(&s_right_encoder));
}

void test_kinematics_inverse_straight(void) {
    wink_diff_drive_params_t p = { .wheel_base = 0.2f, .wheel_radius = 0.03f, .counts_per_rev = 360.0f };
    float vl = 0.0f, vr = 0.0f;
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_diff_drive_to_wheel_speeds(&p, 0.5f, 0.0f, &vl, &vr));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.5f, vl);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.5f, vr);
}

void test_kinematics_inverse_pure_rotation(void) {
    wink_diff_drive_params_t p = { .wheel_base = 0.2f, .wheel_radius = 0.03f, .counts_per_rev = 360.0f };
    float vl = 0.0f, vr = 0.0f;
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_diff_drive_to_wheel_speeds(&p, 0.0f, 1.0f, &vl, &vr));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -0.1f, vl);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f,  0.1f, vr);
}

void test_kinematics_forward_round_trip(void) {
    wink_diff_drive_params_t p = { .wheel_base = 0.15f, .wheel_radius = 0.033f, .counts_per_rev = 360.0f };
    float vl = 0.0f, vr = 0.0f;
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_diff_drive_to_wheel_speeds(&p, 0.3f, 0.5f, &vl, &vr));

    float v2 = 0.0f, w2 = 0.0f;
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_diff_drive_to_chassis_speeds(&p, vl, vr, &v2, &w2));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.3f, v2);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.5f, w2);
}

void test_kinematics_speed_counts_round_trip(void) {
    wink_diff_drive_params_t p = { .wheel_base = 0.2f, .wheel_radius = 0.03f, .counts_per_rev = 360.0f };
    float speed_in = 0.5f;
    float counts = wink_diff_drive_speed_to_counts(&p, speed_in);
    float speed_out = wink_diff_drive_counts_to_speed(&p, counts);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, speed_in, speed_out);
}

void test_kinematics_invalid_args(void) {
    float vl, vr, v, w;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_diff_drive_to_wheel_speeds(NULL, 0, 0, &vl, &vr));

    wink_diff_drive_params_t bad = { .wheel_base = 0.0f, .wheel_radius = 0.03f, .counts_per_rev = 360.0f };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_diff_drive_to_wheel_speeds(&bad, 0, 0, &vl, &vr));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_diff_drive_to_chassis_speeds(&bad, 0, 0, &v, &w));
}

static wink_chassis_config_t make_default_cfg(void) {
    wink_pid_config_t pid = {
        .kp = 0.01f, .ki = 0.0f, .kd = 0.0f,
        .min_output = -1.0f, .max_output = 1.0f,
        .min_integral = -1.0f, .max_integral = 1.0f
    };
    wink_chassis_config_t cfg = {
        .kinematics_params = { .wheel_base = 0.2f, .wheel_radius = 0.03f, .counts_per_rev = 360.0f },
        .left_motor_cfg = { .pid_cfg = pid, .period_ms = 20, .timeout_ms = 500, .counts_per_rev = 360.0f },
        .right_motor_cfg = { .pid_cfg = pid, .period_ms = 20, .timeout_ms = 500, .counts_per_rev = 360.0f },
    };
    return cfg;
}

void test_chassis_invalid_args(void) {
    wink_chassis_config_t cfg = make_default_cfg();
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
        wink_chassis_start(NULL, &s_left_encoder, &s_right_motor, &s_right_encoder, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
        wink_chassis_start(&s_left_motor, &s_left_encoder, NULL, &s_right_encoder, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
        wink_chassis_start(&s_left_motor, &s_left_encoder, &s_right_motor, &s_right_encoder, NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_chassis_stop(NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_chassis_set_velocity(NULL, 0, 0));
}

void test_chassis_lifecycle(void) {
    wink_chassis_config_t cfg = make_default_cfg();

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        wink_chassis_start(&s_left_motor, &s_left_encoder,
                           &s_right_motor, &s_right_encoder, &cfg));

    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_STATE,
        wink_chassis_start(&s_left_motor, &s_left_encoder,
                           &s_right_motor, &s_right_encoder, &cfg));

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_chassis_set_velocity(&s_left_motor, 0.3f, 0.0f));

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_chassis_stop(&s_left_motor));

    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_STATE, wink_chassis_set_velocity(&s_left_motor, 0.1f, 0.0f));
}

void test_chassis_set_velocity_drives_motors(void) {
    wink_chassis_config_t cfg = make_default_cfg();

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        wink_chassis_start(&s_left_motor, &s_left_encoder,
                           &s_right_motor, &s_right_encoder, &cfg));

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_chassis_set_velocity(&s_left_motor, 0.3f, 0.0f));

    for (int i = 0; i < 3; i++) {
        uint64_t now = pal_os_get_us();
        host_sim_advance_to(now + 10000u);
        wink_soft_timer_dispatch();
    }

    TEST_ASSERT_TRUE(s_left_motor.current_speed > 0.0f);
    TEST_ASSERT_TRUE(s_right_motor.current_speed > 0.0f);

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_chassis_stop(&s_left_motor));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_kinematics_inverse_straight);
    RUN_TEST(test_kinematics_inverse_pure_rotation);
    RUN_TEST(test_kinematics_forward_round_trip);
    RUN_TEST(test_kinematics_speed_counts_round_trip);
    RUN_TEST(test_kinematics_invalid_args);
    RUN_TEST(test_chassis_invalid_args);
    RUN_TEST(test_chassis_lifecycle);
    RUN_TEST(test_chassis_set_velocity_drives_motors);
    return UNITY_END();
}
