// SPDX-License-Identifier: Apache-2.0
// Task R2: Mcu51Context container instance and silicon reset implementation.
#include "mcs51_context.h"
#include "mcs51_peripheral.h"
#include "mcs51_sfr_map.h"

#include <cstring>

static Mcu51Context s_default_mcu_context = {};
Mcu51Context* g_active_mcu_context = &s_default_mcu_context;

namespace {

// Compile-time family default (production wasm builds define WINK_MCU_*
// directory-scoped; host tests override through mcs51_context_set_family).
uint8_t s_mcu_family =
#if defined(WINK_MCU_CMS8S78XX)
    MCS51_FAMILY_CMS8S78XX;
#else
    MCS51_FAMILY_CLASSIC;
#endif

// Family-specific silicon seeds applied AFTER memset + peripheral
// init/reset (GAP-04 CKCON reset value, GAP-13 power-on Fosc). Facts come
// from the family descriptor (M1); no per-series branch here.
void apply_silicon_seeds(Mcu51Context* ctx) {
    const mcs51_family_desc_t* d = mcs51_family_desc(s_mcu_family);
    ctx->family = s_mcu_family;
    ctx->sfr_shadow[MCS51_SFR_CKCON] = d->ckcon_reset;
    if (d->fosc_hz != 0u) {
        // Fixed on-chip RC (e.g. CMS8S 24 MHz ±1%). Set the field on the
        // reset context directly (reset may target a context other than
        // the active one). The SFR-access billing quantum stays on its
        // 12 MHz-calibrated budget deliberately (ADR-0072).
        ctx->clock_hz = d->fosc_hz;
    }
    // Else (classic: board crystal): leave clock_hz at 0 so the 12 MHz
    // family default remains in effect; CKCON shadow stays 0x00
    // (counts_to_us picks the /12 divider — no CKCON on classic 8052).
}

}  // namespace

extern "C" {

void mcs51_context_set_family(uint8_t family) {
    s_mcu_family = family;
    apply_silicon_seeds(mcs51_get_context());
}

uint8_t mcs51_context_get_family(void) {
    return s_mcu_family;
}

void mcs51_context_reset(Mcu51Context* ctx) {
    if (ctx == nullptr) {
        return;
    }

    // Preserve registered ISR table, external pin baseline and interrupt trigger mode across hardware reset (world state, ADR-0076)
    void (*saved_isrs[28])(void);
    std::memcpy(saved_isrs, ctx->isr_table, sizeof(saved_isrs));

    Mcu51ExtIntLine saved_lines[2];
    Mcu51PortPinState saved_port_pins[4][8];
    std::memcpy(saved_lines, ctx->extint.lines, sizeof(saved_lines));
    std::memcpy(saved_port_pins, ctx->extint.port_pins, sizeof(saved_port_pins));
    uint8_t saved_tcon_it = ctx->sfr_shadow[0x88] & ((1u << 0) | (1u << 2));

    // Zero entire context memory (also clears family + §8 model states,
    // which are per-instance fields since M2 — no file-static to clean).
    std::memset(ctx, 0, sizeof(Mcu51Context));

    // Restore ISR table and external pin baseline
    std::memcpy(ctx->isr_table, saved_isrs, sizeof(saved_isrs));
    std::memcpy(ctx->extint.lines, saved_lines, sizeof(saved_lines));
    std::memcpy(ctx->extint.port_pins, saved_port_pins, sizeof(saved_port_pins));
    ctx->sfr_shadow[0x88] |= saved_tcon_it;

    // ── Silicon Reset Seeds (Task R2) ────────────────────────────────────────
    // Ports P0..P3 power on as 0xFF (quasi-bidirectional inputs, weak pull-up)
    ctx->sfr_shadow[0x80] = 0xFFu; // P0
    ctx->sfr_shadow[0x90] = 0xFFu; // P1
    ctx->sfr_shadow[0xA0] = 0xFFu; // P2
    ctx->sfr_shadow[0xB0] = 0xFFu; // P3

    // Standard 8051 SFR seeds
    ctx->sfr_shadow[0x81] = 0x07u; // SP = 0x07
    ctx->sfr_shadow[0x87] = 0x00u; // PCON = 0x00

    // XSFR pin share selector seeds (MCS51_XSFR_PS_RESET = no pin connected).
    // Addresses from mcs51_sfr_map.h (M5 single source).
    ctx->xdata_shadow[MCS51_XSFR_PS_ADET] = MCS51_XSFR_PS_RESET;
    ctx->xdata_shadow[MCS51_XSFR_PS_INT0] = MCS51_XSFR_PS_RESET;
    ctx->xdata_shadow[MCS51_XSFR_PS_INT1] = MCS51_XSFR_PS_RESET;
    ctx->xdata_shadow[MCS51_XSFR_PS_T0] = MCS51_XSFR_PS_RESET;
    ctx->xdata_shadow[MCS51_XSFR_PS_T0G] = MCS51_XSFR_PS_RESET;
    ctx->xdata_shadow[MCS51_XSFR_PS_T1] = MCS51_XSFR_PS_RESET;
    ctx->xdata_shadow[MCS51_XSFR_PS_T1G] = MCS51_XSFR_PS_RESET;
    ctx->xdata_shadow[MCS51_XSFR_PS_T2] = MCS51_XSFR_PS_RESET;
    ctx->xdata_shadow[MCS51_XSFR_PS_T2EX] = MCS51_XSFR_PS_RESET;
    ctx->xdata_shadow[MCS51_XSFR_PS_CAP0] = MCS51_XSFR_PS_RESET;
    ctx->xdata_shadow[MCS51_XSFR_PS_CAP1] = MCS51_XSFR_PS_RESET;
    ctx->xdata_shadow[MCS51_XSFR_PS_CAP2] = MCS51_XSFR_PS_RESET;
    ctx->xdata_shadow[MCS51_XSFR_PS_CAP3] = MCS51_XSFR_PS_RESET;

    // Record the family on the context BEFORE peripheral init so models
    // observe a consistent family for the whole reset.
    ctx->family = s_mcu_family;

    // Initialize peripherals via descriptor table (Task R1), filtered by
    // family (M1): series models never install hooks on another family.
    for (uint8_t i = 0; i < g_mcs51_num_peripherals; ++i) {
        if (!mcs51_peripheral_active_for(&g_mcs51_peripherals[i], ctx->family)) {
            continue;
        }
        if (g_mcs51_peripherals[i].init != nullptr) {
            g_mcs51_peripherals[i].init(ctx);
        }
    }

    // Reset peripherals via descriptor table (Task R1), same filter.
    for (uint8_t i = 0; i < g_mcs51_num_peripherals; ++i) {
        if (!mcs51_peripheral_active_for(&g_mcs51_peripherals[i], ctx->family)) {
            continue;
        }
        if (g_mcs51_peripherals[i].reset != nullptr) {
            g_mcs51_peripherals[i].reset(ctx);
        }
    }

    // Family-specific silicon seeds last: CKCON reset value / power-on Fosc
    // must not be disturbed by peripheral resets (GAP-04/GAP-13).
    apply_silicon_seeds(ctx);
}

} // extern "C"
