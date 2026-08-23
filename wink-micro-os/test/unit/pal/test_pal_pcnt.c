// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_pal_pcnt.c
 * @brief PAL PCNT pulse counter and quadrature decoding unit tests.
 */
#include "unity.h"
#include "hal/pal_pcnt.h"
#include "pal_resource.h"
#include "pal_pcnt_stub.h"

void setUp(void) {
    pal_resource_reset();
}

void tearDown(void) {
    pal_resource_reset();
}

void test_pcnt_init_deinit(void) {
    pal_pcnt_config_t cfg = {
        .pin_a = 18,
        .pin_b = 19,
        .mode = PAL_PCNT_MODE_4X,
        .low_limit = -32768,
        .high_limit = 32767,
        .filter_ns = 1000,
    };

    pal_pcnt_unit_handle_t handles[PAL_PCNT_UNIT_MAX];
    for (uint8_t i = 0; i < PAL_PCNT_UNIT_MAX; i++) {
        cfg.pin_a = (wink_pin_t)(10 + i * 2);
        cfg.pin_b = (wink_pin_t)(11 + i * 2);
        TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pcnt_init(&cfg, &handles[i]));
        TEST_ASSERT_NOT_NULL(handles[i]);
    }

    /* 9th unit must return RESOURCE_EXHAUSTED */
    pal_pcnt_unit_handle_t extra_handle = NULL;
    cfg.pin_a = 30;
    cfg.pin_b = 31;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_RESOURCE_EXHAUSTED, pal_pcnt_init(&cfg, &extra_handle));

    /* Deinit unit 0 and re-init */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pcnt_deinit(handles[0]));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pcnt_init(&cfg, &handles[0]));
    TEST_ASSERT_NOT_NULL(handles[0]);

    for (uint8_t i = 0; i < PAL_PCNT_UNIT_MAX; i++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pcnt_deinit(handles[i]));
    }
}

void test_pcnt_count_and_clear(void) {
    pal_pcnt_config_t cfg = {
        .pin_a = 18,
        .pin_b = 19,
        .mode = PAL_PCNT_MODE_4X,
        .filter_ns = 500,
    };

    pal_pcnt_unit_handle_t handle = NULL;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pcnt_init(&cfg, &handle));

    int64_t count = -1;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pcnt_get_count(handle, &count));
    TEST_ASSERT_EQUAL_INT64(0, count);

    /* Step +100 */
    stub_pcnt_step(handle, 100);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pcnt_get_count(handle, &count));
    TEST_ASSERT_EQUAL_INT64(100, count);

    /* Step -30 */
    stub_pcnt_step(handle, -30);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pcnt_get_count(handle, &count));
    TEST_ASSERT_EQUAL_INT64(70, count);

    /* Large 64-bit count (> 32-bit integer) */
    int64_t large_count = 0x100000000LL; /* 4294967296 */
    stub_pcnt_set_count(handle, large_count);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pcnt_get_count(handle, &count));
    TEST_ASSERT_EQUAL_INT64(large_count, count);

    /* Clear */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pcnt_clear(handle));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pcnt_get_count(handle, &count));
    TEST_ASSERT_EQUAL_INT64(0, count);

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pcnt_deinit(handle));
}

void test_pcnt_glitch_filter(void) {
    pal_pcnt_config_t cfg = {
        .pin_a = 18,
        .pin_b = WINK_PIN_NC,
        .mode = PAL_PCNT_MODE_1X,
        .filter_ns = 1000,
    };

    pal_pcnt_unit_handle_t handle = NULL;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pcnt_init(&cfg, &handle));

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pcnt_set_glitch_filter(handle, 2500));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pcnt_deinit(handle));
}

void test_pcnt_forced_failure(void) {
    pal_pcnt_config_t cfg = {
        .pin_a = 18,
        .pin_b = WINK_PIN_NC,
        .mode = PAL_PCNT_MODE_1X,
    };

    pal_pcnt_unit_handle_t handle = NULL;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pcnt_init(&cfg, &handle));

    stub_pcnt_force_failure(handle, WINK_ERR_HARDWARE);

    int64_t count = 0;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_HARDWARE, pal_pcnt_get_count(handle, &count));

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pcnt_deinit(handle));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_pcnt_init_deinit);
    RUN_TEST(test_pcnt_count_and_clear);
    RUN_TEST(test_pcnt_glitch_filter);
    RUN_TEST(test_pcnt_forced_failure);
    return UNITY_END();
}
