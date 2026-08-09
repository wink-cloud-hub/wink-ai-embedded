// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_dal_uart_rx_sim.c
 * @brief Unit tests for Wasm Target UART RX SPSC fifo & PAL UART read/write.
 */
#include "unity.h"
#include "wink_status.h"
#include "hal/pal_uart.h"
#include <string.h>

extern void pal_wasm_push_uart_rx_byte(uint8_t port, uint8_t byte);
extern uint32_t pal_wasm_get_uart_rx_available(uint8_t port);
extern void pal_wasm_ch2_uart_reset(void);

void setUp(void)
{
    pal_wasm_ch2_uart_reset();
    pal_uart_init(0, 1, 3, 115200);
}

void tearDown(void)
{
    pal_uart_deinit(0);
    pal_wasm_ch2_uart_reset();
}

void test_uart_push_and_read_bytes(void)
{
    const char *payload = "Hello UART RX";
    uint32_t len = (uint32_t)strlen(payload);

    for (uint32_t i = 0; i < len; i++) {
        pal_wasm_push_uart_rx_byte(0, (uint8_t)payload[i]);
    }

    TEST_ASSERT_EQUAL_UINT32(len, pal_wasm_get_uart_rx_available(0));

    uint8_t rx_buf[32] = {0};
    uint32_t read_bytes = 0;
    wink_status_t st = pal_uart_read(0, rx_buf, sizeof(rx_buf), &read_bytes);

    TEST_ASSERT_EQUAL_INT(WINK_OK, st);
    TEST_ASSERT_EQUAL_UINT32(len, read_bytes);
    TEST_ASSERT_EQUAL_STRING_LEN(payload, (char *)rx_buf, len);
    TEST_ASSERT_EQUAL_UINT32(0, pal_wasm_get_uart_rx_available(0));
}

void test_uart_read_partial_available(void)
{
    const char *payload = "1234567890";
    uint32_t len = (uint32_t)strlen(payload);

    for (uint32_t i = 0; i < len; i++) {
        pal_wasm_push_uart_rx_byte(0, (uint8_t)payload[i]);
    }

    uint8_t rx_buf[4] = {0};
    uint32_t read_bytes = 0;
    wink_status_t st = pal_uart_read(0, rx_buf, 4, &read_bytes);

    TEST_ASSERT_EQUAL_INT(WINK_OK, st);
    TEST_ASSERT_EQUAL_UINT32(4, read_bytes);
    TEST_ASSERT_EQUAL_UINT32(6, pal_wasm_get_uart_rx_available(0));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_uart_push_and_read_bytes);
    RUN_TEST(test_uart_read_partial_available);
    return UNITY_END();
}
