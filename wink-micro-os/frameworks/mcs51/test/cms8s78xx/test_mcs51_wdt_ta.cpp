// SPDX-License-Identifier: GPL-3.0-only
// Stage 3 Task 4 (A-04, GAP-07): WDT coarse model + TA window narrowing.
//
// Release: WTS interval math per source, enable/feed lifecycle, overflow
// counting (one per episode), health_pot-equivalent zero-regression (10 ms
// feed + 23 ms telemetry scale), TA interleave/timeout rollback.
//
// STRICT (same TU, STRICT compat lib): overflow check aborts via child
// re-execution (mirrors test_mcs51_uart_charge.cpp).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mcs51_context.h"
#include "mcs51_test_harness.h"
#include "mcs51_proxy.hpp"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_wdt.h"

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace {

constexpr uint8_t SFR_CKCON = 0x8E;
constexpr uint8_t SFR_WDCON = 0x97;

WinkSfr TA(0x96);
WinkSfr WDCON(SFR_WDCON);
WinkSfr CKCON(SFR_CKCON);
#ifndef WINK_MCS51_STRICT
WinkSfr ACC(0xE0);  // no model hook: clean intervening write for TA tests
#endif

Mcu51Context s_ctx;

void init_ctx(void) {
    mcs51_set_active_context(&s_ctx);
    mcs51_test_register_family(MCS51_FAMILY_CMS8S78XX);
    mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);
    mcs51_context_reset(&s_ctx);
    wink_mcs51_clock_reset();
}

// Well-formed unlock + protected write through the firmware path (proxy ->
// bridge notify + hook + microstep), exactly like health_pot wdt_feed().
void ta_unlock(void) {
    TA = 0xAAu;
    TA = 0x55u;
}

void wdt_enable(void) {
    ta_unlock();
    WDCON = static_cast<unsigned>((uint8_t)WDCON | 0x02u);  // WDTRE
}

#ifndef WINK_MCS51_STRICT
void wdt_feed(void) {
    ta_unlock();
    WDCON = static_cast<unsigned>((uint8_t)WDCON | 0x01u);  // WDTCLR
}
#endif

void set_wts(uint8_t wts) {
    CKCON = static_cast<unsigned>(((uint8_t)CKCON & 0x1Fu) | ((wts & 0x07u) << 5u));
}

}  // namespace

extern "C" void wink_mcs51_user_main(void) {}

extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

#define CHECK(cond, msg)                                        \
    do {                                                        \
        if (!(cond)) {                                          \
            printf("[wdt-ta] FAIL: %s (line %d)\n", msg, __LINE__); \
            ++fails;                                            \
        }                                                       \
    } while (0)

#ifdef WINK_MCS51_STRICT

int child_main(void) {
#ifdef _MSC_VER
    _set_abort_behavior(0, _WRITE_ABORT_MSG);
#endif
    init_ctx();
    set_wts(0u);  // 2^17 Tsys @24MHz = 5461us
    wdt_enable();
    wink_mcs51_test_advance_virtual_us(6000u);
    wink_mcs51_wdt_check();  // must abort
    return 0;
}

int main(int argc, char** argv) {
    if (argc == 2 && strcmp(argv[1], "--child") == 0) {
        return child_main();
    }
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "\"%s\" --child", argv[0]);
    const int rc = system(cmd);
    if (rc == 0) {
        printf("[wdt-ta] FAIL: overflow check must abort under STRICT\n");
        return 1;
    }
    printf("[wdt-ta] PASS (STRICT): overflow abort\n");
    return 0;
}

#else

int main(void) {
    int fails = 0;

    // ── 1) WTS interval math (vendor wdt.h counts @24MHz) ──────────────────
    {
        init_ctx();
        set_wts(0u);
        CHECK(wink_mcs51_wdt_interval_us() == 5461u, "WTS=0 must be 5461us");
        set_wts(6u);
        CHECK(wink_mcs51_wdt_interval_us() == 699050u, "WTS=6 must be 699050us");
        set_wts(7u);
        CHECK(wink_mcs51_wdt_interval_us() == 2796202u, "WTS=7 must be 2796202us");
    }

    // ── 2) Disabled WDT never counts ───────────────────────────────────────
    {
        init_ctx();
        set_wts(0u);
        wink_mcs51_test_advance_virtual_us(100000u);
        wink_mcs51_wdt_check();
        CHECK(wink_mcs51_wdt_overflow_total() == 0u, "disabled WDT must not count");
    }

    // ── 3) WDTRE reset: one count per arming episode; a fresh boot re-arms ─
    // ADR-0082: a WDTRE=1 overflow triggers a real reset, so "feed re-arms"
    // is a boot cycle, not an in-place re-arm. Any firmware SFR write after
    // the overflow runs the sanitizer, which clears the pending reset,
    // disables the WDT and zeroes the per-boot overflow counter.
    {
        init_ctx();
        set_wts(0u);  // 5461us
        wdt_enable();
        wink_mcs51_test_advance_virtual_us(6000u);
        wink_mcs51_wdt_check();
        CHECK(wink_mcs51_wdt_overflow_total() == 1u, "overflow must count");
        CHECK(wink_mcs51_has_pending_reset(),
              "WDTRE overflow must latch a pending reset");
        wink_mcs51_wdt_check();
        CHECK(wink_mcs51_wdt_overflow_total() == 1u,
              "latch must hold one count");

        // The next proxied SFR write (any firmware write) runs the sanitizer.
        wdt_feed();
        CHECK(!wink_mcs51_has_pending_reset(),
              "sanitizer must clear the pending reset");
        CHECK(wink_mcs51_wdt_overflow_total() == 0u,
              "reset must clear the per-boot overflow counter");
        CHECK((static_cast<uint8_t>(WDCON) & 0x02u) == 0u,
              "reset must clear WDTRE (WDT off until firmware re-enables)");

        // Fresh boot: re-enable and overflow again -> one count in the new
        // episode (proves episode counting, not a stuck latch).
        set_wts(0u);
        wdt_enable();
        wink_mcs51_test_advance_virtual_us(6000u);
        wink_mcs51_wdt_check();
        CHECK(wink_mcs51_wdt_overflow_total() == 1u,
              "post-reset overflow must count");
    }

    // ── 4) health_pot scale: 10ms feed + 23ms telemetry never trips ────────
    {
        init_ctx();
        set_wts(6u);  // 699050us (~0.70s, health_pot WDT_WTS_BITS)
        wdt_enable();
        // 10 ms main-loop feed rhythm x3 with a 23 ms UART frame inside.
        for (int i = 0; i < 3; ++i) {
            wink_mcs51_test_advance_virtual_us(23000u);  // 22B @9600bps
            wdt_feed();
            wink_mcs51_wdt_check();
        }
        CHECK(wink_mcs51_wdt_overflow_total() == 0u, "health_pot rhythm must be clean");
        CHECK(wink_mcs51_wdt_next_event_us(&s_ctx) > wink_mcs51_virtual_us(),
              "deadline must lie in the future after feed");
    }

    // ── 5) Locked WDCON write rolls back (no TA) ───────────────────────────
    {
        init_ctx();
        WDCON = 0x02u;  // no unlock: silicon ignores
        CHECK(((uint8_t)WDCON & 0x02u) == 0u, "locked WDTRE write must roll back");
        CHECK(wink_mcs51_wdt_overflow_total() == 0u, "locked write must not arm WDT");
    }

    // ── 6) TA interleave: unrelated SFR between AA/55 kills the window ─────
    {
        init_ctx();
        TA = 0xAAu;
        ACC = 0x55u;  // intervening firmware SFR write
        TA = 0x55u;
        WDCON = static_cast<unsigned>((uint8_t)WDCON | 0x02u);
        CHECK(((uint8_t)WDCON & 0x02u) == 0u, "interleaved TA must roll back");
    }

    // ── 7) TA timeout: stale 0xAA expires ──────────────────────────────────
    {
        init_ctx();
        TA = 0xAAu;
        wink_mcs51_test_advance_virtual_us(1000u);  // >> 100us coarse window
        TA = 0x55u;
        WDCON = static_cast<unsigned>((uint8_t)WDCON | 0x02u);
        CHECK(((uint8_t)WDCON & 0x02u) == 0u, "stale TA must roll back");
    }

    // ── 8) Clean sequence still passes (control) ───────────────────────────
    {
        init_ctx();
        const uint64_t t0 = wink_mcs51_virtual_us();
        wdt_enable();
        CHECK(((uint8_t)WDCON & 0x02u) != 0u, "clean TA must enable WDTRE");
        // Feed stamp lands in the hook (before the trailing microstep), so
        // it must sit inside the enable window, not equal the exit clock.
        CHECK(wink_mcs51_wdt_last_feed_us() >= t0 &&
              wink_mcs51_wdt_last_feed_us() <= wink_mcs51_virtual_us(),
              "enable must stamp feed time inside the window");
    }

    // ── 9) Classic family: TA/WDT hooks absent, writes plain ───────────────
    {
        mcs51_set_active_context(&s_ctx);
        mcs51_test_register_family(MCS51_FAMILY_CLASSIC);
        mcs51_context_set_family(MCS51_FAMILY_CLASSIC);
        mcs51_context_reset(&s_ctx);
        wink_mcs51_clock_reset();
        WDCON = 0x02u;  // no hook: plain shadow store
        CHECK(((uint8_t)WDCON & 0x02u) != 0u, "classic must not gate WDCON");
        CHECK(wink_mcs51_wdt_interval_us() == 0u, "classic must report no interval");
        wink_mcs51_wdt_check();
        CHECK(wink_mcs51_wdt_overflow_total() == 0u, "classic must never count");
    }

    // ── 10) WDTIE interrupt enable + WDTIF flag + IRQ_SOURCE_WDT trigger ───
    {
        init_ctx();
        set_wts(0u);  // 5461us
        // Enable WDTIE in EIE2 (0xAA, bit 5)
        WinkSfr EIE2(0xAA);
        EIE2 = static_cast<unsigned>((uint8_t)EIE2 | (1u << 5));
        wink_mcs51_isr_enable();

        CHECK((wink_mcs51_get_pending_interrupts() & (1u << IRQ_SOURCE_WDT)) == 0u,
              "WDT IRQ must not be pending initially");

        wink_mcs51_test_advance_virtual_us(6000u);
        wink_mcs51_wdt_check();

        CHECK(((uint8_t)WDCON & 0x08u) != 0u, "WDTIF flag must be set on overflow");
        CHECK((wink_mcs51_get_pending_interrupts() & (1u << IRQ_SOURCE_WDT)) != 0u,
              "WDT IRQ must be raised on overflow when WDTIE is set");

        ta_unlock();
        WDCON = static_cast<unsigned>((uint8_t)WDCON & ~0x08u);
        CHECK(((uint8_t)WDCON & 0x08u) == 0u, "WDTIF flag must be cleared after TA unlock");
    }

    if (fails != 0) {
        return 1;
    }
    printf("[wdt-ta] PASS: intervals, arming, health_pot scale, TA narrowing, IRQ trigger\n");
    return 0;
}

#endif
