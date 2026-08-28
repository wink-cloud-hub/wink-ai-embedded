// SPDX-License-Identifier: Apache-2.0
// MCS-51 analog channel-3 rail (boundary ④, AD-8 / ADR-0057).
//
// Virtual peripherals that need an analog input (the external ADC0832 today,
// the CMS8S on-chip ADC in M5) NEVER touch JS directly and user code never
// calls this API: the peripheral state machine pulls the latest code value
// through mcs51_adc_get_value() at the exact SFR-trap interception point.
//
// Dual-track data path (umbrella SSOT §3.4):
//   * Production (wasm/UniSim 3.0): the value is PULLED from the JS
//     PinArbiter via js_pal_adc_read_norm(pin) → [0.0, 1.0] and scaled to an
//     8-bit code (raw = norm * 255). No 51-specific JS glue — the standard
//     channel-3 interface is reused, so the thermal/NTC plugins work
//     unmodified.
//   * Test/CI (host, or wasm bounded tests): mcs51_adc_set_value() injects an
//     override that wins over the pull, giving deterministic high-speed tests
//     without any JS environment.
//
// Routing convention: the 8051 has no analog pins of its own, so ADC channels
// are addressed through SYNTHETIC pin ids `32 + ch` — the physical MCU pin
// space is only 0..31 (P0.0..P3.7, linear map port*8+bit). PinArbiter routes
// analog sources (NTC, knob, LDR) onto these synthetic ids via the runtime
// device-tree; the firmware-time mcs51_board_config.h is not involved (S3-C4:
// thermal parameters and analog routing stay runtime-side).
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ADC0832 has 2 multiplexed inputs (CH0/CH1 single-ended, or differential
// pairs); the rail keeps a small generous BSS table — CMS8S on-chip channels
// (M5) reuse the same rail.
#define MCS51_ADC_MAX_CHANNELS 8u

// Sentinel: no test value injected on this channel → pull from PinArbiter.
#define MCS51_ADC_RAIL_INJECT_NONE 0xFFFFu

// Pull the current code value (8-bit, 0..255) for analog channel `ch`.
// Injection rail wins; otherwise js_pal_adc_read_norm(32 + ch) scaled.
// Out-of-range channels read 0.
uint16_t mcs51_adc_get_value(uint8_t ch);

// Test/CI injection override (boundary ④ physical injection rail).
// raw = MCS51_ADC_RAIL_INJECT_NONE clears the override back to Pull mode.
void mcs51_adc_set_value(uint8_t ch, uint16_t raw);

// Framework init: clear all injection overrides.
void mcs51_adc_reset(void);

// Forward-compat shims: the external 8-bit ADC0832 maps straight onto the
// unified rail (umbrella SSOT §3.4).
static inline void mcs51_adc0832_set_value(uint8_t ch, uint8_t val) {
    mcs51_adc_set_value(ch, (uint16_t)val);
}
static inline uint8_t mcs51_adc0832_get_value(uint8_t ch) {
    return (uint8_t)(mcs51_adc_get_value(ch) & 0xFFu);
}

#ifdef __cplusplus
}
#endif
