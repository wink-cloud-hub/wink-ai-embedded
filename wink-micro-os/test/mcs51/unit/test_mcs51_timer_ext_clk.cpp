// SPDX-License-Identifier: Apache-2.0
// Task F2: TMOD C/T=1 external pulse counter modeling test.
#include <stdint.h>
#include <stdio.h>
#include <cstring>

#include "mcs51_context.h"
#include "wink_mcs51_isr.h"
#include "wink_mcs51_timer.h"

namespace {

int g_fails = 0;
uint32_t g_t0_hits = 0;
uint32_t g_t1_hits = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        printf("[mcs51-ext-clk] FAIL: %s\n", what);
        ++g_fails;
    }
}

} // namespace

WINK_ISR(1) {
    ++g_t0_hits;
}

WINK_ISR(3) {
    ++g_t1_hits;
}

extern "C" void wink_mcs51_user_main(void) {}
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

int main(void) {
    Mcu51Context* ctx = mcs51_get_context();
    mcs51_context_reset(ctx);
    wink_mcs51_isr_enable();

    // ── Test 1: Timer 0 in Mode 1 (16-bit Counter, C/T=1) ─────────────────────
    // TMOD: bit 2 is C/T0, bit 0..1 is Mode 1 -> TMOD = 0x05 (Mode 1, C/T=1)
    ctx->sfr_shadow[0x89] = 0x05u;
    wink_mcs51_timer_on_write(0x89);

    // Initial counter value: 0xFFFE (2 pulses away from overflow 0x0000)
    ctx->sfr_shadow[0x8A] = 0xFEu; // TL0
    ctx->sfr_shadow[0x8C] = 0xFFu; // TH0
    wink_mcs51_timer_on_write(0x8A);
    wink_mcs51_timer_on_write(0x8C);

    // Enable Timer 0 (TR0 = 1, TCON bit 4)
    ctx->sfr_shadow[0x88] = 0x10u;
    wink_mcs51_timer_on_write(0x88);

    // Enable Timer 0 Interrupt (EA=1, ET0=1)
    ctx->sfr_shadow[0xA8] = 0x82u;

    g_t0_hits = 0;

    // Pulse 1: Counter should increment to 0xFFFF
    wink_mcs51_timer_pulse(0);
    check(ctx->sfr_shadow[0x8A] == 0xFFu, "TL0 must be 0xFF after 1 pulse");
    check(ctx->sfr_shadow[0x8C] == 0xFFu, "TH0 must be 0xFF after 1 pulse");
    check(g_t0_hits == 0, "No overflow yet on pulse 1");

    // Pulse 2: Counter should overflow to 0x0000 and fire ISR
    wink_mcs51_timer_pulse(0);
    check(ctx->sfr_shadow[0x8A] == 0x00u, "TL0 must wrap to 0x00 after overflow");
    check(ctx->sfr_shadow[0x8C] == 0x00u, "TH0 must wrap to 0x00 after overflow");
    check(g_t0_hits == 1, "Timer 0 ISR must be dispatched on overflow");

    // ── Test 2: Timer 1 in Mode 2 (8-bit Auto-Reload Counter, C/T=1) ──────────
    // TMOD: bit 6 is C/T1, bits 4..5 is Mode 2 (0x02 << 4) -> TMOD = 0x60
    ctx->sfr_shadow[0x89] = 0x60u;
    wink_mcs51_timer_on_write(0x89);

    // TH1 = 0xA0 (reload value), TL1 = 0xFE (2 pulses away)
    ctx->sfr_shadow[0x8D] = 0xA0u; // TH1
    ctx->sfr_shadow[0x8B] = 0xFEu; // TL1
    wink_mcs51_timer_on_write(0x8D);
    wink_mcs51_timer_on_write(0x8B);

    // Enable Timer 1 (TR1 = 1, TCON bit 6) and interrupt (EA=1, ET1=1 -> IE bit 3)
    ctx->sfr_shadow[0x88] |= 0x40u;
    wink_mcs51_timer_on_write(0x88);
    ctx->sfr_shadow[0xA8] = 0x88u;

    g_t1_hits = 0;

    // Pulse 1: TL1 becomes 0xFF
    wink_mcs51_timer_pulse(1);
    check(ctx->sfr_shadow[0x8B] == 0xFFu, "TL1 must be 0xFF after 1 pulse");
    check(g_t1_hits == 0, "No overflow on pulse 1 for Timer 1");

    // Pulse 2: TL1 overflows and auto-reloads from TH1 (0xA0)
    wink_mcs51_timer_pulse(1);
    check(ctx->sfr_shadow[0x8B] == 0xA0u, "TL1 must auto-reload to 0xA0 from TH1");
    check(g_t1_hits == 1, "Timer 1 ISR must be dispatched on overflow");

    // ── Test 3: TR = 0 stops pulse counting ───────────────────────────────────
    ctx->sfr_shadow[0x88] &= ~0x40u; // TR1 = 0
    wink_mcs51_timer_on_write(0x88);

    wink_mcs51_timer_pulse(1);
    check(ctx->sfr_shadow[0x8B] == 0xA0u, "TL1 must not increment when TR1=0");
    check(g_t1_hits == 1, "No ISR should be triggered when TR1=0");

    if (g_fails != 0) {
        printf("[mcs51-ext-clk] FAILED with %d errors\n", g_fails);
        return 1;
    }
    printf("[mcs51-ext-clk] PASS: TMOD C/T=1 external pulse counter verified\n");
    return 0;
}
