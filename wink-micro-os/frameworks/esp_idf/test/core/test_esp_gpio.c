/* SPDX-License-Identifier: GPL-3.0-only */
#include "unity.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "soc/soc_caps.h"

void setUp(void) {}
void tearDown(void) {}

void test_esp_gpio_config_and_output(void) {
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << GPIO_NUM_2),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gpio_config(&cfg));

    /* Level writing */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gpio_set_level(GPIO_NUM_2, 1));
    TEST_ASSERT_EQUAL_INT(1, gpio_get_level(GPIO_NUM_2));

    TEST_ASSERT_EQUAL_INT32(ESP_OK, gpio_set_level(GPIO_NUM_2, 0));
    TEST_ASSERT_EQUAL_INT(0, gpio_get_level(GPIO_NUM_2));

    /* Reset pin */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gpio_reset_pin(GPIO_NUM_2));
}

void test_esp_gpio_set_direction(void) {
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gpio_set_direction(GPIO_NUM_4, GPIO_MODE_OUTPUT));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gpio_set_level(GPIO_NUM_4, 1));
    TEST_ASSERT_EQUAL_INT(1, gpio_get_level(GPIO_NUM_4));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gpio_reset_pin(GPIO_NUM_4));
}

void test_esp_gpio_input_only_pin_rejected_for_output(void) {
    /* GPIO 34 is input-only on ESP32 */
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << GPIO_NUM_34),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, gpio_config(&cfg));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, gpio_set_direction(GPIO_NUM_34, GPIO_MODE_OUTPUT));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, gpio_set_level(GPIO_NUM_34, 1));

    /* But valid as input */
    cfg.mode = GPIO_MODE_INPUT;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gpio_config(&cfg));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gpio_reset_pin(GPIO_NUM_34));
}

void test_esp_gpio_out_of_bounds_pin_rejected(void) {
    /* GPIO 40 and GPIO 45 are out of bounds */
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << 45),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, gpio_config(&cfg));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, gpio_set_direction((gpio_num_t)45, GPIO_MODE_OUTPUT));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, gpio_set_level((gpio_num_t)45, 1));
    TEST_ASSERT_EQUAL_INT(0, gpio_get_level((gpio_num_t)45));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, gpio_reset_pin((gpio_num_t)45));

    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, gpio_set_direction((gpio_num_t)40, GPIO_MODE_OUTPUT));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, gpio_set_level((gpio_num_t)40, 1));
    TEST_ASSERT_EQUAL_INT(0, gpio_get_level((gpio_num_t)40));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_esp_gpio_config_and_output);
    RUN_TEST(test_esp_gpio_set_direction);
    RUN_TEST(test_esp_gpio_input_only_pin_rejected_for_output);
    RUN_TEST(test_esp_gpio_out_of_bounds_pin_rejected);
    return UNITY_END();
}
