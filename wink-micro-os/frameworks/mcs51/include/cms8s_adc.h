// SPDX-License-Identifier: Apache-2.0
// CMS8S78xx on-chip 12-bit ADC — instant-conversion model (M5, ADR-0073).
//
// Modeled against the REAL vendor register map (reference manual Ch.22,
// vendor cms8s78xx.h + StdDriver adc.c) — NOT the idealized SSOT map written
// before fixtures were available:
//
//   ADCON0 (0xDF) bit1 ADGO (start / busy, hardware self-clears),
//                 bit6 ADFM (0 = left-justify, 1 = right-justify)
//   ADCON1 (0xDE) bit7 ADEN (module enable)
//   ADCCHS (0xD9) bits5:0 channel (0..25 = AN0..AN25, 0x3F = AN63)
//   ADRESH (0xDD) / ADRESL (0xDC) result, packed per ADFM
//   EIE2 (0xAA) bit4 ADCIE / EIF2 (0xB2) bit4 ADCIF / Keil vector 19
//
// 0-cycle passthrough: a write to ADCON0 with ADGO=1 (and ADEN=1) completes
// the conversion synchronously inside the write hook — pull the 12-bit code
// from the analog rail (mcs51_adc_get_value), pack ADRESH/ADRESL per ADFM,
// self-clear ADGO in the shadow, latch ADCIF, and dispatch vector 19 when
// EA + ADCIE are set — so the vendor poll `while(ADCON0 & 0x02);` exits on
// its first iteration. All state is plain POD (zero-init BSS), static-init
// safe (ADR-0072 D5).
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Register the ADCON0 write hook (called once per framework run from
// mcs51_framework_init, after mcs51_trap_reset() which wipes hook tables).
void cms8s_adc_init(void);

// Test observability: completed conversions since init, and the channel
// (ADCCHS value) of the most recent one.
uint32_t cms8s_adc_conversion_count(void);
uint8_t  cms8s_adc_last_channel(void);

#ifdef __cplusplus
}
#endif
