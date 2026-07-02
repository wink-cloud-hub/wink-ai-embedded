/**
 * @file test_single_task_semantic_regression.c
 * @brief fixup 计划 F5 Step 3 —— avoidance_car 单任务业务字段 baseline 回归。
 *
 * 意图（R-002 缓解措施）：以协作式调度器接入后，avoidance_car 的业务行为
 * (servo.current_angle / wink_trace_count) 相较于协作式调度器接入前不应有回归。
 *
 * 白名单字段（R-002）：只比业务字段，绝不比 tick 数 / 时间戳 / fiber 地址。
 *   1. neck_servo.current_angle 在 clear-obstacle 场景应为 90.0f；
 *   2. neck_servo.current_angle 在 near-obstacle 场景应为 180.0f；
 *   3. wink_trace_count() == 0（无 fault）。
 *
 * baseline 由 test/baseline/avoidance_car_semantic_baseline.h 提供，
 * 缺失即编译失败（fixup 计划 F5 R5 CMake 严格模式）。
 */

#include "wink_runtime.h"
#include "wink_trace.h"
#include "dal_servo.h"
#include "device_tree.h"
#include "host_test_ctrl.h"
#include "unity.h"

/* R5 —— baseline 缺失即编译失败，禁止 CI 静默 auto-generate */
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

    /* Case 1：clear obstacle → servo 90° */
    sim_reset_time();
    sim_set_echo_pin(front_radar.config.echo_pin);
    sim_set_echo_timing(100, 5882);   /* ≈100cm */
    wink_status_t s1 = wink_runtime_run(cb, 1);
    (void)s1;
    TEST_ASSERT_EQUAL_FLOAT(AVOIDANCE_CAR_BASELINE_SERVO_ANGLE_CLEAR,
                            neck_servo.current_angle);

    /* Case 2：near obstacle → servo 180° */
    sim_reset_time();
    sim_set_echo_pin(front_radar.config.echo_pin);
    sim_set_echo_timing(100, 588);    /* ≈10cm */
    wink_status_t s2 = wink_runtime_run(cb, 1);
    (void)s2;
    TEST_ASSERT_EQUAL_FLOAT(AVOIDANCE_CAR_BASELINE_SERVO_ANGLE_NEAR,
                            neck_servo.current_angle);

    /* Case 3：no fault recorded during the run */
    TEST_ASSERT_EQUAL_UINT32(AVOIDANCE_CAR_BASELINE_TRACE_COUNT,
                             wink_trace_count());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_avoidance_car_business_fields_match_baseline);
    return UNITY_END();
}
