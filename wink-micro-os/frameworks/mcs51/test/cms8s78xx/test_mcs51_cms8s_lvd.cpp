// SPDX-License-Identifier: GPL-3.0-only
// Unit tests for the CMS8S78xx Low-Voltage Detect (LVD) peripheral model.
#include <stdint.h>
#include <stdio.h>

#include "cms8s78xx.h"
#include "cms8s_lvd.h"
#include "mcs51_context.h"
#include "mcs51_test_harness.h"
#include "wink_mcs51_isr.h"

#undef main
#undef printf
extern "C" void wink_mcs51_user_main(void) {}
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

namespace {

int g_fails = 0;
uint32_t g_lvd_isr_hits = 0;

void check(bool cond, const char *msg) {
    if (!cond) {
        printf("[mcs51_lvd] FAIL: %s\n", msg);
        ++g_fails;
    }
}

bool lvd_pending(void) {
    return (wink_mcs51_get_pending_interrupts() & (1u << IRQ_SOURCE_LVD)) != 0u;
}

void fresh_context(void) {
    Mcu51Context* ctx = mcs51_get_context();
    mcs51_context_reset(ctx);
    wink_mcs51_isr_enable();
    wink_mcs51_xdata_reset();
    wink_mcs51_reset_irq_state();
    g_lvd_isr_hits = 0;
    cms8s_lvd_init(ctx);
    // NOTE: EA stays 0 here. Every SFR/XSFR proxy access runs a microstep
    // that auto-dispatches pending IRQs when EA=1, so flag/pending asserts
    // below run with EA=0 and enable it only around explicit scan calls.
    IRQ_ALL_DISABLE();
}

}  // namespace

WINK_ISR(26) {
    ++g_lvd_isr_hits;
    SYS_ClearLVDIntFlag();
}

int main(void) {
    printf("[mcs51_lvd] Starting CMS8S78xx LVD unit tests...\n");

    mcs51_test_register_family(MCS51_FAMILY_CMS8S78XX);
    mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);
    Mcu51Context* ctx = mcs51_get_context();

    // ── Test 1: LVDINTF is write-0-to-clear (write-1 never fabricates) ──
    fresh_context();
    SYS_ConfigLVD(SYS_LVD_4_0V);
    SYS_EnableLVDInt();
    SYS_EnableLVD();
    LVDCON |= LVD_LVDCON_LVDINTF_Msk;  // firmware write-1: must be ignored
    cms8s_lvd_poll(ctx);
    check(SYS_GetLVDIntFlag() == 0u, "T1: write-1 must not set LVDINTF");
    check(!lvd_pending(), "T1: write-1 must not raise IRQ");
    cms8s_lvd_set_vdd_mv(ctx, 3500u);  // genuine falling edge below 4.0V
    cms8s_lvd_poll(ctx);
    check(SYS_GetLVDIntFlag() == 1u, "T1: falling edge must latch LVDINTF");
    check(lvd_pending(), "T1: falling edge must raise IRQ_SOURCE_LVD");
    SYS_ClearLVDIntFlag();  // firmware write-0 via shadow
    cms8s_lvd_poll(ctx);
    check(SYS_GetLVDIntFlag() == 0u, "T1: write-0 must clear LVDINTF");
    wink_mcs51_clear_irq(IRQ_SOURCE_LVD);

    // ── Test 2: nominal 5V never raises ──
    fresh_context();
    SYS_ConfigLVD(SYS_LVD_4_0V);
    SYS_EnableLVDInt();
    SYS_EnableLVD();
    // No injection: the virtual sense falls back to 1.0 (5.0V nominal).
    cms8s_lvd_poll(ctx);
    cms8s_lvd_poll(ctx);
    check(SYS_GetLVDIntFlag() == 0u, "T2: 5V nominal must not set LVDINTF");
    check(!lvd_pending(), "T2: 5V nominal must not raise IRQ");

    // ── Test 3: falling edge latches flag, raises, dispatches Vector 26 ──
    fresh_context();
    SYS_ConfigLVD(SYS_LVD_4_0V);
    SYS_EnableLVDInt();
    SYS_EnableLVD();
    cms8s_lvd_set_vdd_mv(ctx, 3500u);
    cms8s_lvd_poll(ctx);
    check(SYS_GetLVDIntFlag() == 1u, "T3: 3.5V must latch LVDINTF");
    check(lvd_pending(), "T3: 3.5V must raise IRQ_SOURCE_LVD");
    check(mcs51_irq_scan_and_dispatch() == 0u, "T3: EA=0 must block dispatch");
    check(g_lvd_isr_hits == 0u, "T3: ISR must not run while EA=0");
    // Raise EA by direct shadow write: the firmware-idiomatic IRQ_ALL_ENABLE()
    // is a proxy write whose trailing microstep would auto-dispatch before the
    // explicit scan call below, hiding the path under test.
    ctx->sfr_shadow[0xA8] |= 0x80u;  // EA=1
    check(mcs51_irq_scan_and_dispatch() == 1u, "T3: scan must dispatch once");
    check(g_lvd_isr_hits == 1u, "T3: Vector 26 ISR must run once");
    ctx->sfr_shadow[0xA8] &= ~0x80u;  // EA=0
    cms8s_lvd_poll(ctx);  // settle the ISR's write-0 clear
    check(SYS_GetLVDIntFlag() == 0u, "T3: ISR write-0 must clear LVDINTF");

    // ── Test 4: sustained undervoltage never re-fires (anti-storm latch) ──
    wink_mcs51_reset_irq_state();
    for (int i = 0; i < 5; ++i) {
        cms8s_lvd_poll(ctx);
    }
    check(SYS_GetLVDIntFlag() == 0u, "T4: held 3.5V must not re-latch LVDINTF");
    check(!lvd_pending(), "T4: held 3.5V must not re-raise IRQ");
    check(g_lvd_isr_hits == 1u, "T4: ISR must not re-run while held low");

    // ── Test 5: recovery past hysteresis re-arms the next edge ──
    cms8s_lvd_set_vdd_mv(ctx, 5000u);  // >= 4.1V hysteresis line
    cms8s_lvd_poll(ctx);
    check(!lvd_pending(), "T5: recovery must not raise IRQ");
    cms8s_lvd_set_vdd_mv(ctx, 3500u);  // second falling edge
    cms8s_lvd_poll(ctx);
    check(SYS_GetLVDIntFlag() == 1u, "T5: second edge must latch LVDINTF");
    check(lvd_pending(), "T5: second edge must raise IRQ again");
    ctx->sfr_shadow[0xA8] |= 0x80u;  // EA=1 (direct: see T3 note)
    check(mcs51_irq_scan_and_dispatch() == 1u, "T5: scan must dispatch again");
    check(g_lvd_isr_hits == 2u, "T5: Vector 26 ISR must run a second time");
    ctx->sfr_shadow[0xA8] &= ~0x80u;  // EA=0

    // ── Test 6: LVDEN / LVDINTE gates ──
    fresh_context();
    SYS_ConfigLVD(SYS_LVD_4_0V);
    SYS_EnableLVDInt();
    SYS_EnableLVD();
    cms8s_lvd_set_vdd_mv(ctx, 5000u);
    cms8s_lvd_poll(ctx);  // NORMAL baseline
    SYS_DisableLVD();     // module off
    cms8s_lvd_set_vdd_mv(ctx, 3500u);
    cms8s_lvd_poll(ctx);
    check(!lvd_pending(), "T6: LVDEN=0 must suppress the raise");
    SYS_EnableLVD();
    SYS_DisableLVDInt();  // interrupt gate off, module on
    cms8s_lvd_set_vdd_mv(ctx, 5000u);
    cms8s_lvd_poll(ctx);  // re-align to NORMAL while gated
    cms8s_lvd_set_vdd_mv(ctx, 3500u);
    cms8s_lvd_poll(ctx);
    check(SYS_GetLVDIntFlag() == 1u, "T6: flag still latches with LVDINTE=0");
    check(!lvd_pending(), "T6: LVDINTE=0 must suppress the raise");

    if (g_fails == 0) {
        printf("[mcs51_lvd] All LVD tests passed successfully!\n");
        return 0;
    }
    printf("[mcs51_lvd] FAILED: %d checks failed\n", g_fails);
    return 1;
}
