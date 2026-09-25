/* SPDX-License-Identifier: CC0-1.0 */
#ifndef ESP_PRIVATE_GPIO_H_
#define ESP_PRIVATE_GPIO_H_

#include <stdint.h>

#define PIN_FUNC_GPIO 0

static inline void gpio_func_sel(int gpio_num, int func) {
    (void)gpio_num;
    (void)func;
}

#endif /* ESP_PRIVATE_GPIO_H_ */
