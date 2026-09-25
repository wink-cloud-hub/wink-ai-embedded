/* SPDX-License-Identifier: GPL-3.0-only */
#include "unity.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_idf_wink.h"
#include "soc/soc_caps.h"

void setUp(void) {
    esp_spi_reset();
}

void tearDown(void) {
    esp_spi_reset();
}

void test_spi_bus_init_and_host_mapping(void) {
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = 23,
        .miso_io_num = 19,
        .sclk_io_num = 18,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
        .flags = 0,
        .intr_flags = 0
    };

    /* SPI1_HOST is reserved for Flash -> rejected */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG,
        spi_bus_initialize(SPI1_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    /* Invalid host device rejected */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG,
        spi_bus_initialize(SPI_HOST_MAX, &bus_cfg, SPI_DMA_CH_AUTO));

    /* SPI2_HOST mapped to PAL 0 -> success */
    TEST_ASSERT_EQUAL_INT32(ESP_OK,
        spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    /* SPI3_HOST mapped to PAL 1 -> success (SoCs with 3 SPI controllers only) */
#if SOC_SPI_PERIPH_NUM >= 3
    TEST_ASSERT_EQUAL_INT32(ESP_OK,
        spi_bus_initialize(SPI3_HOST, &bus_cfg, SPI_DMA_CH_AUTO));
#endif

    TEST_ASSERT_EQUAL_INT32(ESP_OK, spi_bus_free(SPI2_HOST));
#if SOC_SPI_PERIPH_NUM >= 3
    TEST_ASSERT_EQUAL_INT32(ESP_OK, spi_bus_free(SPI3_HOST));
#endif
}

void test_spi_device_add_and_transmit(void) {
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = 23,
        .miso_io_num = 19,
        .sclk_io_num = 18,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
        .flags = 0,
        .intr_flags = 0
    };
    TEST_ASSERT_EQUAL_INT32(ESP_OK, spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_DISABLED));

    spi_device_interface_config_t dev_cfg = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .mode = 0,
        .clock_speed_hz = 10000000,
        .spics_io_num = 5,
        .flags = 0,
        .queue_size = 1
    };
    spi_device_handle_t dev_handle = NULL;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, spi_bus_add_device(SPI2_HOST, &dev_cfg, &dev_handle));
    TEST_ASSERT_NOT_NULL(dev_handle);

    /* Normal buffer transmit */
    uint8_t tx_buf[4] = { 0xAA, 0xBB, 0xCC, 0xDD };
    uint8_t rx_buf[4] = { 0 };
    spi_transaction_t t = {
        .flags = 0,
        .cmd = 0,
        .addr = 0,
        .length = 32,
        .rxlength = 32,
        .user = NULL,
        .tx_buffer = tx_buf,
        .rx_buffer = rx_buf
    };
    TEST_ASSERT_EQUAL_INT32(ESP_OK, spi_device_transmit(dev_handle, &t));

    /* SPI_TRANS_USE_TXDATA and SPI_TRANS_USE_RXDATA internal array transmit */
    spi_transaction_t t_data = {
        .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
        .cmd = 0,
        .addr = 0,
        .length = 16,
        .rxlength = 16,
        .user = NULL,
        .tx_data = { 0x12, 0x34, 0x00, 0x00 },
        .rx_data = { 0 }
    };
    TEST_ASSERT_EQUAL_INT32(ESP_OK, spi_device_transmit(dev_handle, &t_data));

    TEST_ASSERT_EQUAL_INT32(ESP_OK, spi_bus_remove_device(dev_handle));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, spi_bus_free(SPI2_HOST));
}

void test_spi_invalid_args_and_device_pool(void) {
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = 23,
        .miso_io_num = 19,
        .sclk_io_num = 18,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    spi_device_interface_config_t dev_cfg = {
        .mode = 0,
        .clock_speed_hz = 1000000,
        .spics_io_num = 5,
        .queue_size = 1,
    };
    spi_device_handle_t dev = NULL;

    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG,
        spi_bus_initialize(SPI2_HOST, NULL, SPI_DMA_DISABLED));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, spi_bus_free(SPI_HOST_MAX));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, spi_bus_add_device(SPI_HOST_MAX, &dev_cfg, &dev));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, spi_bus_add_device(SPI2_HOST, NULL, &dev));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, spi_bus_add_device(SPI2_HOST, &dev_cfg, NULL));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, spi_bus_remove_device(NULL));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, spi_device_transmit(NULL, NULL));

    TEST_ASSERT_EQUAL_INT32(ESP_OK, spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_DISABLED));

    spi_device_handle_t dev2 = NULL;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, spi_bus_add_device(SPI2_HOST, &dev_cfg, &dev));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, spi_bus_add_device(SPI2_HOST, &dev_cfg, &dev2));

    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
        .length = 8,
        .rxlength = 8,
        .tx_data = { 0xAA, 0, 0, 0 },
        .rx_data = { 0 },
    };
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, spi_device_transmit(dev, NULL));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, spi_device_transmit(dev, &t));

    TEST_ASSERT_EQUAL_INT32(ESP_OK, spi_bus_remove_device(dev));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, spi_bus_remove_device(dev2));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, spi_bus_free(SPI2_HOST));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_spi_bus_init_and_host_mapping);
    RUN_TEST(test_spi_device_add_and_transmit);
    RUN_TEST(test_spi_invalid_args_and_device_pool);
    return UNITY_END();
}
