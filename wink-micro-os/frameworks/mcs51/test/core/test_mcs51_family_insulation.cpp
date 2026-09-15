// SPDX-License-Identifier: GPL-3.0-only
// Stage5 S5-1 Step 2 (PLAN-20260911-MCS51-S5, CPL-06/08): family insulation.
//
//  * A classic context rejects extended vectors even with an ISR registered
//    (descriptor irq_vector_table whitelist) and drops extended IRQ raises.
//  * Dual contexts of different families coexist: a CMS8S context keeps its
//    extended profile while a classic context is reset beside it.
//  * The XSFR window is closed on classic: an in-window address is ordinary
//    out-of-bounds external XDATA and the chip validation hook is never
//    consulted; the CMS8S context still validates its declared register set.
#include <stdint.h>
#include <stdio.h>

#include "absacc.h"
#include "mcs51_context.h"
#include "mcs51_family.h"
#include "mcs51_test_harness.h"
#include "wink_mcs51_isr.h"

namespace {

int g_fails = 0;
uint32_t g_ext_isr_hits = 0;
uint32_t g_validator_calls = 0;

void check(bool cond, const char* msg) {
    if (!cond) {
        printf("[mcs51-insulation] FAIL: %s\n", msg);
        ++g_fails;
    }
}

void ext_vector_isr(void) { ++g_ext_isr_hits; }

bool counting_validator(struct Mcu51Context* ctx, uint64_t addr) {
    (void)ctx;
    ++g_validator_calls;
    (void)addr;
    return false;
}

Mcu51Context s_ctx_a = {};
Mcu51Context s_ctx_b = {};

}  // namespace

extern "C" void wink_mcs51_user_main(void) {}
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

int main(void) {
    // ── A: CMS8S context loads the extended profile ───────────────────────
    // set_family seeds the ACTIVE context, so pin A as active first.
    mcs51_context_init(&s_ctx_a, 0u);
    mcs51_set_active_context(&s_ctx_a);
    mcs51_test_register_family(MCS51_FAMILY_CMS8S78XX);
    mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);
    mcs51_context_reset(&s_ctx_a);
    wink_mcs51_reset_isrs();  // clean ISR state; keeps the loaded profile
    wink_mcs51_set_isr(19u, ext_vector_isr);  // simulate static registration
    wink_mcs51_isr_enable();
    check(wink_mcs51_dispatch_vector(19u) == 1u && g_ext_isr_hits == 1u,
          "A: CMS8S extended vector must dispatch");

    // ── B: classic context resets beside it, fully insulated ──────────────
    // Switch active BEFORE set_family: the selector seeds the ACTIVE
    // context, and A must keep its CMS8S family across B's creation.
    mcs51_context_init(&s_ctx_b, 1u);
    mcs51_set_active_context(&s_ctx_b);
    mcs51_test_register_family(MCS51_FAMILY_CLASSIC);
    mcs51_context_set_family(MCS51_FAMILY_CLASSIC);
    mcs51_context_reset(&s_ctx_b);
    {
        const mcs51_irq_map_entry_t* adc =
            wink_mcs51_get_irq_map_entry(IRQ_SOURCE_ADC);
        check(adc != nullptr && adc->vector == 0xFFu,
              "B: classic must leave extended sources unmapped");
    }
    check(s_ctx_b.irq_map_extend == nullptr &&
          s_ctx_b.irq_flag_predicate == nullptr &&
          s_ctx_b.xsfr_validate == nullptr,
          "B: classic reset must clear every chip hook");
    wink_mcs51_set_isr(19u, ext_vector_isr);  // registered yet unreachable
    wink_mcs51_isr_enable();
    check(wink_mcs51_dispatch_vector(19u) == 0u && g_ext_isr_hits == 1u,
          "B: classic must not dispatch an extended vector");
    mcs51_raise_irq(IRQ_SOURCE_ADC);
    check((wink_mcs51_get_pending_interrupts() & (1u << IRQ_SOURCE_ADC)) == 0u,
          "B: classic must drop an extended raise before pending latches");

    // ── B: closed XSFR window never consults the chip validator ───────────
    s_ctx_b.xsfr_validate = counting_validator;
    wink_mcs51_xdata_reset();
    const uint8_t rb = wink_mcs51_xdata_read(0xF692ull, 2u);
    check(rb == 0xFFu, "B: classic XSFR-window read must be OOB (0xFF)");
    check(g_validator_calls == 0u,
          "B: closed window must never consult the chip validator");
    check(wink_mcs51_xsfr_unmodeled_count() == 0u,
          "B: OOB access is not an unmodeled-XSFR trip");

    // ── A intact while B resets beside it (dual-context coexistence) ──────
    mcs51_set_active_context(&s_ctx_a);
    check(wink_mcs51_get_irq_map_entry(IRQ_SOURCE_ADC)->vector == 19u,
          "A: extended profile must survive B's classic reset");
    check(s_ctx_a.irq_map_extend != nullptr &&
          s_ctx_a.irq_flag_predicate != nullptr &&
          s_ctx_a.xsfr_validate != nullptr,
          "A: chip hooks must survive B's classic reset");
    wink_mcs51_isr_enable();
    check(wink_mcs51_dispatch_vector(19u) == 1u && g_ext_isr_hits == 2u,
          "A: extended vector still dispatches after B reset");

    // ── A: open window validates through the chip allowlist ───────────────
    wink_mcs51_xdata_reset();
    wink_mcs51_xdata_write(0xF000ull, 0x01u, 2u);  // declared -> silent
    check(wink_mcs51_xsfr_unmodeled_count() == 0u,
          "A: declared XSFR write must stay silent");
    wink_mcs51_xdata_write(0xF120ull, 0x01u, 2u);  // undeclared -> trips
    check(wink_mcs51_xsfr_unmodeled_count() == 1u,
          "A: undeclared XSFR write must trip the unmodeled counter");

    if (g_fails) {
        return 1;
    }
    printf("[mcs51-insulation] PASS: classic extended vectors/raise blocked, "
           "XSFR window closed, dual-context profiles coexist\n");
    return 0;
}
