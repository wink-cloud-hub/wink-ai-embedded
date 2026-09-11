// SPDX-License-Identifier: Apache-2.0
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
    // series model (e.g. cms8s_*) never installs hooks on another family.
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

#ifdef __cplusplus
}
#endif
