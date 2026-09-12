// SPDX-License-Identifier: Apache-2.0
// Stage0 (PLAN-20260911-MCS51-S0): McuFamilyDescriptor v2 schema freeze +
// caps_cache snapshot + STRICT stable-number locks. Zero behavior change:
// this TU only reads the descriptor table and the reset snapshot.
#include <stdint.h>
#include <stdio.h>

#include "mcs51_context.h"
#include "mcs51_test_harness.h"
#include "mcs51_family.h"
#include "wink_mcs51_strict.h"

extern "C" void wink_mcs51_user_main(void) {}
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

namespace {

int g_failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        ++g_failures;
        printf("FAIL: %s\n", what);
    }
}

}  // namespace

int main(void) {
    // ── 1) Classic descriptor row ────────────────────────────────────────
    const mcs51_family_desc_t* classic =
        mcs51_family_desc(MCS51_FAMILY_CLASSIC);
    check(classic->capabilities == 0u, "classic caps must be 0 (pure 8051)");
    check(classic->port_pin_masks[0] == 8u &&
          classic->port_pin_masks[1] == 8u &&
          classic->port_pin_masks[2] == 8u &&
          classic->port_pin_masks[3] == 8u, "classic pins {8,8,8,8}");
    check(mcs51_family_port_pin_count(classic, 0u) == 8u &&
          mcs51_family_pin_valid(classic, 3u, 7u) &&
          !mcs51_family_pin_valid(classic, 4u, 0u) &&
          !mcs51_family_pin_valid(classic, 0u, 8u),
          "classic pin accessors (full ports, bounds closed)");
    check(classic->irq_count == 6u, "classic irq_count 6");
    for (uint8_t i = 0; i < classic->irq_count; ++i) {
        char msg[64];
        snprintf(msg, sizeof(msg), "classic vector[%u]==%u", i, i);
        check(classic->irq_vector_table[i] == i, msg);
    }
    check(!classic->wdt_present, "classic wdt absent");
    check(!classic->iap_present, "classic iap absent");
    check(classic->uart_count == 1u, "classic uart_count 1");
    check(classic->timer_caps ==
          (uint8_t)(MCS51_TIMER_CAP_T0 | MCS51_TIMER_CAP_T1 |
                    MCS51_TIMER_CAP_T2), "classic timers T0|T1|T2");
    check(!mcs51_family_has_xsfr(classic), "classic has no XSFR window");

    // ── 2) CMS8S78xx descriptor row ──────────────────────────────────────
    const mcs51_family_desc_t* cms8s =
        mcs51_family_desc(MCS51_FAMILY_CMS8S78XX);
    check((cms8s->capabilities & MCS51_CAP_ENHANCED_IO) != 0u,
          "cms8s caps ENHANCED_IO");
    check((cms8s->capabilities & MCS51_CAP_TIMER34) != 0u,
          "cms8s caps TIMER34");
    check((cms8s->capabilities & MCS51_CAP_PORT_EXTINT) != 0u,
          "cms8s caps PORT_EXTINT");
    check((cms8s->capabilities & MCS51_CAP_UART_REMAP) != 0u,
          "cms8s caps UART_REMAP");
    check((cms8s->capabilities & MCS51_CAP_CHIP_MODELS) != 0u,
          "cms8s caps CHIP_MODELS");
    check(cms8s->port_pin_masks[0] == 8u &&
          cms8s->port_pin_masks[1] == 8u &&
          cms8s->port_pin_masks[2] == 6u &&
          cms8s->port_pin_masks[3] == 4u, "cms8s pins {8,8,6,4}");
    check(mcs51_family_port_pin_count(cms8s, 2u) == 6u &&
          mcs51_family_port_pin_count(cms8s, 3u) == 4u &&
          mcs51_family_pin_valid(cms8s, 2u, 5u) &&
          !mcs51_family_pin_valid(cms8s, 2u, 6u) &&
          !mcs51_family_pin_valid(cms8s, 3u, 4u),
          "cms8s pin accessors (P2 6 / P3 4, ghosts rejected)");
    check(cms8s->irq_count == 28u, "cms8s irq_count 28");
    check(cms8s->wdt_present, "cms8s wdt present");
    check(cms8s->iap_present, "cms8s iap present");
    check(cms8s->uart_count == 1u, "cms8s uart_count 1 (no UART1)");
    check(cms8s->timer_caps ==
          (uint8_t)(MCS51_TIMER_CAP_T0 | MCS51_TIMER_CAP_T1 |
                    MCS51_TIMER_CAP_T2 | MCS51_TIMER_CAP_T3 |
                    MCS51_TIMER_CAP_T4 | MCS51_TIMER_CAP_CAPTURE),
          "cms8s timers T0..T4+CAPTURE");
    check(mcs51_family_has_xsfr(cms8s), "cms8s has XSFR window");

    // ── 3) caps_cache snapshot on reset/set_family ───────────────────────
    Mcu51Context* ctx = mcs51_get_context();
    mcs51_test_register_family(MCS51_FAMILY_CMS8S78XX);
    mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);
    mcs51_context_reset(ctx);
    check(ctx->caps_cache == cms8s->capabilities,
          "caps_cache snapshots cms8s caps after reset");
    mcs51_test_register_family(MCS51_FAMILY_CLASSIC);
    mcs51_context_set_family(MCS51_FAMILY_CLASSIC);
    check(ctx->caps_cache == classic->capabilities,
          "caps_cache snapshots classic caps after set_family");
    mcs51_context_reset(ctx);
    check(ctx->caps_cache == classic->capabilities,
          "caps_cache survives classic reset");
    // Restore the suite default (classic) for later TUs in this process.
    mcs51_test_register_family(MCS51_FAMILY_CLASSIC);
    mcs51_context_set_family(MCS51_FAMILY_CLASSIC);

    // ── 4) STRICT feature numbers frozen (never renumber) ───────────────
    check(MCS51_FEAT_PSW_FLAGS == 1, "STRICT PSW_FLAGS==1");
    check(MCS51_FEAT_COMPUTED_SFR_ADDR == 2, "STRICT COMPUTED_SFR_ADDR==2");
    check(MCS51_FEAT_INLINE_ASM == 3, "STRICT INLINE_ASM==3");
    check(MCS51_FEAT_AT_ABSOLUTE == 4, "STRICT AT_ABSOLUTE==4");
    check(MCS51_FEAT_RC_THERMAL == 5, "STRICT RC_THERMAL==5");
    check(MCS51_FEAT_GENERIC_POINTER == 6, "STRICT GENERIC_POINTER==6");
    check(MCS51_FEAT_ABS_SBIT_ADDR == 7, "STRICT ABS_SBIT_ADDR==7");
    check(MCS51_FEAT_SUBUS_TIMING == 8, "STRICT SUBUS_TIMING==8");
    check(MCS51_FEAT_TIMER_MODE3 == 9, "STRICT TIMER_MODE3==9");
    check(MCS51_FEAT_TIMER_EXT_CLK == 10, "STRICT TIMER_EXT_CLK==10");
    check(MCS51_FEAT_IAP_FLASH == 11, "STRICT IAP_FLASH==11");

    if (g_failures == 0) {
        printf("test_mcs51_family_schema: all checks passed\n");
        return 0;
    }
    printf("test_mcs51_family_schema: %d check(s) failed\n", g_failures);
    return 1;
}
