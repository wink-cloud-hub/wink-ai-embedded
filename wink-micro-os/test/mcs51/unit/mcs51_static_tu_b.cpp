// SPDX-License-Identifier: Apache-2.0
// M2 static-init safety TU B: registers vector 3 + SFR touch during dynamic
// init (see mcs51_static_tu_a.cpp).
#include <stdint.h>

#include "REGX52.H"

extern "C" uint32_t g_static_isr_hits;

WINK_ISR(3) {
    ++g_static_isr_hits;
}

namespace {
struct StaticProbeB {
    StaticProbeB() {
        volatile uint8_t v = static_cast<uint8_t>(P3);
        (void)v;
    }
} s_static_probe_b;
}  // namespace

extern "C" uint32_t mcs51_static_tu_b_marker(void) { return 0xB2u; }
