/* SPDX-License-Identifier: GPL-3.0-only */
#include "unity.h"
#include "driver/dedic_gpio.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_sim_fault.h"
#include <string.h>

extern void esp_dedic_gpio_reset(void);
extern void emulate_uart_send(const uint8_t *tx_msg, uint32_t tx_size, uint32_t tx_bit, uint32_t baudrate);
extern void emulate_uart_receive(uint8_t *rx_msg, uint32_t rx_size, uint32_t rx_bit, uint32_t baudrate);
extern void sim_soft_uart_inject_rx(const uint8_t *data, size_t len);

void setUp(void) {
    esp_dedic_gpio_reset();
    sim_esp_fault_clear();
}

void tearDown(void) {
    esp_dedic_gpio_reset();
    sim_esp_fault_clear();
}

void test_dedic_gpio_bundle_param_validation(void) {
    dedic_gpio_bundle_handle_t bundle = NULL;

    /* NULL parameters */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, dedic_gpio_new_bundle(NULL, &bundle));

    int pins[] = { 16, 17 };
    dedic_gpio_bundle_config_t cfg = {
        .gpio_array = pins,
        .array_size = 2,
        .flags = { .out_en = 1, .in_en = 1 }
    };

    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, dedic_gpio_new_bundle(&cfg, NULL));

    /* Invalid array */
    cfg.gpio_array = NULL;
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, dedic_gpio_new_bundle(&cfg, &bundle));

    cfg.gpio_array = pins;
    cfg.array_size = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, dedic_gpio_new_bundle(&cfg, &bundle));

    cfg.array_size = 33; /* Exceeds max 32 */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, dedic_gpio_new_bundle(&cfg, &bundle));

    /* Valid creation */
    cfg.array_size = 2;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, dedic_gpio_new_bundle(&cfg, &bundle));
    TEST_ASSERT_NOT_NULL(bundle);

    TEST_ASSERT_EQUAL_INT32(ESP_OK, dedic_gpio_del_bundle(bundle));

    /* Double delete rejected */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, dedic_gpio_del_bundle(bundle));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, dedic_gpio_del_bundle(NULL));
}

void test_dedic_gpio_bundle_offsets_and_masks(void) {
    int pins[] = { 10, 11, 12 };
    dedic_gpio_bundle_config_t cfg = {
        .gpio_array = pins,
        .array_size = 3,
        .flags = { .out_en = 1, .in_en = 1 }
    };
    dedic_gpio_bundle_handle_t bundle = NULL;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, dedic_gpio_new_bundle(&cfg, &bundle));
    TEST_ASSERT_NOT_NULL(bundle);

    uint32_t offset = 999;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, dedic_gpio_get_out_offset(bundle, &offset));
    TEST_ASSERT_EQUAL_UINT32(0, offset);

    offset = 999;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, dedic_gpio_get_in_offset(bundle, &offset));
    TEST_ASSERT_EQUAL_UINT32(0, offset);

    uint32_t mask = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, dedic_gpio_get_out_mask(bundle, &mask));
    TEST_ASSERT_EQUAL_UINT32(0x7, mask);

    mask = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, dedic_gpio_get_in_mask(bundle, &mask));
    TEST_ASSERT_EQUAL_UINT32(0x7, mask);

    /* Invalid pointer checks */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, dedic_gpio_get_out_offset(bundle, NULL));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, dedic_gpio_get_in_offset(bundle, NULL));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, dedic_gpio_get_out_mask(bundle, NULL));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, dedic_gpio_get_in_mask(bundle, NULL));

    TEST_ASSERT_EQUAL_INT32(ESP_OK, dedic_gpio_del_bundle(bundle));
}

void test_soft_uart_c_emulation_and_loopback(void) {
    uint8_t dummy = 0;
    const uint8_t write_buffer[] = "Hello, world! This is a message.\r\n";
    uint8_t read_buffer[16] = { 0 };

    /* 1. Send dummy byte */
    emulate_uart_send(&dummy, 1, 0, 115200);

    /* 2. Send 34-byte message */
    emulate_uart_send(write_buffer, sizeof(write_buffer) - 1, 0, 115200);

    /* 3. Receive 16 bytes */
    emulate_uart_receive(read_buffer, sizeof(read_buffer), 0, 115200);

    /* First 16 bytes of "Hello, world! This is a message.\r\n" is "Hello, world! Th" */
    TEST_ASSERT_EQUAL_UINT8_ARRAY((const uint8_t *)"Hello, world! Th", read_buffer, 16);
}

void test_dedic_gpio_fault_injection(void) {
    int pins[] = { 16 };
    dedic_gpio_bundle_config_t cfg = {
        .gpio_array = pins,
        .array_size = 1,
        .flags = { .out_en = 1 }
    };
    dedic_gpio_bundle_handle_t bundle = NULL;

    /* Inject fault */
    TEST_ASSERT_EQUAL_INT32(0, sim_esp_fault_inject(ESP_FAULT_DOMAIN_DEDIC_GPIO, ESP_FAULT_DEDIC_GPIO_ALLOC_FAIL, 0));

    /* Creation must fail with ESP_ERR_INVALID_STATE */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_STATE, dedic_gpio_new_bundle(&cfg, &bundle));
    TEST_ASSERT_NULL(bundle);

    /* Clear fault and re-attempt */
    TEST_ASSERT_EQUAL_INT32(0, sim_esp_fault_clear());
    TEST_ASSERT_EQUAL_INT32(ESP_OK, dedic_gpio_new_bundle(&cfg, &bundle));
    TEST_ASSERT_NOT_NULL(bundle);

    TEST_ASSERT_EQUAL_INT32(ESP_OK, dedic_gpio_del_bundle(bundle));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_dedic_gpio_bundle_param_validation);
    RUN_TEST(test_dedic_gpio_bundle_offsets_and_masks);
    RUN_TEST(test_soft_uart_c_emulation_and_loopback);
    RUN_TEST(test_dedic_gpio_fault_injection);
    return UNITY_END();
}
