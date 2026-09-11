// SPDX-License-Identifier: Apache-2.0
// GAP-12: duplicate ISR vector registration is no longer silent.
// (Stage4 S4-1: the P0..P3EXTIF write-0-to-clear half moved to the cms8s
// suite — EXTIF registers are chip silicon, their W0C hooks exist on chip
// families only. See test_mcs51_extif_w0c.cpp.)
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

    if (fails) {
        return 1;
    }
    printf("[mcs51-gap12] PASS: duplicate-vector guard\n");
    return 0;
}
