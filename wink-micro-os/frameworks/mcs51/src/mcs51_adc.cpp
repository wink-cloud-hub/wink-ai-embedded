// SPDX-License-Identifier: Apache-2.0
// MCS-51 analog channel-3 rail implementation (boundary ④, AD-8 / ADR-0057).
// See mcs51_adc.h for the dual-track data path and the synthetic-pin routing
// convention (js_pal_adc_read_norm(32 + ch)).
#include "mcs51_adc.h"

#include <stdint.h>

extern "C" {

// UniSim 3.0 channel-3 analog pull: normalized [0.0, 1.0]. JS import under
// emscripten (wink_sim_js.js / mcs51_wasm_node_stub.js); host fallback in
// mcs51_uni_bridge.cpp returns 0.0 (tests inject via mcs51_adc_set_value).
float js_pal_adc_read_norm(uint16_t pin);

// BSS injection rail: s_inject_flag[] zero-init means "not injected" (Pull
// track), so the rail is correct from load with zero dynamic initialization —
// mcs51_adc_reset() only needs to clear flags between runs.
static uint16_t s_injected[MCS51_ADC_MAX_CHANNELS];
static uint8_t  s_inject_flag[MCS51_ADC_MAX_CHANNELS];

void mcs51_adc_reset(void) {
    for (unsigned ch = 0; ch < MCS51_ADC_MAX_CHANNELS; ++ch) {
        s_inject_flag[ch] = 0u;
        s_injected[ch] = 0u;
    }
}

void mcs51_adc_set_value(uint8_t ch, uint16_t raw) {
    if (ch >= MCS51_ADC_MAX_CHANNELS) {
        return;
    }
    if (raw == MCS51_ADC_RAIL_INJECT_NONE) {
        s_inject_flag[ch] = 0u;  // explicit clear back to Pull mode
        return;
    }
    s_injected[ch] = raw;
    s_inject_flag[ch] = 1u;
}

uint16_t mcs51_adc_get_value(uint8_t ch) {
    if (ch >= MCS51_ADC_MAX_CHANNELS) {
        return 0;
    }
    if (s_inject_flag[ch] != 0u) {
        return s_injected[ch];
    }
    // Production Pull track: instant channel-3 sample → 12-bit code value
    // (CMS8S78xx native width; the 8-bit ADC0832 masks the low byte in its
    // own shim).
    float norm = js_pal_adc_read_norm((uint16_t)(32u + ch));
    if (norm < 0.0f) {
        norm = 0.0f;
    } else if (norm > 1.0f) {
        norm = 1.0f;
    }
    uint32_t raw = (uint32_t)(norm * 4095.0f + 0.5f);
    if (raw > 4095u) {
        raw = 4095u;
    }
    return (uint16_t)raw;
}

}  // extern "C"
