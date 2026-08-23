// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_pal_uart.c
 * @brief PAL UART event-driven asynchronous and streaming unit tests.
 */
#include "unity.h"
#include "hal/pal_uart.h"
#include "pal_resource.h"
#include "pal_uart_stub.h"
#include <string.h>

void setUp(void) {
    pal_resource_reset();
    for (uint8_t i = 0; i < PAL_UART_PORT_MAX; i++) {
        stub_uart_reset(i);
        pal_uart_deinit(i);
    }
}

void tearDown(void) {
    for (uint8_t i = 0; i < PAL_UART_PORT_MAX; i++) {
        pal_uart_deinit(i);
    }
    pal_resource_reset();
}

void test_uart_init_deinit(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_uart_init(0, 1, 3, 115200));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_uart_init(1, 17, 16, 9600));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_uart_init(2, 4, 5, 115200));

    /* Out of bounds port */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, pal_uart_init(PAL_UART_PORT_MAX, 18, 19, 115200));

    /* Duplicate init must return BUSY */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, pal_uart_init(0, 1, 3, 115200));

    /* Deinit and re-init */
    pal_uart_deinit(0);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_uart_init(0, 1, 3, 115200));
}

void test_uart_polling_read_write(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_uart_init(1, 17, 16, 115200));

    const uint8_t inject_data[4] = {0xAA, 0xBB, 0xCC, 0xDD};
    stub_uart_inject_rx(1, inject_data, 4);

    uint8_t rx_buf[8] = {0};
    uint32_t bytes_read = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_uart_read(1, rx_buf, sizeof(rx_buf), &bytes_read));
    TEST_ASSERT_EQUAL_UINT32(4, bytes_read);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(inject_data, rx_buf, 4);

    const uint8_t tx_data[3] = {0x11, 0x22, 0x33};
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_uart_write(1, tx_data, sizeof(tx_data)));

    uint8_t captured_tx[8] = {0};
    size_t captured_len = sizeof(captured_tx);
    stub_uart_get_last_tx(1, captured_tx, &captured_len);
    TEST_ASSERT_EQUAL_UINT32(3, captured_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(tx_data, captured_tx, 3);
}

typedef struct {
    bool             received;
    pal_uart_event_t last_event;
    uint8_t          data[16];
    size_t           len;
} test_uart_cb_ctx_t;

static void test_uart_event_cb(uint8_t port, pal_uart_event_t event, const uint8_t *data, size_t len, void *arg) {
    (void)port;
    test_uart_cb_ctx_t *ctx = (test_uart_cb_ctx_t *)arg;
    ctx->received = true;
    ctx->last_event = event;
    ctx->len = len;
    if (len > 0 && len <= sizeof(ctx->data) && data != NULL) {
        memcpy(ctx->data, data, len);
    }
}

void test_uart_event_callbacks(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_uart_init(1, 17, 16, 115200));

    test_uart_cb_ctx_t ctx = { .received = false };
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_uart_set_event_callback(1, test_uart_event_cb, &ctx));

    /* Test RX_DATA event */
    const uint8_t rx_msg[2] = {0x41, 0x42};
    stub_uart_inject_rx(1, rx_msg, 2);
    TEST_ASSERT_TRUE(ctx.received);
    TEST_ASSERT_EQUAL_INT(PAL_UART_EVENT_RX_DATA, ctx.last_event);
    TEST_ASSERT_EQUAL_UINT32(2, ctx.len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(rx_msg, ctx.data, 2);

    /* Test hardware error event injection */
    ctx.received = false;
    stub_uart_inject_event(1, PAL_UART_EVENT_BREAK, NULL, 0);
    TEST_ASSERT_TRUE(ctx.received);
    TEST_ASSERT_EQUAL_INT(PAL_UART_EVENT_BREAK, ctx.last_event);

    /* Test TX_DONE event via write_async */
    ctx.received = false;
    const uint8_t tx_msg[1] = {0x99};
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_uart_write_async(1, tx_msg, 1));
    TEST_ASSERT_TRUE(ctx.received);
    TEST_ASSERT_EQUAL_INT(PAL_UART_EVENT_TX_DONE, ctx.last_event);
}

void test_uart_forced_failure(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_uart_init(1, 17, 16, 115200));

    stub_uart_force_failure(1, WINK_ERR_HARDWARE);

    const uint8_t tx[1] = {0xFF};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_HARDWARE, pal_uart_write(1, tx, 1));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_uart_init_deinit);
    RUN_TEST(test_uart_polling_read_write);
    RUN_TEST(test_uart_event_callbacks);
    RUN_TEST(test_uart_forced_failure);
    return UNITY_END();
}
