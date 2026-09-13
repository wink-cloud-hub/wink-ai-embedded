// SPDX-License-Identifier: Apache-2.0
// GAP-04/GAP-13/GAP-22 regression: per-family silicon reset seeds and the
// IRQ source->vector/profile mapping table.
//
//  * CMS8S78xx reset: CKCON = 0x07 (T1M=T0M=1, Fsys/4 per ref manual 8.2.2),
//    hardware clock seeded to 24 MHz (power-on HSI).
//  * Classic AT89C52 reset: CKCON shadow 0x00 (no CKCON on silicon, fixed
//    Fsys/12), clock_hz left at the 12 MHz family fallback.
//  * IRQ profile map matches the vendor vector table and EIE2/EIF2/EIP1/EIP2
//    bit positions exactly (priority bit = vector-8 on EIP1, vector-16 on
//    EIP2); UART1 is unmapped (0xFF) — CMS8S78xx has no UART1.
//  * GAP-01: GPIO_P13_MUX_RXD matches the vendor gpio.h value (0x03).
//  * GAP-01 macro drift (GPIO_P13_MUX_RXD) is covered by the
//    mcs51_shim_audit.py CI script (vendor gpio.h diff), not by this binary.
#include <stdint.h>
#include <stdio.h>

#include "mcs51_context.h"
#include "mcs51_test_harness.h"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_isr.h"

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

void check_irq(mcs51_irq_source_t src, uint8_t vector,
               uint8_t ie_sfr, uint8_t ie_bit,
               uint8_t flag_sfr, uint8_t flag_bit,
               uint8_t prio_sfr, uint8_t prio_bit,
               const char* name) {
    const mcs51_irq_map_entry_t* e = wink_mcs51_get_irq_map_entry(src);
    if (e == nullptr) {
        printf("FAIL: %s map entry missing\n", name);
        ++g_failures;
        return;
    }
    char msg[96];
    snprintf(msg, sizeof(msg), "%s vector (want %u)", name, vector);
    check(e->vector == vector, msg);
    snprintf(msg, sizeof(msg), "%s IE sfr/bit (want 0x%02X.%u)", name, ie_sfr, ie_bit);
    check(e->ie_sfr == ie_sfr && e->ie_bit == ie_bit, msg);
    snprintf(msg, sizeof(msg), "%s flag sfr/bit (want 0x%02X.%u)", name, flag_sfr, flag_bit);
    check(e->flag_sfr == flag_sfr && e->flag_bit == flag_bit, msg);
    snprintf(msg, sizeof(msg), "%s priority sfr/bit (want 0x%02X.%u)", name, prio_sfr, prio_bit);
    check(e->prio_sfr == prio_sfr && e->prio_bit == prio_bit, msg);
}

}  // namespace

int main(void) {
    // ── 1) CMS8S78xx silicon reset seeds ───────────────────────────────────
    mcs51_test_register_family(MCS51_FAMILY_CMS8S78XX);
    mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);
    Mcu51Context* ctx = mcs51_get_context();
    mcs51_context_reset(ctx);

    check(ctx->sfr_shadow[0x8E] == 0x07u,
          "CMS8S reset CKCON must be 0x07 (T1M=T0M=1, Fsys/4)");
    check(ctx->clock_hz == 24000000u,
          "CMS8S reset hardware clock must be 24 MHz");
    // Port seeds must survive family seeding.
    check(ctx->sfr_shadow[0x80] == 0xFFu && ctx->sfr_shadow[0x90] == 0xFFu &&
          ctx->sfr_shadow[0xA0] == 0xFFu && ctx->sfr_shadow[0xB0] == 0xFFu,
          "P0..P3 reset seeds must remain 0xFF");
    // PS pin-share selectors must keep their 0x7F reset seeds.
    check(ctx->xdata_shadow[0xF0CC] == 0x7Fu &&
          ctx->xdata_shadow[0xF0C0] == 0x7Fu,
          "PS_ADET/PS_INT0 reset seeds must remain 0x7F");

    // ── 2) IRQ profile mapping (GAP-22 / stage5 CPL-06) ────────────────────
    // Core standard rows + the chip package extension (loaded per context by
    // the cms8s_sys reset glue; stage5 moved the extended rows out of core).
    // Priority bits follow the vendor IRQ_SET_PRIORITY macro + module enum:
    // EIP1 bit=module-8, EIP2 bit=module-16 (extended module = vector+1).
    check_irq(IRQ_SOURCE_UART0,  4u,  0xA8, 4u, 0x98, 0u, 0xB8, 4u, "UART0");
    check_irq(IRQ_SOURCE_TIMER2, 5u,  0xA8, 5u, 0xC9, 7u, 0xB8, 5u, "TIMER2");
    check_irq(IRQ_SOURCE_ADC,    19u, 0xAA, 4u, 0xB2, 4u, 0xBA, 4u, "ADC");
    check_irq(IRQ_SOURCE_PWM,    18u, 0xAA, 3u, 0xB2, 3u, 0xBA, 3u, "PWM");
    check_irq(IRQ_SOURCE_I2C,    21u, 0xAA, 6u, 0xB2, 6u, 0xBA, 6u, "I2C");
    check_irq(IRQ_SOURCE_SPI,    22u, 0xAA, 7u, 0xB2, 7u, 0xBA, 7u, "SPI");
    check_irq(IRQ_SOURCE_TIMER3, 15u, 0xAA, 0u, 0xB2, 0u, 0xBA, 0u, "TIMER3");
    check_irq(IRQ_SOURCE_TIMER4, 16u, 0xAA, 1u, 0xB2, 1u, 0xBA, 1u, "TIMER4");
    check_irq(IRQ_SOURCE_WDT,    20u, 0xAA, 5u, 0x97, 3u, 0xBA, 5u, "WDT");
    {
        const mcs51_irq_map_entry_t* u1 =
            wink_mcs51_get_irq_map_entry(IRQ_SOURCE_UART1);
        check(u1 != nullptr && (u1->vector == 0xFFu ||
                                u1->vector >= WINK_MCS51_NUM_VECTORS),
              "UART1 must be unmapped on CMS8S78xx (no UART1 peripheral)");
    }
    // Stage5 CPL-06/08: the chip glue must be per-context installed.
    check(ctx->irq_map_extend != nullptr,
          "CMS8S reset must install the IRQ map extension hook");
    check(ctx->irq_flag_predicate != nullptr,
          "CMS8S reset must install the flag-predicate hook");
    check(ctx->xsfr_validate != nullptr,
          "CMS8S reset must install the XSFR validation hook");
    // S5-1 Step 1b: the T2 predicate accepts any enabled T2IF flag
    // (CC1IF here, T2F clear) and rejects flags disabled in T2IE.
    {
        const mcs51_irq_map_entry_t* t2 =
            wink_mcs51_get_irq_map_entry(IRQ_SOURCE_TIMER2);
        ctx->sfr_shadow[0xC9] = (1u << 1);
        ctx->sfr_shadow[0xCF] = (1u << 1);
        check(ctx->irq_flag_predicate(ctx, IRQ_SOURCE_TIMER2, t2),
              "CMS8S T2 predicate must accept any enabled T2IF flag");
        ctx->sfr_shadow[0xCF] = 0u;
        check(!ctx->irq_flag_predicate(ctx, IRQ_SOURCE_TIMER2, t2),
              "CMS8S T2 predicate must reject flags disabled in T2IE");
        ctx->sfr_shadow[0xC9] = 0u;
    }
    // A state reset (pending/RETI clear) re-applies the chip profile.
    wink_mcs51_reset_irq_state();
    check_irq(IRQ_SOURCE_ADC, 19u, 0xAA, 4u, 0xB2, 4u, 0xBA, 4u,
              "ADC after reset_irq_state");
    check_irq(IRQ_SOURCE_WDT, 20u, 0xAA, 5u, 0x97, 3u, 0xBA, 5u,
              "WDT after reset_irq_state");

    // ── 3) Classic AT89C52 reset seeds + family insulation ─────────────────
    mcs51_test_register_family(MCS51_FAMILY_CLASSIC);
    mcs51_context_set_family(MCS51_FAMILY_CLASSIC);
    mcs51_context_reset(ctx);
    check(ctx->sfr_shadow[0x8E] == 0x00u,
          "Classic 51 CKCON shadow must be 0 (fixed Fsys/12)");
    check(ctx->clock_hz == 0u,
          "Classic 51 leaves clock_hz unset (12 MHz family fallback)");
    check(wink_mcs51_get_clock_hz() == 12000000u,
          "Classic 51 effective hardware clock fallback 12 MHz");
    // Stage5 CPL-06 L1: switching family must clear every chip hook and
    // unmaps the extended sources (no "resurrection" via the old profile).
    check(ctx->irq_map_extend == nullptr && ctx->irq_flag_predicate == nullptr &&
          ctx->xsfr_validate == nullptr,
          "classic reset must clear all chip IRQ/XSFR hooks");
    {
        const mcs51_irq_map_entry_t* adc =
            wink_mcs51_get_irq_map_entry(IRQ_SOURCE_ADC);
        check(adc != nullptr && adc->vector == 0xFFu,
              "classic ADC source must be unmapped");
    }
    wink_mcs51_isr_enable();
    mcs51_raise_irq(IRQ_SOURCE_ADC);
    check((wink_mcs51_get_pending_interrupts() & (1u << IRQ_SOURCE_ADC)) == 0u,
          "classic must drop an extended IRQ at raise time");
    check(wink_mcs51_dispatch_vector(19u) == 0u,
          "classic extended vector must be unreachable");

    // ── 4) GAP-01 note ────────────────────────────────────────────────────
    // The vendor mux macro (GPIO_P13_MUX_RXD == 0x03) is enforced by
    // mcs51_shim_audit.py, which can diff the vendor gpio.h fixture.

    if (g_failures == 0) {
        printf("test_mcs51_silicon_seeds: all checks passed\n");
        return 0;
    }
    printf("test_mcs51_silicon_seeds: %d check(s) failed\n", g_failures);
    return 1;
}
