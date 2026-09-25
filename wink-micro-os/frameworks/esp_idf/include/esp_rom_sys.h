/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_ROM_SYS_H
#define WINK_H_GUARD_ESP_ROM_SYS_H
#ifndef __WINK_HARVESTED_ESP_ROM_SYS_H__
#define __WINK_HARVESTED_ESP_ROM_SYS_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#include "sdkconfig.h"
#include "soc/reset_reasons.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */

void esp_rom_delay_us(uint32_t us);
void esp_rom_install_uart_printf(void);


#if defined(__WINK_SIM__)
int esp_rom_cvt(unsigned long long val, long radix, int pad, const char *digits, char *buf) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_cvt out of Core 8 scope.");
#else
int esp_rom_cvt(unsigned long long val, long radix, int pad, const char *digits, char *buf);
#endif

#if defined(__WINK_SIM__)
uint32_t esp_rom_get_cpu_ticks_per_us(void) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_get_cpu_ticks_per_us out of Core 8 scope.");
#else
uint32_t esp_rom_get_cpu_ticks_per_us(void);
#endif

#if defined(__WINK_SIM__)
soc_reset_reason_t esp_rom_get_reset_reason(int cpu_no) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_get_reset_reason out of Core 8 scope.");
#else
soc_reset_reason_t esp_rom_get_reset_reason(int cpu_no);
#endif

#if defined(__WINK_SIM__)
void esp_rom_install_channel_putc(int channel, void (*putc)(char c)) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_install_channel_putc out of Core 8 scope.");
#else
void esp_rom_install_channel_putc(int channel, void (*putc)(char c));
#endif

#if defined(__WINK_SIM__)
void esp_rom_output_to_channels(char c) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_output_to_channels out of Core 8 scope.");
#else
void esp_rom_output_to_channels(char c);
#endif

#if defined(__WINK_SIM__)
int esp_rom_printf(const char *fmt, ...) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_printf out of Core 8 scope.");
#else
int esp_rom_printf(const char *fmt, ...);
#endif

#if defined(__WINK_SIM__)
void esp_rom_route_intr_matrix(int cpu_core, uint32_t periph_intr_id, uint32_t cpu_intr_num) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_route_intr_matrix out of Core 8 scope.");
#else
void esp_rom_route_intr_matrix(int cpu_core, uint32_t periph_intr_id, uint32_t cpu_intr_num);
#endif

#if defined(__WINK_SIM__)
void esp_rom_set_cpu_ticks_per_us(uint32_t ticks_per_us) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_set_cpu_ticks_per_us out of Core 8 scope.");
#else
void esp_rom_set_cpu_ticks_per_us(uint32_t ticks_per_us);
#endif

#if defined(__WINK_SIM__)
void esp_rom_software_reset_cpu(int cpu_no) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_software_reset_cpu out of Core 8 scope.");
#else
void esp_rom_software_reset_cpu(int cpu_no);
#endif

#if defined(__WINK_SIM__)
void esp_rom_software_reset_system(void) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_software_reset_system out of Core 8 scope.");
#else
void esp_rom_software_reset_system(void);
#endif

#if defined(__WINK_SIM__)
int esp_rom_vprintf(const char *fmt, va_list ap) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_vprintf out of Core 8 scope.");
#else
int esp_rom_vprintf(const char *fmt, va_list ap);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_ROM_SYS_H__ */
#endif /* WINK_H_GUARD_ESP_ROM_SYS_H */
