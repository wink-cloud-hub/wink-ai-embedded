// SPDX-License-Identifier: Apache-2.0
// GAP-12: T2/T3/T4 period formulas must follow Fsys (the old code hardcoded
// the 24 MHz reciprocals counts/2 and counts/6). Uses the public test
// wrappers wink_mcs51_test_timer{2,3,4}_reload_period.
#include <stdint.h>
#include <stdio.h>

#include "mcs51_context.h"
#include "mcs51_test_harness.h"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_timer.h"

namespace {

Mcu51Context s_ctx;
int fails = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        printf("[mcs51-t234-fsys] FAIL: %s\n", what);
        ++fails;
    }
}

void begin(uint32_t fsys_hz) {
    mcs51_set_active_context(&s_ctx);
    mcs51_test_register_family(MCS51_FAMILY_CMS8S78XX);
    mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);
    mcs51_context_reset(&s_ctx);
    wink_mcs51_set_hardware_clock_hz(fsys_hz);
}

}  // namespace

extern "C" void wink_mcs51_user_main(void) {}
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

int main(void) {
    // T2: reload 0xFF00 -> counts = 256; T2PS=0 -> Fsys/12.
    //   24 MHz: 256*12/24 = 128 us ; 12 MHz: 256 us (2x).
    begin(24000000u);
    s_ctx.sfr_shadow[0xC8] = 0x00;   // T2CON reload mode, T2PS=0
    s_ctx.sfr_shadow[0xCA] = 0x00;   // RLDL
    s_ctx.sfr_shadow[0xCB] = 0xFF;   // RLDH
    const uint32_t t2_24 = wink_mcs51_test_timer2_reload_period();
    check(t2_24 == 128u, "T2 Fsys/12 @24MHz = 128 us");

    begin(12000000u);
    s_ctx.sfr_shadow[0xC8] = 0x00;
    s_ctx.sfr_shadow[0xCA] = 0x00;
    s_ctx.sfr_shadow[0xCB] = 0xFF;
    const uint32_t t2_12 = wink_mcs51_test_timer2_reload_period();
    check(t2_12 == 256u, "T2 Fsys/12 @12MHz = 256 us (follows Fsys)");
    check(t2_12 == 2u * t2_24, "T2 doubles when Fsys halves");

    // T2PS=1 -> Fsys/24: 256*24/24 = 256 us @24 MHz.
    begin(24000000u);
    s_ctx.sfr_shadow[0xC8] = 0x80;   // T2PS=1
    s_ctx.sfr_shadow[0xCA] = 0x00;
    s_ctx.sfr_shadow[0xCB] = 0xFF;
    check(wink_mcs51_test_timer2_reload_period() == 256u, "T2 Fsys/24 @24MHz = 256 us");

    // T3: mode 2 (bits[1:0]=10), reload TL3=0 -> counts=256.
    //   T3M(bit2)=0 Fsys/12 -> 128 us @24; T3M=1 Fsys/4 -> 256*4/24=42 us.
    begin(24000000u);
    s_ctx.sfr_shadow[0xD2] = 0x02;   // T3 mode 2, T3M=0
    s_ctx.sfr_shadow[0xDA] = 0x00;   // TL3
    check(wink_mcs51_test_timer3_reload_period() == 128u, "T3 Fsys/12 @24MHz = 128 us");
    s_ctx.sfr_shadow[0xD2] = 0x06;   // + T3M=1
    check(wink_mcs51_test_timer3_reload_period() == 42u,
          "T3 Fsys/4 @24MHz = 42 us (parameterized, not counts/6)");
    begin(12000000u);
    s_ctx.sfr_shadow[0xD2] = 0x02;
    s_ctx.sfr_shadow[0xDA] = 0x00;
    check(wink_mcs51_test_timer3_reload_period() == 256u, "T3 follows Fsys @12MHz");

    // T4: mode 2 (bits[5:4]=01 -> 0x20), reload TL4(0xE2)=0 -> counts=256.
    begin(24000000u);
    s_ctx.sfr_shadow[0xD2] = 0x20;   // T4 mode 2, T4M(bit6)=0
    s_ctx.sfr_shadow[0xE2] = 0x00;   // TL4
    check(wink_mcs51_test_timer4_reload_period() == 128u, "T4 Fsys/12 @24MHz = 128 us");
    s_ctx.sfr_shadow[0xD2] = 0x60;   // mode2 + T4M=1
    check(wink_mcs51_test_timer4_reload_period() == 42u, "T4 Fsys/4 @24MHz = 42 us");
    begin(12000000u);
    s_ctx.sfr_shadow[0xD2] = 0x20;
    s_ctx.sfr_shadow[0xE2] = 0x00;
    check(wink_mcs51_test_timer4_reload_period() == 256u, "T4 follows Fsys @12MHz");

    if (fails) {
        return 1;
    }
    printf("[mcs51-t234-fsys] PASS: T2/3/4 periods follow Fsys (GAP-12)\n");
    return 0;
}
