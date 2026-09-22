// SPDX-License-Identifier: GPL-3.0-only
// ADR-0082 / PLAN-20260915-MCS51-RESET-FIDELITY: CMS8S78xx Reset Controller unit test.
//
// Release:
//   1. Bit-level WDCON semantics: PORF clear without TA, write 1 ignored;
//   2. WDTRF clear requires TA, write 1 ignored;
//   3. SWRST 0->1 edge trigger and hardware self-clear;
//   4. Reset source x flags matrix (POR, SWRST, WDT, EXT);
//   5. PORF sticky retention across warm resets (SWRST / WDT / EXT);
//   6. WDTRE + WDTIE arbitration: WDTRE reset priority suppresses Vector 20 IRQ;
//   7. Reset trigger latching & guard.
//
// STRICT (same TU, STRICT compat lib):
//   Overflow with WDTRE=1 aborts via child re-execution tripwire.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mcs51_context.h"
#include "mcs51_test_harness.h"
#include "mcs51_proxy.hpp"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_isr.h"
#include "wink_mcs51_wdt.h"
#include "REG_CMS8S78XX.H"

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace {

constexpr uint8_t SFR_CKCON = 0x8E;
constexpr uint8_t SFR_WDCON = 0x97;

#ifndef WINK_MCS51_STRICT
WinkSfr s_TA(0x96);  // release-only WDCON TA unlock helper
#endif
WinkSfr s_WDCON(SFR_WDCON);
WinkSfr s_CKCON(SFR_CKCON);

Mcu51Context s_ctx;

void init_ctx(void) {
    mcs51_set_active_context(&s_ctx);
    mcs51_test_register_family(MCS51_FAMILY_CMS8S78XX);
    mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);
    s_ctx.last_reset_reason = MCS51_RESET_REASON_POR;
    mcs51_context_reset(&s_ctx);
    wink_mcs51_clock_reset();
}

#ifndef WINK_MCS51_STRICT
void ta_unlock(void) {
    s_TA = 0xAAu;
    s_TA = 0x55u;
}
#endif

void set_wts(uint8_t wts) {
    s_CKCON = static_cast<unsigned>(((uint8_t)s_CKCON & 0x1Fu) | ((wts & 0x07u) << 5u));
}

}  // namespace

// REG_CMS8S78XX.H remaps `main` to the fiber entry; this TU provides its own
// main() (plus the STRICT child re-exec main), so drop the remap first.
#undef main
extern "C" void wink_mcs51_user_main(void) {}

extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

#define CHECK(cond, msg)                                                    \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("[reset-ctrl] FAIL: %s (line %d)\n", msg, __LINE__);     \
            ++fails;                                                        \
        }                                                                   \
    } while (0)

#ifdef WINK_MCS51_STRICT

int child_main(void) {
#ifdef _MSC_VER
    _set_abort_behavior(0, _WRITE_ABORT_MSG);
#endif
    init_ctx();
    set_wts(0u);  // 5461us interval
    SYS_EnableWDTReset();
    wink_mcs51_test_advance_virtual_us(6000u);
    wink_mcs51_wdt_check();  // must abort under STRICT
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
        printf("[reset-ctrl] FAIL: WDT overflow must abort under STRICT\n");
        return 1;
    }
    printf("[reset-ctrl] PASS (STRICT): WDT overflow abort\n");
    return 0;
}

#else

int main(void) {
    int fails = 0;

    // ── 1) PORF clear without TA & write-1 ignored (P06 / ADR-0082 D3) ───────
    {
        init_ctx();
        CHECK(SYS_GetPowerOnResetFlag() == 1, "Cold POR must have PORF=1");

        // Clear PORF without TA (vendor system.c:394)
        SYS_ClearPowerOnResetFlag();
        CHECK(SYS_GetPowerOnResetFlag() == 0, "SYS_ClearPowerOnResetFlag must clear PORF without TA");

        // Attempt write 1 to PORF
        s_WDCON = static_cast<unsigned>((uint8_t)s_WDCON | 0x40u);
        CHECK(SYS_GetPowerOnResetFlag() == 0, "Firmware cannot set PORF to 1");

        // Attempt write 1 with TA
        ta_unlock();
        s_WDCON = static_cast<unsigned>((uint8_t)s_WDCON | 0x40u);
        CHECK(SYS_GetPowerOnResetFlag() == 0, "Firmware cannot set PORF to 1 even with TA");
    }

    // ── 2) WDTRF clear requires TA & write-1 ignored (ADR-0082 D3) ────────────
    {
        init_ctx();
        CHECK(SYS_GetWDTResetFlag() == 0, "Cold POR must have WDTRF=0");

        // Test seam: inject WDTRF in shadow
        s_ctx.sfr_shadow[SFR_WDCON] |= 0x04u;
        CHECK(SYS_GetWDTResetFlag() == 1, "WDTRF test seam injected");

        // Attempt clear without TA
        s_WDCON = static_cast<unsigned>((uint8_t)s_WDCON & ~0x04u);
        CHECK(SYS_GetWDTResetFlag() == 1, "Clearing WDTRF without TA must be ignored");

        // Clear with TA (vendor system.c:301-318)
        SYS_ClearWDTResetFlag();
        CHECK(SYS_GetWDTResetFlag() == 0, "SYS_ClearWDTResetFlag must clear WDTRF");

        // Attempt write 1 with TA
        ta_unlock();
        s_WDCON = static_cast<unsigned>((uint8_t)s_WDCON | 0x04u);
        CHECK(SYS_GetWDTResetFlag() == 0, "Firmware cannot set WDTRF to 1");
    }

    // ── 3) SWRST 0->1 edge trigger and self-clearing (P03 / ADR-0082 D3) ─────
    {
        init_ctx();
        wink_mcs51_clear_pending_reset();

        // Write 0 to SWRST (SYS_DisableSoftwareReset) is legal pre-condition, does not trigger
        SYS_DisableSoftwareReset();
        CHECK(!wink_mcs51_has_pending_reset(), "DisableSoftwareReset must not trigger reset");
        CHECK((s_WDCON & 0x80u) == 0u, "SWRST bit must be 0");

        // Write 1 to SWRST triggers software reset. ADR-0082: the firmware
        // write path runs the sanitizer in its own microstep, so the pending
        // latch is consumed immediately — observe the completed reset
        // (sticky reason + self-cleared SWRST bit) instead of a lingering flag.
        SYS_EnableSoftwareReset();
        CHECK(wink_mcs51_get_last_reset_reason() == MCS51_RESET_REASON_SOFTWARE,
              "EnableSoftwareReset must complete a software reset");
        CHECK(!wink_mcs51_has_pending_reset(),
              "sanitizer must consume the pending latch");
        CHECK((s_WDCON & 0x80u) == 0u, "SWRST bit must self-clear to 0 in shadow");
    }

    // ── 4) Sticky PORF retention across warm resets (P05 / ADR-0082 D3) ──────
    {
        // Case A: PORF was cleared before warm reset -> stays 0
        init_ctx();
        CHECK(SYS_GetPowerOnResetFlag() == 1, "Initial PORF is 1");
        SYS_ClearPowerOnResetFlag();
        CHECK(SYS_GetPowerOnResetFlag() == 0, "PORF cleared to 0");

        // Warm reset via WDT
        wink_mcs51_trigger_reset(MCS51_RESET_REASON_WDT);
        mcs51_context_reset(&s_ctx);
        CHECK(SYS_GetPowerOnResetFlag() == 0, "PORF must remain 0 across warm WDT reset (Sticky)");
        CHECK(SYS_GetWDTResetFlag() == 1, "WDTRF must be set on WDT reset");

        // Warm reset via SWRST
        wink_mcs51_trigger_reset(MCS51_RESET_REASON_SOFTWARE);
        mcs51_context_reset(&s_ctx);
        CHECK(SYS_GetPowerOnResetFlag() == 0, "PORF must remain 0 across warm SWRST reset (Sticky)");
        CHECK(SYS_GetWDTResetFlag() == 0, "WDTRF must be cleared on non-WDT reset");

        // Case B: PORF was NOT cleared before warm reset -> stays 1
        init_ctx();  // Cold POR
        CHECK(SYS_GetPowerOnResetFlag() == 1, "Cold POR has PORF=1");
        wink_mcs51_trigger_reset(MCS51_RESET_REASON_SOFTWARE);
        mcs51_context_reset(&s_ctx);
        CHECK(SYS_GetPowerOnResetFlag() == 1, "PORF must remain 1 across warm SWRST reset if not cleared");
        CHECK(SYS_GetWDTResetFlag() == 0, "WDTRF must be 0 on SWRST");

        // Warm reset via EXT (Task 5 test seam)
        wink_mcs51_trigger_reset(MCS51_RESET_REASON_EXT);
        mcs51_context_reset(&s_ctx);
        CHECK(SYS_GetPowerOnResetFlag() == 1, "PORF must remain 1 across warm EXT reset if not cleared (Sticky)");
        CHECK(SYS_GetWDTResetFlag() == 0, "WDTRF must be 0 on EXT reset");
    }


    // ── 5) WDTRE + WDTIE arbitration suppresses Vector 20 IRQ (P11 / ADR-0082 D4) ──
    {
        init_ctx();
        wink_mcs51_clear_pending_reset();
        set_wts(0u);  // 5461us interval

        // Both RE and IE enabled (like official sample 34)
        SYS_EnableWDTReset();
        WDT_EnableOverflowInt();

        // Advance past overflow threshold
        wink_mcs51_test_advance_virtual_us(6000u);
        wink_mcs51_wdt_check();

        CHECK(wink_mcs51_has_pending_reset(), "WDT overflow with WDTRE=1 must latch pending reset");
        CHECK(wink_mcs51_get_pending_reset_reason() == MCS51_RESET_REASON_WDT,
              "Pending reset reason must be WDT");
        CHECK((wink_mcs51_get_pending_interrupts() & (1u << IRQ_SOURCE_WDT)) == 0u,
              "WDTRE reset priority must suppress Vector 20 IRQ dispatch");

        // Now test when WDTRE=0 and WDTIE=1 (pure timer mode, sample 32)
        init_ctx();
        wink_mcs51_clear_pending_reset();
        set_wts(0u);
        SYS_DisableWDTReset();
        WDT_EnableOverflowInt();

        wink_mcs51_test_advance_virtual_us(6000u);
        wink_mcs51_wdt_check();

        CHECK(!wink_mcs51_has_pending_reset(), "WDT overflow with WDTRE=0 must not latch reset");
        CHECK((wink_mcs51_get_pending_interrupts() & (1u << IRQ_SOURCE_WDT)) != 0u,
              "WDT overflow with WDTRE=0 and WDTIE=1 must dispatch Vector 20 IRQ");
    }

    // ── 6) Nested trigger guard (P15) ───────────────────────────────────────
    {
        init_ctx();
        wink_mcs51_clear_pending_reset();

        wink_mcs51_trigger_reset(MCS51_RESET_REASON_SOFTWARE);
        CHECK(wink_mcs51_get_pending_reset_reason() == MCS51_RESET_REASON_SOFTWARE,
              "First trigger accepted");

        // Secondary trigger while pending must be ignored
        wink_mcs51_trigger_reset(MCS51_RESET_REASON_WDT);
        CHECK(wink_mcs51_get_pending_reset_reason() == MCS51_RESET_REASON_SOFTWARE,
              "Secondary trigger while pending must be guarded");

        wink_mcs51_clear_pending_reset();
        CHECK(!wink_mcs51_has_pending_reset(), "clear_pending_reset clears pending state");
    }

    // ── 7) Full state sanitization and multi-boot re-entry (P01 / P13 / ADR-0082 D1, D5) ──
    {
        init_ctx();
        static int s_boot_count = 0;
        s_boot_count = 0;

        auto test_main = []() {
            s_boot_count++;
            if (s_boot_count == 1) {
                // First boot: inject non-zero interrupt depth and edge queue
                s_ctx.in_service_depth = 2u;
                s_ctx.reti_suppress_one = true;
                s_ctx.edge_head = 3u;
                s_ctx.edge_tail = 1u;

                // Clear PORF so we can check sticky retention across re-entry
                SYS_ClearPowerOnResetFlag();

                // Trigger software reset and yield to safety point
                SYS_EnableSoftwareReset();
                wink_mcs51_microstep();  // Safety interception point: unwinds stack via longjmp!
            } else if (s_boot_count == 2) {
                // Second boot: verify all residuals are sanitized!
                if (s_ctx.in_service_depth != 0) {
                    printf("[reset-ctrl] FAIL: in_service_depth must be 0 after reset sanitization\n");
                }
                if (s_ctx.reti_suppress_one) {
                    printf("[reset-ctrl] FAIL: reti_suppress_one must be false after reset\n");
                }
                if (s_ctx.edge_head != 0 || s_ctx.edge_tail != 0) {
                    printf("[reset-ctrl] FAIL: edge queue must be flushed\n");
                }
                if (SYS_GetPowerOnResetFlag() != 0) {
                    printf("[reset-ctrl] FAIL: PORF must remain 0 across software reset re-entry\n");
                }
                if (SYS_GetWDTResetFlag() != 0) {
                    printf("[reset-ctrl] FAIL: WDTRF must be 0 on software reset\n");
                }
            }
        };

        wink_mcs51_test_run_reentry_loop(+test_main, 3);
        CHECK(s_boot_count == 2, "Test loop must have executed exactly 2 boots");
    }

    if (fails == 0) {

        printf("[reset-ctrl] ALL TESTS PASSED (0 failures)\n");
        return 0;
    } else {
        printf("[reset-ctrl] %d TEST(S) FAILED\n", fails);
        return 1;
    }
}

#endif
