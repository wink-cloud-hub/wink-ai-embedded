// SPDX-License-Identifier: Apache-2.0
// Stage 3 Task 5 (A-08, GAP-24): classic external MOVX bus conflict + IAP.
//
// Release: CMS8S zero-regression (internal XRAM never fights GPIO); classic
// XBYTE-then-GPIO and GPIO-then-XBYTE conflicts count (P0/P2/P3.6-7 only,
// P1 exempt); classic XBYTE stays functional (shadow readback); CMS8S IAP
// block (0xF9-0xFF) reports MCS51_FEAT_IAP_FLASH per access while classic
// treats the same addresses as plain shadow.
//
// STRICT (same TU, STRICT compat lib): bus conflict and IAP access abort
// via child re-execution (mirrors test_mcs51_uart_charge.cpp).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "absacc.h"
#include "mcs51_context.h"
#include "mcs51_proxy.hpp"
#include "wink_mcs51_classic_bus.h"
#include "wink_mcs51_strict.h"

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace {

Mcu51Context s_ctx;

void init_ctx(uint8_t family) {
    mcs51_set_active_context(&s_ctx);
    mcs51_context_set_family(family);
    mcs51_context_reset(&s_ctx);
    wink_mcs51_xdata_reset();
    wink_mcs51_unsupported_reset();
}

#ifndef WINK_MCS51_STRICT
WinkSfr P0(0x80);
WinkSfr P1(0x90);
WinkSfr P2(0xA0);
WinkSfr P3(0xB0);

void xwrite(uint64_t addr, uint8_t v) {
    wink_mcs51_xdata_write(addr, v, 0u);  // kind=XBYTE: raw absolute access
}

uint8_t xread(uint64_t addr) {
    return wink_mcs51_xdata_read(addr, 0u);
}
#endif

}  // namespace

extern "C" void wink_mcs51_user_main(void) {}

extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

#define CHECK(cond, msg)                                             \
    do {                                                             \
        if (!(cond)) {                                               \
            printf("[classic-bus] FAIL: %s (line %d)\n", msg, __LINE__); \
            ++fails;                                                 \
        }                                                            \
    } while (0)

#ifdef WINK_MCS51_STRICT

int child_main(int case_id) {
#ifdef _MSC_VER
    _set_abort_behavior(0, _WRITE_ABORT_MSG);
#endif
    if (case_id == 1) {
        // Classic XBYTE then P3.7 GPIO: bus conflict must abort.
        init_ctx(MCS51_FAMILY_CLASSIC);
        wink_mcs51_xdata_write(0x1234ull, 0x5Au, 0u);
        Mcu51Context* ctx = mcs51_get_context();
        (void)ctx;
        WinkSfr p3(0xB0);
        WinkSbit p37 = p3 ^ 7;
        p37 = 1u;
    } else {
        // CMS8S IAP write: unmodeled Flash must abort.
        init_ctx(MCS51_FAMILY_CMS8S78XX);
        WinkSfr mctrl(0xFF);
        mctrl = 0x01u;
    }
    return 0;
}

int run_child(const char* self, int case_id, bool must_die) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "\"%s\" --child %d", self, case_id);
    const int rc = system(cmd);
    const bool died = (rc != 0);
    if (died != must_die) {
        printf("[classic-bus] FAIL: child %d died=%d want=%d (rc=%d)\n",
               case_id, (int)died, (int)must_die, rc);
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
    if (fails != 0) {
        return 1;
    }
    printf("[classic-bus] PASS (STRICT): conflict + IAP abort\n");
    return 0;
}

#else

int main(void) {
    int fails = 0;

    // ── 1) CMS8S zero-regression: internal XRAM never fights GPIO ─────────
    {
        init_ctx(MCS51_FAMILY_CMS8S78XX);
        Mcu51Context* ctx = mcs51_get_context();
        ctx->sfr_shadow[0x9Au] = 0xFFu;  // P0TRIS: outputs (no A-05 noise)
        ctx->sfr_shadow[0xA2u] = 0xFFu;  // P2TRIS: outputs
        ctx->sfr_shadow[0xA3u] = 0xFFu;  // P3TRIS: outputs
        xwrite(0x0010ull, 0xA5u);
        P3 = 0xFFu;
        P0 = 0x00u;
        P2 = 0x00u;
        CHECK(wink_mcs51_classic_bus_conflict_total() == 0u,
              "CMS8S XBYTE+GPIO must stay silent");
        CHECK(xread(0x0010ull) == 0xA5u, "CMS8S XRAM must round-trip");
    }

    // ── 2) Classic XBYTE then P3.7 GPIO (acceptance case) ──────────────────
    {
        init_ctx(MCS51_FAMILY_CLASSIC);
        xwrite(0x1234ull, 0x5Au);
        CHECK(wink_mcs51_classic_bus_conflict_total() == 0u,
              "bus use alone must not count");
        WinkSbit p37 = P3 ^ 7;
        p37 = 1u;
        CHECK(wink_mcs51_classic_bus_conflict_total() == 1u,
              "P3.7 after XBYTE must count");
    }

    // ── 3) P0/P2 are bus pins, P1 is exempt ────────────────────────────────
    {
        init_ctx(MCS51_FAMILY_CLASSIC);
        xwrite(0x0000ull, 0x01u);
        P0 = 0x55u;
        CHECK(wink_mcs51_classic_bus_conflict_total() == 1u,
              "P0 after XBYTE must count");
        WinkSbit p25 = P2 ^ 5;
        p25 = 1u;
        CHECK(wink_mcs51_classic_bus_conflict_total() == 2u,
              "P2 after XBYTE must count");
        P1 = 0xAAu;
        CHECK(wink_mcs51_classic_bus_conflict_total() == 2u,
              "P1 must stay exempt");
    }

    // ── 4) Reverse order: GPIO first, then MOVX read ───────────────────────
    {
        init_ctx(MCS51_FAMILY_CLASSIC);
        WinkSbit p36 = P3 ^ 6;
        p36 = 0u;
        CHECK(wink_mcs51_classic_bus_conflict_total() == 0u,
              "GPIO use alone must not count");
        (void)xread(0x1000ull);  // inside the 8 KB classic aperture
        CHECK(wink_mcs51_classic_bus_conflict_total() == 1u,
              "XBYTE read after GPIO must count");
        CHECK(xread(0x1000ull) == 0x00u, "classic XBYTE must still serve");
    }

    // ── 5) CMS8S IAP block reports per access ──────────────────────────────
    {
        init_ctx(MCS51_FAMILY_CMS8S78XX);
        WinkSfr mctrl(0xFF);
        mctrl = 0x01u;
        CHECK(wink_mcs51_unsupported_trigger_count(MCS51_FEAT_IAP_FLASH) == 1u,
              "IAP write must trap");
        const uint8_t v = mctrl;
        (void)v;
        CHECK(wink_mcs51_unsupported_trigger_count(MCS51_FEAT_IAP_FLASH) == 2u,
              "IAP read must trap");
        CHECK(mctrl == 0x01u, "IAP shadow must persist the written value");
    }

    // ── 6) Classic: same addresses are plain shadow ────────────────────────
    {
        init_ctx(MCS51_FAMILY_CLASSIC);
        WinkSfr mctrl(0xFF);
        mctrl = 0x01u;
        CHECK(wink_mcs51_unsupported_trigger_count(MCS51_FEAT_IAP_FLASH) == 0u,
              "classic must not trap IAP addresses");
        CHECK(mctrl == 0x01u, "classic shadow must round-trip");
    }

    if (fails != 0) {
        return 1;
    }
    printf("[classic-bus] PASS: conflict both orders, IAP trap, zero-regression\n");
    return 0;
}

#endif
