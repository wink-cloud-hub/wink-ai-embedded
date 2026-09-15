// SPDX-License-Identifier: LGPL-3.0-only
// MCS-51 MCU family descriptor table (maintainability M1).
// Each row is one series' silicon facts; see mcs51_family.h.
// Ref: CMS8S78xx datasheet V1.0.7 (§2.2.3 XRAM 1KB), ref manual V1.1.1
// (§8.2.2 CKCON reset 0x07, §4 power-on 24 MHz internal RC).
#include "mcs51_family.h"

#include <stddef.h>

namespace {

// Stage0 (v2 schema): supported vector numbers per family. Classic 8052 =
// standard vectors 0..5. CMS8S78xx = full 28-vector table width
// (WINK_MCS51_NUM_VECTORS); stage5 refines per-vector insulation, stage0
// only publishes the tables (mechanism files still use their own maps).
const uint8_t kIrqVectorsClassic[] = { 0u, 1u, 2u, 3u, 4u, 5u };
const uint8_t kIrqVectorsCms8s[] = {
    0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u,
    10u, 11u, 12u, 13u, 14u, 15u, 16u, 17u, 18u, 19u,
    20u, 21u, 22u, 23u, 24u, 25u, 26u, 27u,
};

const mcs51_family_desc_t kFamilyDescs[] = {
    {
        MCS51_FAMILY_CLASSIC,
        "AT89C52/classic",
        0u,          // fosc_hz: board crystal, no fixed on-chip RC
        0x00u,       // ckcon_reset: no CKCON on classic 8052
        0u,          // xram_size: no on-chip XRAM (external/board-defined)
        0u,          // xsfr_base: no XSFR window
        0u,          // xsfr_size
        0u,          // capabilities: pure Intel standard semantics
        { 8u, 8u, 8u, 8u },  // port_pin_masks: P0..P3 full 8 pins
        kIrqVectorsClassic,  // irq_vector_table: standard 0..5
        6u,          // irq_count
        false,       // wdt_present: classic 8051 has no WDT
        false,       // iap_present: classic 8051 has no IAP flash
        1u,          // uart_count: UART0 only
        (uint8_t)(MCS51_TIMER_CAP_T0 | MCS51_TIMER_CAP_T1 |
                  MCS51_TIMER_CAP_T2),  // timer_caps: standard timers
    },
    {
        MCS51_FAMILY_CMS8S78XX,
        "CMS8S78xx",
        24000000u,   // fosc_hz: power-on internal RC 24 MHz (±1%)
        0x07u,       // ckcon_reset: WTS=000, T1M=T0M=1 (Fsys/4 out of reset)
        1024u,       // xram_size: 1 KB on-chip XRAM (0x0000..0x03FF)
        0xF000u,     // xsfr_base: extended-SFR MOVX window
        0x1000u,     // xsfr_size: [0xF000, 0x10000)
        (uint32_t)(MCS51_CAP_ENHANCED_IO | MCS51_CAP_TIMER34 |
                   MCS51_CAP_TIMER_CAPTURE | MCS51_CAP_W0C_FLAGS |
                   MCS51_CAP_PORT_EXTINT | MCS51_CAP_UART_REMAP |
                   MCS51_CAP_CHIP_MODELS),
        { 8u, 8u, 6u, 4u },  // port_pin_masks: TSSOP-20 (P2 6, P3 4)
        kIrqVectorsCms8s,  // irq_vector_table: full 28-vector width
        28u,         // irq_count
        true,        // wdt_present: on-chip watchdog + TA window
        true,        // iap_present: MCTRL/MDATA/MADR/MLOCK/PCRCD
        1u,          // uart_count: UART0 only (no UART1 on CMS8S78xx)
        (uint8_t)(MCS51_TIMER_CAP_T0 | MCS51_TIMER_CAP_T1 |
                  MCS51_TIMER_CAP_T2 | MCS51_TIMER_CAP_T3 |
                  MCS51_TIMER_CAP_T4 | MCS51_TIMER_CAP_CAPTURE),
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
