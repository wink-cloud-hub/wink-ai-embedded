// SPDX-License-Identifier: Apache-2.0
// MCS-51 XSFR proxy (C++17) — extended-SFR registers reached via MOVX @DPTR.
//
// Enhanced 8051 vendors (CMS8S78xx) place extra peripheral registers in the
// xdata/MOVX address space: pin mux PxxCFG @ 0xF000..0xF033, ADC LDO ADCLDO
// @ 0xF692, … Vendor headers declare them as
//
//     #define ADCLDO *(volatile unsigned char xdata *) 0xF692
//
// After the cleanup pass erases the Keil `xdata` keyword, that expression
// becomes `*(volatile unsigned char *) 0xF692` — a WILD host-pointer
// dereference (address 0xF692 in the host process). REG_CMS8S.H therefore
// replaces such macros with a WinkXsfr proxy bound to the same address. Every
// load/store/RMW funnels through the bounds-checked C-ABI xdata path
// (mcs51_xdata.cpp, kind = XSFR), so the access lands in the XSFR window of
// the 64 KB shadow, charges one interception microstep, and keeps STRICT
// out-of-bounds semantics — no host pointer is ever formed.
//
// Static-init safety (铁律 2, ADR-0072 D5): the constructor is constexpr with
// a constant address, so `inline WinkXsfr ADCLDO(0xF692);` gets constant
// initialization at load — no dynamic ctor, no ordering hazard.
#pragma once

#include <cstdint>

#include "absacc.h"  // wink_mcs51_xdata_read/write (kind 2 = XSFR)

class WinkXsfr {
public:
    constexpr explicit WinkXsfr(uint16_t addr) : addr_(addr) {}

    // `ADCLDO = 0x80;`
    WinkXsfr& operator=(unsigned v) {
        wink_mcs51_xdata_write(static_cast<uint64_t>(addr_),
                               static_cast<uint8_t>(v & 0xFFu), 2u);
        return *this;
    }
    // `ADCLDO = P00CFG;` (proxy = proxy): read the rhs through its checked
    // path, store the value — never rebind (mirrors WinkXByteProxy::Ref).
    WinkXsfr& operator=(const WinkXsfr& rhs) {
        return *this = static_cast<unsigned>(static_cast<uint8_t>(rhs));
    }

    operator uint8_t() const {
        return wink_mcs51_xdata_read(static_cast<uint64_t>(addr_), 2u);
    }

    // Bitwise RMW (`ADCLDO |= 0x80;`) and arithmetic RMW: checked read,
    // checked write via operator=.
    WinkXsfr& operator|=(unsigned v) {
        return *this = (static_cast<uint8_t>(*this) | v);
    }
    WinkXsfr& operator&=(unsigned v) {
        return *this = (static_cast<uint8_t>(*this) & v);
    }
    WinkXsfr& operator^=(unsigned v) {
        return *this = (static_cast<uint8_t>(*this) ^ v);
    }
    WinkXsfr& operator+=(unsigned v) {
        return *this = (static_cast<uint8_t>(*this) + v);
    }
    WinkXsfr& operator-=(unsigned v) {
        return *this = (static_cast<uint8_t>(*this) - v);
    }
    WinkXsfr& operator++() {
        return *this = static_cast<unsigned>(
                   static_cast<uint8_t>(static_cast<uint8_t>(*this) + 1u));
    }
    uint8_t operator++(int) {
        const uint8_t old = static_cast<uint8_t>(*this);
        *this = static_cast<unsigned>(old + 1u);
        return old;
    }
    WinkXsfr& operator--() {
        return *this = static_cast<unsigned>(
                   static_cast<uint8_t>(static_cast<uint8_t>(*this) - 1u));
    }
    uint8_t operator--(int) {
        const uint8_t old = static_cast<uint8_t>(*this);
        *this = static_cast<unsigned>(old - 1u);
        return old;
    }

private:
    uint16_t addr_;
};
