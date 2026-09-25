/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef ESP_ROM_GPIO_H
#define ESP_ROM_GPIO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void esp_rom_gpio_pad_select_gpio(uint32_t gpio_num);
void esp_rom_gpio_connect_out_signal(uint32_t gpio_num, uint32_t signal_idx, bool out_inv, bool oen_inv);
void esp_rom_gpio_connect_in_signal(uint32_t gpio_num, uint32_t signal_idx, bool inv);

#ifdef __cplusplus
}
#endif

#endif /* ESP_ROM_GPIO_H */
