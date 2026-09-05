// SPDX-License-Identifier: Apache-2.0
// MCS-51 interrupt-service-routine registration + dispatch (boundary ②).
//
// Keil C51 attaches an ISR with `void f(void) interrupt N [using M]`. The
// cleanup pass rewrites that signature to `WINK_ISR(N)`. This macro:
//   1. declares the ISR with C linkage (wink_isr_vector_N) so the vector table
//      can call it across TUs without C++ name decoration;
//   2. emits a file-scope struct whose constructor registers the vector before
//      main() runs — standard C++ static init, NOT __attribute__((constructor)),
//      so MSVC/GCC/emcc behave identically (Spike-S2 §4.4).
//
// Dispatch model (M2, ADR-0072 D5):
//   * The backing table is POD BSS (zero-init before any C++ ctor) — static
//     registration from any TU is safe regardless of init order.
//   * An execution-phase gate (s_interrupts_enabled, false at load) suppresses
//     all dispatch until the framework enables interrupts at runtime, so no
//     ISR can ever fire before the simulation is fully initialized.
//   * Dispatch runs synchronously on the fiber (timer overflow -> ISR); nested
//     virtual interrupts are not modeled (functional level, AD-2).
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 8051 standard vectors: 0 external0, 1 timer0, 2 external1, 3 timer1,
// 4 UART, (5..7 = 8052 extra / RFU). Enhanced-vendor parts (CMS8S78xx) add
// extended vectors 8..27 — e.g. the on-chip ADC end-of-conversion ISR uses
// Keil `interrupt 19` (vector address 0x9B). Table sized for 28 so extended
// ISRs register and dispatch instead of being silently dropped (M5, ADR-0073).
#define WINK_MCS51_NUM_VECTORS 28u

// Register an ISR function for interrupt vector `n` (called by the WINK_ISR
// auto-registration shim). Safe at static-init time (POD table). Defined in
// mcs51_isr.cpp.
void wink_mcs51_set_isr(uint8_t vector_num, void (*isr_fn)(void));

// Fetch the registered ISR for a vector, or NULL if none.
void (*wink_mcs51_get_isr(uint8_t vector_num))(void);

// Execution-phase gate (ADR-0072 D5 rule 3). Dispatch is suppressed until the
// framework enables interrupts at runtime; registration is unaffected.
void wink_mcs51_isr_enable(void);
void wink_mcs51_isr_disable(void);

// Dispatch vector `n` if interrupts are enabled and a handler is registered.
// Called by peripheral models (timer overflow) on the fiber. Returns 1 when
// the ISR actually ran. While the ISR runs, wink_mcs51_in_isr() is true so
// the clock charges time but never yields (ADR-0072 D4).
uint8_t wink_mcs51_dispatch_vector(uint8_t vector_num);

// True while a virtual ISR is executing on the fiber.
bool wink_mcs51_in_isr(void);

// Number of times vector `n` has been dispatched (observability/tests).
uint32_t wink_mcs51_isr_dispatch_count(uint8_t vector_num);

// Clear all registered vectors, disable the gate, zero counters (test
// isolation / reset).
void wink_mcs51_reset_isrs(void);

#ifdef __cplusplus
}  // extern "C"
#endif

#ifdef __cplusplus

#define WINK_ISR_CONCAT_IMPL(a, b) a##b
#define WINK_ISR_CONCAT(a, b) WINK_ISR_CONCAT_IMPL(a, b)

#define WINK_ISR(n)                                                          \
    extern "C" void WINK_ISR_CONCAT(wink_isr_vector_, n)(void);              \
    namespace {                                                              \
    struct WINK_ISR_CONCAT(WinkIsrAutoReg_, n) {                             \
        WINK_ISR_CONCAT(WinkIsrAutoReg_, n)() {                              \
            wink_mcs51_set_isr(n, WINK_ISR_CONCAT(wink_isr_vector_, n));    \
        }                                                                    \
    } WINK_ISR_CONCAT(s_auto_reg_, n);                                       \
    }                                                                        \
    extern "C" void WINK_ISR_CONCAT(wink_isr_vector_, n)(void)

#endif  // __cplusplus
