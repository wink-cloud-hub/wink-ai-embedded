#define LOG_TAG "tst_cl_motor"

#include "unity.h"
#include "control/wink_closed_loop_motor.h"
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

extern void host_sim_advance_to(uint64_t us);

#ifndef WINK_CLOSED_LOOP_MOTOR_MAX
#  define WINK_CLOSED_LOOP_MOTOR_MAX 4
#endif

static dal_dc_motor_t s_motor;
static dal_encoder_t s_encoder;

void setUp(void) {
    // Reset slot pool
    extern void wink_closed_loop_motor_reset(void);
    wink_closed_loop_motor_reset();

    // Reset simulator and runtime
    extern void sim_scheduler_reset(uint32_t flags);
    sim_scheduler_reset(0);
    WINK_IGNORE_RESULT(wink_soft_timer_init());
    pal_resource_reset();
    sim_clear_gpio_ideal();
    sim_reset_time();
    wink_trace_reset();

    // Init motor and encoder structures
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
        .pull = PAL_GPIO_INPUT_PULLUP
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_init(&s_encoder, &encoder_cfg));
}

void tearDown(void) {
    WINK_IGNORE_RESULT(dal_dc_motor_deinit(&s_motor));
    WINK_IGNORE_RESULT(dal_encoder_deinit(&s_encoder));
}

static void tick_once(void) {
    uint64_t now = pal_os_get_us();
    host_sim_advance_to(now + 10000u);   /* +10 ms */
    wink_soft_timer_dispatch();
}

static void tick_n(int n) {
    for (int i = 0; i < n; i++) {
        tick_once();
    }
}

void test_cl_motor_invalid_args(void) {
    const wink_closed_loop_motor_config_t cfg = {
        .pid_cfg = { .kp = 1.0f, .ki = 0.0f, .kd = 0.0f, .min_output = -1.0f, .max_output = 1.0f, .min_integral = -1.0f, .max_integral = 1.0f },
        .period_ms = 20,
        .timeout_ms = 200,
        .counts_per_rev = 360.0f
    };
    
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_closed_loop_motor_start(NULL, &s_encoder, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_closed_loop_motor_start(&s_motor, NULL, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_closed_loop_motor_start(&s_motor, &s_encoder, NULL));
    
    // Period cannot be 0
    wink_closed_loop_motor_config_t invalid_cfg = cfg;
    invalid_cfg.period_ms = 0;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_closed_loop_motor_start(&s_motor, &s_encoder, &invalid_cfg));
    
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_closed_loop_motor_stop(NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_closed_loop_motor_set_speed(NULL, 10.0f));
}

void test_cl_motor_lifecycle(void) {
    const wink_closed_loop_motor_config_t cfg = {
        .pid_cfg = { .kp = 1.0f, .ki = 0.0f, .kd = 0.0f, .min_output = -1.0f, .max_output = 1.0f, .min_integral = -1.0f, .max_integral = 1.0f },
        .period_ms = 20,
        .timeout_ms = 200,
        .counts_per_rev = 360.0f
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_closed_loop_motor_start(&s_motor, &s_encoder, &cfg));
    
    // Duplicate start should be rejected
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_STATE, wink_closed_loop_motor_start(&s_motor, &s_encoder, &cfg));

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_closed_loop_motor_set_speed(&s_motor, 50.0f));

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_closed_loop_motor_stop(&s_motor));
}

void test_cl_motor_pid_control_loop(void) {
    const wink_closed_loop_motor_config_t cfg = {
        .pid_cfg = { .kp = 0.01f, .ki = 0.0f, .kd = 0.0f, .min_output = -1.0f, .max_output = 1.0f, .min_integral = -1.0f, .max_integral = 1.0f },
        .period_ms = 20,
        .timeout_ms = 500,
        .counts_per_rev = 360.0f
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_closed_loop_motor_start(&s_motor, &s_encoder, &cfg));

    // Target speed = 100 ticks/sec, Feedback speed = 0.
    // Error = 100 -> PID Output = kp * error = 0.01 * 100 = 1.0f.
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_closed_loop_motor_set_speed(&s_motor, 100.0f));

    // Run for 2 ticks (20 ms) to trigger the control loop calculation
    tick_n(2);

    // Motor current speed should be adjusted to positive output
    TEST_ASSERT_TRUE(s_motor.current_speed > 0.0f);

    // Target speed = -100 ticks/sec, Feedback speed = 0.
    // Error = -100 -> PID Output = -1.0f.
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_closed_loop_motor_set_speed(&s_motor, -100.0f));
    tick_n(2);
    TEST_ASSERT_TRUE(s_motor.current_speed < 0.0f);

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_closed_loop_motor_stop(&s_motor));
}

void test_cl_motor_failsafe_timeout(void) {
    const wink_closed_loop_motor_config_t cfg = {
        .pid_cfg = { .kp = 0.01f, .ki = 0.0f, .kd = 0.0f, .min_output = -1.0f, .max_output = 1.0f, .min_integral = -1.0f, .max_integral = 1.0f },
        .period_ms = 20,
        .timeout_ms = 100, // short timeout for testing
        .counts_per_rev = 360.0f
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_closed_loop_motor_start(&s_motor, &s_encoder, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_closed_loop_motor_set_speed(&s_motor, 100.0f));

    // Run control loop once: output becomes positive
    tick_n(2);
    TEST_ASSERT_TRUE(s_motor.current_speed > 0.0f);

    // Advance time by 150 ms without any encoder ticks changing
    tick_n(15);

    // Motor should be safe-off (speed = 0)
    TEST_ASSERT_EQUAL_FLOAT(0.0f, s_motor.current_speed);

    // Check if the fault WINK_FAULT_MOTOR_FEEDBACK_LOSS was raised
    TEST_ASSERT_EQUAL_UINT32(WINK_FAULT_MOTOR_FEEDBACK_LOSS, wink_trace_last());

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_closed_loop_motor_stop(&s_motor));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cl_motor_invalid_args);
    RUN_TEST(test_cl_motor_lifecycle);
    RUN_TEST(test_cl_motor_pid_control_loop);
    RUN_TEST(test_cl_motor_failsafe_timeout);
    return UNITY_END();
}
