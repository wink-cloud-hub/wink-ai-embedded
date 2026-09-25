/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef SOC_CAPS_ESP32C6_H_
#define SOC_CAPS_ESP32C6_H_

#define SOC_GPIO_PIN_COUNT          31

/* C6: 31 个引脚 (0..30) 全部有效并可输出 */
#define SOC_GPIO_VALID_GPIO_MASK        (0x7FFFFFFFULL)
#define SOC_GPIO_VALID_OUTPUT_GPIO_MASK (SOC_GPIO_VALID_GPIO_MASK)

#define GPIO_IS_VALID_GPIO(gpio_num) \
    ((((int)(gpio_num)) >= 0 && ((int)(gpio_num)) < 31) && \
     (((1ULL << (gpio_num)) & SOC_GPIO_VALID_GPIO_MASK) != 0))

#define GPIO_IS_VALID_OUTPUT_GPIO(gpio_num) \
    ((((int)(gpio_num)) >= 0 && ((int)(gpio_num)) < 31) && \
     (((1ULL << (gpio_num)) & SOC_GPIO_VALID_OUTPUT_GPIO_MASK) != 0))

/* LEDC capabilities */
#define SOC_LEDC_SUPPORTED          1
#define SOC_LEDC_SUPPORT_HS_MODE    0 /* C6 无 High-Speed 模式；官方惯例为不定义，门面显式 =0（§5.1 裁决项 5） */
#define SOC_LEDC_TIMER_NUM          4
#define SOC_LEDC_CHANNEL_NUM        6 /* C6 6 个通道 */
#define SOC_LEDC_TIMER_BIT_WIDTH    20 /* C6 官方支持 20-bit 定时器计数器 */

/* I2C / UART / SPI capabilities */
#define SOC_I2C_SUPPORTED           1
#define SOC_I2C_NUM                 1 /* HP-only 裁决：v5.1.3 官方=1U；v6.1 官方=2U（HP1+LP1）。见 §5.1 裁决项 4 */
#define SOC_HP_I2C_NUM              1
#define SOC_UART_SUPPORTED          1
#define SOC_UART_NUM                2 /* HP-only 裁决：v5.1.3 官方=2；v6.1 官方=3（HP2+LP1）。见 §5.1 裁决项 4 */
#define SOC_UART_HP_NUM             2
#define SOC_GPTIMER_SUPPORTED       1
#define SOC_SPI_PERIPH_NUM          2
#define SOC_LP_I2C_NUM              0 /* 官方 v6.1=1U（硬件确有 LP I2C）；M3 HP-only 裁决不暴露。见 §5.1 裁决项 4 */

#endif /* SOC_CAPS_ESP32C6_H_ */
