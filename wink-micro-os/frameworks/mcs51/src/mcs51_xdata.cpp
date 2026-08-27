// SPDX-License-Identifier: Apache-2.0
// MCS-51 XDATA shadow + bounds-checked absolute access (M3, R-008).
//
// See absacc.h. A 64 KB linear xdata shadow (BSS) backs XBYTE/XWORD; only the
// first WINK_MCS51_XDATA_SIZE bytes form the legal aperture. Each checked
// access charges one interception microstep — the same interception rationale
// as the SFR proxy, so a tight `while(XBYTE[f] != x) {}` poll advances virtual
// time and yields the fiber. Out-of-bounds accesses (R-008): STRICT asserts;
// release warns once per access kind (XBYTE vs XWORD), drops writes, and
// returns 0xFF for reads.
//
// M4 hook note: an external-xdata-peripheral write hook would attach here,
// before the shadow store, trapping writes to externally-mapped addresses.
#include "absacc.h"

#include "wink_mcs51_clock.h"

#include <cassert>
#include <cstdint>
#include <cstring>

#ifndef WINK_MCS51_STRICT
#include "pal_log.h"
#endif

namespace {

constexpr uint8_t KIND_BYTE = 0u;  // XBYTE accessor
constexpr uint8_t KIND_WORD = 1u;  // XWORD accessor

bool     s_oob_warned[2] = {};   // once-per-kind warning latch
uint32_t s_oob_count = 0;

void oob_trap(uint32_t addr, uint8_t kind, bool is_write) {
    ++s_oob_count;
#ifdef WINK_MCS51_STRICT
    // Debug/test configuration: fail loudly at the offending access.
    assert(0 && "XDATA access outside legal aperture (WINK_MCS51_STRICT)");
#else
    if (kind <= KIND_WORD && !s_oob_warned[kind]) {
        s_oob_warned[kind] = true;
        pal_log_w("MCS51",
                  "XDATA %s %s out of bounds (addr=0x%04lX, aperture=%u): %s",
                  kind == KIND_WORD ? "XWORD" : "XBYTE",
                  is_write ? "write" : "read",
                  (unsigned long)addr, (unsigned)WINK_MCS51_XDATA_SIZE,
                  is_write ? "write dropped" : "returning 0xFF");
    }
#endif
}

}  // namespace

extern "C" {

// Zero-initialised 64 KB xdata space (BSS; no static-init ordering hazard —
// ADR-0070 static-init safety), mirroring wink_mcs51_sfr_shadow.
uint8_t wink_mcs51_xdata_shadow[65536] = {0};

uint8_t wink_mcs51_xdata_read(uint32_t addr, uint8_t kind) {
    wink_mcs51_microstep();
    if (addr < WINK_MCS51_XDATA_SIZE) {
        return wink_mcs51_xdata_shadow[addr];
    }
    oob_trap(addr, kind, false);
    return 0xFFu;
}

void wink_mcs51_xdata_write(uint32_t addr, uint8_t value, uint8_t kind) {
    wink_mcs51_microstep();
    if (addr < WINK_MCS51_XDATA_SIZE) {
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
    s_oob_warned[0] = false;
    s_oob_warned[1] = false;
}

}  // extern "C"
