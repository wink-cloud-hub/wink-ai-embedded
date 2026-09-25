/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_FAULT_H
#define WINK_H_GUARD_ESP_FAULT_H
#ifndef __WINK_HARVESTED_ESP_FAULT_H__
#define __WINK_HARVESTED_ESP_FAULT_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>

#include "esp_rom_sys.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef ESP_FAULT_ASSERT
#define ESP_FAULT_ASSERT(CONDITION) do {                        bool esp_fault_assert_chk;                              asm volatile ("" ::: "memory");                         esp_fault_assert_chk = (CONDITION);                     asm volatile ("" : "+r"(esp_fault_assert_chk));         if(!esp_fault_assert_chk) _ESP_FAULT_RESET();           asm volatile ("" ::: "memory");                         esp_fault_assert_chk = (CONDITION);                     asm volatile ("" : "+r"(esp_fault_assert_chk));         if(!esp_fault_assert_chk) _ESP_FAULT_RESET();           asm volatile ("" ::: "memory");                         esp_fault_assert_chk = (CONDITION);                     asm volatile ("" : "+r"(esp_fault_assert_chk));         if(!esp_fault_assert_chk) _ESP_FAULT_RESET();           } while(0)
#endif
#ifndef _ESP_FAULT_RESET
#define _ESP_FAULT_RESET() do {         esp_rom_software_reset_system();        _ESP_FAULT_ILLEGAL_INSTRUCTION;  } while(0)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_FAULT_H__ */
#endif /* WINK_H_GUARD_ESP_FAULT_H */
