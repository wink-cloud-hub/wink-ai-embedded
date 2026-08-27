// SPDX-License-Identifier: Apache-2.0
// MCS-51 <intrins.h> shim — Keil intrinsic functions.
//
// M1 scope: `_nop_()` is the natural interception point for tight polling
// loops (a bare `while(1) {}` has no SFR access to hook). It maps to the
// bridge microstep, which charges virtual time and cooperatively yields the
// fiber so the simulation master stays responsive.
//
// M3 scope: `_crol_`/`_cror_` (8-bit rotate, Keil rotates mod 8) and
// `_testbit_` (8051 JBC: test a bit and clear it, returning the old value).
// `_testbit_` works on both the C++ WinkSbit proxy (an sbit in user code) and
// a plain uint8_t byte / uint8_t* pointer (framework and test use).
#pragma once

#include <stdint.h>

#ifdef __cplusplus
// Self-contained in C++ mode: the WinkSbit sbit proxy lives next to this
// header (same framework include dir) and depends only on <cstdint>, so it is
// safe for cleaned user code to pull in transitively. pragma once makes the
// repeated include (REGX52.H includes mcs51_proxy.hpp before intrins.h) a
// no-op. Pure-C callers never see it.
#include "mcs51_proxy.hpp"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// One 8051 instruction-cycle interception point. Defined in mcs51_bridge.cpp.
void wink_mcs51_microstep(void);

// Keil _testbit_ on a raw byte pointer: read bit 0 of *p and clear it (JBC
// semantics), returning the old bit value.
static inline uint8_t wink_testbit_u8(uint8_t* p) {
    uint8_t r = (uint8_t)(*p & 1u);
    *p = (uint8_t)(*p & 0xFEu);  // clear bit 0, leave all other bits (JBC)
    return r;
}

// Keil intrinsic: rotate an 8-bit value left by n bits. n is taken mod 8
// (Keil semantics: n=0 returns v unchanged; n>=8 wraps).
static inline uint8_t _crol_(uint8_t v, uint8_t n) {
    n = (uint8_t)(n & 7u);
    if (n == 0u) {
        return v;
    }
    return (uint8_t)((v << n) | (v >> (8u - n)));
}

// Keil intrinsic: rotate an 8-bit value right by n bits (mod 8).
static inline uint8_t _cror_(uint8_t v, uint8_t n) {
    n = (uint8_t)(n & 7u);
    if (n == 0u) {
        return v;
    }
    return (uint8_t)((v >> n) | (v << (8u - n)));
}

#ifdef __cplusplus
}  // extern "C"

// Keil _testbit_ on an sbit proxy: read the bit via WinkSbit's operator, and
// when it was set clear it via its operator= (the JBC clear). The read/clear
// each run the SFR proxy hooks exactly as `if (b) b = 0;` would.
static inline uint8_t wink_testbit_sbit(WinkSbit& b) {
    uint8_t r = static_cast<uint8_t>(b);
    if (r != 0u) {
        b = 0u;
    }
    return r;
}

// Overload set behind the `_testbit_` macro:
//   _testbit_(TI)        -> WinkSbit&  (sbit lvalue; exact match wins)
//   _testbit_(byte)      -> uint8_t&   (bit 0 of a byte lvalue)
//   _testbit_(&byte)     -> uint8_t*   (pointer form)
// No statement-expression needed (MSVC lacks GNU statement expressions);
// ordinary overload resolution dispatches with each argument evaluated once.
static inline uint8_t wink_testbit_dispatch(WinkSbit& b) {
    return wink_testbit_sbit(b);
}
static inline uint8_t wink_testbit_dispatch(uint8_t& b) {
    return wink_testbit_u8(&b);
}
static inline uint8_t wink_testbit_dispatch(uint8_t* p) {
    return wink_testbit_u8(p);
}

#define _testbit_(b) wink_testbit_dispatch(b)
#else
// Pure-C callers: only the byte/pointer form exists (Keil C51 applies
// _testbit_ to a bit variable; in the sandbox sbits are C++ proxies).
#define _testbit_(b) wink_testbit_u8(&(b))
#endif

// Keil intrinsic: a single CPU cycle delay (NOP).
#define _nop_() wink_mcs51_microstep()
