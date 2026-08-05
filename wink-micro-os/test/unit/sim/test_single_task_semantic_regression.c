// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_single_task_semantic_regression.c
 * @brief Single-task business field semantic regression unit tests.
 */
#include "wink_runtime.h"
#include "wink_trace.h"
#include "dal_rc_servo.h"
#include "device_tree.h"
#include "host_test_ctrl.h"
#include "unity.h"

#ifndef HAS_BASELINE_HEADER
#  error "avoidance_car_semantic_baseline.h missing — build with -DHAS_BASELINE_HEADER=1 " \
         "after human-review of the diff (see fixup plan F5 R5)."
#endif
#include "baseline/avoidance_car_semantic_baseline.h"

extern const wink_app_callbacks_t *wink_app_get_callbacks(void);

void setUp(void) {}
void tearDown(void) {}

void test_avoidance_car_business_fields_match_baseline(void) {
    const wink_app_callbacks_t *cb = wink_app_get_callbacks();

    sim_reset_time();
    sim_set_echo_pin(front_radar.config.echo_pin);
    sim_set_echo_timing(100, 5882);
    wink_status_t s1 = wink_runtime_run(cb, 1);
    (void)s1;
    TEST_ASSERT_EQUAL_UINT16(AVOIDANCE_CAR_BASELINE_SERVO_ANGLE_CLEAR,
                            neck_servo.current_angle_ddeg);

    sim_reset_time();
    sim_set_echo_pin(front_radar.config.echo_pin);
    sim_set_echo_timing(100, 588);
    wink_status_t s2 = wink_runtime_run(cb, 1);
    (void)s2;
    TEST_ASSERT_EQUAL_UINT16(AVOIDANCE_CAR_BASELINE_SERVO_ANGLE_NEAR,
                            neck_servo.current_angle_ddeg);

    TEST_ASSERT_EQUAL_UINT32(AVOIDANCE_CAR_BASELINE_TRACE_COUNT,
                             wink_trace_count());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_avoidance_car_business_fields_match_baseline);
    return UNITY_END();
}
