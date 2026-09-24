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

#endif /* SOC_CAPS_ESP32_H_ */
