// SPDX-License-Identifier: Apache-2.0
// M4 data-plane unit test 1: RMW Read-Latch integrity (data-plane SSOT §7.1).
//
// A whole-port RMW instruction (`P1 |= …`, `P1 &= …`) MUST read the port LATCH,
// never the pins. An external peripheral holding a pin low (button, sensor)
// must not have that low level written back into the latch bit — on real
// quasi-bidirectional 8051 ports that turns on the low-drive FET and locks the
// pin low forever. This test reconstructs an external low on P1.3 through an
// on_read trap, performs RMW operations, removes the trap, and asserts the
// latch bit survived.
//
// Framework-side C++ TU (own main; no REGX52.H — it #defines main). Microstep
// charging outside a fiber is a safe no-op.
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

uint8_t read_external_low(void* ctx) {
    (void)ctx;
    return 0u;  // external button/sensor holding the pin low
}

void test_rmw_latch_integrity() {
    mcs51_trap_reset();

    WinkSfr P1(0x90);
    P1 = 0xFF;  // all latch bits high (quasi-bidirectional input-ready state)

    // External low on P1.3, visible only through Read-Pin reconstruction.
    mcs51_trap_register_read(/*port*/ 1, /*bit*/ 3, &read_external_low, nullptr);

    uint8_t pin_view = static_cast<uint8_t>(P1);
    check(pin_view == 0xF7u, "whole-port Read-Pin reconstructs P1.3 low (0xF7)");
    check((pin_view & 0x08u) == 0u, "P1.3 reads low while externally held");

    // RMW set: reads LATCH 0xFF, result 0xFF — the low pin must not sneak in.
    P1 |= 0x01;
    // RMW clear on another bit: latch 0xFF -> 0xFE, bit3 must stay 1.
    P1 &= ~0x01u;

    // Release the external low: the latch bit must read high again.
    mcs51_trap_clear_pin(1, 3);

    uint8_t after = static_cast<uint8_t>(P1);
    check(after == 0xFEu, "latch after RMW is 0xFE (bit3 never corrupted)");
    check((after & 0x08u) == 0x08u,
          "P1.3 latch stayed 1 through externally-low RMW (no FET lock-up)");
}

void test_rmw_sbit_latch_integrity() {
    // Bit-level RMW (CPL/ORL/ANL bit class) likewise reads the latch: a
    // toggled neighbouring sbit must not resolve through the pin trap.
    mcs51_trap_reset();

    WinkSfr P1(0x90);
    P1 = 0xFF;
    mcs51_trap_register_read(1, 3, &read_external_low, nullptr);

    WinkSbit led(P1 ^ 0);  // relative form, carries port index 1
    check(static_cast<uint8_t>(led) == 1u, "P1.0 reads high pre-toggle");
    led = 0;
    led = 1;  // two edges on bit0 only
    led ^= 1u;  // CPL bit: Read-Latch of bit0 (1 -> 0)

    mcs51_trap_clear_pin(1, 3);
    check(static_cast<uint8_t>(P1) == 0xFEu,
          "sbit RMW leaves P1.3 latch high (0xFE)");
}

}  // namespace

extern "C" void wink_mcs51_user_main(void) {}
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

int main(void) {
    test_rmw_latch_integrity();
    test_rmw_sbit_latch_integrity();

    if (g_fails != 0) {
        printf("[mcs51] FAIL: RMW latch integrity test had %d failures\n",
               g_fails);
        return 1;
    }
    printf("[mcs51] PASS: SFR RMW latch integrity — external-low pin never "
           "written back into latch (whole-port + sbit RMW)\n");
    return 0;
}
