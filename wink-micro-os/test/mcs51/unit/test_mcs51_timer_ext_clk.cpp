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
uint32_t g_t2_hits = 0;

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

WINK_ISR(5) {
    ++g_t2_hits;
}

extern "C" void wink_mcs51_user_main(void) {}
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}
extern "C" void mcs51_timer_init(struct Mcu51Context* ctx);
extern "C" void mcs51_timer_poll(struct Mcu51Context* ctx);
extern "C" void wink_mcs51_host_set_ext_pin(uint16_t pin, uint8_t state);

int main(void) {
    /* M1: Test 6 uses CMS8S-only Timer2 compare channels (CCEN/CCLx) with a
     * 24 MHz period assumption — select the family explicitly. */
    mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);
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

    // ── Test 4: Timer 2 16-bit Auto-Reload Counter Mode ─────────────────────
    // T2CON: T2In=2 (Counter mode), T2Rn=2 (Auto-reload) -> 0x12
    ctx->sfr_shadow[0xC8] = 0x12u; // T2CON
    wink_mcs51_timer_on_write(0xC8);

    // Initial counter & reload: 65534 (2 pulses from overflow)
    ctx->sfr_shadow[0xCA] = 0xFEu; // RLDL
    ctx->sfr_shadow[0xCB] = 0xFFu; // RLDH
    ctx->sfr_shadow[0xCC] = 0xFEu; // TL2
    ctx->sfr_shadow[0xCD] = 0xFFu; // TH2
    wink_mcs51_timer_on_write(0xCA);
    wink_mcs51_timer_on_write(0xCB);
    wink_mcs51_timer_on_write(0xCC);
    wink_mcs51_timer_on_write(0xCD);

    // Enable Timer 2 Overflow Interrupt (T2IE bit 7, SFR 0xCF) and global ET2 (IE bit 5)
    ctx->sfr_shadow[0xCF] = 0x80u; // T2IE
    ctx->sfr_shadow[0xA8] |= 0x20u; // ET2

    g_t2_hits = 0;

    // Pulse 1: TL2/TH2 increments to 0xFFFF
    wink_mcs51_timer_pulse(2);
    check(ctx->sfr_shadow[0xCC] == 0xFFu, "TL2 must be 0xFF after pulse 1");
    check(ctx->sfr_shadow[0xCD] == 0xFFu, "TH2 must be 0xFF after pulse 1");
    check(g_t2_hits == 0, "No overflow on pulse 1 for Timer 2");

    // Pulse 2: overflows to 0x0000, auto-reloads to 0xFFFE from RLDH:RLDL, dispatches ISR 5
    wink_mcs51_timer_pulse(2);
    check(ctx->sfr_shadow[0xCC] == 0xFEu, "TL2 must auto-reload to 0xFE from RLDL");
    check(ctx->sfr_shadow[0xCD] == 0xFFu, "TH2 must auto-reload to 0xFF from RLDH");
    check(g_t2_hits == 1, "Timer 2 ISR must be dispatched on overflow");
    check((ctx->sfr_shadow[0xC9] & 0x80u) != 0, "T2IF.T2F flag must be set on overflow");

    // Stop Timer 2 (T2In = 0)
    ctx->sfr_shadow[0xC8] = 0x00u;
    wink_mcs51_timer_on_write(0xC8);

    wink_mcs51_timer_pulse(2);
    check(ctx->sfr_shadow[0xCC] == 0xFEu, "TL2 must not increment when Timer 2 stopped");
    check(g_t2_hits == 1, "No ISR should be triggered when Timer 2 stopped");

    // ── Test 5: Timer 2 Capture Mode (CAP0) & W0C Semantics on T2IF ──────────
    // Reset context & timer
    mcs51_context_reset(ctx);
    mcs51_timer_init(ctx);
    wink_mcs51_isr_enable();
    g_t2_hits = 0;

    // 1) Test W0C: write 0x80 directly into T2IF
    ctx->sfr_shadow[0xC9] = 0x80u;
    // Software clears overflow flag by writing 0x7F (~0x80)
    // Hook must preserve write-0-to-clear semantics: old_val & new_val = 0x80 & 0x7F = 0x00
    ctx->sfr_write_hooks[0xC9](ctx, 0xC9, 0x80u, 0x7Fu);
    check(ctx->sfr_shadow[0xC9] == 0x00u, "T2IF W0C: writing ~0x80 must clear bit 7 without setting lower bits");

    // If T2IF is 0x81 (T2F and CC0 flags both set)
    ctx->sfr_shadow[0xC9] = 0x81u;
    ctx->sfr_write_hooks[0xC9](ctx, 0xC9, 0x81u, 0x7Fu);
    check(ctx->sfr_shadow[0xC9] == 0x01u, "T2IF W0C: clearing overflow must leave CC0 flag set");

    ctx->sfr_write_hooks[0xC9](ctx, 0xC9, 0x01u, 0xFEu);
    check(ctx->sfr_shadow[0xC9] == 0x00u, "T2IF W0C: clearing CC0 flag must yield 0");

    // 2) Configure Timer 2 Capture Mode CC0
    // CCEN: CC0 in capture mode 1 (bits 1:0 = 0x01)
    ctx->sfr_shadow[0xCE] = 0x01u; // SFR_CCEN
    // T2CON: timing mode (mode 1), rising edge (I3FR = bit 6)
    ctx->sfr_shadow[0xC8] = (1u << 6) | 0x01u;
    wink_mcs51_timer_on_write(0xC8);

    // Set counter value TL2/TH2 = 0x1234
    ctx->sfr_shadow[0xCC] = 0x34u;
    ctx->sfr_shadow[0xCD] = 0x12u;

    // Enable CC0 capture interrupt (T2IE bit 0) and global interrupts (EA=1, ET2=1)
    ctx->sfr_shadow[0xCF] = 0x01u;
    ctx->sfr_shadow[0xA8] = 0xA0u; // EA=1, ET2=1

    // Simulate rising edge on P0.0 (pin 0)
    // Baseline LOW (0)
    wink_mcs51_host_set_ext_pin(0, 0);
    mcs51_timer_poll(ctx);
    // Transition to HIGH (1)
    wink_mcs51_host_set_ext_pin(0, 1);
    mcs51_timer_poll(ctx);

    check((ctx->sfr_shadow[0xC9] & 0x01u) != 0, "T2IF.T2C0IF must be set on CAP0 rising edge");
    check(ctx->sfr_shadow[0xCA] == 0x34u, "RLDL must capture TL2 value (0x34)");
    check(ctx->sfr_shadow[0xCB] == 0x12u, "RLDH must capture TH2 value (0x12)");
    check(g_t2_hits == 1, "Timer 2 ISR must fire on CAP0 capture event");

    // ── Test 6: Timer 2 Compare Mode & Periodic Stepping ─────────────────────
    mcs51_context_reset(ctx);
    mcs51_timer_init(ctx);
    wink_mcs51_isr_enable();

    static int s_ovf_count = 0;
    static int s_cmp_count = 0;
    static uint8_t s_p32 = 0;
    static uint64_t s_last_ovf_us = 0;
    static uint64_t s_last_cmp_us = 0;

    s_ovf_count = 0;
    s_cmp_count = 0;
    s_p32 = 0;
    s_last_ovf_us = 0;
    s_last_cmp_us = 0;

    wink_mcs51_set_isr(5, []() {
        struct Mcu51Context* c = mcs51_get_context();
        uint8_t t2if = c->sfr_shadow[0xC9];
        uint64_t now = c->virtual_us;
        if (t2if & 0x80u) {
            s_ovf_count++;
            s_p32 ^= 1;
            (void)s_last_ovf_us;
            s_last_ovf_us = now;
            // TMR2_ConfigTimerPeriod(65536 - 2000)
            c->sfr_shadow[0xCC] = (uint8_t)(65536 - 2000);
            c->sfr_write_hooks[0xCC](c, 0xCC, 0, (uint8_t)(65536 - 2000));
            c->sfr_shadow[0xCD] = (uint8_t)((65536 - 2000) >> 8);
            c->sfr_write_hooks[0xCD](c, 0xCD, 0, (uint8_t)((65536 - 2000) >> 8));
            // TMR2_ClearOverflowIntFlag() -> write 0x7F to T2IF
            c->sfr_write_hooks[0xC9](c, 0xC9, c->sfr_shadow[0xC9], 0x7Fu);
        }
        for (int ch = 0; ch < 4; ++ch) {
            if (t2if & (1u << ch)) {
                s_cmp_count++;
                (void)s_last_cmp_us;
                s_last_cmp_us = now;
                // Clear compare flag
                c->sfr_write_hooks[0xC9](c, 0xC9, c->sfr_shadow[0xC9], static_cast<uint8_t>(~(1u << ch)));
            }
        }
    });

    // 1) Set run mode: TMR2_MODE_TIMING (0x01), TMR2_LOAD_DISBALE (0x00)
    ctx->sfr_shadow[0xC8] = 0x01u;
    // 2) CCEN: CC0..3 compare mode (0xAA)
    ctx->sfr_shadow[0xCE] = 0xAAu;
    // 3) Clock: TMR2_CLK_DIV_12 (T2PS=0)
    // 4) Period: 65536 - 2000
    ctx->sfr_shadow[0xCC] = (uint8_t)(65536 - 2000);
    ctx->sfr_shadow[0xCD] = (uint8_t)((65536 - 2000) >> 8);
    // 5) Compare values: 65536 - 1000
    ctx->sfr_shadow[0xCA] = (uint8_t)(65536 - 1000); // RLDL
    ctx->sfr_shadow[0xCB] = (uint8_t)((65536 - 1000) >> 8); // RLDH
    ctx->sfr_shadow[0xC2] = (uint8_t)(65536 - 1000); // CCL1
    ctx->sfr_shadow[0xC3] = (uint8_t)((65536 - 1000) >> 8); // CCH1
    ctx->sfr_shadow[0xC4] = (uint8_t)(65536 - 1000); // CCL2
    ctx->sfr_shadow[0xC5] = (uint8_t)((65536 - 1000) >> 8); // CCH2
    ctx->sfr_shadow[0xC6] = (uint8_t)(65536 - 1000); // CCL3
    ctx->sfr_shadow[0xC7] = (uint8_t)((65536 - 1000) >> 8); // CCH3
    // 6) Interrupts: Overflow + CC0..3
    ctx->sfr_shadow[0xCF] = 0x8Fu; // T2IE
    ctx->sfr_shadow[0xA8] = 0xA0u; // EA=1, ET2=1

    // Start Timer 2
    ctx->sfr_write_hooks[0xC8](ctx, 0xC8, 0x00u, 0x01u);
    ctx->sfr_write_hooks[0xCC](ctx, 0xCC, 0, (uint8_t)(65536 - 2000));
    ctx->sfr_write_hooks[0xCD](ctx, 0xCD, 0, (uint8_t)((65536 - 2000) >> 8));

    // Step through 20ms in 100us intervals
    for (uint64_t t = 100; t <= 20000; t += 100) {
        ctx->virtual_us = t;
        wink_mcs51_timers_step_to(t);
    }

    check(s_ovf_count == 20, "Timer 2 compare mode: exactly 20 overflows in 20ms (1ms period)");
    check(s_cmp_count == 80, "Timer 2 compare mode: 4 channels * 20 periods = 80 compare matches");
    check(s_p32 == 0, "P3.2 toggles 20 times (500Hz square wave), ending at 0");

    if (g_fails != 0) {
        printf("[mcs51-ext-clk] FAILED with %d errors\n", g_fails);
        return 1;
    }
    printf("[mcs51-ext-clk] PASS: TMOD C/T=1, Timer 2 counter, capture, compare mode, and W0C verified\n");
    return 0;
}
