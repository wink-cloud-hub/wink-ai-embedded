// SPDX-License-Identifier: GPL-3.0-only
// Unit tests for CMS8S78xx on-chip Enhanced PWM (EPWM) peripheral model.
#include <stdint.h>
#include <stdio.h>

#include "cms8s_epwm.h"
#include "cms8s_priv.h"
#include "cms8s78xx.h"
#include "mcs51_context.h"
#include "mcs51_test_harness.h"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_isr.h"

#undef main
#undef printf
extern "C" void wink_mcs51_user_main(void) {}
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}
extern "C" void wink_mcs51_host_set_ext_pin(uint16_t pin, uint8_t state);

namespace {

int g_fails = 0;
uint32_t g_epwm_isr_hits = 0;
uint32_t g_epwm_fb_hits = 0;
uint8_t  g_last_cleared_ch = 0xFFu;

void check(bool cond, const char *msg) {
    if (!cond) {
        printf("[mcs51_epwm] FAIL: %s (isr_hits=%u, fb_hits=%u, ZIF=0x%02X, ZIE=0x%02X)\n",
               msg, g_epwm_isr_hits, g_epwm_fb_hits,
               mcs51_get_context()->xdata_shadow[0xF16Du],
               mcs51_get_context()->xdata_shadow[0xF169u]);
        ++g_fails;
    }
}

void test_epwm_isr(void) {
    ++g_epwm_isr_hits;
    if (EPWM_GetZeroIntFlag(EPWM0)) {
        EPWM_ClearZeroIntFlag(EPWM0);
        g_last_cleared_ch = EPWM0;
    }
    if (EPWM_GetFaultBrakeIntFlag()) {
        ++g_epwm_fb_hits;
        EPWM_ClearFaultBrakeIntFlag();
    }
}

}  // namespace

int main(void) {
    printf("[mcs51_epwm] Starting CMS8S78xx EPWM unit tests...\n");

    mcs51_test_register_family(MCS51_FAMILY_CMS8S78XX);
    mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);
    Mcu51Context* ctx = mcs51_get_context();
    mcs51_context_reset(ctx);
    wink_mcs51_isr_enable();
    wink_mcs51_xdata_reset();

    cms8s_epwm_init(ctx);

    // Register ISR for EPWM vector (vector 18)
    ctx->isr_table[18] = test_epwm_isr;

    // ── Test 1: Down-Count Mode (周期与过零中断) ──────────────────────────────
    printf("[mcs51_epwm] Test 1: Down-count mode zero interrupt\n");
    g_epwm_isr_hits = 0;

    // Enable EA and EPWM interrupt in EIE2 (bit 3)
    EA = 1;
    EPWM_AllIntEnable();
    check((EIE2 & IRQ_EIE2_PWMIE_Msk) != 0, "EIE2.PWMIE is enabled");

    // Configure Down-Count, Period = 4800 (200us at 24MHz), Div=1
    EPWM_ConfigRunMode(EPWM_COUNT_DOWN);
    EPWM_ConfigChannelClk(EPWM0, EPWM_CLK_DIV_1);
    EPWM_ConfigChannelPeriod(EPWM0, 0x12C0); // 4800 ticks
    EPWM_ConfigChannelSymDuty(EPWM0, 0x0960);
    EPWM_EnableZeroInt(EPWM_CH_0_MSK);

    EPWM_Start(EPWM_CH_0_MSK);
    check((PWMCNTE & EPWM_CH_0_MSK) != 0, "PWMCNTE CH0 is enabled");

    // Advance 100us: not reached 0 yet
    wink_mcs51_test_advance_virtual_us(100u);
    cms8s_epwm_poll(ctx);
    check(g_epwm_isr_hits == 0, "No ISR hit at 100us (half period)");

    // Advance another 100us (total 200us): zero event should trigger
    wink_mcs51_test_advance_virtual_us(100u);
    cms8s_epwm_poll(ctx);
    mcs51_irq_scan_and_dispatch();

    check(g_epwm_isr_hits == 1, "ISR hit at 200us (first cycle complete)");
    check(g_last_cleared_ch == EPWM0, "Zero flag cleared in ISR");
    check(EPWM_GetZeroIntFlag(EPWM0) == 0, "Zero flag bit is 0 after clear");

    // Advance another 200us (total 400us): second zero event
    wink_mcs51_test_advance_virtual_us(200u);
    cms8s_epwm_poll(ctx);
    mcs51_irq_scan_and_dispatch();
    check(g_epwm_isr_hits == 2, "Second ISR hit at 400us");

    // ── Test 2: Stop and Counter Hold ─────────────────────────────────────────
    printf("[mcs51_epwm] Test 2: Stop and counter freeze\n");
    EPWM_Stop(EPWM_CH_0_MSK); // Stop EPWM via PWMCNTE
    check((PWMCNTE & EPWM_CH_0_MSK) == 0, "PWMCNTE CH0 is cleared");
    wink_mcs51_test_advance_virtual_us(500u);
    cms8s_epwm_poll(ctx);
    mcs51_irq_scan_and_dispatch();
    check(g_epwm_isr_hits == 2, "No ISR while stopped");

    // ── Test 3: Up-Down Count Mode (中心对称模式) ─────────────────────────────
    printf("[mcs51_epwm] Test 3: Up-down count mode zero and period flags\n");
    mcs51_context_reset(ctx);
    wink_mcs51_isr_enable();
    wink_mcs51_xdata_reset();
    cms8s_epwm_init(ctx);
    ctx->isr_table[18] = test_epwm_isr;

    EA = 1;
    EPWM_AllIntEnable();
    EPWM_ConfigRunMode(EPWM_COUNT_UP_DOWN);
    EPWM_ConfigChannelClk(EPWM0, EPWM_CLK_DIV_1);
    EPWM_ConfigChannelPeriod(EPWM0, 0x12C0); // 4800 ticks (200us up, 200us down)
    EPWM_EnableZeroInt(EPWM_CH_0_MSK);

    EPWM_Start(EPWM_CH_0_MSK);
    g_epwm_isr_hits = 0;

    // Advance 200us: should hit Period match (top of cycle)
    wink_mcs51_test_advance_virtual_us(200u);
    cms8s_epwm_poll(ctx);
    check((PWMPIF & EPWM_CH_0_MSK) != 0, "Period match flag set at 200us");
    PWMPIF &= ~EPWM_CH_0_MSK; // clear period flag

    // Advance another 200us (total 400us): should hit Zero match (bottom of cycle)
    wink_mcs51_test_advance_virtual_us(200u);
    cms8s_epwm_poll(ctx);
    mcs51_irq_scan_and_dispatch();
    check(g_epwm_isr_hits == 1, "Zero match ISR fired at 400us in up-down mode");

    // ── Test 4: next_event_us Scheduling ──────────────────────────────────────
    printf("[mcs51_epwm] Test 4: next_event_us computation\n");
    uint64_t next_evt = cms8s_epwm_next_event_us(ctx);
    uint64_t cur_us = wink_mcs51_virtual_us();
    check(next_evt > cur_us, "next_event_us is in the future");
    check(next_evt <= cur_us + 250u, "next_event_us is scheduled within half cycle");

    // ── Test 5: Software Fault Brake & Recover Mode ───────────────────────────
    printf("[mcs51_epwm] Test 5: Software Fault Brake & Recover Mode\n");
    mcs51_context_reset(ctx);
    wink_mcs51_isr_enable();
    wink_mcs51_xdata_reset();
    cms8s_epwm_init(ctx);
    ctx->isr_table[18] = test_epwm_isr;
    g_epwm_isr_hits = 0;
    g_epwm_fb_hits = 0;

    EA = 1;
    EPWM_AllIntEnable();
    EPWM_ConfigRunMode(EPWM_COUNT_DOWN);
    EPWM_ConfigChannelClk(EPWM0, EPWM_CLK_DIV_1);
    EPWM_ConfigChannelPeriod(EPWM0, 0x12C0);
    EPWM_EnableZeroInt(EPWM_CH_0_MSK);

    EPWM_ConfigBrakeMode(EPWM_BRK_RECOVER, EPWM_BRK_LOAD_EPWM0);
    EPWM_ConfigChannelBrakeLevel(EPWM_CH_0_MSK | EPWM_CH_1_MSK, 1);
    EPWM_ConfigChannelBrakeLevel(EPWM_CH_2_MSK | EPWM_CH_3_MSK, 0);
    EPWM_EnableFaultBrake();
    EPWM_EnableFaultBrakeInt();

    EPWM_Start(EPWM_CH_0_MSK);

    // Run 100us
    wink_mcs51_test_advance_virtual_us(100u);
    cms8s_epwm_poll(ctx);
    check(EPWM_GetBrakeActiveFlag() == 0, "Brake not active initially");
    check(EPWM_GetBrakeOutputStatus() == 0, "BRKOSF is 0 initially");

    // Trigger Software Brake
    EPWM_TrigSoftwareBrake();
    cms8s_epwm_poll(ctx);
    mcs51_irq_scan_and_dispatch();

    check(EPWM_GetBrakeActiveFlag() == 1, "Brake is active (BRKAF=1)");
    check(EPWM_GetBrakeOutputStatus() == 1, "Brake output status active (BRKOSF=1)");
    check(g_epwm_fb_hits == 1, "Fault brake ISR hit on trigger");
    check(EPWM_GetFaultBrakeIntFlag() == 0, "Fault brake flag cleared by ISR");

    // Release software brake
    EPWM_DisableSoftwareBrake();
    cms8s_epwm_poll(ctx);
    check(EPWM_GetBrakeActiveFlag() == 0, "Brake input removed (BRKAF=0)");
    check(EPWM_GetBrakeOutputStatus() == 1, "In Recover mode, BRKOSF still 1 until reload");

    // Advance to next zero reload (100us)
    wink_mcs51_test_advance_virtual_us(100u);
    cms8s_epwm_poll(ctx);
    mcs51_irq_scan_and_dispatch();
    check(EPWM_GetBrakeOutputStatus() == 0, "BRKOSF cleared on reload in Recover mode");

    // ── Test 6: Brake Suspend Mode ──────────────────────────────────
    printf("[mcs51_epwm] Test 6: Brake Suspend Mode\n");
    EPWM_ConfigBrakeMode(EPWM_BRK_SUSPEND, EPWM_BRK_LOAD_EPWM0);
    EPWM_TrigSoftwareBrake();
    cms8s_epwm_poll(ctx);
    check(EPWM_GetBrakeOutputStatus() == 1, "Suspend mode: BRKOSF=1 while brake active");

    EPWM_DisableSoftwareBrake();
    cms8s_epwm_poll(ctx);
    check(EPWM_GetBrakeActiveFlag() == 0, "Suspend mode: BRKAF=0 after release");
    check(EPWM_GetBrakeOutputStatus() == 1, "Suspend mode: BRKOSF remains 1 until clear");

    EPWM_ClearFaultBrake();
    cms8s_epwm_poll(ctx);
    check(EPWM_GetBrakeOutputStatus() == 0, "Suspend mode: BRKOSF cleared by EPWM_ClearFaultBrake()");

    // ── Test 7: Brake Stop Mode & Manual Clear ────────────────────────────────
    printf("[mcs51_epwm] Test 7: Brake Stop Mode & Manual Clear\n");
    EPWM_ConfigBrakeMode(EPWM_BRK_STOP, EPWM_BRK_LOAD_EPWM0);
    EPWM_TrigSoftwareBrake();
    cms8s_epwm_poll(ctx);
    check(EPWM_GetBrakeOutputStatus() == 1, "Stop mode: BRKOSF=1");
    check((PWMCNTE & EPWM_CH_0_MSK) == 0, "Stop mode: PWMCNTE hardware cleared to 0");

    EPWM_DisableSoftwareBrake();
    cms8s_epwm_poll(ctx);
    check(EPWM_GetBrakeOutputStatus() == 1, "Stop mode: BRKOSF still 1 after brake released");

    // Even after reload period, remains stopped
    wink_mcs51_test_advance_virtual_us(300u);
    cms8s_epwm_poll(ctx);
    check(EPWM_GetBrakeOutputStatus() == 1, "Stop mode: still 1 after reload");

    // Manual clear via EPWM_ClearFaultBrake()
    EPWM_ClearFaultBrake();
    cms8s_epwm_poll(ctx);
    check(EPWM_GetBrakeOutputStatus() == 0, "Stop mode: cleared by EPWM_ClearFaultBrake()");

    // ── Test 8: Hardware Pin Fault Brake (FB0 on P1.4) ────────────────────────
    printf("[mcs51_epwm] Test 8: Hardware Pin Fault Brake (FB0 on P1.4)\n");
    EPWM_ConfigBrakeMode(EPWM_BRK_SUSPEND, EPWM_BRK_LOAD_EPWM0);
    EPWM_EnableFBBrake(EPWM_BRK_FB0, EPWM_BRK_FB_LOW);
    wink_mcs51_host_set_ext_pin(12u, 1u); // P1.4 = 1 (normal)
    cms8s_epwm_poll(ctx);
    check(EPWM_GetBrakeActiveFlag() == 0, "FB0 pin high: not braking");

    wink_mcs51_host_set_ext_pin(12u, 0u); // P1.4 = 0 (tripped!)
    cms8s_epwm_poll(ctx);
    check(EPWM_GetBrakeActiveFlag() == 1, "FB0 pin low: brake tripped!");
    check(EPWM_GetBrakeOutputStatus() == 1, "FB0 pin low: BRKOSF active");

    wink_mcs51_host_set_ext_pin(12u, 1u); // P1.4 back high
    cms8s_epwm_poll(ctx);
    check(EPWM_GetBrakeActiveFlag() == 0, "FB0 pin high: brake released");
    EPWM_ClearFaultBrake();
    cms8s_epwm_poll(ctx);
    check(EPWM_GetBrakeOutputStatus() == 0, "Suspend mode: BRKOSF cleared");
    EPWM_DisableFBBrake(EPWM_BRK_FB0);

    // ── Test 9: ACMP Comparator Brake Linkage ─────────────────────────────────
    printf("[mcs51_epwm] Test 9: ACMP Comparator Brake Linkage\n");
    EPWM_ConfigBrakeMode(EPWM_BRK_SUSPEND, EPWM_BRK_LOAD_EPWM0);
    EPWM_EnableACMPLEBrake(EPWM_BRK_ACMP0, EPWM_BRK_ACMP_HIGH);
    Cms8sPriv* priv = cms8s_priv(ctx);
    priv->acmp.last_c0out = 0u;
    cms8s_epwm_poll(ctx);
    check(EPWM_GetBrakeActiveFlag() == 0, "ACMP0 output 0: not braking");

    priv->acmp.last_c0out = 1u;
    cms8s_epwm_poll(ctx);
    check(EPWM_GetBrakeActiveFlag() == 1, "ACMP0 output 1: brake tripped!");
    check(EPWM_GetBrakeOutputStatus() == 1, "ACMP0 brake: BRKOSF active");

    priv->acmp.last_c0out = 0u;
    cms8s_epwm_poll(ctx);
    check(EPWM_GetBrakeActiveFlag() == 0, "ACMP0 back to 0: brake released");
    EPWM_ClearFaultBrake();
    cms8s_epwm_poll(ctx);
    check(EPWM_GetBrakeOutputStatus() == 0, "BRKOSF cleared after ACMP brake release");
    EPWM_DisableACMPLEBrake(EPWM_BRK_ACMP0);

    // ── Test 10: Complementary Mode Shoot-Through Prevention ──────────────────
    printf("[mcs51_epwm] Test 10: Complementary mode PG0/PG1 shoot-through prevention\n");
    mcs51_context_reset(ctx);
    wink_mcs51_xdata_reset();
    cms8s_epwm_init(ctx);
    priv = cms8s_priv(ctx);

    EPWM_ConfigRunMode(EPWM_WFG_COMPLEMENTARY | EPWM_OC_INDEPENDENT | EPWM_OCU_SYMMETRIC | EPWM_COUNT_DOWN);
    EPWM_ConfigChannelClk(EPWM0, EPWM_CLK_DIV_1);
    EPWM_ConfigChannelPeriod(EPWM0, 4800);
    EPWM_ConfigChannelSymDuty(EPWM0, 2400); // 50% duty
    EPWM_EnableOutput(EPWM_CH_0_MSK | EPWM_CH_1_MSK);
    // Configure P2.0 and P2.1 for EPWM (P20CFG=0x04, P21CFG=0x04)
    ctx->xdata_shadow[0xF020u] = 0x04u;
    ctx->xdata_shadow[0xF021u] = 0x04u;

    EPWM_Start(EPWM_CH_0_MSK);

    bool shoot_through = false;
    for (uint32_t step = 0; step < 20; ++step) {
        wink_mcs51_test_advance_virtual_us(10u);
        cms8s_epwm_poll(ctx);
        uint8_t pg0 = priv->epwm.pg_pin_level[0];
        uint8_t pg1 = priv->epwm.pg_pin_level[1];
        if (pg0 == 1 && pg1 == 1) {
            shoot_through = true;
        }
        check(pg0 != pg1, "PG0 and PG1 must be strictly complementary");
    }
    check(!shoot_through, "No shoot-through condition (PG0=1 && PG1=1) observed");

    // ── Test 11: Up-Down Count Mode Non-50% Duty Center-Alignment ─────────────
    printf("[mcs51_epwm] Test 11: Up-down count mode non-50%% duty center-alignment\n");
    mcs51_context_reset(ctx);
    wink_mcs51_xdata_reset();
    cms8s_epwm_init(ctx);
    priv = cms8s_priv(ctx);

    EPWM_ConfigRunMode(EPWM_WFG_INDEPENDENT | EPWM_OC_INDEPENDENT | EPWM_OCU_SYMMETRIC | EPWM_COUNT_UP_DOWN);
    EPWM_ConfigChannelClk(EPWM0, EPWM_CLK_DIV_1);
    EPWM_ConfigChannelPeriod(EPWM0, 4800); // 200us up, 200us down, total 400us period
    EPWM_ConfigChannelSymDuty(EPWM0, 3600); // Duty CMP=3600: High only when CNT > 3600 (top 50us of up, top 50us of down)
    EPWM_EnableOutput(EPWM_CH_0_MSK);
    ctx->xdata_shadow[0xF020u] = 0x04u;

    EPWM_Start(EPWM_CH_0_MSK);

    // At t=50us (T/8 = 50us, CNT ~ 1200 < 3600): should be LOW (0)
    wink_mcs51_test_advance_virtual_us(50u);
    cms8s_epwm_poll(ctx);
    check(priv->epwm.pg_pin_level[0] == 0, "Up-down count at 50us (T/8) must be LOW");

    // At t=200us (T/2 = 200us, CNT = 4800 >= 3600): should be HIGH (1)
    wink_mcs51_test_advance_virtual_us(150u);
    cms8s_epwm_poll(ctx);
    check(priv->epwm.pg_pin_level[0] == 1, "Up-down count at 200us (center peak) must be HIGH");

    // At t=350us (7T/8 = 350us, CNT ~ 1200 < 3600): should be LOW (0)
    wink_mcs51_test_advance_virtual_us(150u);
    cms8s_epwm_poll(ctx);
    check(priv->epwm.pg_pin_level[0] == 0, "Up-down count at 350us (7T/8) must be LOW");

    if (g_fails == 0) {
        printf("[mcs51_epwm] PASS: all CMS8S78xx EPWM unit tests passed.\n");
        return 0;
    } else {
        printf("[mcs51_epwm] FAIL: %d tests failed.\n", g_fails);
        return 1;
    }
}
