/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef SOC_CAPS_ESP32_H_
#define SOC_CAPS_ESP32_H_

#define SOC_GPIO_PIN_COUNT          40

/* 官方原式：0xFFFFFFFFFFULL(10×F=40bit) 排除 24/28/29/30/31；输出再排除 34~39 */
#define SOC_GPIO_VALID_GPIO_MASK \
    (0xFFFFFFFFFFULL & ~(0ULL | (1ULL<<24) | (1ULL<<28) | (1ULL<<29) | (1ULL<<30) | (1ULL<<31)))
#define SOC_GPIO_VALID_OUTPUT_GPIO_MASK \
    (SOC_GPIO_VALID_GPIO_MASK & ~(0ULL | (1ULL<<34) | (1ULL<<35) | (1ULL<<36) | (1ULL<<37) | (1ULL<<38) | (1ULL<<39)))

#define GPIO_IS_VALID_GPIO(gpio_num) \
    ((((int)(gpio_num)) >= 0 && ((int)(gpio_num)) < 40) && \
     (((1ULL << (gpio_num)) & SOC_GPIO_VALID_GPIO_MASK) != 0))

#define GPIO_IS_VALID_OUTPUT_GPIO(gpio_num) \
    ((((int)(gpio_num)) >= 0 && ((int)(gpio_num)) < 40) && \
     (((1ULL << (gpio_num)) & SOC_GPIO_VALID_OUTPUT_GPIO_MASK) != 0))

/* LEDC capabilities */
#define SOC_LEDC_SUPPORTED          1
#define SOC_LEDC_SUPPORT_HS_MODE    1
#define SOC_LEDC_TIMER_NUM          4
#define SOC_LEDC_CHANNEL_NUM        8
#define SOC_LEDC_TIMER_BIT_WIDTH    20

/* I2C capabilities */
#define SOC_I2C_SUPPORTED           1
#define SOC_I2C_NUM                 2
#define SOC_HP_I2C_NUM              2

/* UART capabilities */
#define SOC_UART_SUPPORTED          1
#define SOC_UART_NUM                3
#define SOC_UART_HP_NUM             3

/* GPTimer capabilities */
#define SOC_GPTIMER_SUPPORTED       1

/* SPI capabilities */
#define SOC_SPI_PERIPH_NUM          3

#endif /* SOC_CAPS_ESP32_H_ */
