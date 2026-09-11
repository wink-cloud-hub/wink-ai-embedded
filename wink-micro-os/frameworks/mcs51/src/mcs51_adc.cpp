// SPDX-License-Identifier: Apache-2.0
// MCS-51 analog channel-3 rail implementation (boundary ④, AD-8 / ADR-0057).
// See mcs51_adc.h for the dual-track data path and the synthetic-pin routing
// convention (js_pal_adc_read_norm(32 + ch)).
#include "mcs51_adc.h"
#include "mcs51_context.h"

#include <stdint.h>

extern "C" {

// UniSim 3.0 channel-3 analog pull: normalized [0.0, 1.0]. JS import under
// emscripten (wink_sim_js.js / mcs51_wasm_node_stub.js); host fallback in
// mcs51_uni_bridge.cpp returns 0.0 (tests inject via mcs51_adc_set_value).
float js_pal_adc_read_norm(uint16_t pin);

// M2: injection rail lives in Mcu51Context (adc_injected/adc_inject_flag),
// so two contexts never share test overrides. Zero flag = "not injected"
// (Pull track); mcs51_context_reset() memsets the whole context, and
// mcs51_adc_reset() clears the ACTIVE context's rail between runs.
void mcs51_adc_reset(void) {
    Mcu51Context* ctx = mcs51_get_context();
    for (unsigned ch = 0; ch < MCS51_ADC_MAX_CHANNELS; ++ch) {
        ctx->adc_inject_flag[ch] = 0u;
        ctx->adc_injected[ch] = 0u;
    }
}

void mcs51_adc_set_value(uint8_t ch, uint16_t raw) {
    if (ch >= MCS51_ADC_MAX_CHANNELS) {
        return;
    }
    Mcu51Context* ctx = mcs51_get_context();
    if (raw == MCS51_ADC_RAIL_INJECT_NONE) {
        ctx->adc_inject_flag[ch] = 0u;  // explicit clear back to Pull mode
        return;
    }
    ctx->adc_injected[ch] = raw;
    ctx->adc_inject_flag[ch] = 1u;
}

uint16_t mcs51_adc_get_value(uint8_t ch) {
    if (ch >= MCS51_ADC_MAX_CHANNELS) {
        return 0;
    }
    Mcu51Context* ctx = mcs51_get_context();
    if (ctx->adc_inject_flag[ch] != 0u) {
        return ctx->adc_injected[ch];
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
