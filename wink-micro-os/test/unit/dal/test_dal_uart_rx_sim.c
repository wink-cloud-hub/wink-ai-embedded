// SPDX-License-Identifier: GPL-3.0-only
/**
 * @file test_dal_uart_rx_sim.c
 * @brief Unit tests for Wasm Target UART RX SPSC fifo & PAL UART read/write.
 */
#include "unity.h"
#include "wink_status.h"
#include "hal/pal_uart.h"
#include <string.h>

extern bool pal_wasm_push_uart_rx_byte(uint8_t port, uint8_t byte);
extern void pal_wasm_push_uart_rx_error(uint8_t port, uint8_t error_flags);
extern uint32_t pal_wasm_get_uart_rx_available(uint8_t port);
extern void pal_wasm_ch2_uart_reset(void);

void setUp(void)
{
    pal_wasm_ch2_uart_reset();
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_uart_init(0, 1, 3, 115200));
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

static void receive_byte(uint8_t port, pal_uart_event_t event, const uint8_t *data,
                         size_t len, void *arg)
{
    uint32_t *received = arg;
    TEST_ASSERT_EQUAL_UINT8(0, port);
    TEST_ASSERT_EQUAL_INT(PAL_UART_EVENT_RX_DATA, event);
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_EQUAL_UINT32(1, len);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)*received, data[0]);
    (*received)++;
}

void test_uart_callback_consumption_does_not_fill_polling_fifo(void)
{
    uint32_t received = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_uart_set_event_callback(0, receive_byte, &received));
    for (uint32_t i = 0; i < 768; i++) {
        TEST_ASSERT_TRUE(pal_wasm_push_uart_rx_byte(0, (uint8_t)i));
    }
    TEST_ASSERT_EQUAL_UINT32(768, received);
    TEST_ASSERT_EQUAL_UINT32(0, pal_wasm_get_uart_rx_available(0));

    /* Switching back to polling retains subsequent bytes exactly once. */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_uart_set_event_callback(0, NULL, NULL));
    TEST_ASSERT_TRUE(pal_wasm_push_uart_rx_byte(0, 0xA5));
    uint8_t byte = 0;
    uint32_t read = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_uart_read(0, &byte, 1, &read));
    TEST_ASSERT_EQUAL_UINT32(1, read);
    TEST_ASSERT_EQUAL_UINT8(0xA5, byte);
    TEST_ASSERT_EQUAL_UINT32(0, pal_wasm_get_uart_rx_available(0));
}

void test_uart_polling_overflow_preserves_buffered_bytes(void)
{
    for (uint32_t i = 0; i < 256; i++) {
        TEST_ASSERT_TRUE(pal_wasm_push_uart_rx_byte(0, (uint8_t)i));
    }
    TEST_ASSERT_FALSE(pal_wasm_push_uart_rx_byte(0, 0xEE));
    uint8_t data[256] = {0};
    uint32_t read = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_uart_read(0, data, sizeof(data), &read));
    TEST_ASSERT_EQUAL_UINT32(256, read);
    for (uint32_t i = 0; i < 256; i++) {
        TEST_ASSERT_EQUAL_UINT8((uint8_t)i, data[i]);
    }
    TEST_ASSERT_EQUAL_UINT32(0, pal_wasm_get_uart_rx_available(0));
}

static pal_uart_event_t received_errors[3];
static uint32_t received_error_count;

static void receive_error(uint8_t port, pal_uart_event_t event, const uint8_t *data,
                          size_t len, void *arg)
{
    (void)arg;
    TEST_ASSERT_EQUAL_UINT8(0, port);
    TEST_ASSERT_NULL(data);
    TEST_ASSERT_EQUAL_UINT32(0, len);
    TEST_ASSERT_LESS_THAN_UINT32(3, received_error_count);
    received_errors[received_error_count++] = event;
}

void test_uart_error_flags_reach_callback_and_ignore_unknown_inputs(void)
{
    received_error_count = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_uart_set_event_callback(0, receive_error, NULL));
    pal_wasm_push_uart_rx_error(2, 7);
    pal_wasm_push_uart_rx_error(0, 0);
    pal_wasm_push_uart_rx_error(0, 0x80);
    TEST_ASSERT_EQUAL_UINT32(0, received_error_count);
    pal_wasm_push_uart_rx_error(0, 1);
    pal_wasm_push_uart_rx_error(0, 2);
    pal_wasm_push_uart_rx_error(0, 4);
    TEST_ASSERT_EQUAL_UINT32(3, received_error_count);
    TEST_ASSERT_EQUAL_INT(PAL_UART_EVENT_FRAME_ERR, received_errors[0]);
    TEST_ASSERT_EQUAL_INT(PAL_UART_EVENT_PARITY_ERR, received_errors[1]);
    TEST_ASSERT_EQUAL_INT(PAL_UART_EVENT_RX_FIFO_OVF, received_errors[2]);
    TEST_ASSERT_EQUAL_UINT32(0, pal_wasm_get_uart_rx_available(0));
}

void test_uart_combined_error_flags_deliver_each_event_once(void)
{
    received_error_count = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_uart_set_event_callback(0, receive_error, NULL));
    pal_wasm_push_uart_rx_error(0, 7);
    TEST_ASSERT_EQUAL_UINT32(3, received_error_count);
    TEST_ASSERT_EQUAL_INT(PAL_UART_EVENT_FRAME_ERR, received_errors[0]);
    TEST_ASSERT_EQUAL_INT(PAL_UART_EVENT_PARITY_ERR, received_errors[1]);
    TEST_ASSERT_EQUAL_INT(PAL_UART_EVENT_RX_FIFO_OVF, received_errors[2]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_uart_push_and_read_bytes);
    RUN_TEST(test_uart_read_partial_available);
    RUN_TEST(test_uart_callback_consumption_does_not_fill_polling_fifo);
    RUN_TEST(test_uart_polling_overflow_preserves_buffered_bytes);
    RUN_TEST(test_uart_error_flags_reach_callback_and_ignore_unknown_inputs);
    RUN_TEST(test_uart_combined_error_flags_deliver_each_event_once);
    return UNITY_END();
}
