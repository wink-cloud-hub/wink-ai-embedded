/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_ROM_UART_H
#define WINK_H_GUARD_ESP_ROM_UART_H
#ifndef __WINK_HARVESTED_ESP_ROM_UART_H__
#define __WINK_HARVESTED_ESP_ROM_UART_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef esp_rom_uart_set_clock_baudrate
#define esp_rom_uart_set_clock_baudrate(serial_num, clock_hz, baud_rate) uart_ll_set_baudrate(UART_LL_GET_HW(serial_num), baud_rate, clock_hz)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    ESP_ROM_UART_0 = 0,
    ESP_ROM_UART_1 = 1,
    ESP_ROM_UART_USB = 2,
} esp_rom_uart_num_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */

void esp_rom_uart_flush_tx(uint8_t serial_num);
void esp_rom_uart_putc(char c);
int esp_rom_uart_rx_one_char(uint8_t *c);
int esp_rom_uart_rx_string(uint8_t *str, uint8_t max_len);
void esp_rom_uart_set_as_console(uint8_t serial_num);
void esp_rom_uart_switch_buffer(uint8_t serial_num);
int esp_rom_uart_tx_one_char(uint8_t c);
void esp_rom_uart_tx_wait_idle(uint8_t serial_num);
void esp_rom_uart_usb_acm_init(void *cdc_acm_work_mem, int cdc_acm_work_mem_len);


#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_ROM_UART_H__ */
#endif /* WINK_H_GUARD_ESP_ROM_UART_H */
