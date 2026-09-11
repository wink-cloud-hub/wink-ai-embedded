// SPDX-License-Identifier: Apache-2.0
// GAP-12: (2) duplicate ISR vector registration is no longer silent;
// (5) P0..P3EXTIF are write-0-to-clear like T2IF/EIF2.
#include <stdint.h>
#include <stdio.h>

#include "mcs51_context.h"
#include "wink_mcs51_isr.h"

namespace {

Mcu51Context s_ctx;
int fails = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        printf("[mcs51-gap12] FAIL: %s\n", what);
        ++fails;
    }
}

void isr_a(void) {}
void isr_b(void) {}

}  // namespace

extern "C" void wink_mcs51_user_main(void) {}
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

int main(void) {
    mcs51_set_active_context(&s_ctx);
    mcs51_context_reset(&s_ctx);
    wink_mcs51_reset_isrs();

    // ── (2) duplicate vector ─────────────────────────────────────────────
    check(wink_mcs51_duplicate_vector_count() == 0u, "no dup initially");
    wink_mcs51_set_isr(7, isr_a);
    check(wink_mcs51_duplicate_vector_count() == 0u, "first registration clean");
    check(wink_mcs51_get_isr(7) == isr_a, "handler a installed");
    wink_mcs51_set_isr(7, isr_b);
    check(wink_mcs51_duplicate_vector_count() == 1u, "second registration counts");
    check(wink_mcs51_get_isr(7) == isr_b, "newest handler wins");
    wink_mcs51_set_isr(7, isr_a);
    check(wink_mcs51_duplicate_vector_count() == 2u, "third registration counts again");
    // A different vector is independent.
    wink_mcs51_set_isr(3, isr_a);
    check(wink_mcs51_duplicate_vector_count() == 2u, "other vector not a dup");

    // ── (5) EXTIF write-0-to-clear ─────────────────────────────────────
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
    printf("[mcs51-gap12] PASS: duplicate-vector guard + EXTIF W0C\n");
    return 0;
}
