// SPDX-License-Identifier: Apache-2.0
// M4 data-plane unit test 3: full WinkSfr/WinkSbit operator algebra coverage
// (data-plane SSOT §7.3). Every compound assignment / increment / shift a Keil
// program can emit (ORL/ANL/XRL/INC/DEC/RL/RR-class) must compile, read the
// LATCH for RMW forms, reconstruct pins for plain reads, and land in the
// shadow with byte semantics (mod 256 wrap).
#include <stdint.h>
#include <stdio.h>

#include "mcs51_proxy.hpp"

namespace {

int g_fails = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        printf("[mcs51] FAIL: %s\n", what);
        ++g_fails;
    }
}

// Shadow helper (read the latch directly, bypassing Read-Pin reconstruction).
uint8_t& latch(uint8_t addr) {
    return wink_mcs51_sfr_shadow[addr];
}

void test_compound_assignments() {
    mcs51_trap_reset();
    WinkSfr P1(0x90);

    P1 = 0x10;
    P1 |= 0x01;  check(latch(0x90) == 0x11u, "P1 |= 0x01");
    P1 &= ~0x10u; check(latch(0x90) == 0x01u, "P1 &= ~0x10");
    P1 ^= 0x0F;  check(latch(0x90) == 0x0Eu, "P1 ^= 0x0F");
    P1 += 0xF0;  check(latch(0x90) == 0xFEu, "P1 += wraps mod 256");
    P1 -= 0xFF;  check(latch(0x90) == 0xFFu, "P1 -= wraps mod 256 (0xFE-0xFF)");

    P1 = 0x01;
    P1 <<= 3;    check(latch(0x90) == 0x08u, "P1 <<= 3");
    P1 >>= 2;    check(latch(0x90) == 0x02u, "P1 >>= 2");
    P1 <<= 7;    check(latch(0x90) == 0x00u, "P1 <<= 7 shifts out (0x02<<7)");

    // RMW reads the latch even with a Read-Pin trap forcing a pin low.
    P1 = 0xFF;
    latch(0x90) = 0xFF;
    mcs51_trap_register_read(1, 0,
        [](void*) -> uint8_t { return 0u; }, nullptr);  // P1.0 externally low
    P1 &= ~0x02u;  // clear bit1 only
    check(latch(0x90) == 0xFDu,
          "RMW &= under external-low pin reads latch (0xFD, bit0 stays 1)");
    P1 |= 0x02;
    check(latch(0x90) == 0xFFu, "RMW |= restores bit1, latch intact");
    mcs51_trap_clear_pin(1, 0);
}

void test_increment_decrement() {
    mcs51_trap_reset();
    WinkSfr P2(0xA0);

    P2 = 0x10;
    ++P2;                     check(latch(0xA0) == 0x11u, "prefix ++P2");
    uint8_t old = P2++;       check(old == 0x11u && latch(0xA0) == 0x12u,
                                   "postfix P2++ returns old, stores new");
    --P2;                     check(latch(0xA0) == 0x11u, "prefix --P2");
    old = P2--;               check(old == 0x11u && latch(0xA0) == 0x10u,
                                   "postfix P2-- returns old, stores new");

    P2 = 0xFF;
    ++P2;                     check(latch(0xA0) == 0x00u, "++P2 wraps 0xFF->0x00");
    P2 = 0x00;
    --P2;                     check(latch(0xA0) == 0xFFu, "--P2 wraps 0x00->0xFF");
}

void test_sbit_operators() {
    mcs51_trap_reset();
    WinkSfr P3(0xB0);
    P3 = 0x00;

    WinkSbit led(P3 ^ 2);
    led = 1;                 check((latch(0xB0) & 0x04u) == 0x04u, "sbit = 1 sets bit");
    led = 0;                 check((latch(0xB0) & 0x04u) == 0x00u, "sbit = 0 clears bit");
    led ^= 1u;               check((latch(0xB0) & 0x04u) == 0x04u, "sbit ^= 1 toggles low");
    led ^= 1u;               check((latch(0xB0) & 0x04u) == 0x00u, "sbit ^= 1 toggles back");
    led = 1;
    led |= 0u;               check((latch(0xB0) & 0x04u) == 0x04u, "sbit |= 0 holds");
    led &= 1u;               check((latch(0xB0) & 0x04u) == 0x04u, "sbit &= 1 holds");
    led &= 0u;               check((latch(0xB0) & 0x04u) == 0x00u, "sbit &= 0 clears");

    // sbit copy: source read goes through Read-Pin; sink write dispatches.
    P3 = 0x00;
    WinkSbit src(P3 ^ 0);
    WinkSbit dst(P3 ^ 1);
    src = 1;
    dst = src;               check((latch(0xB0) & 0x02u) == 0x02u,
                                   "sbit = sbit copies level");
    P3 = 0xFF;
    WinkSfr P1(0x90);
    P1 = P3;                 // port-to-port copy reads P3 pins (=0xFF latch)
    check(latch(0x90) == 0xFFu, "WinkSfr copy assignment reads source port");
}

void test_absolute_sbit_forms() {
    mcs51_trap_reset();
    // Predefined-register absolute bit address form (REGX52.H style):
    // 0x93 = P1.3; the ctor must derive SFR addr 0x90, port 1, bit 3.
    WinkSbit p1_3(0x93);
    check(p1_3.addr == 0x90u && p1_3.port == 1u && p1_3.bit == 3u,
          "absolute sbit 0x93 -> addr 0x90, port 1, bit 3");
    WinkSbit tf0(0x8D);      // TCON.5
    check(tf0.addr == 0x88u && tf0.port == 0xFFu && tf0.bit == 5u,
          "absolute sbit 0x8D -> TCON addr 0x88, non-GPIO port 0xFF, bit 5");

    WinkSfr P1(0x90);
    P1 = 0x00;
    p1_3 = 1;
    check(latch(0x90) == 0x08u, "absolute-form sbit write lands in P1 bit3");

    // Non-GPIO extended SFR (vendor SFR): port 0xFF, plain latch shadow.
    WinkSfr auxr(0x8E);
    check(auxr.port == 0xFFu, "vendor SFR 0x8E maps to non-GPIO port 0xFF");
    auxr = 0x40;
    auxr |= 0x01;
    check(latch(0x8E) == 0x41u, "vendor SFR RMW works on latch shadow");
}

}  // namespace

extern "C" void wink_mcs51_user_main(void) {}
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

int main(void) {
    test_compound_assignments();
    test_increment_decrement();
    test_sbit_operators();
    test_absolute_sbit_forms();

    if (g_fails != 0) {
        printf("[mcs51] FAIL: operators coverage test had %d failures\n",
               g_fails);
        return 1;
    }
    printf("[mcs51] PASS: SFR operator algebra — |= &= ^= += -= <<= >>=, "
           "pre/post ++/--, sbit ops, copy assign, absolute sbit, vendor SFR\n");
    return 0;
}
