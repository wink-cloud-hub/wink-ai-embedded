/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_SOC_SOC_ULP_H
#define WINK_H_GUARD_SOC_SOC_ULP_H
#ifndef __WINK_HARVESTED_SOC_SOC_ULP_H__
#define __WINK_HARVESTED_SOC_SOC_ULP_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef IS_BIT_SET
#define IS_BIT_SET(m, i) (((m) >> (i)) & 1)
#endif
#ifndef MASK_TO_WIDTH_HELPER1
#define MASK_TO_WIDTH_HELPER1(m, i) IS_BIT_SET(m, i)
#endif
#ifndef MASK_TO_WIDTH_HELPER16
#define MASK_TO_WIDTH_HELPER16(m, i) (MASK_TO_WIDTH_HELPER8(m, i)  + MASK_TO_WIDTH_HELPER8(m, i + 8))
#endif
#ifndef MASK_TO_WIDTH_HELPER2
#define MASK_TO_WIDTH_HELPER2(m, i) (MASK_TO_WIDTH_HELPER1(m, i)  + MASK_TO_WIDTH_HELPER1(m, i + 1))
#endif
#ifndef MASK_TO_WIDTH_HELPER32
#define MASK_TO_WIDTH_HELPER32(m, i) (MASK_TO_WIDTH_HELPER16(m, i) + MASK_TO_WIDTH_HELPER16(m, i + 16))
#endif
#ifndef MASK_TO_WIDTH_HELPER4
#define MASK_TO_WIDTH_HELPER4(m, i) (MASK_TO_WIDTH_HELPER2(m, i)  + MASK_TO_WIDTH_HELPER2(m, i + 2))
#endif
#ifndef MASK_TO_WIDTH_HELPER8
#define MASK_TO_WIDTH_HELPER8(m, i) (MASK_TO_WIDTH_HELPER4(m, i)  + MASK_TO_WIDTH_HELPER4(m, i + 4))
#endif
#ifndef READ_RTC_FIELD
#define READ_RTC_FIELD(rtc_reg, field) READ_RTC_REG(rtc_reg, field ## _S, MASK_TO_WIDTH_HELPER16(field ## _V, 0))
#endif
#ifndef READ_RTC_REG
#define READ_RTC_REG(rtc_reg, low_bit, bit_width) REG_RD (((rtc_reg) - DR_REG_RTCCNTL_BASE) / 4), ((low_bit) + (bit_width) - 1), (low_bit)
#endif
#ifndef WRITE_RTC_FIELD
#define WRITE_RTC_FIELD(rtc_reg, field, value) WRITE_RTC_REG(rtc_reg, field ## _S, MASK_TO_WIDTH_HELPER8(field ## _V, 0), ((value) & field ## _V))
#endif
#ifndef WRITE_RTC_REG
#define WRITE_RTC_REG(rtc_reg, low_bit, bit_width, value) REG_WR (((rtc_reg) - DR_REG_RTCCNTL_BASE) / 4), ((low_bit) + (bit_width) - 1), (low_bit), ((value) & 0xff)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_SOC_SOC_ULP_H__ */
#endif /* WINK_H_GUARD_SOC_SOC_ULP_H */
