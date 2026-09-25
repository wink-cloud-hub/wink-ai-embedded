/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef SOC_CAPS_ESP32S3_H_
#define SOC_CAPS_ESP32S3_H_

#define SOC_GPIO_PIN_COUNT          49

/* S3: 官方有效引脚 45 个（0~21、26~48）；GPIO 22~25 物理不存在，
 * 官方掩码显式排除（v5.1.3 / v6.1 一致），枚举同步跳过 22~25。
 * 模组级 Flash/PSRAM 占用（26~32）属模组差异，不在芯片级拦截范围。 */
#define SOC_GPIO_VALID_GPIO_MASK        \
    (0x1FFFFFFFFFFFFULL & ~(0ULL | (1ULL<<22) | (1ULL<<23) | (1ULL<<24) | (1ULL<<25)))
#define SOC_GPIO_VALID_OUTPUT_GPIO_MASK (SOC_GPIO_VALID_GPIO_MASK)

#define GPIO_IS_VALID_GPIO(gpio_num) \
    ((((int)(gpio_num)) >= 0 && ((int)(gpio_num)) < 49) && \
     (((1ULL << (gpio_num)) & SOC_GPIO_VALID_GPIO_MASK) != 0))

#define GPIO_IS_VALID_OUTPUT_GPIO(gpio_num) \
    ((((int)(gpio_num)) >= 0 && ((int)(gpio_num)) < 49) && \
     (((1ULL << (gpio_num)) & SOC_GPIO_VALID_OUTPUT_GPIO_MASK) != 0))

/* LEDC capabilities */
#define SOC_LEDC_SUPPORTED          1
#define SOC_LEDC_SUPPORT_HS_MODE    0 /* S3 无 High-Speed 模式；官方惯例为不定义，门面显式 =0 供取反判断（§5.1 裁决项 5） */
#define SOC_LEDC_TIMER_NUM          4
#define SOC_LEDC_CHANNEL_NUM        8
#define SOC_LEDC_TIMER_BIT_WIDTH    14

/* I2C / UART / SPI capabilities */
#define SOC_I2C_SUPPORTED           1
#define SOC_I2C_NUM                 2
#define SOC_HP_I2C_NUM              2
#define SOC_UART_SUPPORTED          1
#define SOC_UART_NUM                3
#define SOC_UART_HP_NUM             3
#define SOC_GPTIMER_SUPPORTED       1
#define SOC_SPI_PERIPH_NUM          3

#endif /* SOC_CAPS_ESP32S3_H_ */
