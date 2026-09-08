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

// Architecture-neutral semantic interrupt sources (ADR-0078 D1)
typedef enum {
    IRQ_SOURCE_INT0 = 0,
    IRQ_SOURCE_TIMER0,
    IRQ_SOURCE_INT1,
    IRQ_SOURCE_TIMER1,
    IRQ_SOURCE_UART0,
    IRQ_SOURCE_TIMER2,
    IRQ_SOURCE_ADC,
    IRQ_SOURCE_UART1,
    IRQ_SOURCE_PWM,
    IRQ_SOURCE_I2C,
    IRQ_SOURCE_SPI,
    IRQ_SOURCE__COUNT
} mcs51_irq_source_t;

#define MCS51_IRQ_HW_AUTO_CLEAR 0
#define MCS51_IRQ_SW_CLEAR      1

// Interrupt profile mapping entry (ADR-0078 D1)
typedef struct {
    uint8_t vector;        // Physical vector number (Tier 2 looks up isr_table, Tier 3 jumps 0x0003+8*vector)
    uint8_t ie_sfr;        // Interrupt enable SFR address (IE/EIE1/EIE2..., 0xFF = none/always open)
    uint8_t ie_bit;        // Enable bit position (0..7)
    uint8_t flag_sfr;      // Flag SFR address (TCON/SCON/EIF2..., 0xFF = internal/none)
    uint8_t flag_bit;      // Flag bit position (0..7)
    uint8_t prio_sfr;      // Priority SFR address (IP/EIP1/EIP2..., 0xFF = default prio 0)
    uint8_t prio_bit;      // Priority bit position (0..7)
    uint8_t clear_mode;    // MCS51_IRQ_HW_AUTO_CLEAR / MCS51_IRQ_SW_CLEAR
} mcs51_irq_map_entry_t;

typedef enum {
    MCS51_IRQ_EVT_RAISE = 0,
    MCS51_IRQ_EVT_DISPATCH = 1,
    MCS51_IRQ_EVT_RETI = 2
} mcs51_irq_event_t;

#ifndef WINK_TRACE_MCS51_IRQ
#define WINK_TRACE_MCS51_IRQ(src, vector, prio, virtual_us, event_type) ((void)0)
#endif

// Peripheral entry point: raise a semantic interrupt request (ADR-0078 D1)
void mcs51_raise_irq(mcs51_irq_source_t src);

// Rendezvous arbitration & dispatch point (ADR-0078 D2, D3)
uint8_t mcs51_irq_scan_and_dispatch(void);

// Profile mapping table configuration & inspection
void wink_mcs51_set_irq_map_entry(mcs51_irq_source_t src, const mcs51_irq_map_entry_t* entry);
const mcs51_irq_map_entry_t* wink_mcs51_get_irq_map_entry(mcs51_irq_source_t src);
void wink_mcs51_reset_irq_map(void);

// RETI / write IE/IP single-instruction suppression (ADR-0078 D4)
void wink_mcs51_suppress_next_irq(void);
void wink_mcs51_clear_reti_suppress(void);

// Peripheral reset / flag clearing: clear pending bit for an IRQ source
void wink_mcs51_clear_irq(mcs51_irq_source_t src);

// Observability & diagnostic getters
uint32_t wink_mcs51_get_pending_interrupts(void);
uint8_t wink_mcs51_get_in_service_depth(void);
uint8_t wink_mcs51_get_in_service_prio(uint8_t depth_index);

// Reset dynamic IRQ state (pending, in-service stack, reti suppression) without wiping registered ISR vectors
void wink_mcs51_reset_irq_state(void);

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
