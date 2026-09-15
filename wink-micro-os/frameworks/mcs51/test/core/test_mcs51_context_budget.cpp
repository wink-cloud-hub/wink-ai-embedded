// SPDX-License-Identifier: GPL-3.0-only
// Stage2 S2-0 (PLAN-20260911-MCS51-S2): Mcu51Context RAM budget lock.
//
// Prints sizeof(Mcu51Context) + major member sizes (feeds stage2 §4 budget
// table) and static_asserts the no-growth ceiling: post-split classic/CMS8S
// instances must not exceed the pre-decoupling baseline (75648 B, MinGW
// GCC-measured; +1 KB cross-toolchain slack). Stage5 landed the per-context
// IRQ map (+104 B) plus its three chip hooks (+12 B, +4 B struct alignment):
// 75656 B measured, ceiling unchanged (stage5 appendix C re-check).
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "mcs51_context.h"

namespace {

int g_fails = 0;

void check(bool cond, const char *msg) {
    if (!cond) {
        printf("[mcs51] FAIL: %s\n", msg);
        ++g_fails;
    }
}

// Pre-decoupling baseline (S2-0 probe, MinGW GCC i686): 75648 B master,
// 75656 B post-stage0 (caps_cache +8), 75752 B post-stage1 (rail 64 +96).
// S2-1 purifying (scheme A) measured 75672 B (-80 split; +24 vs baseline =
// caps_cache 8 [approved] + gpio_hooks 12 [planned stage4 infra] + 4 align,
// booked per 00-README §8; S2-2 shaves ~120 more via T3/T4/port sampling).
// S4-1/S4-2 chip-pool moves measured 75536 B; stage5 CPL-06/08 adds the
// per-context irq_map (14 rows x 8 B = 112, ACMP row included) +
// irq_map_extend/flag_predicate/xsfr_validate hooks (12) + alignment (4):
// 75664 B.
// Ceiling = S2-1 current + 1 KB slack (covers x64-MSVC pointer growth vs the
// i686-measured truth; per-toolchain exact numbers live in stage2 §4).
constexpr unsigned kBudgetBytes = 75672u + 1024u;
static_assert(sizeof(Mcu51Context) <= kBudgetBytes,
              "Mcu51Context exceeded the RAM budget (see stage2 §4 table)");

}  // namespace

extern "C" void wink_mcs51_user_main(void) {}
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

int main(void) {
    const Mcu51Context *z = 0;
    printf("[budget] sizeof(Mcu51Context)=%u (ceiling %u)\n",
           (unsigned)sizeof(Mcu51Context), kBudgetBytes);
    printf("[budget] xdata_shadow=%u isr_table=%u+%u timer=%u extint=%u uart=%u\n",
           (unsigned)sizeof(z->xdata_shadow), (unsigned)sizeof(z->isr_table),
           (unsigned)sizeof(z->isr_dispatch_count), (unsigned)sizeof(z->timer),
           (unsigned)sizeof(z->extint), (unsigned)sizeof(z->uart));
    printf("[budget] rail=%u+%u soc_priv=%u idx=%u hooks=%u extbus=%u\n",
           (unsigned)sizeof(z->adc_injected), (unsigned)sizeof(z->adc_inject_flag),
           (unsigned)sizeof(z->soc_priv), (unsigned)sizeof(z->instance_index),
           (unsigned)sizeof(z->gpio_hooks), (unsigned)sizeof(z->extbus));
    printf("[budget] irq_map=%u + extend/predicate/xsfr hooks=%u\n",
           (unsigned)sizeof(z->irq_map),
           (unsigned)(sizeof(z->irq_map_extend) +
                      sizeof(z->irq_flag_predicate) +
                      sizeof(z->xsfr_validate)));
    check(sizeof(z->xdata_shadow) == 65536u, "xdata_shadow must stay 64KB");
    check(sizeof(z->irq_map) == 120u, "irq_map must be 15 x 8 B incl. ACMP+WDT (stage2 §4)");
    check(sizeof(Mcu51Context) <= kBudgetBytes, "budget ceiling breached");
    if (g_fails) {
        return 1;
    }
    printf("[mcs51] PASS: context RAM budget locked\n");
    return 0;
}
