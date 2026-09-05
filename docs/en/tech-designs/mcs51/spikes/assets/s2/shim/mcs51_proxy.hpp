// SPDX-License-Identifier: Apache-2.0
// Spike-S2 MINIMAL proxy stub (throwaway, NOT production). Just enough to
// prove the dialect chain compiles/links on GCC/MSVC/emcc in C++17.
#pragma once
#include <cstdint>

// SFR shadow is a C-ABI boundary (trap table / runtime consume it from .cpp);
// extern "C" keeps linkage identical across GCC/MSVC/emcc.
extern "C" {
extern uint8_t s_sfr_shadow[256];
}

struct WinkSfrBitProxy {
    uint8_t addr, bit;
    WinkSfrBitProxy(uint8_t a, uint8_t b) : addr(a), bit(b) {}
    WinkSfrBitProxy& operator=(uint8_t v) {
        if (v) s_sfr_shadow[addr] |=  (uint8_t)(1u << bit);
        else   s_sfr_shadow[addr] &= (uint8_t)~(1u << bit);
        return *this;
    }
    operator uint8_t() const { return (s_sfr_shadow[addr] >> bit) & 1u; }
};

struct WinkSfr {
    uint8_t addr;
    uint8_t port;
    WinkSfr(uint8_t a) : addr(a), port(1) {}            // single-arg (vendor sfr)
    WinkSfr(uint8_t a, uint8_t p) : addr(a), port(p) {} // brace form {addr,port}
    WinkSfr& operator=(uint8_t v) { s_sfr_shadow[addr] = v; return *this; }
    WinkSfr& operator|=(uint8_t v) { s_sfr_shadow[addr] |= v; return *this; } // RMW: latch
    WinkSfr& operator&=(uint8_t v) { s_sfr_shadow[addr] &= v; return *this; }
    operator uint8_t() const { return s_sfr_shadow[addr]; }
    WinkSfrBitProxy operator^(int b) { return WinkSfrBitProxy(addr, (uint8_t)b); } // int param: beats built-in operator^(int,int) for `P1^0`
};
