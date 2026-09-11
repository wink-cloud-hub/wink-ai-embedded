// SPDX-License-Identifier: Apache-2.0
// Stage2 S2-0 (PLAN-20260911-MCS51-S2): Mcu51Context RAM budget lock.
//
// Prints sizeof(Mcu51Context) + major member sizes (feeds stage2 §4 budget
// table) and static_asserts the no-growth ceiling: post-split classic/CMS8S
// instances must not exceed the pre-decoupling baseline (75648 B, MinGW
// GCC-measured; +1 KB cross-toolchain slack). Planned stage5 +104 B (IRQ
// map into ctx) raises this ceiling explicitly when it lands.
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

// Pre-decoupling baseline (S2-0 probe, MinGW GCC): 75648 B master,
// 75656 B post-stage0 (caps_cache +8), 75752 B post-stage1 (rail 64 +96).
// Ceiling = current + 1 KB slack for cross-toolchain packing drift.
constexpr unsigned kBudgetBytes = 75752u + 1024u;
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
    printf("[budget] adc0832=%u sysProt=%u buzzer=%u cms8sAdc=%u rail=%u+%u\n",
           (unsigned)sizeof(z->adc0832), (unsigned)sizeof(z->sysProt),
           (unsigned)sizeof(z->buzzer), (unsigned)sizeof(z->cms8sAdc),
           (unsigned)sizeof(z->adc_injected), (unsigned)sizeof(z->adc_inject_flag));
    check(sizeof(z->xdata_shadow) == 65536u, "xdata_shadow must stay 64KB");
    check(sizeof(Mcu51Context) <= kBudgetBytes, "budget ceiling breached");
    if (g_fails) {
        return 1;
    }
    printf("[mcs51] PASS: context RAM budget locked\n");
    return 0;
}
