/* SPDX-License-Identifier: GPL-3.0-only */
#include "unity.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/i2c.h"
#include "driver/uart.h"
#include "driver/spi_master.h"
#include "esp_idf_wink.h"
#include "soc/soc_caps.h"

void setUp(void) {
    esp_ledc_reset();
    esp_i2c_legacy_reset();
    esp_i2c_master_reset();
    esp_uart_reset();
    esp_spi_reset();
}

void tearDown(void) {
    esp_ledc_reset();
    esp_i2c_legacy_reset();
    esp_i2c_master_reset();
    esp_uart_reset();
    esp_spi_reset();
}

void test_soc_gpio_boundary(void) {
#if defined(CONFIG_IDF_TARGET_ESP32)
    /* ESP32: GPIO 34 是输入专用，配置输出必须失败 */
    gpio_config_t cfg_in_only = {
        .pin_bit_mask = (1ULL << 34),
        .mode = GPIO_MODE_OUTPUT,
    };
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, gpio_config(&cfg_in_only));
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
    /* ESP32-C3: 仅 22 个引脚，操作 GPIO 22 必须越界失败 */
    gpio_config_t cfg_oob = {
        .pin_bit_mask = (1ULL << 22),
        .mode = GPIO_MODE_OUTPUT,
    };
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, gpio_config(&cfg_oob));
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    /* ESP32-S3: GPIO 48 是合法输出引脚 */
    gpio_config_t cfg_s3 = {
        .pin_bit_mask = (1ULL << 48),
        .mode = GPIO_MODE_OUTPUT,
    };
    TEST_ASSERT_EQUAL(ESP_OK, gpio_config(&cfg_s3));
#else
    TEST_IGNORE_MESSAGE("SoC matrix GPIO boundary case not defined for this target");
#endif
}

void test_soc_ledc_hs_mode_restriction(void) {
    ledc_timer_config_t t_cfg = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 5000,
    };
#if !SOC_LEDC_SUPPORT_HS_MODE
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ledc_timer_config(&t_cfg));
#else
    TEST_ASSERT_EQUAL(ESP_OK, ledc_timer_config(&t_cfg));
#endif
}

void test_soc_i2c_port_restriction(void) {
#if SOC_HP_I2C_NUM < 2
    /* C3 官方仅 1 个 I2C；C6 官方为 HP1+LP1，M3 按 HP-only 裁决拦截（§5.1 裁决项 4） */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, i2c_driver_install((i2c_port_t)1, I2C_MODE_MASTER, 0, 0, 0));
#else
    TEST_ASSERT_EQUAL(ESP_OK, i2c_driver_install(I2C_NUM_1, I2C_MODE_MASTER, 0, 0, 0));
    i2c_driver_delete(I2C_NUM_1);
#endif
}

void test_soc_uart_port_restriction(void) {
#if SOC_UART_HP_NUM < 3
    /* C3 官方仅 2 个 UART；C6 官方为 HP2+LP1，M3 按 HP-only 裁决拦截（§5.1 裁决项 4） */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, uart_driver_install(UART_NUM_2, 256, 256, 0, NULL, 0));
#else
    TEST_ASSERT_EQUAL(ESP_OK, uart_driver_install(UART_NUM_2, 256, 256, 0, NULL, 0));
    uart_driver_delete(UART_NUM_2);
#endif
}

void test_soc_spi_host_restriction(void) {
#if SOC_SPI_PERIPH_NUM < 3
    /* C3 / C6 仅有 2 个 SPI 控制器，SPI3_HOST 越界必须报错 */
    spi_bus_config_t bus_cfg = { .mosi_io_num = 1, .sclk_io_num = 2, .miso_io_num = -1 };
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      spi_bus_initialize(SPI3_HOST, &bus_cfg, SPI_DMA_DISABLED));
#else
    TEST_IGNORE_MESSAGE("SoC exposes 3 SPI controllers; SPI3_HOST is valid");
#endif
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_soc_gpio_boundary);
    RUN_TEST(test_soc_ledc_hs_mode_restriction);
    RUN_TEST(test_soc_i2c_port_restriction);
    RUN_TEST(test_soc_uart_port_restriction);
    RUN_TEST(test_soc_spi_host_restriction);
    return UNITY_END();
}
