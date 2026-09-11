// SPDX-License-Identifier: Apache-2.0
// MCS-51 MCU family descriptors (maintainability M1).
//
// Silicon facts per series live HERE (and only here). Generic mechanism
// files (xdata aperture, UART readiness, reset seeds) query this table —
// they must not hard-code per-series `== MCS51_FAMILY_XXX` branches.
// Adding a series = one table row (+ its peripheral init/reset entries);
// mechanism files stay untouched.
//
// Static dispatch (ADR-0004): plain POD table, no vtable.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// MCU family ids. Bit position doubles as the peripheral family_mask bit.
#define MCS51_FAMILY_CLASSIC   0u  // AT89C52/STC89 and other classic 12T parts
#define MCS51_FAMILY_CMS8S78XX 1u  // Cmsemicon CMS8S78xx

#define MCS51_FAMILY_MASK_CLASSIC   (1u << MCS51_FAMILY_CLASSIC)
#define MCS51_FAMILY_MASK_CMS8S78XX (1u << MCS51_FAMILY_CMS8S78XX)
#define MCS51_FAMILY_MASK_ALL       0xFFu

typedef struct {
    uint8_t      id;
    const char*  name;
    uint32_t     fosc_hz;      // Power-on Fosc. 0 = no fixed on-chip RC
                               // (classic: board crystal, keep clock_hz 0 so
                               // the 12 MHz family default stays in effect).
    uint8_t      ckcon_reset;  // CKCON reset value. 0x00 when the part has
                               // no CKCON (classic timers are fixed Fsys/12).
    uint32_t     xram_size;    // On-chip XRAM bytes. 0 = no on-chip XRAM
                               // (external MOVX size is board-defined, the
                               // sim keeps the WINK_MCS51_XDATA_SIZE knob).
    uint32_t     xsfr_base;    // MOVX extended-SFR window base. 0 = no window.
    uint32_t     xsfr_size;    // Window size bytes. 0 = no window.
} mcs51_family_desc_t;

// Descriptor for `family`. Unknown ids fall back to CLASSIC (documented:
// the sim stays alive on classic semantics rather than running unseeded).
const mcs51_family_desc_t* mcs51_family_desc(uint8_t family);
uint8_t mcs51_family_count(void);

// Capability query: true when the family exposes the MOVX XSFR window
// (FUNCCR/CFG/PS_xx class registers). Mechanism files branch on THIS,
// never on a family id comparison.
static inline bool mcs51_family_has_xsfr(const mcs51_family_desc_t* d) {
    return (d != NULL) && (d->xsfr_size != 0u);
}

#ifdef __cplusplus
}
#endif
