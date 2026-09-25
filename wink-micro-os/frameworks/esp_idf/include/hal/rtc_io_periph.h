/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_RTC_IO_PERIPH_H
#define WINK_H_GUARD_HAL_RTC_IO_PERIPH_H
#ifndef __WINK_HARVESTED_HAL_RTC_IO_PERIPH_H__
#define __WINK_HARVESTED_HAL_RTC_IO_PERIPH_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdint.h>

#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    uint32_t reg;
    uint32_t mux;
    uint32_t func;
    uint32_t ie;
    uint32_t pullup;
    uint32_t pulldown;
    uint32_t slpsel;
    uint32_t slpie;
    uint32_t slpoe;
    uint32_t hold;
    uint32_t hold_force;
    uint32_t drv_v;
    uint32_t drv_s;
    int rtc_num;
} rtc_io_desc_t;



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_RTC_IO_PERIPH_H__ */
#endif /* WINK_H_GUARD_HAL_RTC_IO_PERIPH_H */
