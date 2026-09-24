/* SPDX-License-Identifier: GPL-3.0-only */
#include "unity.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_system.h"
#include "soc/soc_caps.h"
#include <stdbool.h>

/* Simulation reset hooks, defined (strong) in src/esp_idf_bridge.c and
 * consumed (weak) by targets/wasm/wasm_entry.c. No public header on purpose:
 * they are sim plumbing, not official IDF C-ABI. */
extern bool pal_wasm_target_has_pending_reset(void);
extern int pal_wasm_target_get_reset_reason(void);
extern void pal_wasm_target_clear_pending_reset(void);

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

void test_esp_restart_pending_flag(void) {
    /* Never touches host exit/abort: only raises the pending flag. */
    pal_wasm_target_clear_pending_reset();
    TEST_ASSERT_FALSE(pal_wasm_target_has_pending_reset());

    esp_restart();
    TEST_ASSERT_TRUE(pal_wasm_target_has_pending_reset());
    TEST_ASSERT_EQUAL_INT(4, pal_wasm_target_get_reset_reason()); /* SOFTWARE */
    TEST_ASSERT_EQUAL_INT(ESP_RST_SW, esp_reset_reason());

    pal_wasm_target_clear_pending_reset();
    TEST_ASSERT_FALSE(pal_wasm_target_has_pending_reset());
}

void test_esp_gpio_unsupported_apis_fail_loud(void) {
    /* ADR-0012: unsupported APIs must Fail-Loud, never silent ESP_OK. */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NOT_SUPPORTED,
        gpio_set_pull_mode(GPIO_NUM_2, GPIO_PULLUP_ONLY));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NOT_SUPPORTED, gpio_pullup_en(GPIO_NUM_2));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NOT_SUPPORTED, gpio_set_intr_type(GPIO_NUM_2, GPIO_INTR_POSEDGE));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NOT_SUPPORTED, gpio_intr_enable(GPIO_NUM_2));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NOT_SUPPORTED, gpio_install_isr_service(0));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NOT_SUPPORTED,
        gpio_isr_handler_add(GPIO_NUM_2, NULL, NULL));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NOT_SUPPORTED, gpio_isr_handler_remove(GPIO_NUM_2));
    /* Out-of-range still wins over NOT_SUPPORTED. */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG,
        gpio_set_pull_mode((gpio_num_t)45, GPIO_PULLUP_ONLY));
    /* void C-ABI: must not crash. */
    gpio_uninstall_isr_service();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_esp_gpio_config_and_output);
    RUN_TEST(test_esp_gpio_set_direction);
    RUN_TEST(test_esp_gpio_input_only_pin_rejected_for_output);
    RUN_TEST(test_esp_gpio_out_of_bounds_pin_rejected);
    RUN_TEST(test_esp_restart_pending_flag);
    RUN_TEST(test_esp_gpio_unsupported_apis_fail_loud);
    return UNITY_END();
}
