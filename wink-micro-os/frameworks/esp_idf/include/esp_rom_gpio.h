/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_ROM_GPIO_H
#define WINK_H_GUARD_ESP_ROM_GPIO_H
#ifndef __WINK_HARVESTED_ESP_ROM_GPIO_H__
#define __WINK_HARVESTED_ESP_ROM_GPIO_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>

#include "soc/gpio_pins.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */

void esp_rom_gpio_connect_in_signal(uint32_t gpio_num, uint32_t signal_idx, bool inv);
void esp_rom_gpio_connect_out_signal(uint32_t gpio_num, uint32_t signal_idx, bool out_inv, bool oen_inv);
void esp_rom_gpio_pad_pullup_only(uint32_t iopad_num);
void esp_rom_gpio_pad_select_gpio(uint32_t iopad_num);
void esp_rom_gpio_pad_set_drv(uint32_t iopad_num, uint32_t drv);
void esp_rom_gpio_pad_unhold(uint32_t gpio_num);


#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_ROM_GPIO_H__ */
#endif /* WINK_H_GUARD_ESP_ROM_GPIO_H */
