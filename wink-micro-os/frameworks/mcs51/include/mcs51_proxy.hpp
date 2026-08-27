// SPDX-License-Identifier: Apache-2.0
// MCS-51 SFR proxy (C++17). Models the 8051 Special Function Register file so
// unmodified Keil C51 code (`sfr P1 = 0x90; sbit LED = P1^0; P1 |= 0x01;`)
// compiles and runs in the host/wasm sandbox behind named POD types (ADR-0004
// static dispatch — no vtable, no container_of).
//
// M1 scope: basic read/write/latch + sbit. Diff edge dispatch, Read-Pin vs
// Read-Latch semantics, and RMW isolation arrive in M4 (ADR-0071).
#pragma once

#include <cstdint>

// ── C-ABI boundary (boundary ③) ─────────────────────────────────────────────
// The SFR shadow and the per-access hook are consumed across TUs (the user
// app TU, the bridge TU, and future timer/ADC model TUs). They MUST carry C
// linkage: MSVC decorates C++ symbols and rejects mismatched linkage, while
// GCC/emcc loose-link it (Spike-S2 §4.2 caught this only on MSVC).
extern "C" {

// 256-byte SFR address space. Index = SFR address (e.g. P1 at 0x90).
extern uint8_t wink_mcs51_sfr_shadow[256];

// Called on every observable SFR access from the proxy. The bridge charges
// virtual time and performs the cooperative quota yield (M1: periodic yield;
// M2: virtual-microsecond quota + timer catch-up). Defined in mcs51_bridge.cpp.
void wink_mcs51_on_sfr_access(uint8_t addr);

}  // extern "C"

// ── Bit proxy: an sbit ──────────────────────────────────────────────────────
// Two Keil sbit forms must both work:
//   sbit LED  = P1^0;   -> user form, bit of a named SFR
//   sbit P1_0 = 0x90;   -> REGX52.H predefined form, absolute bit address
//                          (SFR addr = abs & 0xF8, bit = abs & 0x07).
struct WinkSbit {
    uint8_t addr;
    uint8_t bit;

    // Bit of a named SFR (used by WinkSfr::operator^).
    WinkSbit(uint8_t a, uint8_t b) : addr(a), bit(b) {}

    // Absolute bit-address form (`sbit TF0 = 0x8D;`). Non-explicit so the
    // copy-initialization `inline WinkSbit TF0 = 0x8D;` binds.
    WinkSbit(int abs_bit_addr)  // NOLINT(google-explicit-constructor)
        : addr(static_cast<uint8_t>(abs_bit_addr & 0xF8u)),
          bit(static_cast<uint8_t>(abs_bit_addr & 0x07u)) {}

    // sbit write: `LED = 1;` / `LED = !LED;`
    WinkSbit& operator=(uint8_t v) {
        if (v) {
            wink_mcs51_sfr_shadow[addr] |= static_cast<uint8_t>(1u << bit);
        } else {
            wink_mcs51_sfr_shadow[addr] &= static_cast<uint8_t>(~(1u << bit));
        }
        wink_mcs51_on_sfr_access(addr);
        return *this;
    }

    // sbit read: `if (LED)` / `LED = !LED;`
    operator uint8_t() const {
        wink_mcs51_on_sfr_access(addr);
        return static_cast<uint8_t>((wink_mcs51_sfr_shadow[addr] >> bit) & 1u);
    }
};

// ── SFR proxy: `sfr P1 = 0x90;` ─────────────────────────────────────────────
struct WinkSfr {
    uint8_t addr;

    // Non-explicit: the dialect header declares ports as `sfr P1 = 0x90;`,
    // which expands to copy-initialization `inline WinkSfr P1 = 0x90;`.
    WinkSfr(uint8_t a) : addr(a) {}  // NOLINT(google-explicit-constructor)

    // Whole-register write: `P1 = 0x55;`
    WinkSfr& operator=(uint8_t v) {
        wink_mcs51_sfr_shadow[addr] = v;
        wink_mcs51_on_sfr_access(addr);
        return *this;
    }

    // Read-Modify-Write: `P1 |= 0x01;`, `P1 &= 0xFE;` (latch read; M4 adds the
    // read-latch vs read-pin distinction per ADR-0071).
    WinkSfr& operator|=(uint8_t v) {
        wink_mcs51_sfr_shadow[addr] |= v;
        wink_mcs51_on_sfr_access(addr);
        return *this;
    }
    WinkSfr& operator&=(uint8_t v) {
        wink_mcs51_sfr_shadow[addr] &= v;
        wink_mcs51_on_sfr_access(addr);
        return *this;
    }
    WinkSfr& operator^=(uint8_t v) {
        wink_mcs51_sfr_shadow[addr] ^= v;
        wink_mcs51_on_sfr_access(addr);
        return *this;
    }

    // Whole-register read: `if (P1)` / `unsigned char x = P1;`
    operator uint8_t() const {
        wink_mcs51_on_sfr_access(addr);
        return wink_mcs51_sfr_shadow[addr];
    }

    // sbit formation: `P1^0`. Parameter is `int` (not uint8_t) so this member
    // beats the built-in `operator^(int,int)` for integer literals — Spike-S2
    // §4.3 found uint8_t ambiguous under GCC.
    WinkSbit operator^(int b) {
        return WinkSbit(addr, static_cast<uint8_t>(b));
    }
};
