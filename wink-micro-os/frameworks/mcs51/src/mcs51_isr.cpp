// SPDX-License-Identifier: Apache-2.0
// MCS-51 interrupt vector table (C linkage, boundary ② storage).
//
// WINK_ISR() in the user/cleaned TU auto-registers each vector before main()
// via a file-scope constructor. This TU owns the backing POD table — plain
// BSS function pointers, no static-init ordering dependency (ADR-0070).
#include "wink_mcs51_isr.h"

namespace {
// C-linkage ISR entry points are `extern "C" void(void)`; store as plain
// function pointers. Zero-init (BSS) = all vectors unregistered.
using isr_fn_t = void (*)(void);
isr_fn_t s_isr_table[WINK_MCS51_NUM_VECTORS] = {nullptr};
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

void wink_mcs51_reset_isrs(void) {
    for (uint32_t i = 0; i < WINK_MCS51_NUM_VECTORS; ++i) {
        s_isr_table[i] = nullptr;
    }
}

}  // extern "C"
