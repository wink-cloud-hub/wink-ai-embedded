// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_pal_rmt.c
 * @brief PAL RMT multi-channel pulse transceiver unit tests.
 */
#include "unity.h"
#include "hal/pal_rmt.h"
#include "pal_resource.h"
#include "pal_rmt_stub.h"
#include <string.h>

void setUp(void) {
    pal_resource_reset();
}

void tearDown(void) {
    pal_resource_reset();
}

void test_rmt_acquire_release_channel(void) {
    pal_rmt_channel_config_t cfg = {
        .pin = 18,
        .direction = PAL_RMT_DIR_TX,
        .resolution_hz = 10000000,
        .mem_block_symbols = 64,
    };

    pal_rmt_channel_handle_t chs[PAL_RMT_CHAN_MAX];
    for (uint8_t i = 0; i < PAL_RMT_CHAN_MAX; i++) {
        cfg.pin = (wink_pin_t)(10 + i);
        TEST_ASSERT_EQUAL_INT(WINK_OK, pal_rmt_acquire_channel(&cfg, &chs[i]));
        TEST_ASSERT_NOT_NULL(chs[i]);
    }

    /* 9th channel must return RESOURCE_EXHAUSTED */
    pal_rmt_channel_handle_t extra_ch = NULL;
    cfg.pin = 30;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_RESOURCE_EXHAUSTED, pal_rmt_acquire_channel(&cfg, &extra_ch));

    /* Release channel 0 and reclaim */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_rmt_release_channel(chs[0]));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_rmt_acquire_channel(&cfg, &chs[0]));
    TEST_ASSERT_NOT_NULL(chs[0]);

    for (uint8_t i = 0; i < PAL_RMT_CHAN_MAX; i++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, pal_rmt_release_channel(chs[i]));
    }
}

typedef struct {
    bool          called;
    wink_status_t result;
} test_tx_ctx_t;

static void test_tx_done_cb(void *arg, wink_status_t result) {
    test_tx_ctx_t *ctx = (test_tx_ctx_t *)arg;
    ctx->called = true;
    ctx->result = result;
}

void test_rmt_tx_send_and_capture(void) {
    pal_rmt_channel_config_t cfg = {
        .pin = 18,
        .direction = PAL_RMT_DIR_TX,
        .resolution_hz = 10000000, /* 10 MHz -> 1 tick = 100ns */
        .mem_block_symbols = 64,
    };

    pal_rmt_channel_handle_t ch = NULL;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_rmt_acquire_channel(&cfg, &ch));

    pal_rmt_symbol_t symbols[3];
    /* WS2812 bit 1: 800ns HIGH, 450ns LOW (at 10MHz: 8 ticks, 5 ticks approx) */
    symbols[0].duration0_ticks = 8;
    symbols[0].level0 = 1;
    symbols[0].duration1_ticks = 5;
    symbols[0].level1 = 0;

    /* WS2812 bit 0: 400ns HIGH, 850ns LOW */
    symbols[1].duration0_ticks = 4;
    symbols[1].level0 = 1;
    symbols[1].duration1_ticks = 9;
    symbols[1].level1 = 0;

    /* Reset symbol: 50us LOW */
    symbols[2] = pal_rmt_make_reset_symbol(10000000, 50);
    TEST_ASSERT_EQUAL_UINT16(500, symbols[2].duration0_ticks);
    TEST_ASSERT_EQUAL_UINT8(0, symbols[2].level0);

    test_tx_ctx_t ctx = { .called = false, .result = WINK_ERR_HARDWARE };
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_rmt_tx_send(ch, symbols, 3, test_tx_done_cb, &ctx));

    TEST_ASSERT_TRUE(ctx.called);
    TEST_ASSERT_EQUAL_INT(WINK_OK, ctx.result);

    /* Verify stub captured TX symbols */
    pal_rmt_symbol_t captured[4];
    size_t captured_cnt = 4;
    stub_rmt_get_last_tx(ch, captured, &captured_cnt);
    TEST_ASSERT_EQUAL_UINT32(3, captured_cnt);
    TEST_ASSERT_EQUAL_UINT16(8, captured[0].duration0_ticks);
    TEST_ASSERT_EQUAL_UINT16(4, captured[1].duration0_ticks);
    TEST_ASSERT_EQUAL_UINT16(500, captured[2].duration0_ticks);

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_rmt_release_channel(ch));
}

typedef struct {
    bool             received;
    pal_rmt_symbol_t symbols[4];
    size_t           count;
} test_rx_ctx_t;

static void test_rx_done_cb(void *arg, const pal_rmt_symbol_t *symbols, size_t count) {
    test_rx_ctx_t *ctx = (test_rx_ctx_t *)arg;
    ctx->received = true;
    ctx->count = count;
    if (count <= 4 && symbols != NULL) {
        memcpy(ctx->symbols, symbols, count * sizeof(pal_rmt_symbol_t));
    }
}

void test_rmt_rx_injection(void) {
    pal_rmt_channel_config_t cfg = {
        .pin = 19,
        .direction = PAL_RMT_DIR_RX,
        .resolution_hz = 1000000, /* 1 MHz -> 1us/tick */
        .mem_block_symbols = 64,
    };

    pal_rmt_channel_handle_t ch = NULL;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_rmt_acquire_channel(&cfg, &ch));

    test_rx_ctx_t rx_ctx = { .received = false, .count = 0 };
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_rmt_rx_set_callback(ch, test_rx_done_cb, &rx_ctx));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_rmt_rx_start(ch));

    pal_rmt_symbol_t inject_syms[2];
    inject_syms[0].duration0_ticks = 1000;
    inject_syms[0].level0 = 1;
    inject_syms[0].duration1_ticks = 500;
    inject_syms[0].level1 = 0;

    inject_syms[1].duration0_ticks = 2000;
    inject_syms[1].level0 = 1;
    inject_syms[1].duration1_ticks = 0;
    inject_syms[1].level1 = 0;

    stub_rmt_inject_rx(ch, inject_syms, 2);

    TEST_ASSERT_TRUE(rx_ctx.received);
    TEST_ASSERT_EQUAL_UINT32(2, rx_ctx.count);
    TEST_ASSERT_EQUAL_UINT16(1000, rx_ctx.symbols[0].duration0_ticks);
    TEST_ASSERT_EQUAL_UINT16(2000, rx_ctx.symbols[1].duration0_ticks);

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_rmt_rx_stop(ch));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_rmt_release_channel(ch));
}

void test_rmt_forced_failure(void) {
    pal_rmt_channel_config_t cfg = {
        .pin = 18,
        .direction = PAL_RMT_DIR_TX,
        .resolution_hz = 10000000,
    };

    pal_rmt_channel_handle_t ch = NULL;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_rmt_acquire_channel(&cfg, &ch));

    stub_rmt_force_failure(ch, WINK_ERR_HARDWARE);

    pal_rmt_symbol_t sym = { .duration0_ticks = 10, .level0 = 1 };
    test_tx_ctx_t ctx = { .called = false, .result = WINK_OK };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_HARDWARE, pal_rmt_tx_send(ch, &sym, 1, test_tx_done_cb, &ctx));
    TEST_ASSERT_TRUE(ctx.called);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_HARDWARE, ctx.result);

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_rmt_release_channel(ch));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_rmt_acquire_release_channel);
    RUN_TEST(test_rmt_tx_send_and_capture);
    RUN_TEST(test_rmt_rx_injection);
    RUN_TEST(test_rmt_forced_failure);
    return UNITY_END();
}
