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
// is backed by reachable storage. Accesses outside the aperture are R-008
// out-of-bounds: in a WINK_MCS51_STRICT build they assert; otherwise they are
// warned once (rate-limited per access kind), writes are dropped, and reads
// return 0xFF.
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
// shadow 1:1; indices >= SIZE are out of bounds. Keep even so a XWORD pair
// never straddles the aperture edge.
#ifndef WINK_MCS51_XDATA_SIZE
#define WINK_MCS51_XDATA_SIZE 8192u
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Full 64 KB xdata shadow (BSS; defined in mcs51_xdata.cpp). Only the first
// WINK_MCS51_XDATA_SIZE bytes are reachable through the bounds-checked path.
extern uint8_t wink_mcs51_xdata_shadow[65536];

// Bounds-checked byte access (boundary ③ C ABI; microstep charging, OOB
// handling and the future M4 peripheral write hook live in mcs51_xdata.cpp,
// keeping this header free of PAL includes). `kind` tags the accessor for the
// rate-limited OOB warning: 0 = XBYTE, 1 = XWORD.
uint8_t  wink_mcs51_xdata_read(uint32_t addr, uint8_t kind);
void     wink_mcs51_xdata_write(uint32_t addr, uint8_t value, uint8_t kind);
// Test observability: OOB access count since framework reset.
uint32_t wink_mcs51_xdata_oob_count(void);
// Framework lifecycle: zero the aperture + counters (test isolation).
void     wink_mcs51_xdata_reset(void);

#ifdef __cplusplus
}  // extern "C"

// ── C++ lvalue proxies ──────────────────────────────────────────────────────
// Keil XBYTE/XWORD are used as lvalues, including RMW (`XBYTE[a] |= m;`). The
// proxy's operator[] returns a lightweight accessor bound to one xdata
// address; every load, store and RMW funnels through the checked C-ABI path,
// so an in-bounds index hits the shadow while an OOB index drops writes and
// reads back 0xFF — no dangling reference into unreachable storage.
class WinkXByteProxy {
public:
    class Ref {
    public:
        explicit Ref(uint32_t addr) : addr_(addr) {}
        Ref& operator=(uint8_t v) {
            wink_mcs51_xdata_write(addr_, v, 0u);
            return *this;
        }
        operator uint8_t() const {
            return wink_mcs51_xdata_read(addr_, 0u);
        }
        // RMW: `XBYTE[a] |= m;` / `&=` / `^=` — checked read, checked store.
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

    private:
        uint32_t addr_;
    };

    Ref operator[](uint32_t addr) const {
        return Ref(addr);
    }
};

// XWORD: 16-bit big-endian access (Keil C51 stores the high byte at the lower
// address). Word index i covers xdata bytes 2i (high) and 2i+1 (low); with an
// even aperture both bytes are either in bounds or OOB together.
class WinkXWordProxy {
public:
    class Ref {
    public:
        explicit Ref(uint32_t word_index) : addr_(word_index * 2u) {}
        Ref& operator=(uint16_t v) {
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

    private:
        uint32_t addr_;
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
