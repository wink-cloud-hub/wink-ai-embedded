// SPDX-License-Identifier: Apache-2.0
// MCS-51 interrupt-service-routine registration (boundary ②).
//
// Keil C51 attaches an ISR with `void f(void) interrupt N [using M]`. The
// cleanup pass rewrites that signature to `WINK_ISR(N)`. This macro:
//   1. declares the ISR with C linkage (wink_isr_vector_N) so the vector table
//      can call it across TUs without C++ name decoration;
//   2. emits a file-scope struct whose constructor registers the vector before
//      main() runs — standard C++ static init, NOT __attribute__((constructor)),
//      so MSVC/GCC/emcc behave identically (Spike-S2 §4.4).
//
// M1 scope: the vector table is populated and queryable; timer-driven dispatch
// arrives in M2.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 8051 standard vectors: 0 external0, 1 timer0, 2 external1, 3 timer1,
// 4 UART, (5..7 = 8052 extra / RFU). Table sized for the core 8 vectors.
#define WINK_MCS51_NUM_VECTORS 8u

// Register an ISR function for interrupt vector `n` (called by the WINK_ISR
// auto-registration shim). Defined in mcs51_isr.cpp.
void wink_mcs51_set_isr(uint8_t vector_num, void (*isr_fn)(void));

// Fetch the registered ISR for a vector, or NULL if none. Used by the M2
// interrupt dispatch model.
void (*wink_mcs51_get_isr(uint8_t vector_num))(void);

// Clear all registered vectors (test isolation / reset).
void wink_mcs51_reset_isrs(void);

#ifdef __cplusplus
}  // extern "C"
#endif

#ifdef __cplusplus

#define WINK_ISR(n)                                                          \
    extern "C" void wink_isr_vector_##n(void);                               \
    namespace {                                                              \
    struct WinkIsrAutoReg_##n {                                              \
        WinkIsrAutoReg_##n() { wink_mcs51_set_isr(n, wink_isr_vector_##n); } \
    } s_auto_reg_##n;                                                        \
    }                                                                        \
    extern "C" void wink_isr_vector_##n(void)

#endif  // __cplusplus
