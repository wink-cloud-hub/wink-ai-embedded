/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef SOC_CAPS_ESP32C3_H_
#define SOC_CAPS_ESP32C3_H_

#define SOC_GPIO_PIN_COUNT          22

/* C3: 22 个引脚 (0..21) 全部有效并可输出 */
#define SOC_GPIO_VALID_GPIO_MASK        (0x3FFFFFULL)
#define SOC_GPIO_VALID_OUTPUT_GPIO_MASK (SOC_GPIO_VALID_GPIO_MASK)

#define GPIO_IS_VALID_GPIO(gpio_num) \
    ((((int)(gpio_num)) >= 0 && ((int)(gpio_num)) < 22) && \
     (((1ULL << (gpio_num)) & SOC_GPIO_VALID_GPIO_MASK) != 0))

#define GPIO_IS_VALID_OUTPUT_GPIO(gpio_num) \
    ((((int)(gpio_num)) >= 0 && ((int)(gpio_num)) < 22) && \
     (((1ULL << (gpio_num)) & SOC_GPIO_VALID_OUTPUT_GPIO_MASK) != 0))

/* LEDC capabilities */
#define SOC_LEDC_SUPPORTED          1
#define SOC_LEDC_SUPPORT_HS_MODE    0 /* C3 无 High-Speed 模式；官方惯例为不定义，门面显式 =0（§5.1 裁决项 5） */
#define SOC_LEDC_TIMER_NUM          4
#define SOC_LEDC_CHANNEL_NUM        6 /* C3 仅 6 个通道 (0..5) */
#define SOC_LEDC_TIMER_BIT_WIDTH    14

/* I2C / UART / SPI capabilities */
#define SOC_I2C_SUPPORTED           1
#define SOC_I2C_NUM                 1 /* C3 仅 1 个 I2C 控制器 */
#define SOC_HP_I2C_NUM              1
#define SOC_UART_SUPPORTED          1
#define SOC_UART_NUM                2 /* C3 仅 2 个 UART 控制器 */
#define SOC_UART_HP_NUM             2
#define SOC_GPTIMER_SUPPORTED       1
#define SOC_SPI_PERIPH_NUM          2 /* C3 仅 2 个 SPI */

#endif /* SOC_CAPS_ESP32C3_H_ */
