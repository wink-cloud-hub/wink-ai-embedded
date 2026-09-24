// SPDX-License-Identifier: LGPL-3.0-only
// CMS8S78xx Low-Voltage Detect (LVD) peripheral model (XSFR LVDCON @ 0xF690).
//
// Behavioral model notes (honesty record, see PLAN-20260924-CMS8S78XX-LVD):
//   * Hysteresis: the datasheet only states "an interrupt request is generated
//     when VDD falls below the threshold" with no hysteresis value or edge
//     polarity. The 100 mV re-arm band below is a BEHAVIORAL modeling choice
//     (anti interrupt-storm + re-arm), NOT a silicon-calibrated value.
//   * VDD stimulus arrives through a virtual sense rail key (board-channel
//     space, adcChannel 62), NOT a silicon sense pin: it is the behavioral
//     injection seam for VDD monitoring. The rail key is 1:1 with the
//     INPUT_ANALOG adcChannel (same convention as the ACMP scenarios, where
//     adcChannel 9 drives pin key 9). Key 62 (>31, <64) avoids every physical
//     MCU pin and the ADC0832 board keys; keys >= 64 do not exist on the
//     64-entry injection rail and must never be used here.
#include "cms8s_lvd.h"

#include "cms8s_priv.h"
#include "mcs51_context.h"
#include "mcs51_family.h"
#include "wink_mcs51_isr.h"

#include <cstdint>

extern "C" {
float js_pal_adc_read_norm(uint16_t pin);
}

namespace {

constexpr uint16_t XSFR_LVDCON = 0xF690u;

// Virtual VDD sense rail key == headless INPUT_ANALOG adcChannel (see above).
constexpr uint16_t LVD_VDD_SENSE_PIN = 62u;

// LVDSEL[3:0] -> threshold in millivolts (vendor system.h SYS_LVD_* values).
constexpr uint16_t kLvdThresholdMv[16] = {
    2000u, 2160u, 2310u, 2450u, 2600u, 2730u, 2880u, 2980u,
    3210u, 3420u, 3620u, 3810u, 4000u, 4200u, 4430u, 4600u,
};

// Behavioral re-arm hysteresis in millivolts (NOT a datasheet value).
constexpr uint16_t kLvdHysteresisMv = 100u;

constexpr uint8_t LVD_STATE_NORMAL = 0u;
constexpr uint8_t LVD_STATE_UNDERVOLT = 1u;

static float read_vdd_norm(Mcu51Context* ctx) {
    if (ctx->adc_inject_flag[LVD_VDD_SENSE_PIN] != 0u) {
        return static_cast<float>(ctx->adc_injected[LVD_VDD_SENSE_PIN]) / 4095.0f;
    }
    float norm = js_pal_adc_read_norm(LVD_VDD_SENSE_PIN);
    if (norm <= 0.0005f) {
        // No stimulus injected on this key (host fallback / JS default 0.0):
        // assume nominal 5.0 V so boot never sees a phantom falling edge.
        return 1.0f;
    }
    return norm;
}

}  // namespace

extern "C" {

void cms8s_lvd_init(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    if (!ctx) return;
    cms8s_soc_bind(ctx);
    Cms8sPriv* priv = cms8s_priv(ctx);
    if (priv) {
        priv->lvd.state = LVD_STATE_NORMAL;
        priv->lvd.lvdintf = 0u;
        priv->lvd.vdd_norm = 1.0f;
        priv->lvd.last_poll_us = 0u;
    }
}

void cms8s_lvd_reset(struct Mcu51Context* ctx) {
    cms8s_lvd_init(ctx);
}

void cms8s_lvd_poll(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    if (!ctx || ctx->family != MCS51_FAMILY_CMS8S78XX) return;

    Cms8sPriv* priv = cms8s_priv(ctx);
    if (!priv) return;

    // LVDINTF is write-0-to-clear (W0C): firmware clears by writing 0 while
    // 1-bits preserve. Settle any pending firmware write first (zeroes
    // clear, ones preserve), run edge detection, then publish the effective
    // flag bit back to the shadow. There is no per-address XSFR write trap,
    // so the settle happens here (same pattern as ACMP CNIF @ 0xF509).
    priv->lvd.lvdintf &= static_cast<uint8_t>(ctx->xdata_shadow[XSFR_LVDCON] & 0x01u);

    const uint8_t lvdcon = ctx->xdata_shadow[XSFR_LVDCON];
    const uint16_t thresh_mv = kLvdThresholdMv[(lvdcon >> 4) & 0x0Fu];
    const bool enabled = (lvdcon & 0x08u) != 0u;  // LVDEN
    const bool int_en = (lvdcon & 0x02u) != 0u;   // LVDINTE

    const float vdd_norm = read_vdd_norm(ctx);
    priv->lvd.vdd_norm = vdd_norm;
    const uint32_t vdd_mv = static_cast<uint32_t>(vdd_norm * 5000.0f + 0.5f);

    if (!enabled) {
        // Module off: comparator frozen. Silently align the state to the
        // present level (no latch, no raise) so re-enabling while already
        // low cannot synthesize a bogus edge.
        priv->lvd.state = (vdd_mv < thresh_mv) ? LVD_STATE_UNDERVOLT : LVD_STATE_NORMAL;
    } else if (priv->lvd.state == LVD_STATE_NORMAL) {
        if (vdd_mv < thresh_mv) {
            // Single falling-edge latch: the ONLY raise site. A sustained
            // undervoltage never re-fires until the level recovers past the
            // hysteresis band (anti interrupt-storm).
            priv->lvd.state = LVD_STATE_UNDERVOLT;
            priv->lvd.lvdintf = 1u;
            if (int_en) {
                mcs51_raise_irq(IRQ_SOURCE_LVD);
            }
        }
    } else {
        if (vdd_mv >= static_cast<uint32_t>(thresh_mv) + kLvdHysteresisMv) {
            // Recovered past Vth+Vhys: re-arm the next falling-edge
            // detection. The latched LVDINTF is left for firmware to clear.
            priv->lvd.state = LVD_STATE_NORMAL;
        }
    }

    // Publish the settled W0C flag bit (model-owned) back to the shadow;
    // configuration bits stay firmware-owned. Reserved bit 2 reads 0.
    uint8_t shadow = ctx->xdata_shadow[XSFR_LVDCON];
    shadow = static_cast<uint8_t>((shadow & 0xFEu) | (priv->lvd.lvdintf & 0x01u));
    shadow = static_cast<uint8_t>(shadow & ~0x04u);
    ctx->xdata_shadow[XSFR_LVDCON] = shadow;
}

uint64_t cms8s_lvd_next_event_us(struct Mcu51Context* ctx) {
    (void)ctx;
    return UINT64_MAX;
}

void cms8s_lvd_set_vdd_mv(struct Mcu51Context* ctx, uint16_t mv) {
    if (!ctx) ctx = mcs51_get_context();
    if (!ctx) return;
    if (mv > 5000u) mv = 5000u;
    ctx->adc_injected[LVD_VDD_SENSE_PIN] = static_cast<uint16_t>((static_cast<uint32_t>(mv) * 4095u) / 5000u);
    ctx->adc_inject_flag[LVD_VDD_SENSE_PIN] = 1u;
}

}  // extern "C"
