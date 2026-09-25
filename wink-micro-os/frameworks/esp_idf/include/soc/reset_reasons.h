/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_SOC_RESET_REASONS_H
#define WINK_H_GUARD_SOC_RESET_REASONS_H
#ifndef __WINK_HARVESTED_SOC_RESET_REASONS_H__
#define __WINK_HARVESTED_SOC_RESET_REASONS_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    RESET_REASON_CHIP_POWER_ON = 1,
    RESET_REASON_CORE_SW = 3,
    RESET_REASON_CORE_DEEP_SLEEP = 5,
    RESET_REASON_CORE_SDIO = 6,
    RESET_REASON_CORE_MWDT0 = 7,
    RESET_REASON_CORE_MWDT1 = 8,
    RESET_REASON_CORE_RTC_WDT = 9,
    RESET_REASON_CPU0_MWDT0 = 11,
    RESET_REASON_CPU1_MWDT1 = 11,
    RESET_REASON_CPU0_SW = 12,
    RESET_REASON_CPU1_SW = 12,
    RESET_REASON_CPU0_RTC_WDT = 13,
    RESET_REASON_CPU1_RTC_WDT = 13,
    RESET_REASON_CPU1_CPU0 = 14,
    RESET_REASON_SYS_BROWN_OUT = 15,
    RESET_REASON_SYS_RTC_WDT = 16,
} soc_reset_reason_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_SOC_RESET_REASONS_H__ */
#endif /* WINK_H_GUARD_SOC_RESET_REASONS_H */
