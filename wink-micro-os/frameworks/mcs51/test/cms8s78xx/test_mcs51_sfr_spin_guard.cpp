// SPDX-License-Identifier: GPL-3.0-only
// T1.4: SFR spin guard unit tests.
//
// Locks the defence-in-depth contract from §5.4 of the CMS8S78xx I2C/SPI
// deadlock resolution plan:
//   * only SFR reads count: pure software delays and transpiler microsteps
//     never trip (no false kills);
//   * a tight ≤3-address spin with no external activity for > 50 ms of
//     virtual time trips exactly once per episode (latched) and, in release,
//     keeps executing with diagnostics;
//   * every anchor clears the episode: proxied SFR write, UART RX injection
//     and a fired timer overflow keep legitimate polls clean even when their
//     total wait exceeds the budget;
//   * an installed fail-fast hook (longjmp = ADR-0082 fiber-exit semantics)
//     turns the trip into a hard failure and must not return.
#include <csetjmp>
#include <stdint.h>
#include <stdio.h>

#include "cms8s78xx.h"
#include "mcs51_context.h"
#include "mcs51_test_harness.h"
#include "mcs51_trap.h"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_uart.h"

#undef main
#undef printf
extern "C" void wink_mcs51_user_main(void) {}
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

namespace {

int g_fails = 0;

void check(bool cond, const char* msg) {
    if (!cond) {
        printf("[spin-guard] FAIL: %s\n", msg);
        ++g_fails;
    }
}

// Probe SFRs with no chip hook (CCL1..CCL3 are plain chip registers), so a
// probe read is pure firmware polling traffic for the guard.
WinkSfr s_probe_a(0xC2);
WinkSfr s_probe_b(0xC3);

volatile uint8_t s_sink = 0u;

// Two-address rotation (~10 us virtual per loop: two reads x 5 us microstep).
void spin_two_addrs(uint32_t loops) {
    for (uint32_t i = 0u; i < loops; ++i) {
        s_sink = static_cast<uint8_t>(s_probe_a);
        s_sink = static_cast<uint8_t>(s_probe_b);
    }
}

std::jmp_buf s_fail_jmp;
uint32_t s_hook_calls = 0u;

extern "C" void spin_fail_hook(void) {
    ++s_hook_calls;
    std::longjmp(s_fail_jmp, 1);  // ADR-0082 fiber exit: never returns
}

}  // namespace

int main(void) {
    printf("[spin-guard] Starting SFR spin guard tests...\n");

    mcs51_test_register_family(MCS51_FAMILY_CMS8S78XX);
    mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);
    Mcu51Context* ctx = mcs51_get_context();
    mcs51_context_reset(ctx);
    ctx->clock_hz = 24000000u;
    ctx->virtual_us = 0;
    wink_mcs51_spin_guard_reset_counters();

    // ── 1) Pure software delay (no SFR reads) never trips ───────────────────
    wink_mcs51_test_advance_virtual_us(200000u);  // 200 ms with no reads
    s_sink = static_cast<uint8_t>(s_probe_a);
    check(wink_mcs51_spin_guard_trip_count() == 0u,
          "pure software delay must not trip");
    check(wink_mcs51_spin_guard_reads() == 1u,
          "first read after an idle stretch must open a fresh episode");

    // ── 2) Tight spin trips once per episode and keeps running (release) ────
    spin_two_addrs(12000u);  // 120 ms virtual, no events
    check(wink_mcs51_spin_guard_trip_count() == 1u,
          "tight spin must trip exactly once");
    check(wink_mcs51_spin_guard_trip_reads() > 10000u,
          "trip must record the episode read count");
    {
        const uint8_t hit = wink_mcs51_spin_guard_trip_addr();
        check(hit == 0xC2u || hit == 0xC3u, "trip address not recorded");
    }
    spin_two_addrs(2000u);  // latched: no new trips in the same episode
    check(wink_mcs51_spin_guard_trip_count() == 1u,
          "episode trip must latch until an anchor");

    // ── 3) SFR write anchor (①): repeated 40 ms episodes stay clean ─────────
    // Reaching this point already proves the release build kept executing
    // after the trip; the write below opens the next episode.
    s_probe_a = 0x01u;
    spin_two_addrs(4000u);  // 40 ms < budget
    s_probe_a = 0x02u;      // anchor again
    spin_two_addrs(4000u);  // another 40 ms
    check(wink_mcs51_spin_guard_trip_count() == 1u,
          "write anchors must prevent budget accumulation across episodes");
    // A fresh unanchored 60 ms episode may still trip.
    s_probe_a = 0x03u;
    spin_two_addrs(6000u);
    check(wink_mcs51_spin_guard_trip_count() == 2u,
          "a fresh episode must be able to trip again");

    // ── 4) UART RX injection anchor (④) keeps `while(!RI)` waits clean ──────
    wink_mcs51_spin_guard_reset_counters();
    for (uint8_t episode = 0u; episode < 3u; ++episode) {
        for (uint32_t i = 0u; i < 8000u; ++i) {  // 40 ms per episode
            s_sink = static_cast<uint8_t>(SCON);
        }
        wink_mcs51_uart_rx_push(0x41u);  // anchor ④
    }
    check(wink_mcs51_spin_guard_trip_count() == 0u,
          "RX injection anchor must keep a poll wait clean");

    // ── 5) Timer overflow anchor (④) keeps a long flag wait clean ───────────
    wink_mcs51_spin_guard_reset_counters();
    TMOD = 0x02u;  // Timer0 mode 2: 8-bit auto-reload
    TH0 = 0x00u;   // 256 counts @ 24 MHz -> overflow every 128 us
    TL0 = 0x00u;
    TR0 = 1u;
    for (uint32_t i = 0u; i < 30000u; ++i) {  // 150 ms virtual, no writes
        s_sink = static_cast<uint8_t>(TCON);
    }
    check(wink_mcs51_spin_guard_trip_count() == 0u,
          "periodic timer overflows must keep a long wait clean");
    check((static_cast<uint8_t>(TCON) & 0x20u) != 0u,
          "timer must have overflowed (TF0 latch)");
    TR0 = 0u;

    // ── 6) Fail-fast hook: trip becomes a hard exit and must not return ─────
    wink_mcs51_spin_guard_reset_counters();
    wink_mcs51_spin_guard_set_abort_hook(spin_fail_hook);
    if (setjmp(s_fail_jmp) == 0) {
        spin_two_addrs(12000u);
        check(false, "spin guard must invoke the fail-fast hook");
    } else {
        check(s_hook_calls == 1u, "fail-fast hook must run exactly once");
        check(wink_mcs51_spin_guard_trip_count() == 1u,
              "hard fail must still record the trip");
    }
    wink_mcs51_spin_guard_set_abort_hook(nullptr);

    // ── 7) Counter reset restores a clean slate ─────────────────────────────
    wink_mcs51_spin_guard_reset_counters();
    check(wink_mcs51_spin_guard_trip_count() == 0u &&
              wink_mcs51_spin_guard_reads() == 0u,
          "reset counters must clear diagnostics and the window");

    if (g_fails) {
        printf("[spin-guard] %d failure(s)\n", g_fails);
        return 1;
    }
    printf("[spin-guard] PASS: no false kills (delay/RX/timer), tight-spin "
           "diagnosis with per-episode latch, write/RX/timer anchors, "
           "fail-fast longjmp exit\n");
    return 0;
}
