// SPDX-License-Identifier: Apache-2.0
// MCS-51 interrupt vector table + dispatch (C linkage, boundary ②).
//
// WINK_ISR() in the user/cleaned TU auto-registers each vector before main()
// via a file-scope constructor. This TU owns the backing POD table — plain
// BSS function pointers, no static-init ordering dependency (铁律 1,
// ADR-0072 D5). Dispatch is additionally gated by an execution-phase flag
// that is false at load and only enabled at runtime (铁律 3).
#include "wink_mcs51_isr.h"

#include <cstdint>

namespace {
using isr_fn_t = void (*)(void);

// Zero-init (BSS): all vectors unregistered, gate closed, counters zero —
// established before any C++ constructor runs.
isr_fn_t s_isr_table[WINK_MCS51_NUM_VECTORS] = {nullptr};
bool     s_interrupts_enabled = false;
bool     s_in_isr = false;
uint32_t s_dispatch_count[WINK_MCS51_NUM_VECTORS] = {0};
}  // namespace

extern "C" {

void wink_mcs51_set_isr(uint8_t vector_num, void (*isr_fn)(void)) {
    if (vector_num < WINK_MCS51_NUM_VECTORS) {
        s_isr_table[vector_num] = reinterpret_cast<isr_fn_t>(isr_fn);
    }
}

void (*wink_mcs51_get_isr(uint8_t vector_num))(void) {
    if (vector_num < WINK_MCS51_NUM_VECTORS) {
        return s_isr_table[vector_num];
    }
    return nullptr;
}

void wink_mcs51_isr_enable(void)  { s_interrupts_enabled = true; }
void wink_mcs51_isr_disable(void) { s_interrupts_enabled = false; }

bool wink_mcs51_in_isr(void) { return s_in_isr; }

uint8_t wink_mcs51_dispatch_vector(uint8_t vector_num) {
    if (!s_interrupts_enabled || vector_num >= WINK_MCS51_NUM_VECTORS) {
        return 0;
    }
    isr_fn_t fn = s_isr_table[vector_num];
    if (fn == nullptr) {
        return 0;
    }
    ++s_dispatch_count[vector_num];
    s_in_isr = true;
    fn();
    s_in_isr = false;
    return 1;
}

uint32_t wink_mcs51_isr_dispatch_count(uint8_t vector_num) {
    if (vector_num >= WINK_MCS51_NUM_VECTORS) {
        return 0;
    }
    return s_dispatch_count[vector_num];
}

void wink_mcs51_reset_isrs(void) {
    for (uint32_t i = 0; i < WINK_MCS51_NUM_VECTORS; ++i) {
        s_isr_table[i] = nullptr;
        s_dispatch_count[i] = 0;
    }
    s_interrupts_enabled = false;
    s_in_isr = false;
}

}  // extern "C"
