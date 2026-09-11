// SPDX-License-Identifier: Apache-2.0
// GAP-09 family-gated XDATA aperture — host tests.
//
// CMS8S78xx has 1 KB on-chip XRAM (0x0000..0x03FF) then a hole until the
// 0xF000 XSFR window; classic 8052 has no on-chip XRAM and uses the
// configurable WINK_MCS51_XDATA_SIZE external aperture with no XSFR window.
#include <stdint.h>
#include <stdio.h>

#include "absacc.h"
#include "mcs51_context.h"
#include "mcs51_test_harness.h"

namespace {

Mcu51Context s_ctx;
int fails = 0;

void reset_with_family(uint8_t family) {
    mcs51_set_active_context(&s_ctx);
    mcs51_test_register_family(family);
    mcs51_context_set_family(family);
    mcs51_context_reset(&s_ctx);
    wink_mcs51_xdata_reset();
}

void xwrite(uint64_t addr, uint8_t v) {
    wink_mcs51_xdata_write(addr, v, 0u);
}

uint8_t xread(uint64_t addr) {
    return wink_mcs51_xdata_read(addr, 0u);
}

#define CHECK(cond, msg)                                              \
    do {                                                              \
        if (!(cond)) {                                                \
            printf("[mcs51-xram-aperture] FAIL: %s (line %d)\n",      \
                   msg, __LINE__);                                    \
            ++fails;                                                  \
        }                                                             \
    } while (0)

}  // namespace

extern "C" void wink_mcs51_user_main(void) {}
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

int main(void) {
    // ── CMS8S78xx: 1 KB XRAM ceiling ─────────────────────────────────────
    reset_with_family(MCS51_FAMILY_CMS8S78XX);
    xwrite(0x03FFu, 0xA5u);                       // last legal XRAM byte
    CHECK(xread(0x03FFu) == 0xA5u, "0x03FF legal");
    CHECK(wink_mcs51_xdata_oob_count() == 0u, "no OOB at 0x03FF");

    xwrite(0x0400u, 0x11u);                       // hole starts here
    xread(0x0400u);
    xwrite(0x0EFFFu, 0x22u);                      // still in the hole
    CHECK(wink_mcs51_xdata_oob_count() == 3u, "0x0400..0xEFFF OOB on CMS8S");

    // XSFR window is legal on this family (write lands, unmodeled trips
    // the separate GAP-23 counter, not the OOB counter).
    reset_with_family(MCS51_FAMILY_CMS8S78XX);
    xwrite(0xF000u, 0x01u);                       // P00CFG, allowlisted
    CHECK(wink_mcs51_xdata_oob_count() == 0u, "0xF000 XSFR legal on CMS8S");

    // ── Classic 8052: configurable aperture, NO XSFR window ──────────────
    reset_with_family(MCS51_FAMILY_CLASSIC);
    xwrite(0x03FFu, 0x5Au);
    CHECK(xread(0x03FFu) == 0x5Au, "low XRAM legal on classic");
    CHECK(wink_mcs51_xdata_oob_count() == 0u, "no OOB low");
    // 0xF000 is external MOVX space on classic — outside the configured
    // aperture, not an XSFR register: OOB, and must NOT trip the
    // unmodeled-XSFR counter (that is CMS8S-only).
    xwrite(0xF000u, 0x01u);
    CHECK(wink_mcs51_xdata_oob_count() == 1u, "0xF000 OOB on classic (no XSFR window)");
    CHECK(wink_mcs51_xsfr_unmodeled_count() == 0u,
          "XSFR tripwire is family-gated (silent on classic)");

    if (fails) {
        return 1;
    }
    printf("[mcs51-xram-aperture] PASS: family-gated aperture + XSFR window\n");
    return 0;
}
