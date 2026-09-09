// SPDX-License-Identifier: Apache-2.0
// Task R2: Mcu51Context container instance and silicon reset implementation.
#include "mcs51_context.h"
#include "mcs51_peripheral.h"

#include <cstring>

static Mcu51Context s_default_mcu_context = {};
Mcu51Context* g_active_mcu_context = &s_default_mcu_context;

extern "C" {

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
}

} // extern "C"
