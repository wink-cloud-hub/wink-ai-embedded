// SPDX-License-Identifier: Apache-2.0
// Stage4 test-only family harness (PLAN-20260911-MCS51-S4 S4-2 Step 5).
//
// TEST LINKAGE ONLY — never into production libraries (lives under test/,
// no CMake target references it outside host tests). Idempotent family
// switch: registry reset + family register + set_family + reset, so every
// cms8s unit test and bridge-driven e2e gets its chip models explicitly
// (production gets them through the generated mcs51_family_select.h glue,
// stage6; pre-glue production keeps the static rows until Commit C).
#pragma once

#include <stdint.h>

#include "mcs51_context.h"
#include "mcs51_family.h"
#include "mcs51_peripheral.h"

#ifdef __cplusplus
extern "C" {
#endif

// Chip register entries (defined in chips/*/src/*_register.cpp).
void cms8s78xx_register(void);
void at89c52_register(void);

// Switch the (single, active) test context to `family` with its chip
// models registered. Replaces raw set_family + context_reset pairs in
// cms8s tests and e2e drivers (classic tests keep raw reset: the empty
// at89 register is a no-op but documents the protocol).
static inline void mcs51_test_use_family(uint8_t family) {
    mcs51_peripheral_registry_reset();
    if (family == MCS51_FAMILY_CMS8S78XX) {
        cms8s78xx_register();
    } else {
        at89c52_register();
    }
    mcs51_context_set_family(family);
    mcs51_context_reset(mcs51_get_context());
}

#ifdef __cplusplus
}
#endif
