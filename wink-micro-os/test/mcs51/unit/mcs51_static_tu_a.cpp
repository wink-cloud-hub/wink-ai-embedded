// SPDX-License-Identifier: Apache-2.0
// M2 static-init safety TU A (clock SSOT §6.3): registers vector 2 via a
// static constructor and touches an SFR proxy during dynamic init. The SFR
// shadow is POD BSS and the WinkSfr instance is constant-initialized, so this
// is order-independent across TUs (ADR-0072 D5).
#include <stdint.h>

#include "REGX52.H"

extern "C" uint32_t g_static_isr_hits;

WINK_ISR(2) {
    ++g_static_isr_hits;
}

namespace {
struct StaticProbeA {
    StaticProbeA() {
        // SFR read during static init: hook must be a safe no-op before the
        // scheduler exists (no fiber context, interrupt gate closed).
        volatile uint8_t v = static_cast<uint8_t>(P1);
        (void)v;
    }
} s_static_probe_a;
}  // namespace

// Referenced by the driver so the archive member (and its static ctor) links.
extern "C" uint32_t mcs51_static_tu_a_marker(void) { return 0xA1u; }
