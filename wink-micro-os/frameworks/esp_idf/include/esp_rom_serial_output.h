/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_ROM_SERIAL_OUTPUT_H
#define WINK_H_GUARD_ESP_ROM_SERIAL_OUTPUT_H
#ifndef __WINK_HARVESTED_ESP_ROM_SERIAL_OUTPUT_H__
#define __WINK_HARVESTED_ESP_ROM_SERIAL_OUTPUT_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef ESP_ROM_CDC_ACM_WORK_BUF_MIN
#define ESP_ROM_CDC_ACM_WORK_BUF_MIN 128
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
void esp_rom_output_flush_tx(uint8_t serial_num) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_output_flush_tx out of Core 8 scope.");
#else
void esp_rom_output_flush_tx(uint8_t serial_num);
#endif

#if defined(__WINK_SIM__)
void esp_rom_output_putc(char c) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_output_putc out of Core 8 scope.");
#else
void esp_rom_output_putc(char c);
#endif

#if defined(__WINK_SIM__)
int esp_rom_output_rx_one_char(uint8_t *c) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_output_rx_one_char out of Core 8 scope.");
#else
int esp_rom_output_rx_one_char(uint8_t *c);
#endif

#if defined(__WINK_SIM__)
int esp_rom_output_rx_string(uint8_t *str, uint8_t max_len) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_output_rx_string out of Core 8 scope.");
#else
int esp_rom_output_rx_string(uint8_t *str, uint8_t max_len);
#endif

#if defined(__WINK_SIM__)
void esp_rom_output_set_as_console(uint8_t serial_num) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_output_set_as_console out of Core 8 scope.");
#else
void esp_rom_output_set_as_console(uint8_t serial_num);
#endif

#if defined(__WINK_SIM__)
void esp_rom_output_switch_buffer(uint8_t serial_num) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_output_switch_buffer out of Core 8 scope.");
#else
void esp_rom_output_switch_buffer(uint8_t serial_num);
#endif

#if defined(__WINK_SIM__)
int esp_rom_output_tx_one_char(uint8_t c) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_output_tx_one_char out of Core 8 scope.");
#else
int esp_rom_output_tx_one_char(uint8_t c);
#endif

#if defined(__WINK_SIM__)
void esp_rom_output_tx_wait_idle(uint8_t serial_num) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_output_tx_wait_idle out of Core 8 scope.");
#else
void esp_rom_output_tx_wait_idle(uint8_t serial_num);
#endif

#if defined(__WINK_SIM__)
void esp_rom_output_usb_acm_init(void *cdc_acm_work_mem, int cdc_acm_work_mem_len) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_output_usb_acm_init out of Core 8 scope.");
#else
void esp_rom_output_usb_acm_init(void *cdc_acm_work_mem, int cdc_acm_work_mem_len);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_ROM_SERIAL_OUTPUT_H__ */
#endif /* WINK_H_GUARD_ESP_ROM_SERIAL_OUTPUT_H */
