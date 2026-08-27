// SPDX-License-Identifier: Apache-2.0
// M2 static-init safety test (clock SSOT §6.3 / ADR-0072 D5): three separate
// TUs (mcs51_static_tu_a/b/c.cpp) each register an ISR vector via a static
// C++ constructor (WINK_ISR) and touch an SFR proxy during dynamic init.
//
// This proves cross-TU static initialization is order-independent:
//   * the SFR shadow / ISR table are POD BSS (zero-init before any ctor);
//   * WinkSfr instances are constant-initialized;
//   * SFR access before the scheduler exists is a safe no-op;
//   * the execution-phase gate suppresses all dispatch until explicitly
//     enabled at runtime (no ISR can fire during static init).
#include <stdint.h>
#include <stdio.h>

#include "wink_runtime.h"
#include "wink_app.h"
#include "wink_status.h"

#include "wink_mcs51_isr.h"

extern "C" {
uint32_t g_static_isr_hits = 0;
}
extern "C" uint32_t mcs51_static_tu_a_marker(void);
extern "C" uint32_t mcs51_static_tu_b_marker(void);
extern "C" uint32_t mcs51_static_tu_c_marker(void);

// The bridge TU is pulled in by the SFR hooks the probe TUs call; it
// references the user entry. This test supplies its own callbacks, so a
// no-op user main suffices to close the link.
extern "C" void wink_mcs51_user_main(void) {}

namespace {
// Bounded app loop (this test supplies its own callbacks; no user main).
void static_init_app_loop(void) {
    // Nothing to do: static registration happened before main(); just let
    // the runtime tick a few times to prove post-init execution is healthy.
}
}  // namespace

extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

int main(void) {
    int fails = 0;

    // All three TUs linked (their static ctors ran at load).
    if (mcs51_static_tu_a_marker() != 0xA1u ||
        mcs51_static_tu_b_marker() != 0xB2u ||
        mcs51_static_tu_c_marker() != 0xC3u) {
        printf("[mcs51] FAIL: static-init TUs not linked\n");
        fails++;
    }

    // Vectors registered by static constructors before any runtime code.
    if (wink_mcs51_get_isr(2) == nullptr ||
        wink_mcs51_get_isr(3) == nullptr ||
        wink_mcs51_get_isr(5) == nullptr) {
        printf("[mcs51] FAIL: static-registered ISR vectors missing\n");
        fails++;
    }

    // Execution-phase gate: dispatch is suppressed before runtime enable,
    // even though vectors are registered.
    if (wink_mcs51_dispatch_vector(2) != 0u || g_static_isr_hits != 0u) {
        printf("[mcs51] FAIL: ISR dispatched before interrupt gate enabled\n");
        fails++;
    }

    // Enable and dispatch: vectors 2/3/5 fire.
    wink_mcs51_isr_enable();
    wink_mcs51_dispatch_vector(2);
    wink_mcs51_dispatch_vector(3);
    wink_mcs51_dispatch_vector(5);
    if (g_static_isr_hits != 3u) {
        printf("[mcs51] FAIL: post-enable dispatch hits=%u, want 3\n",
               (unsigned)g_static_isr_hits);
        fails++;
    }

    // Run the cooperative runtime briefly to prove no static-init hazard
    // surfaces when the scheduler/fiber comes up.
    static const wink_app_callbacks_t cb = {
        nullptr,                 // init
        static_init_app_loop,    // loop
        nullptr, nullptr, nullptr, nullptr, nullptr,
    };
    wink_status_t st = wink_runtime_run(&cb, 20u);
    if (st != WINK_OK) {
        printf("[mcs51] FAIL: runtime run returned %d\n", (int)st);
        fails++;
    }

    if (fails) {
        return 1;
    }
    printf("[mcs51] PASS: 3-TU static init safe (vectors registered pre-main, "
           "gate closed, SFR access no-op, runtime healthy)\n");
    return 0;
}
