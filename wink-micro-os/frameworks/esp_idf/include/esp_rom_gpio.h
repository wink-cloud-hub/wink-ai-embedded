/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef ESP_ROM_GPIO_H
#define ESP_ROM_GPIO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void esp_rom_gpio_pad_select_gpio(uint32_t gpio_num);

#ifdef __cplusplus
}
#endif

#endif /* ESP_ROM_GPIO_H */
