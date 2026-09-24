// SPDX-License-Identifier: GPL-3.0-only
// PLAN-20260924-MCS51-T01-OVERFLOW-REARM-FIX regression: T0/T1 overflow
// re-arm ordering (ADR-0078 eager dispatch).
//
// on_overflow() runs the ISR from inside the timer model. The ISR's proxied
// SFR accesses synchronously pump wink_mcs51_microstep(), which re-enters
// step_timer(); if the fired deadline is still armed at `at_us`, the same
// overflow fires again and the duplicate pending request runs the ISR body a
// second time per period (measured on the vendor apps: ~2x frequency).
//
// These cases pin the exact dispatch count for the two ISR shapes that expose
// the re-entrancy:
//   * T0 mode 2 (auto-reload): ISR only toggles a pin (no TH0/TL0 write);
//   * T1 mode 1 (software reload): ISR toggles BEFORE writing TH1/TL1.
// Both must dispatch exactly once per timer period; the pre-fix code
// dispatches twice (case A) / ~1.8x (case B).
#include <stdint.h>
#include <stdio.h>

#include "cms8s78xx.h"
#include "mcs51_context.h"
#include "mcs51_test_harness.h"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_isr.h"

#undef main
#undef printf
extern "C" void wink_mcs51_user_main(void) {}
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

namespace {

int g_fails = 0;
uint32_t g_t0_body_runs = 0;
uint32_t g_t1_body_runs = 0;
bool g_t1_reload_in_isr = true;

void check(bool cond, const char *msg) {
    if (!cond) {
        printf("[mcs51-rearm] FAIL: %s\n", msg);
        ++g_fails;
    }
}

// Advance the firmware virtual clock in microstep quanta until `budget_us`.
void run_to_us(Mcu51Context *ctx, uint64_t budget_us) {
    uint32_t guard = 0u;
    while (ctx->virtual_us < budget_us && guard < 10000000u) {
        wink_mcs51_microstep();
        ++guard;
    }
}

void fresh_context(void) {
    mcs51_test_use_family(MCS51_FAMILY_CMS8S78XX);
    wink_mcs51_reset_irq_state();
    wink_mcs51_isr_enable();
    g_t0_body_runs = 0u;
    g_t1_body_runs = 0u;
}

}  // namespace

// T0 ISR: mode-2 shape - pin toggle only, NO TH0/TL0 write.
WINK_ISR(1) {
    ++g_t0_body_runs;
    P32 = ~P32;
}

// T1 ISR: mode-1 shape - pin toggle FIRST, then the software reload.
WINK_ISR(3) {
    ++g_t1_body_runs;
    P32 = ~P32;
    if (g_t1_reload_in_isr) {
        TH1 = static_cast<uint8_t>((65536u - 200u) >> 8);
        TL1 = static_cast<uint8_t>((65536u - 200u) & 0xFFu);
    }
}

int main(void) {
    printf("[mcs51-rearm] Starting T0/T1 overflow re-arm regression tests...\n");

    // ── Case A: T0 mode 2 auto-reload, toggle-only ISR (the vendor shape) ──
    {
        fresh_context();
        Mcu51Context *ctx = mcs51_get_context();
        TMOD = 0x02;  // T0 mode 2 (8-bit auto-reload), internal clock
        TH0 = static_cast<uint8_t>(256u - 200u);  // 200 counts = 100us
        TL0 = 0;
        P3TRIS = 0x04;  // P3.2 output (the vendor app enables it)
        ET0 = 1;
        EA = 1;
        TR0 = 1;  // start
        const uint64_t t0 = ctx->virtual_us;
        run_to_us(ctx, t0 + 10000u);  // 10 ms -> ~100 overflows
        const uint32_t runs = g_t0_body_runs;
        const uint32_t count = wink_mcs51_isr_dispatch_count(1u);
        const uint64_t elapsed = ctx->virtual_us - t0;
        const uint32_t nominal = static_cast<uint32_t>(elapsed / 100u);
        printf("[mcs51-rearm] A t0_mode2: body=%u dispatch=%u nominal=%u\n",
               (unsigned)runs, (unsigned)count, (unsigned)nominal);
        check(count == runs, "A: ISR body runs != dispatch count");
        check(count <= nominal + 2u,
              "A: T0 mode2 dispatched more than once per period "
              "(overflow re-arm regression)");
        check(count + 2u >= nominal,
              "A: T0 mode2 dispatched too few times (timer stalled)");
    }

    // ── Case B: T1 mode 1 software reload, toggle-before-reload ISR ────────
    {
        fresh_context();
        Mcu51Context *ctx = mcs51_get_context();
        TMOD = 0x10;  // T1 mode 1 (16-bit), internal clock
        TH1 = static_cast<uint8_t>((65536u - 200u) >> 8);
        TL1 = static_cast<uint8_t>((65536u - 200u) & 0xFFu);
        P3TRIS = 0x04;
        ET1 = 1;
        EA = 1;
        TR1 = 1;
        const uint64_t t0 = ctx->virtual_us;
        run_to_us(ctx, t0 + 10000u);
        const uint32_t runs = g_t1_body_runs;
        const uint32_t count = wink_mcs51_isr_dispatch_count(3u);
        const uint64_t elapsed = ctx->virtual_us - t0;
        const uint32_t nominal = static_cast<uint32_t>(elapsed / 100u);
        printf("[mcs51-rearm] B t1_mode1: body=%u dispatch=%u nominal=%u\n",
               (unsigned)runs, (unsigned)count, (unsigned)nominal);
        check(count == runs, "B: ISR body runs != dispatch count");
        check(count <= nominal + 2u,
              "B: T1 mode1 dispatched more than once per period "
              "(overflow re-arm regression)");
        check(count >= (nominal * 7u) / 10u,
              "B: T1 mode1 dispatched too few times (timer stalled)");
    }

    // ── Case C: mode 1 with NO reload in the ISR must not hang ─────────────
    {
        fresh_context();
        g_t1_reload_in_isr = false;
        Mcu51Context *ctx = mcs51_get_context();
        TMOD = 0x10;
        TH1 = static_cast<uint8_t>((65536u - 200u) >> 8);
        TL1 = static_cast<uint8_t>((65536u - 200u) & 0xFFu);
        P3TRIS = 0x04;
        ET1 = 1;
        EA = 1;
        TR1 = 1;
        const uint64_t t0 = ctx->virtual_us;
        run_to_us(ctx, t0 + 2000u);
        const uint32_t count = wink_mcs51_isr_dispatch_count(3u);
        printf("[mcs51-rearm] C no_reload: dispatch=%u\n", (unsigned)count);
        check(count >= 15u,
              "C: no-reload mode1 ISR must be re-armed by the fallback");
        g_t1_reload_in_isr = true;
    }

    // ── Case D: reset-in-flight leaves a clean (stopped) timer ─────────────
    {
        fresh_context();
        Mcu51Context *ctx = mcs51_get_context();
        TMOD = 0x02;
        TH0 = static_cast<uint8_t>(256u - 200u);
        TL0 = 0;
        P3TRIS = 0x04;
        ET0 = 1;
        EA = 1;
        TR0 = 1;
        run_to_us(ctx, ctx->virtual_us + 1000u);
        check(wink_mcs51_isr_dispatch_count(1u) >= 8u,
              "D: timer must run before reset");
        mcs51_context_reset(ctx);
        wink_mcs51_isr_enable();
        const uint32_t before = wink_mcs51_isr_dispatch_count(1u);
        run_to_us(ctx, ctx->virtual_us + 1000u);
        check(wink_mcs51_isr_dispatch_count(1u) == before,
              "D: reset must stop the timer (no post-reset dispatch)");
    }

    if (g_fails == 0) {
        printf("[mcs51-rearm] ALL RE-ARM TESTS PASSED!\n");
        return 0;
    }
    printf("[mcs51-rearm] FAILED: %d checks failed\n", g_fails);
    return 1;
}
