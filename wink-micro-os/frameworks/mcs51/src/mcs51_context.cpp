// SPDX-License-Identifier: Apache-2.0
// Task R2: Mcu51Context container instance and silicon reset implementation.
#include "mcs51_context.h"
#include "mcs51_peripheral.h"

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
// init/reset (GAP-04 CKCON reset value, GAP-13 power-on Fosc).
void apply_silicon_seeds(Mcu51Context* ctx) {
    if (s_mcu_family == MCS51_FAMILY_CMS8S78XX) {
        // Ref manual §8.2.2: CKCON reset = 0x07 (WTS=000, T1M=T0M=1,
        // Timer0/1 clock Fsys/4 out of reset).
        ctx->sfr_shadow[0x8E] = 0x07u;
        // CMS8S78xx power-on internal RC = 24 MHz (datasheet ±1%). Set the
        // field on the reset context directly (reset may target a context
        // other than the active one). The SFR-access billing quantum stays
        // on its 12 MHz-calibrated budget deliberately (ADR-0072).
        ctx->clock_hz = 24000000u;
    } else {
        // Classic 8052 has no CKCON SFR; timers are fixed at Fsys/12.
        // Shadow stays 0 (counts_to_us picks the /12 divider). Leave
        // clock_hz at 0 so the 12 MHz family defaults remain in effect.
        ctx->sfr_shadow[0x8E] = 0x00u;
    }
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

    // Zero entire context memory
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

    // XSFR pin share selector seeds (0x7F = no pin connected)
    ctx->xdata_shadow[0xF0CC] = 0x7Fu; // PS_ADET
    ctx->xdata_shadow[0xF0C0] = 0x7Fu; // PS_INT0
    ctx->xdata_shadow[0xF0C1] = 0x7Fu; // PS_INT1
    ctx->xdata_shadow[0xF0C2] = 0x7Fu; // PS_T0
    ctx->xdata_shadow[0xF0C3] = 0x7Fu; // PS_T0G
    ctx->xdata_shadow[0xF0C4] = 0x7Fu; // PS_T1
    ctx->xdata_shadow[0xF0C5] = 0x7Fu; // PS_T1G
    ctx->xdata_shadow[0xF0C6] = 0x7Fu; // PS_T2
    ctx->xdata_shadow[0xF0C7] = 0x7Fu; // PS_T2EX
    ctx->xdata_shadow[0xF0C8] = 0x7Fu; // PS_CAP0
    ctx->xdata_shadow[0xF0C9] = 0x7Fu; // PS_CAP1
    ctx->xdata_shadow[0xF0CA] = 0x7Fu; // PS_CAP2
    ctx->xdata_shadow[0xF0CB] = 0x7Fu; // PS_CAP3

    // Initialize standard peripherals via descriptor table (Task R1)
    for (uint8_t i = 0; i < g_mcs51_num_peripherals; ++i) {
        if (g_mcs51_peripherals[i].init != nullptr) {
            g_mcs51_peripherals[i].init(ctx);
        }
    }

    // Reset standard peripherals via descriptor table (Task R1)
    for (uint8_t i = 0; i < g_mcs51_num_peripherals; ++i) {
        if (g_mcs51_peripherals[i].reset != nullptr) {
            g_mcs51_peripherals[i].reset(ctx);
        }
    }

    // Family-specific silicon seeds last: CKCON reset value / power-on Fosc
    // must not be disturbed by peripheral resets (GAP-04/GAP-13).
    apply_silicon_seeds(ctx);
}

} // extern "C"
