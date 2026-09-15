// SPDX-License-Identifier: LGPL-3.0-only
// Task R1: MCS-51 const peripheral descriptor table (ADR-0004).
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "mcs51_family.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Mcu51Context;

typedef enum {
    MCS51_PHASE_CLOCK    = 0,
    MCS51_PHASE_RX_DRAIN = 1,
    MCS51_PHASE_EXTINT   = 2,
    MCS51_PHASE_ADC      = 3,
    MCS51_PHASE_EDGE     = 4,
    MCS51_PHASE_MAX
} mcs51_poll_phase_t;

typedef struct {
    const char*        name;
    void             (*init)(struct Mcu51Context* ctx);
    void             (*reset)(struct Mcu51Context* ctx);
    void             (*poll)(struct Mcu51Context* ctx);
    uint64_t         (*next_event_us)(struct Mcu51Context* ctx);
    mcs51_poll_phase_t phase;
    // Families this model exists on, as MCS51_FAMILY_MASK_* bits (M1).
    // init/reset/poll/next_event loops skip non-matching families, so a
    // chip-family model never installs hooks on another family.
    uint8_t            family_mask;
} mcs51_peripheral_desc_t;

// True when `d` exists on `family` (family id doubles as the mask bit).
static inline bool mcs51_peripheral_active_for(const mcs51_peripheral_desc_t* d,
                                               uint8_t family) {
    return (d != NULL) && ((d->family_mask & (1u << family)) != 0u);
}

// Strong symbol descriptor table (ADR-0004: compile-time static dispatch, NO weak symbols).
extern const mcs51_peripheral_desc_t g_mcs51_peripherals[];
extern const uint8_t g_mcs51_num_peripherals;

// ── Stage4 CPL-10: chip self-registration ─────────────────────────────────
// Core-owned bounded BSS registry (static dispatch, ADR-0004: no weak
// symbols, no malloc, no exceptions). Chip packages append their descriptors
// through their family register entry (the chips/*/src/*_register.cpp TUs);
// the three dispatch loops (context init/reset, bridge microstep poll,
// pcon next-event) traverse the core table first, then this registry,
// applying the same family_mask filter to both. Registration is idempotent
// (same pointer or same name registers once); the table never shrinks
// except through the test seam below.
//
// Capacity rationale (stage4 review follow-up): core 3 + current chip
// package 7 = 10 today; 12 leaves headroom for one more family. Overflow is
// a build contract violation and ABORTS in every build (assert is
// NDEBUG-compiled out; the unconditional abort is the real fuse) — never a
// silent drop.
#ifndef MCS51_MAX_PERIPHERALS
#define MCS51_MAX_PERIPHERALS 12u
#endif
void mcs51_peripheral_register(const mcs51_peripheral_desc_t* desc);
uint8_t mcs51_peripheral_registered_count(void);
const mcs51_peripheral_desc_t* mcs51_peripheral_registered(uint8_t i);
// Test seam only (production never calls it): drop every registered entry
// so family harnesses start from a clean slate. Core table untouched.
void mcs51_peripheral_registry_reset(void);

#ifdef __cplusplus
}
#endif
