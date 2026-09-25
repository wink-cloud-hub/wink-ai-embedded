/* SPDX-License-Identifier: GPL-3.0-only */
#include "unity.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_idf_wink.h"

void setUp(void) {
    esp_uart_reset();
}

void tearDown(void) {
    esp_uart_reset();
}

void test_uart_param_config_and_pins(void) {
    uart_config_t cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = 0
    };
    TEST_ASSERT_EQUAL_INT32(ESP_OK, uart_param_config(UART_NUM_0, &cfg));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, uart_param_config(UART_NUM_MAX, &cfg));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, uart_param_config(UART_NUM_0, NULL));

    /* Support UART_PIN_NO_CHANGE (-1) */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, uart_set_pin(UART_NUM_0, 1, 3, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, -1, -1));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, uart_set_pin(UART_NUM_MAX, 1, 3, -1, -1));
}

void test_uart_driver_install_and_lifecycle(void) {
    QueueHandle_t uart_queue = NULL;
    TEST_ASSERT_EQUAL_INT32(ESP_OK,
        uart_driver_install(UART_NUM_1, 256, 256, 10, &uart_queue, 0));
    TEST_ASSERT_NOT_NULL(uart_queue);

    /* Re-installation on active port returns INVALID_STATE */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_STATE,
        uart_driver_install(UART_NUM_1, 256, 256, 10, NULL, 0));

    /* Basic write */
    const char *msg = "hello";
    TEST_ASSERT_EQUAL_INT(5, uart_write_bytes(UART_NUM_1, msg, 5));

    /* Read with empty buffer and 0 wait returns 0 immediately */
    char rx_buf[16] = { 0 };
    TEST_ASSERT_EQUAL_INT(0, uart_read_bytes(UART_NUM_1, rx_buf, sizeof(rx_buf), 0));

    /* Buffered len is 0 */
    size_t buffered = 100;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, uart_get_buffered_data_len(UART_NUM_1, &buffered));
    TEST_ASSERT_EQUAL_UINT32(0, buffered);

    /* Flush */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, uart_flush(UART_NUM_1));

    /* Delete driver */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, uart_driver_delete(UART_NUM_1));

    /* Operation on deleted driver fails */
    TEST_ASSERT_EQUAL_INT(-1, uart_write_bytes(UART_NUM_1, msg, 5));
    TEST_ASSERT_EQUAL_INT(-1, uart_read_bytes(UART_NUM_1, rx_buf, sizeof(rx_buf), 0));
}

void test_uart_boundary_checks(void) {
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG,
        uart_driver_install(UART_NUM_MAX, 256, 256, 0, NULL, 0));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG,
        uart_driver_delete(UART_NUM_MAX));
    TEST_ASSERT_EQUAL_INT(-1,
        uart_write_bytes(UART_NUM_MAX, "x", 1));
    TEST_ASSERT_EQUAL_INT(-1,
        uart_read_bytes(UART_NUM_MAX, NULL, 10, 0));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG,
        uart_flush(UART_NUM_MAX));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_uart_param_config_and_pins);
    RUN_TEST(test_uart_driver_install_and_lifecycle);
    RUN_TEST(test_uart_boundary_checks);
    return UNITY_END();
}
