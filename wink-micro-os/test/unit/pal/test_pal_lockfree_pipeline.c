// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_pal_lockfree_pipeline.c
 * @brief PAL Lock-free double-buffered pipeline unit tests.
 */
#include "unity.h"
#include "osal/pal_lockfree_pipeline.h"

void setUp(void) {}
void tearDown(void) {}

void test_pipeline_init_and_publish_consume(void) {
    foc_pipeline_t pipe;
    foc_pipeline_init(&pipe);

    foc_slow_to_fast_cmd_t cmd1 = {
        .target_speed_q15 = 1200,
        .target_angle_q15 = 450,
        .seq_id = 1,
    };
    foc_publish_cmd(&pipe, &cmd1);

    foc_slow_to_fast_cmd_t rx1 = foc_consume_cmd(&pipe);
    TEST_ASSERT_EQUAL_INT16(1200, rx1.target_speed_q15);
    TEST_ASSERT_EQUAL_INT16(450, rx1.target_angle_q15);
    TEST_ASSERT_EQUAL_UINT32(1, rx1.seq_id);

    foc_slow_to_fast_cmd_t cmd2 = {
        .target_speed_q15 = -800,
        .target_angle_q15 = 900,
        .seq_id = 2,
    };
    foc_publish_cmd(&pipe, &cmd2);

    foc_slow_to_fast_cmd_t rx2 = foc_consume_cmd(&pipe);
    TEST_ASSERT_EQUAL_INT16(-800, rx2.target_speed_q15);
    TEST_ASSERT_EQUAL_INT16(900, rx2.target_angle_q15);
    TEST_ASSERT_EQUAL_UINT32(2, rx2.seq_id);
}

void test_pipeline_status_publish_consume(void) {
    foc_pipeline_t pipe;
    foc_pipeline_init(&pipe);

    foc_fast_to_slow_status_t st1 = {
        .actual_current_q15 = 350,
        .actual_velocity_q15 = 1190,
        .fault_flags = 0x0004,
        .seq_id = 100,
    };
    foc_publish_status(&pipe, &st1);

    foc_fast_to_slow_status_t rx_st = foc_consume_status(&pipe);
    TEST_ASSERT_EQUAL_INT16(350, rx_st.actual_current_q15);
    TEST_ASSERT_EQUAL_INT16(1190, rx_st.actual_velocity_q15);
    TEST_ASSERT_EQUAL_UINT16(0x0004, rx_st.fault_flags);
    TEST_ASSERT_EQUAL_UINT32(100, rx_st.seq_id);
}

void test_pipeline_stress_torn_read_immunity(void) {
    foc_pipeline_t pipe;
    foc_pipeline_init(&pipe);

    for (uint32_t i = 1; i <= 10000; i++) {
        foc_slow_to_fast_cmd_t cmd = {
            .target_speed_q15 = (q15_t)i,
            .target_angle_q15 = (q15_t)(-((int32_t)i)),
            .seq_id = i,
        };
        foc_publish_cmd(&pipe, &cmd);

        foc_slow_to_fast_cmd_t rx = foc_consume_cmd(&pipe);
        TEST_ASSERT_EQUAL_INT16(-rx.target_speed_q15, rx.target_angle_q15);
        TEST_ASSERT_EQUAL_UINT32(i, rx.seq_id);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_pipeline_init_and_publish_consume);
    RUN_TEST(test_pipeline_status_publish_consume);
    RUN_TEST(test_pipeline_stress_torn_read_immunity);
    return UNITY_END();
}
