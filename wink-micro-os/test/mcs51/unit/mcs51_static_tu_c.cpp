// SPDX-License-Identifier: Apache-2.0
// M2 static-init safety TU C: registers vector 5 + SFR touch during dynamic
// init (see mcs51_static_tu_a.cpp).
#include <stdint.h>

#include "REGX52.H"

extern "C" uint32_t g_static_isr_hits;

WINK_ISR(5) {
    ++g_static_isr_hits;
}

namespace {
struct StaticProbeC {
    StaticProbeC() {
        volatile uint8_t v = static_cast<uint8_t>(P2);
        (void)v;
    }
} s_static_probe_c;
}  // namespace

extern "C" uint32_t mcs51_static_tu_c_marker(void) { return 0xC3u; }
