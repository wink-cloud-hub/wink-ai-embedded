// SPDX-License-Identifier: Apache-2.0
// Spike-S2 MINIMAL WINK_ISR (throwaway). Standard C++ static-struct auto
// registration (no __attribute__((constructor)) so MSVC behaves identically).
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
void wink_mcs51_set_isr(uint8_t vector_num, void (*isr_fn)(void));
#ifdef __cplusplus
}
#endif

#define WINK_ISR(n) \
    extern "C" void wink_isr_vector_##n(void); \
    namespace { \
      struct WinkIsrAutoReg_##n { \
        WinkIsrAutoReg_##n() { wink_mcs51_set_isr(n, wink_isr_vector_##n); } \
      } s_auto_reg_##n; \
    } \
    extern "C" void wink_isr_vector_##n(void)
