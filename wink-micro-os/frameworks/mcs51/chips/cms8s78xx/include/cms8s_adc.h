// SPDX-License-Identifier: Apache-2.0
// CMS8S78xx on-chip 12-bit ADC — instant-conversion model (M5, ADR-0073).
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Mcu51Context;

// Register the ADCON0 write hook (called once per framework run from descriptor init).
void cms8s_adc_init(struct Mcu51Context* ctx);

// Reset ADET hardware trigger edge baseline and seed PS_ADET selector to 0x7F.
void cms8s_adc_model_reset(struct Mcu51Context* ctx);

// Poll ADET hardware trigger pin and dispatch conversion on configured edge.
void cms8s_adc_poll(struct Mcu51Context* ctx);

// Next scheduled internal event timestamp
uint64_t cms8s_adc_next_event_us(struct Mcu51Context* ctx);

// Test observability: completed conversions since init, and the channel
// (ADCCHS value) of the most recent one.
uint32_t cms8s_adc_conversion_count(void);
uint8_t  cms8s_adc_last_channel(void);

// Stage7 S7-1 (closes Stage1 S1-2 dual-read): old frontends driving the
// on-chip path through the synthetic key `32+ch` are REJECTED, never
// redirected. Physical pin keys (AN_TO_PIN) are the v2 contract. STRICT
// builds abort; release builds report the full-scale sentinel below, count
// the misuse, and warn once. Board-space `32+ch` pulls by devices/ stay
// permanently legal and are never probed here.
#define CMS8S_ADC_SYNTH_REJECT_SENTINEL 0x0FFFu
extern uint32_t cms8s_adc_synth_misuse_count;

// A-02 ADC readiness gate (GAP-05): reason-bit mask + per-reason counters
// (GAP-10 runner pattern, mirrors mcs51_uart notready). Bits:
#define WINK_MCS51_ADC_NOTREADY_LDO  (1u << 0)
#define WINK_MCS51_ADC_NOTREADY_MUX  (1u << 1)
uint32_t cms8s_adc_notready_mask(void);
uint32_t cms8s_adc_notready_count(uint32_t reason_bit);
uint32_t cms8s_adc_notready_total(void);

#ifdef __cplusplus
}
#endif
