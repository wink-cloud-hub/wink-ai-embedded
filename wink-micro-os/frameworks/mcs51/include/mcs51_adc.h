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
//     PinArbiter via js_pal_adc_read_norm(pin) → [0.0, 1.0] and scaled to a
//     12-bit code (raw = norm * 4095). No 51-specific JS glue — the standard
//     channel-3 interface is reused, so the thermal/NTC plugins work
//     unmodified. The 8-bit ADC0832 masks to the low byte at its own shim;
//     the 12-bit CMS8S78xx on-chip ADC consumes the full width (M5).
//   * Test/CI (host, or wasm bounded tests): mcs51_adc_set_value() injects an
//     override that wins over the pull, giving deterministic high-speed tests
//     without any JS environment.
//
// Routing convention (Stage1 dual-space partition, PLAN-20260911-MCS51-S1):
// the rail is keyed by a 6-bit rail key, NOT by AN channel number.
//   * `0~31`  = MCU fabric physical Pin: on-chip peripherals map to it in
//     the chip layer (e.g. CMS8S AN0 -> Pin 0 via AN_TO_PIN); core owns no
//     mapping knowledge and performs no mapping.
//   * `32~63` = Board fabric channel: owned by device-tree/fronted,
//     consumed by `devices/` through each device's own net-id mapping
//     (see devices/adc0832/include/adc0832.h); core passes keys through
//     untouched.
// What Stage1 abolishes is the OLD on-chip misuse (passing an AN channel
// number where a synth key was expected), not the board space itself.
// (Stage3 S3-2: the `mcs51_adc0832_*` channel shims moved to the device
// header with the mapping; this core header is mapping-free.)
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ADC0832 has 2 multiplexed inputs (CH0/CH1 single-ended, or differential
// pairs); the CMS8S78xx on-chip ADC exposes AN0..AN25 (26 external channels,
// plus internal AN63). The rail keeps a 64-entry BSS table covering the full
// dual-space key range (0~31 MCU pins + 32~63 board channels).
#define MCS51_ADC_MAX_RAIL_KEYS 64u

// Full-scale code of the unified rail: 12-bit (CMS8S78xx native width). The
// 8-bit ADC0832 shims mask the low byte.
#define MCS51_ADC_RAW_MAX 4095u

// Sentinel: no test value injected on this channel → pull from PinArbiter.
#define MCS51_ADC_RAIL_INJECT_NONE 0xFFFFu

// Pull the current code value (12-bit, 0..4095) for rail `key` (0~63,
// dual-space partition above). Injection rail wins; otherwise
// js_pal_adc_read_norm(key) scaled. Out-of-range keys read 0.
uint16_t mcs51_adc_get_value(uint8_t key);

// Test/CI injection override (boundary ④ physical injection rail).
// raw = MCS51_ADC_RAIL_INJECT_NONE clears the override back to Pull mode.
void mcs51_adc_set_value(uint8_t key, uint16_t raw);

// Framework init: clear all injection overrides.
void mcs51_adc_reset(void);

// A-02 reference rail (GAP-05): Vref/Vrail in mV, set by the chip layer
// through these generic rail parameters (no ADCLDO knowledge in core).
// Pull-track conversion scales norm->raw by Vrail/Vref; injection rail
// bypasses scaling (deterministic).
void mcs51_adc_set_vref_mv(uint16_t mv);
void mcs51_adc_set_vrail_mv(uint16_t mv);
uint16_t mcs51_adc_get_vref_mv(void);
uint16_t mcs51_adc_get_vrail_mv(void);

#ifdef __cplusplus
}
#endif
