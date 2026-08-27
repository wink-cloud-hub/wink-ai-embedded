// SPDX-License-Identifier: Apache-2.0
// MCS-51 <intrins.h> shim — Keil intrinsic functions.
//
// M1 scope: `_nop_()` is the natural interception point for tight polling
// loops (a bare `while(1) {}` has no SFR access to hook). It maps to the
// bridge microstep, which charges virtual time and cooperatively yields the
// fiber so the simulation master stays responsive. `_crol_/_cror_/_testbit_`
// arrive in M3.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// One 8051 instruction-cycle interception point. Defined in mcs51_bridge.cpp.
void wink_mcs51_microstep(void);

#ifdef __cplusplus
}  // extern "C"
#endif

// Keil intrinsic: a single CPU cycle delay (NOP).
#define _nop_() wink_mcs51_microstep()
