// SPDX-License-Identifier: Apache-2.0
// MCS-51 SFR proxy (C++17). Models the 8051 Special Function Register file so
// unmodified Keil C51 code (`sfr P1 = 0x90; sbit LED = P1^0; P1 |= 0x01;`)
// compiles and runs in the host/wasm sandbox behind named POD types (ADR-0004
// static dispatch — no vtable, no container_of).
//
// Data plane (M4, ADR-0071):
//   * Whole-register writes compute `diff = old ^ val`; only bits that really
//     edge fire the Level-2 pin on_write trap and the channel-1 instant
//     notification js_pal_gpio_write (Zero False-Trigger, D2/D3). diff == 0
//     is a fast no-op path.
//   * Whole-register READS reconstruct the external pin level (Read-Pin) with
//     a three-way resolution per GPIO bit: (1) a per-bit on_read trap owned by
//     an internal model (e.g. the ADC0832 DO line) wins; (2) else the UniSim
//     channel-1 external level js_pal_gpio_read_state() — a driven 0/1 from the
//     JS PinArbiter / a button plugin is returned, while HiZ/conflict (2/3)
//     means "no external driver"; (3) else (HiZ, or a non-GPIO control SFR)
//     the LATCH shadow is used. Compound assignments (RMW: |= &= ^= += -=
//     ++ -- <<= >>=) read the LATCH shadow only — never the pin or the
//     external level — so an externally-held-low input bit can never be
//     written back into the latch (quasi-bidirectional FET lock-up, data-plane
//     SSOT §2.2).
//   * Non-GPIO SFRs (port 0xFF: TCON/SCON/ADCON/…) route through the SFR
//     read/write hook tables instead (timer lazy eval, UART, CMS8S ADC).
//
// Static-init safety (铁律 2, ADR-0072 D5): all constructors are constexpr
// with const-address arguments, so every `inline WinkSfr P1 = 0x90;` gets
// constant initialization at load — before any dynamic C++ ctor and before
// the POD tables in mcs51_trap.h are ever touched.
#pragma once

#include <cstdint>

#include "mcs51_trap.h"

// ── C-ABI interception entries (defined in mcs51_bridge.cpp, boundary ③→④) ──
// Read: run the SFR read-hook for this address (lazy timer evaluation) and
// charge one interception microstep. Write: run the SFR write-hook (timer /
// UART / future CMS8S models) and charge one microstep. GPIO port addresses
// have no SFR hooks registered (their peripherals use pin traps), so for
// P0..P3 these calls effect only the microstep charge.
extern "C" {
void wink_mcs51_on_sfr_read(uint8_t addr);
void wink_mcs51_on_sfr_write(uint8_t addr, uint8_t old_val, uint8_t new_val);

// UniSim 3.0 channel 1: instant pin-edge notification (AD-18). A JS import
// under emscripten (wink_sim_js.js / node stub); a weak host fallback in
// mcs51_uni_bridge.cpp counts notifications. global_pin = (port << 3) | bit.
void js_pal_gpio_write(uint16_t pin, bool level);

// UniSim 3.0 channel 1 (read direction): external digital pin level driven by
// the JS PinArbiter / an input plugin (button). A JS import under emscripten;
// a host fallback in mcs51_uni_bridge.cpp. Returns the platform JS_GPIO_STATE_*
// code: 0 = driven LOW, 1 = driven HIGH, 2 = HiZ (no external driver), 3 =
// conflict. Only 0/1 are real pin levels; HiZ/conflict mean the caller must
// fall back to the latch.
uint8_t js_pal_gpio_read_state(uint16_t pin);
}  // extern "C"

// Resolve the externally-driven level of one GPIO bit. Returns 0/1 when the
// PinArbiter actively drives the pin, or -1 when it is HiZ/conflict (no
// external driver) so the Read-Pin path falls back to the latch shadow.
inline int mcs51_ext_pin_level(uint8_t port, uint8_t bit) {
    const uint8_t st = js_pal_gpio_read_state(
        static_cast<uint16_t>((static_cast<uint16_t>(port) << 3) | bit));
    return st == 1u ? 1 : (st == 0u ? 0 : -1);
}

// Port index for an SFR address: P0=0x80…P3=0xB0 → 0..3; anything else 0xFF.
constexpr uint8_t wink_mcs51_port_for(uint8_t addr) {
    return (addr == 0x80u) ? 0u
         : (addr == 0x90u) ? 1u
         : (addr == 0xA0u) ? 2u
         : (addr == 0xB0u) ? 3u : 0xFFu;
}

// ── Bit proxy: an sbit ──────────────────────────────────────────────────────
// Two Keil sbit forms must both work:
//   sbit LED  = P1^0;   -> user form, bit of a named SFR (carries port_idx)
//   sbit P1_0 = 0x90;   -> REGX52.H predefined form, absolute bit address
//                          (SFR addr = abs & 0xF8, bit = abs & 0x07, port
//                          inferred from the SFR address).
struct WinkSbit {
    uint8_t addr;   // SFR address (0x80, 0x88, 0x90, …)
    uint8_t port;   // 0..3 for P0..P3; 0xFF for bit-addressable control SFRs
    uint8_t bit;    // 0..7

    constexpr WinkSbit(uint8_t a, uint8_t p, uint8_t b)
        : addr(a), port(p), bit(b) {}

    // Absolute bit-address form (`sbit TF0 = 0x8D;`). Non-explicit so the
    // copy-initialization `inline WinkSbit TF0 = 0x8D;` binds.
    constexpr WinkSbit(int abs_bit_addr)  // NOLINT(google-explicit-constructor)
        : addr(static_cast<uint8_t>(abs_bit_addr & 0xF8u)),
          port(wink_mcs51_port_for(static_cast<uint8_t>(abs_bit_addr & 0xF8u))),
          bit(static_cast<uint8_t>(abs_bit_addr & 0x07u)) {}

    // sbit write: `LED = 1;` / `LED = !LED;`. Parameter `unsigned` (not
    // uint8_t): `ADC_DIO = channel & 1;` promotes to int and would narrow at
    // the call site under /WX.
    WinkSbit& operator=(unsigned v) {
        const uint8_t old_val = wink_mcs51_sfr_shadow[addr];
        const uint8_t old_bit = static_cast<uint8_t>((old_val >> bit) & 1u);
        const uint8_t new_bit = v ? 1u : 0u;
        const uint8_t mask = static_cast<uint8_t>(1u << bit);
        const uint8_t new_val = new_bit ? static_cast<uint8_t>(old_val | mask)
                                        : static_cast<uint8_t>(old_val & ~mask);
        wink_mcs51_sfr_shadow[addr] = new_val;

        if (port < 4u && old_bit != new_bit) {
            // GPIO pin edge: channel-1 instant notify + Level-2 write trap.
            js_pal_gpio_write(static_cast<uint16_t>((port << 3) | bit),
                              new_bit != 0u);
            const mcs51_pin_trap_t& trap = wink_mcs51_pin_traps[port][bit];
            if (trap.on_write != nullptr) {
                trap.on_write(trap.write_ctx, new_bit);
            }
        }
        // Non-GPIO SFR hooks (timer/UART/…) fire via the bridge entry; for
        // GPIO addresses the hook slot is empty (microstep charge only).
        wink_mcs51_on_sfr_write(addr, old_val, new_val);
        return *this;
    }

    // sbit-to-sbit assignment: reads the rhs through its normal read path.
    WinkSbit& operator=(const WinkSbit& rhs) {
        return *this = static_cast<uint8_t>(rhs);
    }

    // sbit read: `if (LED)` / `LED = !LED;`
    operator uint8_t() const {
        wink_mcs51_on_sfr_read(addr);
        if (port < 4u) {
            const mcs51_pin_trap_t& trap = wink_mcs51_pin_traps[port][bit];
            if (trap.on_read != nullptr) {
                return trap.on_read(trap.read_ctx) ? 1u : 0u;  // Read-Pin (model)
            }
            // Channel-1 external level (button plugin / PinArbiter). A driven
            // 0/1 wins; HiZ (-1) falls through to the latch.
            const int ext = mcs51_ext_pin_level(port, bit);
            if (ext >= 0) {
                return static_cast<uint8_t>(ext);
            }
        }
        // Control SFR (hook already ran above) or latched GPIO bit (HiZ input).
        return static_cast<uint8_t>((wink_mcs51_sfr_shadow[addr] >> bit) & 1u);
    }

    // Bit-level RMW (`LED ^= 1;`): the Keil CPL/ORL/ANL bit class reads the
    // LATCH, never the pin. Base value comes straight from the shadow.
    WinkSbit& operator^=(unsigned v) {
        const uint8_t latch =
            static_cast<uint8_t>((wink_mcs51_sfr_shadow[addr] >> bit) & 1u);
        return *this = (latch ^ (v & 1u));
    }
    WinkSbit& operator|=(unsigned v) {
        const uint8_t latch =
            static_cast<uint8_t>((wink_mcs51_sfr_shadow[addr] >> bit) & 1u);
        return *this = (latch | (v & 1u));
    }
    WinkSbit& operator&=(unsigned v) {
        const uint8_t latch =
            static_cast<uint8_t>((wink_mcs51_sfr_shadow[addr] >> bit) & 1u);
        return *this = (latch & (v & 1u));
    }
};

// ── SFR proxy: `sfr P1 = 0x90;` ─────────────────────────────────────────────
struct WinkSfr {
    uint8_t addr;
    uint8_t port;  // 0..3 for P0..P3; 0xFF for non-GPIO SFRs

    constexpr WinkSfr(uint8_t a)  // NOLINT(google-explicit-constructor)
        : addr(a), port(wink_mcs51_port_for(a)) {}

    constexpr WinkSfr(uint8_t a, uint8_t p) : addr(a), port(p) {}

    // sbit formation: `P1^0`. Parameter is `int` (not uint8_t) so this member
    // beats the built-in `operator^(int,int)` for integer literals — Spike-S2
    // §4.3 found uint8_t ambiguous under GCC. Carries the real SFR address and
    // port index so TCON^5 / P1^0 both resolve correctly.
    constexpr WinkSbit operator^(int b) const {
        return WinkSbit(addr, port, static_cast<uint8_t>(b));
    }

    // Whole-register write: `P1 = 0x55;` — diff edge dispatch for GPIO.
    // Parameter is `unsigned` (not uint8_t): the classic Keil clear idiom is
    // `P1 = ~0x01;` / `P1 &= ~(1<<n)`, where ~ promotes to int/unsigned and
    // would otherwise narrow at the call site (C4305). Mask to byte internally
    // so unmodified user code compiles clean.
    WinkSfr& operator=(unsigned v) {
        const uint8_t nv = static_cast<uint8_t>(v & 0xFFu);
        const uint8_t old_val = wink_mcs51_sfr_shadow[addr];
        wink_mcs51_sfr_shadow[addr] = nv;

        if (port < 4u) {
            const uint8_t diff = static_cast<uint8_t>(old_val ^ nv);
            if (diff != 0u) {
                for (uint8_t b = 0; b < 8u; ++b) {
                    if ((diff & static_cast<uint8_t>(1u << b)) != 0u) {
                        const uint8_t level =
                            static_cast<uint8_t>((nv >> b) & 1u);
                        js_pal_gpio_write(
                            static_cast<uint16_t>((port << 3) | b),
                            level != 0u);
                        const mcs51_pin_trap_t& trap =
                            wink_mcs51_pin_traps[port][b];
                        if (trap.on_write != nullptr) {
                            trap.on_write(trap.write_ctx, level);
                        }
                    }
                }
            }
        }
        // Non-GPIO SFR write hook (timer/UART/CMS8S) + microstep; for GPIO
        // ports the hook slot is empty (microstep charge only).
        wink_mcs51_on_sfr_write(addr, old_val, nv);
        return *this;
    }

    // Copy assignment: `P1 = P0;` reads the rhs pin levels (MOV port,port is
    // a Read-Pin on the source), then writes with full edge dispatch.
    WinkSfr& operator=(const WinkSfr& rhs) {
        return *this = static_cast<uint8_t>(rhs);
    }

    // Whole-register read: `if (P3 & 0x04)` / `uint8_t v = P1;`.
    // GPIO: reconstruct external pin levels from on_read traps (Read-Pin),
    // latched bits default to the latch. Non-GPIO: the read hook (lazy timer
    // evaluation) runs inside wink_mcs51_on_sfr_read.
    operator uint8_t() const {
        if (port < 4u) {
            uint8_t val = wink_mcs51_sfr_shadow[addr];
            for (uint8_t b = 0; b < 8u; ++b) {
                const mcs51_pin_trap_t& trap = wink_mcs51_pin_traps[port][b];
                uint8_t pin_level;
                if (trap.on_read != nullptr) {
                    pin_level = trap.on_read(trap.read_ctx) ? 1u : 0u;  // model
                } else {
                    const int ext = mcs51_ext_pin_level(port, b);  // channel-1
                    if (ext < 0) {
                        continue;  // HiZ/conflict: keep the latch bit
                    }
                    pin_level = static_cast<uint8_t>(ext);
                }
                val = pin_level ? static_cast<uint8_t>(val | (1u << b))
                                : static_cast<uint8_t>(val & ~(1u << b));
            }
            wink_mcs51_on_sfr_read(addr);  // microstep (GPIO hook slot empty)
            return val;
        }
        wink_mcs51_on_sfr_read(addr);  // lazy SFR hook + microstep
        return wink_mcs51_sfr_shadow[addr];
    }

    // ── RMW compound assignments (Read-LATCH, golden rule, SSOT §2.2) ───────
    // The base value is the latch shadow itself — NEVER operator uint8_t()
    // (Read-Pin): an externally-held-low input bit must not be written back
    // into the latch and lock the pull-down FET. The resulting value then
    // goes through operator= so edge dispatch stays exact.
    // Parameters are `unsigned` for the same Keil-idiom reason as operator=:
    // `P1 &= ~0x01;` / `P1 |= 1 << n;` carry int/unsigned operands that must
    // not narrow at the call site. operator= masks to a byte.
    WinkSfr& operator|=(unsigned v) {
        return *this = (wink_mcs51_sfr_shadow[addr] | v);
    }
    WinkSfr& operator&=(unsigned v) {
        return *this = (wink_mcs51_sfr_shadow[addr] & v);
    }
    WinkSfr& operator^=(unsigned v) {
        return *this = (wink_mcs51_sfr_shadow[addr] ^ v);
    }
    WinkSfr& operator+=(unsigned v) {
        return *this = (wink_mcs51_sfr_shadow[addr] + v);
    }
    WinkSfr& operator-=(unsigned v) {
        return *this = (wink_mcs51_sfr_shadow[addr] - v);
    }
    WinkSfr& operator<<=(unsigned s) {
        return *this = (wink_mcs51_sfr_shadow[addr] << s);
    }
    WinkSfr& operator>>=(unsigned s) {
        return *this = (wink_mcs51_sfr_shadow[addr] >> s);
    }

    // Prefix/postfix inc/dec (INC/DEC port are RMW-latch instructions).
    WinkSfr& operator++() {
        return *this = static_cast<uint8_t>(wink_mcs51_sfr_shadow[addr] + 1u);
    }
    uint8_t operator++(int) {
        const uint8_t old = wink_mcs51_sfr_shadow[addr];
        *this = static_cast<uint8_t>(old + 1u);
        return old;
    }
    WinkSfr& operator--() {
        return *this = static_cast<uint8_t>(wink_mcs51_sfr_shadow[addr] - 1u);
    }
    uint8_t operator--(int) {
        const uint8_t old = wink_mcs51_sfr_shadow[addr];
        *this = static_cast<uint8_t>(old - 1u);
        return old;
    }
};
