// SPDX-License-Identifier: Apache-2.0
// MCS-51 analog rail-key implementation (boundary ④, AD-8 / ADR-0057).
// See mcs51_adc.h for the dual-track data path and the dual-space key
// partition (Stage1: MCU pins 0~31, board channels 32~63, no mapping here).
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
    for (unsigned key = 0; key < MCS51_ADC_MAX_RAIL_KEYS; ++key) {
        ctx->adc_inject_flag[key] = 0u;
        ctx->adc_injected[key] = 0u;
    }
    // S2-1: clears the injection table ONLY (CPL-19). Reference defaults
    // live with the chip layer (injected by chip reset); classic has none.
}

void mcs51_adc_set_vref_mv(uint16_t mv) {
    mcs51_get_context()->adc_vref_mv = (mv != 0u) ? mv : 3000u;
}

void mcs51_adc_set_vrail_mv(uint16_t mv) {
    mcs51_get_context()->adc_vrail_mv = (mv != 0u) ? mv : 3000u;
}

uint16_t mcs51_adc_get_vref_mv(void) {
    return mcs51_get_context()->adc_vref_mv;
}

uint16_t mcs51_adc_get_vrail_mv(void) {
    return mcs51_get_context()->adc_vrail_mv;
}

void mcs51_adc_set_value(uint8_t key, uint16_t raw) {
    if (key >= MCS51_ADC_MAX_RAIL_KEYS) {
        return;
    }
    Mcu51Context* ctx = mcs51_get_context();
    if (raw == MCS51_ADC_RAIL_INJECT_NONE) {
        ctx->adc_inject_flag[key] = 0u;  // explicit clear back to Pull mode
        return;
    }
    ctx->adc_injected[key] = raw;
    ctx->adc_inject_flag[key] = 1u;
}

uint16_t mcs51_adc_get_value(uint8_t key) {
    if (key >= MCS51_ADC_MAX_RAIL_KEYS) {
        return 0;
    }
    Mcu51Context* ctx = mcs51_get_context();
    if (ctx->adc_inject_flag[key] != 0u) {
        return ctx->adc_injected[key];
    }
    // Production Pull track: instant rail-key sample → 12-bit code value
    // (CMS8S78xx native width; the 8-bit ADC0832 masks the low byte in its
    // own shim). The key passes through untouched: 0~31 reads the MCU
    // physical pin, 32~63 the board channel. A-02 (GAP-05): scale by
    // Vrail/Vref so a VDD-railed NTC divider reads higher than a
    // VREF-railed one at the same ratio.
    // Injection rail bypasses scaling (deterministic test codes).
    float norm = js_pal_adc_read_norm((uint16_t)key);
    if (norm < 0.0f) {
        norm = 0.0f;
    } else if (norm > 1.0f) {
        norm = 1.0f;
    }
    const uint32_t vref = (ctx->adc_vref_mv != 0u) ? ctx->adc_vref_mv : 3000u;
    const uint32_t vrail = (ctx->adc_vrail_mv != 0u) ? ctx->adc_vrail_mv : 3000u;
    uint32_t raw = (uint32_t)(norm * 4095.0f + 0.5f);
    raw = (raw * vrail) / vref;
    if (raw > 4095u) {
        raw = 4095u;
    }
    return (uint16_t)raw;
}

}  // extern "C"
