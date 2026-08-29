// SPDX-License-Identifier: Apache-2.0
// MCS-51 <absacc.h> shim — absolute XDATA access (Keil XBYTE/XWORD).
//
// Keil C51 exposes the external 64 KB data space as absolute-access arrays:
//
//     XBYTE[0x1234] = 0xAB;   // byte  access to xdata 0x1234
//     v = XBYTE[0x2000];
//     XWORD[0x10]   = 0x1234; // word  access: xdata 0x20 = 0x12 (high),
//                             //                  xdata 0x21 = 0x34 (low)
//
// In the sandbox all Keil storage spaces flatten onto one linear host memory,
// but XBYTE/XWORD still model the xdata bus: a 64 KB linear shadow backs them
// (defined in mcs51_xdata.cpp), each access charges one interception
// microstep (so a `while(XBYTE[f]!=x){}` poll cannot freeze the fiber), and
// only the configurable legal aperture (WINK_MCS51_XDATA_SIZE, default 8 KB)
// plus the CMS8S78xx extended-SFR window [0xF000, 0x10000) (pin mux PxxCFG,
// ADCLDO @ 0xF692 — reached by vendor code as `xdata` pointers; proxied by
// WinkXsfr in mcs51_xsfr.hpp) are backed by reachable storage. Accesses
// outside both apertures are R-008 out-of-bounds: in a WINK_MCS51_STRICT
// build they assert; otherwise they are warned once (rate-limited per access
// kind), writes are dropped, and reads return 0xFF.
//
// The proxies support the full set of lvalue operations Keil code uses on
// XBYTE/XWORD: plain assignment, bitwise RMW (|= &= ^=), arithmetic RMW
// (+= -=) and pre/post increment/decrement (++ --). All route through the
// checked C-ABI read/write path, so OOB semantics (drop write / 0xFF read)
// hold for every form.
//
// M4 hook note: xdata writes to externally-mapped peripheral addresses would
// attach in the access path of mcs51_xdata.cpp (a write hook before the shadow
// store); the M3 shadow is plain RAM.
//
// PBYTE/PWORD (pdata, 256-byte window) are not provided in M3 — no sample
// uses them; add as thin aliases over shadow[0..255] when needed.
#pragma once

#include <stdint.h>

// Legal xdata aperture size in bytes (compile-time overridable via CMake;
// default 8 KB, inside the spec's 4–16 KB band). Index 0..SIZE-1 maps to the
// shadow 1:1; indices >= SIZE are out of bounds. Must stay even so a XWORD
// pair never straddles the aperture edge.
#ifndef WINK_MCS51_XDATA_SIZE
#define WINK_MCS51_XDATA_SIZE 8192u
#endif

#ifdef __cplusplus
// Misconfiguration guard: the shadow is 64 KB, and an odd aperture would let a
// XWORD pair straddle the edge (low byte in bounds, high byte OOB).
static_assert(WINK_MCS51_XDATA_SIZE <= 65536u &&
                  (WINK_MCS51_XDATA_SIZE % 2u) == 0u,
              "WINK_MCS51_XDATA_SIZE must be <= 65536 and even");
extern "C" {
#endif

// Full 64 KB xdata shadow (BSS; defined in mcs51_xdata.cpp). Only the first
// WINK_MCS51_XDATA_SIZE bytes are reachable through the bounds-checked path.
extern uint8_t wink_mcs51_xdata_shadow[65536];

// Bounds-checked byte access (boundary ③ C ABI; microstep charging, OOB
// handling and the future M4 peripheral write hook live in mcs51_xdata.cpp,
// keeping this header free of PAL includes). The address is 64-bit so a
// XWORD byte address computed from a large word index (2*i) cannot wrap and
// alias low memory before the `< aperture` check. `kind` tags the accessor
// for the rate-limited OOB warning: 0 = XBYTE, 1 = XWORD.
uint8_t  wink_mcs51_xdata_read(uint64_t addr, uint8_t kind);
void     wink_mcs51_xdata_write(uint64_t addr, uint8_t value, uint8_t kind);
// Test observability: OOB access count since framework reset.
uint32_t wink_mcs51_xdata_oob_count(void);
// Framework lifecycle: zero the aperture + counters (test isolation).
void     wink_mcs51_xdata_reset(void);

#ifdef __cplusplus
}  // extern "C"

// ── C++ lvalue proxies ──────────────────────────────────────────────────────
// Keil XBYTE/XWORD are used as lvalues. operator[] returns a lightweight
// accessor bound to one xdata byte/word; every load, store and RMW funnels
// through the checked C-ABI path, so an in-bounds index hits the shadow while
// an OOB index drops writes and reads back 0xFF (or 0xFFFF for a word) — no
// dangling reference into unreachable storage.
class WinkXByteProxy {
public:
    class Ref {
    public:
        explicit Ref(uint64_t addr) : addr_(addr) {}
        Ref& operator=(uint8_t v) {
            wink_mcs51_xdata_write(addr_, v, 0u);
            return *this;
        }
        // Ref = Ref (`XBYTE[a] = XBYTE[b];`): READ the rhs through its checked
        // accessor and write the value at THIS address. The compiler-generated
        // trivial copy assignment would rebind addr_ and store nothing.
        Ref& operator=(const Ref& rhs) {
            wink_mcs51_xdata_write(addr_,
                                   wink_mcs51_xdata_read(rhs.addr_, 0u), 0u);
            return *this;
        }
        operator uint8_t() const {
            return wink_mcs51_xdata_read(addr_, 0u);
        }
        // Bitwise RMW: `XBYTE[a] |= m;` — checked read, checked store.
        Ref& operator|=(uint8_t v) {
            *this = static_cast<uint8_t>(uint8_t(*this) | v);
            return *this;
        }
        Ref& operator&=(uint8_t v) {
            *this = static_cast<uint8_t>(uint8_t(*this) & v);
            return *this;
        }
        Ref& operator^=(uint8_t v) {
            *this = static_cast<uint8_t>(uint8_t(*this) ^ v);
            return *this;
        }
        // Arithmetic RMW / increment-decrement (Keil idioms: `XBYTE[p] += n;`,
        // `XBYTE[i]++;`). Wrapping 8-bit arithmetic; OOB reads 0xFF and the
        // resulting write is dropped, so the shadow is never touched.
        Ref& operator+=(uint8_t v) {
            *this = static_cast<uint8_t>(uint8_t(*this) + v);
            return *this;
        }
        Ref& operator-=(uint8_t v) {
            *this = static_cast<uint8_t>(uint8_t(*this) - v);
            return *this;
        }
        Ref& operator++() {  // prefix
            *this = static_cast<uint8_t>(uint8_t(*this) + 1u);
            return *this;
        }
        uint8_t operator++(int) {  // postfix: returns the old value
            uint8_t old = wink_mcs51_xdata_read(addr_, 0u);
            *this = static_cast<uint8_t>(old + 1u);
            return old;
        }
        Ref& operator--() {  // prefix
            *this = static_cast<uint8_t>(uint8_t(*this) - 1u);
            return *this;
        }
        uint8_t operator--(int) {  // postfix: returns the old value
            uint8_t old = wink_mcs51_xdata_read(addr_, 0u);
            *this = static_cast<uint8_t>(old - 1u);
            return old;
        }

    private:
        uint64_t addr_;
    };

    Ref operator[](uint32_t addr) const {
        return Ref(static_cast<uint64_t>(addr));
    }
};

// XWORD: 16-bit big-endian access (Keil C51 stores the high byte at the lower
// address). Word index i covers xdata bytes 2i (high) and 2i+1 (low); the byte
// address is computed in 64-bit so an oversized index is rejected as OOB
// rather than wrapping to alias low memory. With an even aperture both bytes
// of a pair are either in bounds or OOB together.
class WinkXWordProxy {
public:
    class Ref {
    public:
        explicit Ref(uint32_t word_index)
            : addr_(static_cast<uint64_t>(word_index) * 2u) {}
        Ref& operator=(uint16_t v) {
            wink_mcs51_xdata_write(addr_,
                                   static_cast<uint8_t>(v >> 8), 1u);
            wink_mcs51_xdata_write(addr_ + 1u,
                                   static_cast<uint8_t>(v & 0xFFu), 1u);
            return *this;
        }
        // Ref = Ref (`XWORD[a] = XWORD[b];`): copy the VALUE, not the index.
        Ref& operator=(const Ref& rhs) {
            const uint16_t v = static_cast<uint16_t>(rhs);
            wink_mcs51_xdata_write(addr_,
                                   static_cast<uint8_t>(v >> 8), 1u);
            wink_mcs51_xdata_write(addr_ + 1u,
                                   static_cast<uint8_t>(v & 0xFFu), 1u);
            return *this;
        }
        operator uint16_t() const {
            uint16_t hi = wink_mcs51_xdata_read(addr_, 1u);
            uint16_t lo = wink_mcs51_xdata_read(addr_ + 1u, 1u);
            return static_cast<uint16_t>((hi << 8) | lo);
        }
        Ref& operator|=(uint16_t v) {
            *this = static_cast<uint16_t>(uint16_t(*this) | v);
            return *this;
        }
        Ref& operator&=(uint16_t v) {
            *this = static_cast<uint16_t>(uint16_t(*this) & v);
            return *this;
        }
        Ref& operator^=(uint16_t v) {
            *this = static_cast<uint16_t>(uint16_t(*this) ^ v);
            return *this;
        }
        Ref& operator+=(uint16_t v) {
            *this = static_cast<uint16_t>(uint16_t(*this) + v);
            return *this;
        }
        Ref& operator-=(uint16_t v) {
            *this = static_cast<uint16_t>(uint16_t(*this) - v);
            return *this;
        }
        Ref& operator++() {  // prefix
            *this = static_cast<uint16_t>(uint16_t(*this) + 1u);
            return *this;
        }
        uint16_t operator++(int) {  // postfix: returns the old value
            uint16_t old = uint16_t(*this);
            *this = static_cast<uint16_t>(old + 1u);
            return old;
        }
        Ref& operator--() {  // prefix
            *this = static_cast<uint16_t>(uint16_t(*this) - 1u);
            return *this;
        }
        uint16_t operator--(int) {  // postfix: returns the old value
            uint16_t old = uint16_t(*this);
            *this = static_cast<uint16_t>(old - 1u);
            return old;
        }

    private:
        uint64_t addr_;
    };

    Ref operator[](uint32_t word_index) const {
        return Ref(word_index);
    }
};

// C++17 inline variables: ODR-safe single instances across TUs, mirroring the
// `inline WinkSfr` sfr pattern in REGX52.H.
inline WinkXByteProxy XBYTE;
inline WinkXWordProxy XWORD;
#endif  // __cplusplus
