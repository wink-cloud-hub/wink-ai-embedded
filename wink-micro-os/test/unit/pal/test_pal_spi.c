// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_pal_spi.c
 * @brief PAL SPI master driver and DMA transfer unit tests.
 */
#include "unity.h"
#include "hal/pal_spi.h"
#include "pal_resource.h"
#include "pal_spi_stub.h"
#include <string.h>

void setUp(void) {
    pal_resource_reset();
    stub_spi_reset(0);
    stub_spi_reset(1);
    pal_spi_deinit_bus(0);
    pal_spi_deinit_bus(1);
}

void tearDown(void) {
    pal_spi_deinit_bus(0);
    pal_spi_deinit_bus(1);
    pal_resource_reset();
}

void test_spi_init_deinit_bus(void) {
    pal_spi_bus_config_t cfg = {
        .spi_bus = 0,
        .sclk = 18,
        .mosi = 23,
        .miso = 19,
        .clock_hz = 10000000,
        .mode = 0,
        .dma_enabled = true,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_spi_init_bus(&cfg));
    /* Duplicate init must return BUSY */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, pal_spi_init_bus(&cfg));

    /* Out of bounds bus must return INVALID_ARG */
    pal_spi_bus_config_t invalid_cfg = cfg;
    invalid_cfg.spi_bus = PAL_SPI_BUS_MAX;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, pal_spi_init_bus(&invalid_cfg));

    /* Deinit */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_spi_deinit_bus(0));
    /* Idempotent deinit */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_spi_deinit_bus(0));
}

void test_spi_add_remove_device(void) {
    pal_spi_bus_config_t bus_cfg = {
        .spi_bus = 0,
        .sclk = 18,
        .mosi = 23,
        .miso = 19,
        .clock_hz = 10000000,
        .mode = 0,
        .dma_enabled = true,
    };
    pal_spi_device_config_t dev_cfg = {
        .cs_pin = 5,
        .clock_hz = 10000000,
        .mode = 0,
        .cs_active_high = false,
        .cs_setup_ns = 50,
        .cs_hold_ns = 50,
    };

    pal_spi_device_handle_t dev_handle = NULL;

    /* Add device before bus init -> INVALID_STATE */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_STATE, pal_spi_add_device(0, &dev_cfg, &dev_handle));

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_spi_init_bus(&bus_cfg));

    /* Add max devices */
    pal_spi_device_handle_t devs[PAL_SPI_DEV_MAX_PER_BUS];
    wink_pin_t cs_pins[PAL_SPI_DEV_MAX_PER_BUS] = {5, 4, 2, 15};
    for (size_t i = 0; i < PAL_SPI_DEV_MAX_PER_BUS; i++) {
        dev_cfg.cs_pin = cs_pins[i];
        TEST_ASSERT_EQUAL_INT(WINK_OK, pal_spi_add_device(0, &dev_cfg, &devs[i]));
        TEST_ASSERT_NOT_NULL(devs[i]);
    }

    /* 5th device should exceed capacity */
    pal_spi_device_handle_t extra_dev = NULL;
    dev_cfg.cs_pin = 21;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_RESOURCE_EXHAUSTED, pal_spi_add_device(0, &dev_cfg, &extra_dev));

    /* Remove 1st device and add again */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_spi_remove_device(devs[0]));
    dev_cfg.cs_pin = 21;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_spi_add_device(0, &dev_cfg, &devs[0]));
    TEST_ASSERT_NOT_NULL(devs[0]);
}

void test_spi_polling_tx_rx_injection(void) {
    pal_spi_bus_config_t bus_cfg = {
        .spi_bus = 0,
        .sclk = 18,
        .mosi = 23,
        .miso = 19,
        .clock_hz = 10000000,
        .mode = 0,
        .dma_enabled = true,
    };
    pal_spi_device_config_t dev_cfg = {
        .cs_pin = 5,
        .clock_hz = 10000000,
        .mode = 0,
        .cs_active_high = false,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_spi_init_bus(&bus_cfg));
    pal_spi_device_handle_t dev = NULL;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_spi_add_device(0, &dev_cfg, &dev));

    /* Inject response bytes */
    const uint8_t expected_rx[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    stub_spi_inject_rx(0, expected_rx, 4);

    const uint8_t tx_data[4] = {0x01, 0x02, 0x03, 0x04};
    uint8_t rx_data[4] = {0};

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_spi_transfer_polling(dev, tx_data, rx_data, 4));

    /* Assert received data */
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_rx, rx_data, 4);

    /* Assert last TX captured */
    uint8_t captured_tx[16] = {0};
    size_t captured_len = sizeof(captured_tx);
    stub_spi_get_last_tx(0, captured_tx, &captured_len);
    TEST_ASSERT_EQUAL_UINT32(4, captured_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(tx_data, captured_tx, 4);
}

typedef struct {
    bool          called;
    wink_status_t result;
} test_dma_cb_ctx_t;

static void test_dma_cb(void *arg, wink_status_t result) {
    test_dma_cb_ctx_t *ctx = (test_dma_cb_ctx_t *)arg;
    ctx->called = true;
    ctx->result = result;
}

void test_spi_dma_async_callback(void) {
    pal_spi_bus_config_t bus_cfg = {
        .spi_bus = 0,
        .sclk = 18,
        .mosi = 23,
        .miso = 19,
        .clock_hz = 10000000,
        .mode = 0,
        .dma_enabled = true,
    };
    pal_spi_device_config_t dev_cfg = {
        .cs_pin = 5,
        .clock_hz = 10000000,
        .mode = 0,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_spi_init_bus(&bus_cfg));
    pal_spi_device_handle_t dev = NULL;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_spi_add_device(0, &dev_cfg, &dev));

    const uint8_t expected_rx[2] = {0x55, 0xAA};
    stub_spi_inject_rx(0, expected_rx, 2);

    const uint8_t tx[2] = {0x11, 0x22};
    uint8_t rx[2] = {0};
    test_dma_cb_ctx_t ctx = { .called = false, .result = WINK_ERR_HARDWARE };

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_spi_transfer_dma(dev, tx, rx, 2, test_dma_cb, &ctx));

    TEST_ASSERT_TRUE(ctx.called);
    TEST_ASSERT_EQUAL_INT(WINK_OK, ctx.result);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_rx, rx, 2);
}

void test_spi_forced_error(void) {
    pal_spi_bus_config_t bus_cfg = {
        .spi_bus = 0,
        .sclk = 18,
        .mosi = 23,
        .miso = 19,
        .clock_hz = 10000000,
        .mode = 0,
        .dma_enabled = true,
    };
    pal_spi_device_config_t dev_cfg = {
        .cs_pin = 5,
        .clock_hz = 10000000,
        .mode = 0,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_spi_init_bus(&bus_cfg));
    pal_spi_device_handle_t dev = NULL;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_spi_add_device(0, &dev_cfg, &dev));

    stub_spi_force_failure(0, WINK_ERR_HARDWARE);

    const uint8_t tx[2] = {0x11, 0x22};
    uint8_t rx[2] = {0};
    test_dma_cb_ctx_t ctx = { .called = false, .result = WINK_OK };

    wink_status_t st = pal_spi_transfer_dma(dev, tx, rx, 2, test_dma_cb, &ctx);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_HARDWARE, st);
    TEST_ASSERT_TRUE(ctx.called);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_HARDWARE, ctx.result);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_spi_init_deinit_bus);
    RUN_TEST(test_spi_add_remove_device);
    RUN_TEST(test_spi_polling_tx_rx_injection);
    RUN_TEST(test_spi_dma_async_callback);
    RUN_TEST(test_spi_forced_error);
    return UNITY_END();
}
