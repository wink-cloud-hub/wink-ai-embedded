// SPDX-License-Identifier: Apache-2.0
// GAP-23 unmodeled-XSFR tripwire (A-07) — host tests.
//
// Release build (no WINK_MCS51_STRICT): unlisted XSFR-window accesses count
// (saturating) with first-offender addresses recorded, while declared
// addresses — including the health_pot configuration set — stay silent.
// STRICT build (same TU, STRICT-built compat lib): the first unlisted access
// aborts, verified via child re-execution.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "absacc.h"
#include "mcs51_context.h"
#include "mcs51_test_harness.h"

#ifdef _MSC_VER
// No abort dialog in CI: fail fast with an exit code instead.
#include <crtdbg.h>
#endif

namespace {

// Own BSS context for isolation (never stack/fiber); the framework default
// context is left untouched.
Mcu51Context s_ctx;

void init_ctx(void) {
    mcs51_set_active_context(&s_ctx);
    // M1: the XSFR window/tripwire exists on XSFR families only — this test
    // is a CMS8S-window test, select the family explicitly.
    mcs51_test_register_family(MCS51_FAMILY_CMS8S78XX);
    mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);
    mcs51_context_reset(&s_ctx);
    wink_mcs51_xdata_reset();
}

// EPWM PWMCON: vendor XSFR (0xF120) with no framework model — the canonical
// "silently succeeding" unmodeled register from the GAP-23 audit.
constexpr uint64_t kUnmodeledPwmcon = 0xF120ull;
// Declared XSFR addresses (audited allowlist members).
constexpr uint64_t kP00Cfg = 0xF000ull;
constexpr uint64_t kP22Cfg = 0xF022ull;
constexpr uint64_t kLedsDrP1L = 0xF712ull;
constexpr uint64_t kPsRxd = 0xF69Full;
constexpr uint64_t kBrtCon = 0xF5C0ull;

void xwrite(uint64_t addr, uint8_t v) {
    wink_mcs51_xdata_write(addr, v, 0u);  // kind=XBYTE: raw absolute access
}

uint8_t xread(uint64_t addr) {
    return wink_mcs51_xdata_read(addr, 0u);
}

}  // namespace

extern "C" void wink_mcs51_user_main(void) {}

extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

#define CHECK(cond, msg)                                              \
    do {                                                              \
        if (!(cond)) {                                                \
            printf("[mcs51-tripwire] FAIL: %s (line %d)\n", msg, __LINE__); \
            ++fails;                                                  \
        }                                                             \
    } while (0)

#ifdef WINK_MCS51_STRICT

// ── STRICT: death-test policy via child re-execution ────────────────────────
int child_main(int case_id) {
#ifdef _MSC_VER
    _set_abort_behavior(0, _WRITE_ABORT_MSG);
#endif
    init_ctx();
    switch (case_id) {
        case 1:  // unmodeled write must abort
            xwrite(kUnmodeledPwmcon, 0x01u);
            break;
        case 2:  // unmodeled read must abort
            (void)xread(kUnmodeledPwmcon);
            break;
        case 9:  // declared write must survive
            xwrite(kP00Cfg, 0x01u);
            break;
        default:
            printf("[mcs51-tripwire] FAIL: unknown child case %d\n", case_id);
            return 2;
    }
    return 0;
}

int run_child(const char* exe, int case_id, bool expect_abort) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "\"%s\" --child %d", exe, case_id);
    const int rc = system(cmd);
    if (expect_abort && rc == 0) {
        printf("[mcs51-tripwire] FAIL: child %d should have aborted (rc=0)\n",
               case_id);
        return 1;
    }
    if (!expect_abort && rc != 0) {
        printf("[mcs51-tripwire] FAIL: declared-access child exited rc=%d\n", rc);
        return 1;
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc == 3 && strcmp(argv[1], "--child") == 0) {
        return child_main(atoi(argv[2]));
    }
    int fails = 0;
    fails += run_child(argv[0], 1, true);
    fails += run_child(argv[0], 2, true);
    fails += run_child(argv[0], 9, false);
    if (fails) {
        return 1;
    }
    printf("[mcs51-tripwire] PASS (STRICT): unmodeled R/W abort, declared survives\n");
    return 0;
}

#else

// ── Release: counters + first-offender addresses + zero-regression ──────────
int main(void) {
    int fails = 0;

    // ── A: unmodeled write trips, lands in shadow (visibility, not block) ──
    init_ctx();
    xwrite(kUnmodeledPwmcon, 0xABu);
    CHECK(wink_mcs51_xsfr_unmodeled_count() == 1u, "A: write counted");
    CHECK(wink_mcs51_xsfr_unmodeled_addr(0) == 0xF120u, "A: address recorded");
    CHECK(xread(kUnmodeledPwmcon) == 0xABu, "A: access still lands in shadow");
    // Reads trip too: polling an unmodeled status register is as silent as
    // configuring one. 0xF121 is likewise unlisted.
    CHECK(xread(kUnmodeledPwmcon + 1u) == 0x00u, "A: unlisted read returns shadow");
    CHECK(wink_mcs51_xsfr_unmodeled_count() == 3u, "A: R+R/W counted");
    CHECK(wink_mcs51_xsfr_unmodeled_addr(1) == 0xF120u, "A: 2nd record (read)");
    CHECK(wink_mcs51_xsfr_unmodeled_addr(2) == 0xF121u, "A: 3rd record");
    CHECK(wink_mcs51_xsfr_unmodeled_addr(8) == 0xFFFFu, "A: OOB index reads 0xFFFF");

    // ── B: declared addresses stay silent ───────────────────────────────────
    init_ctx();
    xwrite(kP00Cfg, 0x01u);
    xwrite(kP22Cfg, 0x03u);
    xwrite(kLedsDrP1L, 0x02u);
    xwrite(kPsRxd, 0x21u);
    xwrite(kBrtCon, 0x80u);
    (void)xread(kPsRxd);
    CHECK(wink_mcs51_xsfr_unmodeled_count() == 0u, "B: declared set silent");

    // ── C: health_pot XSFR configuration set — zero-regression e2e ─────────
    // (P00CFG/P30..33CFG/P10..17CFG/P01/02/06CFG/P20CFG/P04CFG/P22CFG +
    // LEDSDRP1L/H; TRIS are direct SFRs, TLM slots are XRAM — both out of
    // tripwire scope by construction.)
    init_ctx();
    xwrite(0xF000u, 0x01u);  // P00CFG = AN0
    for (uint64_t a = 0xF030u; a <= 0xF033u; ++a) xwrite(a, 0x00u);  // P30..33
    for (uint64_t a = 0xF010u; a <= 0xF017u; ++a) xwrite(a, 0x00u);  // P10..17
    xwrite(0xF001u, 0x00u);  // P01CFG
    xwrite(0xF002u, 0x00u);  // P02CFG
    xwrite(0xF006u, 0x00u);  // P06CFG
    xwrite(0xF020u, 0x00u);  // P20CFG
    xwrite(0xF004u, 0x00u);  // P04CFG
    xwrite(kP22Cfg, 0x03u);  // P22CFG = TXD
    xwrite(kLedsDrP1L, 0x02u);
    xwrite(0xF713u, 0x02u);  // LEDSDRP1H
    xwrite(0x0010u, 0x10u);  // XRAM TLM slot: below the XSFR window, never trips
    CHECK(wink_mcs51_xsfr_unmodeled_count() == 0u, "C: health_pot set silent");

    // ── D: reset clears ─────────────────────────────────────────────────────
    xwrite(kUnmodeledPwmcon, 0x01u);
    CHECK(wink_mcs51_xsfr_unmodeled_count() == 1u, "D: trips again");
    wink_mcs51_xdata_reset();
    CHECK(wink_mcs51_xsfr_unmodeled_count() == 0u, "D: reset clears count");
    CHECK(wink_mcs51_xsfr_unmodeled_addr(0) == 0xFFFFu, "D: reset clears addrs");

    if (fails) {
        return 1;
    }
    printf("[mcs51-tripwire] PASS: unmodeled R/W counted + attributed, declared silent (A-D)\n");
    return 0;
}

#endif
