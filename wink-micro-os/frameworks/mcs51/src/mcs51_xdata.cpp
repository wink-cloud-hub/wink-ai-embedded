// SPDX-License-Identifier: Apache-2.0
// MCS-51 XDATA shadow + bounds-checked absolute access (M3, R-008).
//
// See absacc.h. A 64 KB linear xdata shadow (BSS) backs XBYTE/XWORD. Two
// apertures are legal (M5, CMS8S78xx XSFR):
//   * [0, WINK_MCS51_XDATA_SIZE) — ordinary XRAM/XRAM aperture (R-008);
//   * [0xF000, 0x10000) — extended-SFR window (pin mux PxxCFG @ 0xF000..,
//     ADCLDO @ 0xF692, …), reached via MOVX @DPTR on enhanced 8051s.
// Each checked access charges one interception microstep — the same
// interception rationale as the SFR proxy, so a tight `while(XBYTE[f] != x){}`
// poll advances virtual time and yields the fiber. Out-of-bounds accesses
// (R-008): STRICT traps (assert message in debug + unconditional abort so a
// release/NDEBUG STRICT build still fails loudly); release warns once per
// access kind (XBYTE vs XWORD), drops writes, and returns 0xFF for reads.
//
// M4 hook note: an external-xdata-peripheral write hook would attach here,
// before the shadow store, trapping writes to externally-mapped addresses.
#include "absacc.h"

#include "wink_mcs51_clock.h"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#ifndef WINK_MCS51_STRICT
#include "pal_log.h"
#endif

namespace {

constexpr uint8_t KIND_BYTE = 0u;  // XBYTE accessor
constexpr uint8_t KIND_WORD = 1u;  // XWORD accessor
constexpr uint8_t KIND_XSFR = 2u;  // XSFR proxy (WinkXsfr / REG_CMS8S.H)

// CMS8S78xx extended-SFR window (MOVX @DPTR, manual §2.2): pin config
// PxxCFG @ 0xF000..0xF033, ADC LDO ADCLDO @ 0xF692, …
constexpr uint64_t XSFR_WINDOW_BASE = 0xF000ull;

bool     s_oob_warned[3] = {};   // once-per-kind warning latch
uint32_t s_oob_count = 0;

// Legal xdata: ordinary XRAM aperture OR the XSFR window.
bool xdata_addr_legal(uint64_t addr) {
    return addr < (uint64_t)WINK_MCS51_XDATA_SIZE ||
           (addr >= XSFR_WINDOW_BASE && addr < 0x10000ull);
}

void oob_trap(uint64_t addr, uint8_t kind, bool is_write) {
    ++s_oob_count;
#ifdef WINK_MCS51_STRICT
    // Debug/test configuration: fail loudly at the offending access. assert
    // gives the diagnostic in debug builds; std::abort is unconditional so a
    // STRICT build compiled with NDEBUG (assert compiled out) still traps
    // instead of silently falling through to the drop/0xFF release behavior.
    (void)addr;
    (void)kind;
    (void)is_write;
    assert(0 && "XDATA access outside legal aperture (WINK_MCS51_STRICT)");
    std::abort();
#else
    if (kind <= KIND_XSFR && !s_oob_warned[kind]) {
        s_oob_warned[kind] = true;
        const char *what = kind == KIND_WORD ? "XWORD"
                         : kind == KIND_XSFR ? "XSFR" : "XBYTE";
        pal_log_w("MCS51",
                  "XDATA %s %s out of bounds (addr=0x%04llX, legal: [0,%u) and "
                  "[0xF000,0x10000)): %s",
                  what,
                  is_write ? "write" : "read",
                  (unsigned long long)addr, (unsigned)WINK_MCS51_XDATA_SIZE,
                  is_write ? "write dropped" : "returning 0xFF");
    }
#endif
}

}  // namespace

extern "C" {

// Zero-initialised 64 KB xdata space (BSS; no static-init ordering hazard —
// ADR-0070 static-init safety), mirroring wink_mcs51_sfr_shadow.
uint8_t wink_mcs51_xdata_shadow[65536] = {0};

uint8_t wink_mcs51_xdata_read(uint64_t addr, uint8_t kind) {
    wink_mcs51_microstep();
    if (xdata_addr_legal(addr)) {
        return wink_mcs51_xdata_shadow[addr];
    }
    oob_trap(addr, kind, false);
    return 0xFFu;
}

void wink_mcs51_xdata_write(uint64_t addr, uint8_t value, uint8_t kind) {
    wink_mcs51_microstep();
    if (xdata_addr_legal(addr)) {
        wink_mcs51_xdata_shadow[addr] = value;
        return;
    }
    oob_trap(addr, kind, true);
}

uint32_t wink_mcs51_xdata_oob_count(void) {
    return s_oob_count;
}

void wink_mcs51_xdata_reset(void) {
    std::memset(wink_mcs51_xdata_shadow, 0, sizeof(wink_mcs51_xdata_shadow));
    s_oob_count = 0;
    for (uint8_t k = 0; k <= KIND_XSFR; ++k) {
        s_oob_warned[k] = false;
    }
}

}  // extern "C"
