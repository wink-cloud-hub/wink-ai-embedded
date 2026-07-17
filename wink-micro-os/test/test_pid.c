#include "unity.h"
#include "wink_pid.h"
#include <math.h>

void setUp(void) {}
void tearDown(void) {}

void test_pid_basic_p_control(void)
{
    wink_pid_t pid;
    // Only P term: Kp = 2.0, Ki = 0.0, Kd = 0.0
    wink_pid_init(&pid, 2.0f, 0.0f, 0.0f, -10.0f, 10.0f);
    
    // Setpoint = 10, Feedback = 8 -> error = 2
    // Output should be Kp * error = 2.0 * 2 = 4.0
    float out = wink_pid_update(&pid, 10.0f, 8.0f, 0.1f);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 4.0f, out);
}

void test_pid_clamping(void)
{
    wink_pid_t pid;
    // Kp = 100.0 (high enough to exceed clamp)
    wink_pid_init(&pid, 100.0f, 0.0f, 0.0f, -5.0f, 5.0f);
    
    float out = wink_pid_update(&pid, 10.0f, 0.0f, 0.1f);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 5.0f, out);
}

void test_pid_integral_and_windup(void)
{
    wink_pid_t pid;
    // Kp = 0.0, Ki = 1.0, Kd = 0.0. Clamps: -10.0 to 10.0
    wink_pid_init(&pid, 0.0f, 1.0f, 0.0f, -10.0f, 10.0f);
    
    // Error = 5, dt = 0.5 -> integral increases by 2.5, output = Ki * 2.5 = 2.5
    float out = wink_pid_update(&pid, 5.0f, 0.0f, 0.5f);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 2.5f, out);
    
    // Repeat to accumulate and exceed limit.
    // Error = 5, dt = 10.0 -> integral increases by 50. Output should be clamped to 10.0.
    out = wink_pid_update(&pid, 5.0f, 0.0f, 10.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 10.0f, out);
    
    // Check if internal integral value was clamped to prevent winding up (integral * Ki <= max_integral)
    // Since Ki = 1.0, integral should be capped at 10.0.
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 10.0f, pid.integral);
}

void test_pid_derivative(void)
{
    wink_pid_t pid;
    // Kp = 0.0, Ki = 0.0, Kd = 1.0
    wink_pid_init(&pid, 0.0f, 0.0f, 1.0f, -100.0f, 100.0f);
    
    // First update: error changes from 0 to 10 (dt = 0.1) -> D term = 1.0 * (10 - 0) / 0.1 = 100.0
    float out = wink_pid_update(&pid, 10.0f, 0.0f, 0.1f);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 100.0f, out);
    
    // Second update: error goes from 10 to 5 (dt = 0.1) -> D term = 1.0 * (5 - 10) / 0.1 = -50.0
    out = wink_pid_update(&pid, 5.0f, 0.0f, 0.1f);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -50.0f, out);
}

void test_pid_reset(void)
{
    wink_pid_t pid;
    wink_pid_init(&pid, 1.0f, 1.0f, 1.0f, -10.0f, 10.0f);
    
    // Create some state
    wink_pid_update(&pid, 5.0f, 0.0f, 0.1f);
    
    TEST_ASSERT_NOT_EQUAL(0.0f, pid.integral);
    TEST_ASSERT_NOT_EQUAL(0.0f, pid.prev_error);
    
    wink_pid_reset(&pid);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, pid.integral);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, pid.prev_error);
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
