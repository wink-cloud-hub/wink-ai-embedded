// SPDX-License-Identifier: Apache-2.0
// M4 data-plane unit test 2: diff edge-dispatch accuracy (data-plane SSOT
// §7.2) and the linear pin map global_pin = (port<<3)|bit (§4.4).
//
// Pins are imagined wired as ADC0832-style bit-bang lines: P1.0=DI, P1.1=CLK,
// P1.2=CS. A whole-port write that changes only DI and CS must not fire the
// CLK trap (a false clock edge would advance an uninvolved state machine); a
// later RMW that only raises CLK must fire CLK alone. Separately, every pin
// edge must reach js_pal_gpio_write with the exact flattened linear pin id,
// and repeat writes (diff == 0) must fire nothing — the Zero False-Trigger
// guarantee at both dispatch sinks (pin traps + channel-1 notifications).
#include <stdint.h>
#include <stdio.h>

#include "mcs51_proxy.hpp"

extern "C" {
// Host channel-1 fallback observability (mcs51_uni_bridge.cpp).
uint32_t wink_mcs51_host_gpio_notify_count(void);
void     wink_mcs51_host_gpio_notify_reset(void);
uint16_t wink_mcs51_host_gpio_notify_pin(uint32_t i);
uint8_t  wink_mcs51_host_gpio_notify_level(uint32_t i);
// ADR-0077: quasi-bidirectional drive strength per notification
// (1 = WEAK weak pull-up on a rising latch edge, 3 = SUPPLY strong NMOS
// pull-down on a falling edge; identity-maps the host DriveStrength enum).
uint8_t  wink_mcs51_host_gpio_notify_strength(uint32_t i);
}

namespace {

int g_fails = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        printf("[mcs51] FAIL: %s\n", what);
        ++g_fails;
    }
}

// Edge counters for the three imagined peripheral pins.
uint32_t s_di_edges, s_clk_edges, s_cs_edges;

void count_di(void* ctx, uint8_t level)  { (void)ctx; (void)level; ++s_di_edges; }
void count_clk(void* ctx, uint8_t level) { (void)ctx; (void)level; ++s_clk_edges; }
void count_cs(void* ctx, uint8_t level)  { (void)ctx; (void)level; ++s_cs_edges; }

void test_pin_trap_dispatch() {
    mcs51_trap_reset();
    s_di_edges = s_clk_edges = s_cs_edges = 0;

    mcs51_trap_register_write(1, 0, &count_di, nullptr);
    mcs51_trap_register_write(1, 1, &count_clk, nullptr);
    mcs51_trap_register_write(1, 2, &count_cs, nullptr);

    WinkSfr P1(0x90);
    P1 = 0x00;  // baseline: 0->1 edges ignored (start state)
    s_di_edges = s_clk_edges = s_cs_edges = 0;

    // P1 = 0x05: DI(bit0)=1, CS(bit2)=1, CLK(bit1) stays 0.
    P1 = 0x05;
    check(s_di_edges == 1u && s_cs_edges == 1u,
          "P1=0x05 fires DI + CS exactly once each");
    check(s_clk_edges == 0u,
          "P1=0x05 must NOT fire CLK (zero false clock edge)");

    // P1 |= 0x02: only CLK rises.
    P1 |= 0x02;
    check(s_clk_edges == 1u, "P1|=0x02 fires CLK exactly once");
    check(s_di_edges == 1u && s_cs_edges == 1u,
          "P1|=0x02 leaves DI/CS edge counts unchanged");

    // P1 = 0x07 then P1 = 0x07: zero-delta repeat fires nothing.
    P1 = 0x07;
    uint32_t di = s_di_edges, clk = s_clk_edges, cs = s_cs_edges;
    P1 = 0x07;
    check(s_di_edges == di && s_clk_edges == clk && s_cs_edges == cs,
          "zero-delta whole-port write fires no pin trap (fast path)");

    // sbit form toggles only its own pin.
    WinkSbit clk_bit(P1 ^ 1);
    clk_bit = 0;
    check(s_clk_edges == clk + 1u, "sbit CLK=0 fires CLK once");
    check(s_di_edges == di && s_cs_edges == cs,
          "sbit CLK write leaves DI/CS quiet");
}

void test_linear_pin_map() {
    mcs51_trap_reset();
    wink_mcs51_host_gpio_notify_reset();

    WinkSfr P0(0x80), P1(0x90), P2(0xA0), P3(0xB0);
    P0 = 0x00; P1 = 0x00; P2 = 0x00; P3 = 0x00;
    wink_mcs51_host_gpio_notify_reset();

    // One bit per port, low->high: global pins 0 (P0.0), 8 (P1.0),
    // 16 (P2.0), 24 (P3.0).
    P0 = 0x01;
    P1 = 0x01;
    P2 = 0x01;
    P3 = 0x01;
    check(wink_mcs51_host_gpio_notify_count() == 4u,
          "one notification per rising port edge");
    check(wink_mcs51_host_gpio_notify_pin(0) == 0u
          && wink_mcs51_host_gpio_notify_level(0) == 1u,
          "P0.0 -> global_pin 0 high");
    check(wink_mcs51_host_gpio_notify_pin(1) == 8u,
          "P1.0 -> global_pin 8");
    check(wink_mcs51_host_gpio_notify_pin(2) == 16u,
          "P2.0 -> global_pin 16");
    check(wink_mcs51_host_gpio_notify_pin(3) == 24u,
          "P3.0 -> global_pin 24");
    // ADR-0077: a rising latch edge enables the weak internal pull-up.
    for (uint32_t i = 0; i < 4u; ++i) {
        check(wink_mcs51_host_gpio_notify_strength(i) == 1u,
              "rising latch edge reports WEAK (1) drive strength");
    }

    // P2 = 0x80: bit0 falls 1->0 (strong NMOS pull-down, SUPPLY) while bit7
    // rises 0->1 (weak pull-up, WEAK) — two notifications, bit order b=0..7.
    P2 = 0x80;
    uint32_t n = wink_mcs51_host_gpio_notify_count();
    check(n == 6u, "P2=0x80 fires two edges (bit0 fall + bit7 rise)");
    check(wink_mcs51_host_gpio_notify_pin(4) == 16u
          && wink_mcs51_host_gpio_notify_level(4) == 0u
          && wink_mcs51_host_gpio_notify_strength(4) == 3u,
          "P2.0 falling edge reports SUPPLY (3) drive strength");
    check(wink_mcs51_host_gpio_notify_pin(5) == 23u
          && wink_mcs51_host_gpio_notify_level(5) == 1u
          && wink_mcs51_host_gpio_notify_strength(5) == 1u,
          "P2.7 rising edge reports WEAK (1) drive strength");
    P2 = 0x00;
    check(wink_mcs51_host_gpio_notify_count() == n + 1u,
          "P2 falling edge notifies once");
    uint32_t last = wink_mcs51_host_gpio_notify_count() - 1u;
    check(wink_mcs51_host_gpio_notify_pin(last) == 23u
          && wink_mcs51_host_gpio_notify_level(last) == 0u
          && wink_mcs51_host_gpio_notify_strength(last) == 3u,
          "P2.7 -> global_pin 23, falling level 0, SUPPLY (3)");

    // sbit path uses the same linear map: P3.7 = 31.
    WinkSbit p3_7(P3 ^ 7);
    p3_7 = 1;
    last = wink_mcs51_host_gpio_notify_count() - 1u;
    check(wink_mcs51_host_gpio_notify_pin(last) == 31u
          && wink_mcs51_host_gpio_notify_level(last) == 1u
          && wink_mcs51_host_gpio_notify_strength(last) == 1u,
          "sbit P3.7 -> global_pin 31 rising, WEAK (1)");

    // Non-GPIO SFR (TCON 0x88) never reaches the channel-1 GPIO sink.
    uint32_t before = wink_mcs51_host_gpio_notify_count();
    WinkSfr TCON(0x88);
    TCON = 0x10;
    TCON = 0x00;
    check(wink_mcs51_host_gpio_notify_count() == before,
          "control-SFR writes never emit js_pal_gpio_write");
}

}  // namespace

extern "C" void wink_mcs51_user_main(void) {}
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

int main(void) {
    test_pin_trap_dispatch();
    test_linear_pin_map();

    if (g_fails != 0) {
        printf("[mcs51] FAIL: edge dispatch test had %d failures\n", g_fails);
        return 1;
    }
    printf("[mcs51] PASS: SFR edge dispatch — diff=old^val Zero False-Trigger, "
           "linear pin map (port<<3)|bit, sbit+whole-port parity\n");
    return 0;
}
