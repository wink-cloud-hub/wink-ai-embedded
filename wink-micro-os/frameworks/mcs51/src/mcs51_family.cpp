// SPDX-License-Identifier: Apache-2.0
// MCS-51 MCU family descriptor table (maintainability M1).
// Each row is one series' silicon facts; see mcs51_family.h.
// Ref: CMS8S78xx datasheet V1.0.7 (§2.2.3 XRAM 1KB), ref manual V1.1.1
// (§8.2.2 CKCON reset 0x07, §4 power-on 24 MHz internal RC).
#include "mcs51_family.h"

#include <stddef.h>

namespace {

const mcs51_family_desc_t kFamilyDescs[] = {
    {
        MCS51_FAMILY_CLASSIC,
        "AT89C52/classic",
        0u,          // fosc_hz: board crystal, no fixed on-chip RC
        0x00u,       // ckcon_reset: no CKCON on classic 8052
        0u,          // xram_size: no on-chip XRAM (external/board-defined)
        0u,          // xsfr_base: no XSFR window
        0u,          // xsfr_size
    },
    {
        MCS51_FAMILY_CMS8S78XX,
        "CMS8S78xx",
        24000000u,   // fosc_hz: power-on internal RC 24 MHz (±1%)
        0x07u,       // ckcon_reset: WTS=000, T1M=T0M=1 (Fsys/4 out of reset)
        1024u,       // xram_size: 1 KB on-chip XRAM (0x0000..0x03FF)
        0xF000u,     // xsfr_base: extended-SFR MOVX window
        0x1000u,     // xsfr_size: [0xF000, 0x10000)
    },
};

}  // namespace

extern "C" {

const mcs51_family_desc_t* mcs51_family_desc(uint8_t family) {
    for (size_t i = 0; i < sizeof(kFamilyDescs) / sizeof(kFamilyDescs[0]); ++i) {
        if (kFamilyDescs[i].id == family) {
            return &kFamilyDescs[i];
        }
    }
    return &kFamilyDescs[0];  // unknown -> CLASSIC (documented fallback)
}

uint8_t mcs51_family_count(void) {
    return (uint8_t)(sizeof(kFamilyDescs) / sizeof(kFamilyDescs[0]));
}

}  // extern "C"
