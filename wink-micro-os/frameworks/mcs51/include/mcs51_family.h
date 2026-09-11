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

// ── Stage0 (PLAN-20260911-MCS51-S0): capability bits (v2 schema) ─────────────
// Hot-path short-circuit mask (audit §3 criterion 2, ADR-0004 static
// dispatch): mechanism files read ctx->caps_cache, never a family-id
// comparison and never a function-pointer hook on the standard path.
// Bits are stable (never renumber; append new bits at the end).
#define MCS51_CAP_ENHANCED_IO  (1u << 0)  // TRIS/OD/UP/CFG push-pull etc.
#define MCS51_CAP_TIMER34      (1u << 1)  // Timer3/Timer4 extended counters
#define MCS51_CAP_TIMER_CAPTURE (1u << 2)  // T2 capture/compare unit (CCEN)
#define MCS51_CAP_W0C_FLAGS    (1u << 3)  // T2IF/EIF2 write-0-to-clear flags
#define MCS51_CAP_PORT_EXTINT  (1u << 4)  // full-port level-change extint
#define MCS51_CAP_UART_REMAP   (1u << 5)  // FUNCCR clock sel + PS_RXD remap

// ── Stage0 (v2 schema): timer capability bits ────────────────────────────────
// Bit per timer/capture unit present on the silicon. Standard 8052 =
// T0|T1|T2; CMS8S78xx adds T3|T4|CAPTURE. Stable, append-only.
#define MCS51_TIMER_CAP_T0      (1u << 0)
#define MCS51_TIMER_CAP_T1      (1u << 1)
#define MCS51_TIMER_CAP_T2      (1u << 2)
#define MCS51_TIMER_CAP_T3      (1u << 3)
#define MCS51_TIMER_CAP_T4      (1u << 4)
#define MCS51_TIMER_CAP_CAPTURE (1u << 5)

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
    // ── v2 schema (Stage0 frozen; stage4/5 consume, stage0 adds only) ─────
    uint32_t     capabilities;   // MCS51_CAP_* bitmask (see caps_cache).
    uint8_t      port_pin_masks[4];  // Valid-pin counts P0..P3 (classic
                               // {8,8,8,8}; CMS8S78xx TSSOP-20 {8,8,6,4}).
                               // NAME NOTE (review finding): despite "masks",
                               // these are contiguous-prefix COUNTS compared
                               // as `bit < count` — correct for all current
                               // parts (contiguous pins). Non-contiguous
                               // packages (unbound middle pins) need a true
                               // bitmask field; migrate on the first such
                               // family (schema change, stage0 thaw), not now.
    const uint8_t* irq_vector_table;  // Supported vector numbers (stage5
                               // switches ISR to load extended vectors
                               // from here; stage0 only publishes).
    uint8_t      irq_count;    // Entries in irq_vector_table (classic 6,
                               // CMS8S78xx 28).
    bool         wdt_present;  // On-chip watchdog (classic: false).
    bool         iap_present;  // In-application flash programming
                               // (classic: false).
    uint8_t      uart_count;   // UART peripherals (both families: 1).
    uint8_t      timer_caps;   // MCS51_TIMER_CAP_* bitmask.
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
