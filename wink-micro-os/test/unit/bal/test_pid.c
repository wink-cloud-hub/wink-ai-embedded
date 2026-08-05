// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_pid.c
 * @brief BAL PID controller math unit tests.
 */
#include "unity.h"
#include "math/wink_pid.h"
#include <math.h>

void setUp(void) {}
void tearDown(void) {}

void test_pid_basic_p_control(void)
{
    wink_pid_t pid;
    const wink_pid_config_t cfg = {
        .kp = 2.0f, .ki = 0.0f, .kd = 0.0f,
        .min_output = -10.0f, .max_output = 10.0f,
        .min_integral = -10.0f, .max_integral = 10.0f
    };
    wink_pid_init(&pid, &cfg);
    
    // Setpoint = 10, Feedback = 8 -> error = 2
    // Output should be Kp * error = 2.0 * 2 = 4.0
    float out = 0.0f;
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_pid_update(&pid, 10.0f, 8.0f, 0.1f, &out));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 4.0f, out);
}

void test_pid_clamping(void)
{
    wink_pid_t pid;
    const wink_pid_config_t cfg = {
        .kp = 100.0f, .ki = 0.0f, .kd = 0.0f,
        .min_output = -5.0f, .max_output = 5.0f,
        .min_integral = -5.0f, .max_integral = 5.0f
    };
    wink_pid_init(&pid, &cfg);
    
    float out = 0.0f;
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_pid_update(&pid, 10.0f, 0.0f, 0.1f, &out));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 5.0f, out);
}

void test_pid_integral_and_windup(void)
{
    wink_pid_t pid;
    const wink_pid_config_t cfg = {
        .kp = 0.0f, .ki = 1.0f, .kd = 0.0f,
        .min_output = -10.0f, .max_output = 10.0f,
        .min_integral = -10.0f, .max_integral = 10.0f
    };
    wink_pid_init(&pid, &cfg);
    
    // Error = 5, dt = 0.5 -> integral increases by 2.5, output = Ki * 2.5 = 2.5
    float out = 0.0f;
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_pid_update(&pid, 5.0f, 0.0f, 0.5f, &out));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 2.5f, out);
    
    // Repeat to accumulate and exceed limit.
    // Error = 5, dt = 10.0 -> integral increases by 50. Output should be clamped to 10.0.
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_pid_update(&pid, 5.0f, 0.0f, 10.0f, &out));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 10.0f, out);
    
    // Check if internal integral value was clamped to prevent winding up (integral * Ki <= max_integral)
    // Since Ki = 1.0, integral should be capped at 10.0.
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 10.0f, pid.integral);
}

void test_pid_derivative(void)
{
    wink_pid_t pid;
    const wink_pid_config_t cfg = {
        .kp = 0.0f, .ki = 0.0f, .kd = 1.0f,
        .min_output = -100.0f, .max_output = 100.0f,
        .min_integral = -100.0f, .max_integral = 100.0f
    };
    wink_pid_init(&pid, &cfg);
    
    float out = 0.0f;
    // First update: error is 10, feedback is 0. Since it's the first run, D term is 0.
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_pid_update(&pid, 10.0f, 0.0f, 0.1f, &out));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, out);
    
    // Second update: setpoint=10, feedback=2, dt=0.1.
    // Feedback changed from 0 to 2 -> d_feedback/dt = 2 / 0.1 = 20.0.
    // D term = -kd * 20.0 = -20.0.
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_pid_update(&pid, 10.0f, 2.0f, 0.1f, &out));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -20.0f, out);
}

void test_pid_reset(void)
{
    wink_pid_t pid;
    const wink_pid_config_t cfg = {
        .kp = 1.0f, .ki = 1.0f, .kd = 1.0f,
        .min_output = -10.0f, .max_output = 10.0f,
        .min_integral = -10.0f, .max_integral = 10.0f
    };
    wink_pid_init(&pid, &cfg);
    
    // Create some state
    float out = 0.0f;
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_pid_update(&pid, 5.0f, 0.0f, 0.1f, &out));
    
    TEST_ASSERT_NOT_EQUAL(0.0f, pid.integral);
    TEST_ASSERT_FALSE(pid.first_run);
    
    wink_pid_reset(&pid);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, pid.integral);
    TEST_ASSERT_TRUE(pid.first_run);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_pid_basic_p_control);
    RUN_TEST(test_pid_clamping);
    RUN_TEST(test_pid_integral_and_windup);
    RUN_TEST(test_pid_derivative);
    RUN_TEST(test_pid_reset);
    return UNITY_END();
}
