// SPDX-License-Identifier: GPL-3.0-only
// GAP-12 (5): P0..P3EXTIF are write-0-to-clear like T2IF/EIF2.
//
// Split out of the generic gap12 test in stage4 S4-1 (CPL-07/CPL-22):
// EXTIF registers are chip silicon, so their W0C hooks exist on chip
// families only — the family harness below is load-bearing, not decorative.
#include <stdint.h>
#include <stdio.h>

#include "mcs51_context.h"
#include "mcs51_test_harness.h"

namespace {

Mcu51Context s_ctx;
int fails = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        printf("[mcs51-extif-w0c] FAIL: %s\n", what);
        ++fails;
    }
}

}  // namespace

extern "C" void wink_mcs51_user_main(void) {}
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

int main(void) {
    mcs51_set_active_context(&s_ctx);
    mcs51_test_use_family(MCS51_FAMILY_CMS8S78XX);

    // W0C registers cannot be SET by firmware writes (old & new == 0 when
    // old == 0), so pending bits are poked directly to simulate the HW
    // setting them, then cleared through the registered write hook with
    // vendor-style values (e.g. 0xFB == 0xFF & ~bit2). Same pattern as
    // test_mcs51_timer_ext_clk.cpp Test 5 for T2IF.
    constexpr uint8_t P0EXTIF = 0xB4;
    check(s_ctx.sfr_write_hooks[P0EXTIF] != nullptr, "P0EXTIF has W0C hook");
    check(s_ctx.sfr_write_hooks[0xB7] != nullptr, "P3EXTIF has W0C hook");
    // Pins 0 and 2 of P0 pending (0x05); clear pin 2 via 0xFB.
    s_ctx.sfr_shadow[P0EXTIF] = 0x05u;
    s_ctx.sfr_write_hooks[P0EXTIF](&s_ctx, P0EXTIF, 0x05u, 0xFBu);
    check(s_ctx.sfr_shadow[P0EXTIF] == 0x01u,
          "W0C: write clears bit2, leaves bit0");
    // Writing 1 leaves bits set; writing 0 clears.
    s_ctx.sfr_shadow[P0EXTIF] = 0xFFu;
    s_ctx.sfr_write_hooks[P0EXTIF](&s_ctx, P0EXTIF, 0xFFu, 0x00u);
    check(s_ctx.sfr_shadow[P0EXTIF] == 0x00u, "W0C: write 0x00 clears all");
    // P3EXTIF (0xB7): clear bit0 only.
    s_ctx.sfr_shadow[0xB7] = 0x0Fu;
    s_ctx.sfr_write_hooks[0xB7](&s_ctx, 0xB7, 0x0Fu, 0xFEu);
    check(s_ctx.sfr_shadow[0xB7] == 0x0Eu, "P3EXTIF W0C clears bit0 only");

    if (fails) {
        return 1;
    }
    printf("[mcs51-extif-w0c] PASS: EXTIF write-0-to-clear\n");
    return 0;
}
