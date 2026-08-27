// SPDX-License-Identifier: Apache-2.0
// M3 shim unit test: intrins.h (_crol_/_cror_/_testbit_) + absacc.h
// (XBYTE/XWORD linear XDATA shadow, aperture OOB semantics R-008) + the
// WINK_MCS51_STRICT dual-mode unsupported-feature mechanism (release path:
// once-per-id warning latch + counters).
//
// Framework-side C++ TU (own main; no REGX51.H — it #defines main). Drives the
// checked accessors and counters directly; microstep charging outside a fiber
// is a safe no-op (wink_mcs51_charge_us returns when no scheduler is ready).
#include <stdint.h>
#include <stdio.h>

#include "mcs51_proxy.hpp"
#include "intrins.h"
#include "absacc.h"
#include "wink_mcs51_strict.h"

namespace {

int g_fails = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        printf("[mcs51] FAIL: %s\n", what);
        ++g_fails;
    }
}

void test_rotates(void) {
    struct Vec { uint8_t v, n, want_l, want_r; };
    static const Vec vecs[] = {
        {0x12, 1, 0x24, 0x09},  // _cror_(0x12,1): 0x09 (bit0 wraps to bit7)
        {0x80, 1, 0x01, 0x40},
        {0xFF, 3, 0xFF, 0xFF},
        {0x01, 0, 0x01, 0x01},  // n=0: identity
        {0x01, 8, 0x01, 0x01},  // n=8 wraps mod 8: identity
        {0x01, 1, 0x02, 0x80},
        {0x90, 4, 0x09, 0x09},  // 0x90 rot by 4 is its own mirror
    };
    for (const Vec& t : vecs) {
        check(_crol_(t.v, t.n) == t.want_l, "_crol_ vector mismatch");
        check(_cror_(t.v, t.n) == t.want_r, "_cror_ vector mismatch");
    }
    // n >= 8 wraps mod 8: _crol_(0x12, 9) == _crol_(0x12, 1).
    check(_crol_(0x12, 9) == _crol_(0x12, 1), "_crol_ n mod 8 wrap");
    check(_cror_(0x12, 10) == _cror_(0x12, 2), "_cror_ n mod 8 wrap");
}

void test_testbit(void) {
    // WinkSbit on SCON.0 (RI, absolute bit address 0x98).
    WinkSbit ri(0x98);
    ri = 1u;
    check(static_cast<uint8_t>(ri) == 1u, "sbit set before _testbit_");
    uint8_t r1 = _testbit_(ri);
    check(r1 == 1u, "_testbit_(sbit) must return old value 1");
    check(static_cast<uint8_t>(ri) == 0u, "_testbit_ must clear the sbit");
    uint8_t r2 = _testbit_(ri);
    check(r2 == 0u, "_testbit_(sbit) on clear bit returns 0");

    // Pointer form: bit 0 of a raw byte.
    uint8_t byte = 0x01u;
    uint8_t r3 = _testbit_(&byte);
    check(r3 == 1u && byte == 0u, "_testbit_(&byte) JBC on bit0");

    // Byte-lvalue form: only bit 0 is tested/cleared (JBC), other bits stay.
    byte = 0x03u;
    uint8_t r4 = _testbit_(byte);
    check(r4 == 1u && byte == 0x02u, "_testbit_(byte) clears only bit0");
    byte = 0x02u;
    uint8_t r5 = _testbit_(byte);
    check(r5 == 0u && byte == 0x02u, "_testbit_(byte) bit0 clear -> 0");
}

void test_xdata(void) {
    wink_mcs51_xdata_reset();

    // Byte write/read at low and high (in-aperture) addresses.
    XBYTE[0x0010] = 0xAB;
    XBYTE[0x1FF0] = 0xCD;  // aperture 8 KB: last 16 bytes still in bounds
    check(static_cast<uint8_t>(XBYTE[0x0010]) == 0xAB, "XBYTE low readback");
    check(static_cast<uint8_t>(XBYTE[0x1FF0]) == 0xCD, "XBYTE high readback");
    check(wink_mcs51_xdata_shadow[0x0010] == 0xAB, "XBYTE low hits shadow");
    check(wink_mcs51_xdata_shadow[0x1FF0] == 0xCD, "XBYTE high hits shadow");

    // RMW lvalue form.
    XBYTE[0x0010] = 0xF0u;
    XBYTE[0x0010] |= 0x0Fu;
    check(static_cast<uint8_t>(XBYTE[0x0010]) == 0xFFu, "XBYTE |= RMW");
    XBYTE[0x0010] &= 0x0Fu;
    check(static_cast<uint8_t>(XBYTE[0x0010]) == 0x0Fu, "XBYTE &= RMW");
    XBYTE[0x0010] ^= 0xFFu;
    check(static_cast<uint8_t>(XBYTE[0x0010]) == 0xF0u, "XBYTE ^= RMW");

    // XWORD 16-bit big-endian: word i occupies bytes 2i (high), 2i+1 (low).
    XWORD[0x0010] = 0x1234;
    check(wink_mcs51_xdata_shadow[0x0020] == 0x12u, "XWORD high byte at 2i");
    check(wink_mcs51_xdata_shadow[0x0021] == 0x34u, "XWORD low byte at 2i+1");
    check(static_cast<uint16_t>(XWORD[0x0010]) == 0x1234u, "XWORD readback BE");
    XWORD[0x0FFF] = 0xBEEF;  // bytes 0x1FFE/0x1FFF: last in-aperture word
    check(static_cast<uint16_t>(XWORD[0x0FFF]) == 0xBEEFu,
          "XWORD high-address readback");
    XWORD[0x0020] = 0x00F0;
    XWORD[0x0020] |= 0x0F00;
    check(static_cast<uint16_t>(XWORD[0x0020]) == 0x0FF0u, "XWORD |= RMW");

    // OOB (R-008): write dropped, read returns 0xFF, counter advances, shadow
    // beyond the aperture is never touched.
    uint32_t oob_before = wink_mcs51_xdata_oob_count();
    XBYTE[0x8000] = 0x5A;  // >= 8 KB aperture
    check(wink_mcs51_xdata_shadow[0x8000] == 0u, "OOB write must not hit shadow");
    check(static_cast<uint8_t>(XBYTE[0x8000]) == 0xFFu, "OOB read returns 0xFF");
    check(static_cast<uint16_t>(XWORD[0x4000]) == 0xFFFFu,
          "OOB XWORD read returns 0xFFFF");
    check(wink_mcs51_xdata_oob_count() >= oob_before + 4u,
          "OOB access counter advances");
}

void test_unsupported(void) {
    wink_mcs51_unsupported_reset();

    wink_mcs51_unsupported(MCS51_FEAT_PSW_FLAGS, "PSW arithmetic flags");
    wink_mcs51_unsupported(MCS51_FEAT_PSW_FLAGS, "PSW arithmetic flags");
    check(wink_mcs51_unsupported_trigger_count(MCS51_FEAT_PSW_FLAGS) == 2u,
          "unsupported trigger count increments per call");
    check(wink_mcs51_unsupported_warning_count() == 1u,
          "unsupported warning latches once per feature id");

    wink_mcs51_unsupported(MCS51_FEAT_INLINE_ASM, "inline asm");
    check(wink_mcs51_unsupported_warning_count() == 2u,
          "second feature id warns once more");
    check(wink_mcs51_unsupported_trigger_count(MCS51_FEAT_INLINE_ASM) == 1u,
          "second feature id trigger count");

    wink_mcs51_unsupported_reset();
    check(wink_mcs51_unsupported_warning_count() == 0u &&
          wink_mcs51_unsupported_trigger_count(MCS51_FEAT_PSW_FLAGS) == 0u,
          "unsupported reset zeroes counters");
}

}  // namespace

// The bridge TU (pulled in via the compat lib) references the user entry; this
// test drives the shims directly, so an empty definition closes the link.
extern "C" void wink_mcs51_user_main(void) {}

extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

int main(void) {
    test_rotates();
    test_testbit();
    test_xdata();
    test_unsupported();

    if (g_fails != 0) {
        printf("[mcs51] FAIL: shims test had %d failures\n", g_fails);
        return 1;
    }
    printf("[mcs51] PASS: intrins _crol_/_cror_/_testbit_, absacc "
           "XBYTE/XWORD aperture + R-008 OOB, STRICT unsupported latch\n");
    return 0;
}
