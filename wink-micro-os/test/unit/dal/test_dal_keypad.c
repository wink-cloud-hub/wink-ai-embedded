// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_dal_keypad.c
 * @brief DAL keypad driver unit tests.
 */
#ifndef WINK_USE_KEYPAD
#define WINK_USE_KEYPAD 1
#endif

#include "unity.h"
#include "wink_status.h"
#include "input/dal_keypad.h"
#include "hal/pal_hal.h"
#include "pal_resource.h"
#include "host_test_ctrl.h"
#include "internal/pal_test_loopback.h"

static const char *const OWNER = "test_dal_keypad";

void setUp(void) {
    pal_resource_reset();
    pal_host_reset_gpio_levels();
}

void tearDown(void) {}

void test_keypad_init_null_returns_invalid_arg(void) {
    dal_keypad_config_t cfg = {
        .owner = OWNER,
        .row_pins = {10, 11, 12, 13},
        .col_pins = {14, 15, 16, 17},
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_keypad_init(NULL, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_keypad_init(NULL, NULL));

    dal_keypad_t dev = {0};
    dal_keypad_config_t null_owner = { .owner = NULL };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_keypad_init(&dev, &null_owner));
}

void test_keypad_ops_before_init_returns_not_initialized(void) {
    dal_keypad_t dev = {0};
    char key = '\0';
    bool pressed = false;
    bool changed = false;
    wink_status_t st = WINK_OK;

    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_keypad_get_key(&dev, &key));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_keypad_is_pressed(&dev, &pressed));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_keypad_poll(&dev, &changed, &key));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_keypad_get_status(&dev, &st));
}

void test_keypad_already_initialized_returns_error(void) {
    dal_keypad_t dev = {0};
    dal_keypad_config_t cfg = {
        .owner = OWNER,
        .row_pins = {10, 11, 12, 13},
        .col_pins = {14, 15, 16, 17},
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_keypad_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_ALREADY_INITIALIZED, dal_keypad_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_keypad_deinit(&dev));
}

void test_keypad_4x4_scanning_key1(void) {
    dal_keypad_t dev = {0};
    dal_keypad_config_t cfg = {
        .owner = OWNER,
        .row_pins = {10, 11, 12, 13},
        .col_pins = {14, 15, 16, 17},
        .variant = DAL_KEYPAD_VARIANT_MATRIX_4X4,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_keypad_init(&dev, &cfg));

    /* Idle state: no key pressed */
    char key = '\0';
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_keypad_get_key(&dev, &key));
    TEST_ASSERT_EQUAL_CHAR('\0', key);

    bool pressed = true;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_keypad_is_pressed(&dev, &pressed));
    TEST_ASSERT_FALSE(pressed);

    /* Loopback row 10 to col 14 (pressing key '1' at row 0, col 0) */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_test_enable_hardware_loopback(14, 10));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_keypad_get_key(&dev, &key));
    TEST_ASSERT_EQUAL_CHAR('1', key);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_keypad_is_pressed(&dev, &pressed));
    TEST_ASSERT_TRUE(pressed);

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_test_disable_hardware_loopback(14, 10));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_keypad_deinit(&dev));
}

void test_keypad_4x4_scanning_keyA(void) {
    dal_keypad_t dev = {0};
    dal_keypad_config_t cfg = {
        .owner = OWNER,
        .row_pins = {10, 11, 12, 13},
        .col_pins = {14, 15, 16, 17},
        .variant = DAL_KEYPAD_VARIANT_MATRIX_4X4,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_keypad_init(&dev, &cfg));

    /* Loopback row 10 (R0) to col 17 (C3) -> key 'A' */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_test_enable_hardware_loopback(17, 10));

    char key = '\0';
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_keypad_get_key(&dev, &key));
    TEST_ASSERT_EQUAL_CHAR('A', key);

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_test_disable_hardware_loopback(17, 10));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_keypad_deinit(&dev));
}

void test_keypad_3x4_scanning_key_hash(void) {
    dal_keypad_t dev = {0};
    dal_keypad_config_t cfg = {
        .owner = OWNER,
        .row_pins = {20, 21, 22, 23},
        .col_pins = {24, 25, 26, -1},
        .variant = DAL_KEYPAD_VARIANT_MATRIX_3X4,
        .num_rows = 4,
        .num_cols = 3,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_keypad_init(&dev, &cfg));

    /* Loopback row 23 (R3) to col 26 (C2) -> key '#' */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_test_enable_hardware_loopback(26, 23));

    char key = '\0';
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_keypad_get_key(&dev, &key));
    TEST_ASSERT_EQUAL_CHAR('#', key);

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_test_disable_hardware_loopback(26, 23));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_keypad_deinit(&dev));
}

void test_keypad_custom_variant_scanning(void) {
    dal_keypad_t dev = {0};
    dal_keypad_config_t cfg = {
        .owner = OWNER,
        .row_pins = {30, 31, -1, -1},
        .col_pins = {32, 33, 34, -1},
        .variant = DAL_KEYPAD_VARIANT_CUSTOM,
        .custom_keymap = "XYZUVW",
        .num_rows = 2,
        .num_cols = 3,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_keypad_init(&dev, &cfg));

    /* Loopback row 31 (R1) to col 33 (C1) -> custom key 'V' */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_test_enable_hardware_loopback(33, 31));

    char key = '\0';
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_keypad_get_key(&dev, &key));
    TEST_ASSERT_EQUAL_CHAR('V', key);

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_test_disable_hardware_loopback(33, 31));

    /* Loopback row 30 (R0) to col 34 (C2) -> custom key 'Z' */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_test_enable_hardware_loopback(34, 30));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_keypad_get_key(&dev, &key));
    TEST_ASSERT_EQUAL_CHAR('Z', key);

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_test_disable_hardware_loopback(34, 30));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_keypad_deinit(&dev));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_keypad_init_null_returns_invalid_arg);
    RUN_TEST(test_keypad_ops_before_init_returns_not_initialized);
    RUN_TEST(test_keypad_already_initialized_returns_error);
    RUN_TEST(test_keypad_4x4_scanning_key1);
    RUN_TEST(test_keypad_4x4_scanning_keyA);
    RUN_TEST(test_keypad_3x4_scanning_key_hash);
    RUN_TEST(test_keypad_custom_variant_scanning);
    return UNITY_END();
}
