// SPDX-License-Identifier: Apache-2.0
// MCS-51 simulation bridge (boundary ④): binds the cleaned Keil user program
// into the Wink cooperative runtime and provides the interception microstep.
//
// The user `main` (remapped to wink_mcs51_user_main by REGX52.H) is a bare-metal
// init+`while(1)` super-loop that never returns. We expose it as the runtime
// `loop` callback; the runtime wraps that in its app_main fiber. Every SFR
// access and every `_nop_()` funnels through the microstep, which periodically
// yields the fiber (duration-0) so the simulation master stays responsive.
//
// M1: periodic cooperative yield only. M2 replaces the counter with the
// virtual-microsecond quota and adds timer catch-up (Spike-S1).
#include "pal_osal.h"
#include "wink_app.h"

#include <cstdint>

// The user entry point, emitted by the cleaned TU as C linkage (REGX52.H
// boundary ①). Defined by the linked user program.
extern "C" void wink_mcs51_user_main(void);

namespace {

// Yield once every N interception points. A pure polling loop that touches an
// SFR or calls _nop_() each iteration thus hands control back to the master
// deterministically; a truly empty `while(1){}` with no interception point is
// unrecoverable and caught by the WCET 8002 fault (Spike-S1 §3).
constexpr uint32_t kMicrostepYieldEvery = 64u;
uint32_t s_step_count = 0u;

}  // namespace

extern "C" {

// Single interception point used by both <intrins.h> _nop_() and the SFR proxy.
void wink_mcs51_microstep(void) {
    if (++s_step_count >= kMicrostepYieldEvery) {
        s_step_count = 0u;
        // pal_os_sleep_ms(0) performs a duration-0 timed yield + fiber switch
        // to the master (same primitive Arduino's yield() uses). The task is
        // immediately re-ready (wakeup <= now), so it round-trips cooperatively.
        // It is marked deprecated for blocking use; here it is the sanctioned
        // cooperative-yield bridge, so suppress the diagnostic on all compilers.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
        pal_os_sleep_ms(0u);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif
    }
}

// SFR proxy hook (boundary ③ crosses into this TU). M1 treats every observable
// SFR access as a microstep; M4 layers diff/edge dispatch on top (ADR-0071).
void wink_mcs51_on_sfr_access(uint8_t addr) {
    (void)addr;
    wink_mcs51_microstep();
}

}  // extern "C"

// ── Runtime callback binding (boundary ④) ───────────────────────────────────
// Provided with C linkage: the Keil user program never defines wink callbacks
// (it only defines main -> wink_mcs51_user_main), so the interception layer
// supplies them. C linkage lets the C host test, the C wasm entry
// (wasm_entry.c), and the C runtime all resolve the same symbol. Weak
// (selectany on MSVC) so a test/embedding can override.
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
        nullptr,        // init  (user program does its own init inside main)
        mcs51_app_loop,  // loop  (the bare-metal super-loop; never returns)
        nullptr,        // on_fault
        nullptr,        // on_boot
        nullptr,        // init_status
        nullptr,        // on_fault_status
        nullptr,        // on_event
    };
    return &s_mcs51_callbacks;
}
