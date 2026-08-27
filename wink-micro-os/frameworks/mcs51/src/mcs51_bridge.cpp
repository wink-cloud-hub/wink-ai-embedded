// SPDX-License-Identifier: Apache-2.0
// MCS-51 simulation bridge (boundary ④): binds the cleaned Keil user program
// into the Wink cooperative runtime and wires the interception points to the
// virtual clock and peripheral models.
//
// The user `main` (remapped to wink_mcs51_user_main by REGX52.H) is a bare-metal
// init+`while(1)` super-loop that never returns. We expose it as the runtime
// `loop` callback; the runtime wraps that in its app_main fiber. Every SFR
// access and every `_nop_()` funnels through the virtual clock, which charges
// functional microseconds and force-yields the fiber on quota (ADR-0072 D1~D3),
// while timer overflows are evaluated at catch-up/resume points.
#include "pal_osal.h"
#include "wink_app.h"

#include "absacc.h"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_isr.h"
#include "wink_mcs51_strict.h"
#include "wink_mcs51_timer.h"
#include "wink_mcs51_uart.h"

#include <cstdint>

// The user entry point, emitted by the cleaned TU as C linkage (REGX52.H
// boundary ①). Defined by the linked user program.
extern "C" void wink_mcs51_user_main(void);

namespace {

// One-time framework bring-up (runtime init callback, runs on the host/main
// context before the fiber is registered). Static C++ registration has already
// populated the ISR vector table; here we only reset model state, wire the
// catch-up hook, and open the execution-phase interrupt gate (铁律 3,
// ADR-0072 D5).
void mcs51_framework_init(void) {
    wink_mcs51_clock_reset();
    wink_mcs51_timers_reset();
    wink_mcs51_uart_reset();
    wink_mcs51_xdata_reset();
    wink_mcs51_unsupported_reset();
    wink_mcs51_set_catchup_hook(wink_mcs51_timers_step_to);
    wink_mcs51_isr_enable();
}

}  // namespace

extern "C" {

// Interception point for <intrins.h> _nop_(): charge one functional microstep.
void wink_mcs51_microstep(void) {
    wink_mcs51_charge_us(WINK_MCS51_MICROSTEP_US);
}

// SFR proxy hooks (boundary ③ crosses into this TU). Read: lazily evaluate
// due timer overflows before the value is observed (closes the `while(!TF0)`
// polling loop on the read), then charge time. Write: latch the new value
// into the timer model first (TR start/stop, reload) so a quota-triggered
// catch-up on the same interception point steps the freshly configured timer.
void wink_mcs51_on_sfr_read(uint8_t addr) {
    wink_mcs51_timer_on_read(addr);
    wink_mcs51_uart_on_read(addr);
    wink_mcs51_microstep();
}

void wink_mcs51_on_sfr_write(uint8_t addr) {
    // Latch the UART model BEFORE charging the microstep, mirroring the timer
    // ordering: a SBUF write emits + sets TI + vectors synchronously in the
    // same interception point.
    wink_mcs51_uart_on_write(addr);
    wink_mcs51_timer_on_write(addr);
    wink_mcs51_microstep();
}

}  // extern "C"

// ── Runtime callback binding (boundary ④) ───────────────────────────────────
// Provided with C linkage: the Keil user program never defines wink callbacks
// (it only defines main -> wink_mcs51_user_main), so the interception layer
// supplies them. C linkage lets the C host test, the C wasm entry
// (wasm_entry.c), and the C runtime all resolve the same symbol.
namespace {
void mcs51_app_loop(void) {
    wink_mcs51_user_main();  // bare-metal super-loop; does not return
}
}  // namespace

// Strong, single definition (NOT weak): the Keil user program never supplies
// wink callbacks, so the interception layer is the sole provider. On MinGW/PE a
// weak function definition with no strong fallback resolves to null (unlike
// ELF), which broke the host link; a plain strong extern "C" definition is
// correct for both host and wasm.
extern "C" const wink_app_callbacks_t* wink_app_get_callbacks(void)
{
    static const wink_app_callbacks_t s_mcs51_callbacks = {
        mcs51_framework_init,  // init  (framework bring-up; user does its own in main)
        mcs51_app_loop,        // loop  (the bare-metal super-loop; never returns)
        nullptr,               // on_fault
        nullptr,               // on_boot
        nullptr,               // init_status
        nullptr,               // on_fault_status
        nullptr,               // on_event
    };
    return &s_mcs51_callbacks;
}
