// SPDX-License-Identifier: Apache-2.0
// M4 ADC0832 unit test (timing SSOT §6.1, B1 / AD-15): 3-wire DIO shared
// state machine, leading-null alignment, bus-release absorption, and the
// requirement that BOTH Keil drive styles — sbit toggles and whole-port RMW
// (P1 |= / P1 &=) — close the loop and read back the same injected code
// value. Also covers 4-wire (separate DI/DO) and CH0/CH1 mux selection.
//
// Bit-bang sequence (read-after-falling idiom: DO presents the next bit on the
// CLK falling edge, MCU reads while CLK is low / raises CLK next):
//   CS fall; 3 config bits sampled on CLK rises (start=1, SGL/DIF, ODD/SIGN);
//   then 8 CLK falls present MSB..LSB; CS rise aborts to idle.
#include <stdint.h>
#include <stdio.h>

#include "ADC0832.H"
#include "mcs51_adc.h"
#include "mcs51_proxy.hpp"

namespace {

int g_fails = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        printf("[mcs51] FAIL: %s\n", what);
        ++g_fails;
    }
}

// Pin wiring for the test: CS=P1.2, CLK=P1.1, DIO=P1.0; 4-wire DO=P1.4.
constexpr uint8_t P1_ADDR = 0x90;
constexpr uint8_t BIT_CS  = 0x04;
constexpr uint8_t BIT_CLK = 0x02;
constexpr uint8_t BIT_DIO = 0x01;

void reset_pins() {
    wink_mcs51_sfr_shadow[P1_ADDR] = 0xFF;  // idle: CS=1, CLK=1, DIO high
}

// Drive style A: sbit toggles (the classic Keil bit-bang).
uint8_t adc_read_sbit(uint8_t channel) {
    mcs51_trap_reset();
    mcs51_adc0832_init(/*CS*/1,2, /*CLK*/1,1, /*DI*/1,0, /*DO*/1,0);
    reset_pins();

    WinkSfr PORT(P1_ADDR);
    WinkSbit CS(PORT ^ 2), CLK(PORT ^ 1), DIO(PORT ^ 0);

    CS = 0;            // CS fall -> PHASE_INPUT
    CLK = 0;
    DIO = 1;  CLK = 1; CLK = 0;   // rise 1: Start bit = 1
    DIO = 1;  CLK = 1; CLK = 0;   // rise 2: SGL/DIF = 1 (single-ended)
    DIO = channel ? 1u : 0u;      // ODD/SIGN = channel select
    CLK = 1;                      // rise 3: latch channel -> PHASE_OUTPUT
    DIO = 1;                      // bus release (absorbed by on_write)

    uint8_t value = 0;
    for (int i = 0; i < 8; ++i) {
        CLK = 0;                  // falling: DO presents next bit (fall#3=MSB)
        uint8_t b = DIO;          // Read-Pin reconstruction
        value = (uint8_t)((value << 1) | (b & 1u));
        CLK = 1;
    }
    CS = 1;            // CS rise -> IDLE
    return value;
}

// Drive style B: whole-port RMW writes (`P1 |= 0x02; P1 &= ~0x02;`).
uint8_t adc_read_rmw(uint8_t channel) {
    mcs51_trap_reset();
    mcs51_adc0832_init(1,2, 1,1, 1,0, 1,0);
    reset_pins();

    WinkSfr PORT(P1_ADDR);
    PORT &= (uint8_t)~BIT_CS;                 // CS fall
    PORT &= (uint8_t)~BIT_CLK;                // CLK = 0
    PORT |= BIT_DIO;  PORT |= BIT_CLK;  PORT &= (uint8_t)~BIT_CLK;  // start=1
    PORT |= BIT_DIO;  PORT |= BIT_CLK;  PORT &= (uint8_t)~BIT_CLK;  // SGL=1
    if (channel) { PORT |= BIT_DIO; } else { PORT &= (uint8_t)~BIT_DIO; }
    PORT |= BIT_CLK;                                           // rise 3
    PORT |= BIT_DIO;                                           // release

    uint8_t value = 0;
    for (int i = 0; i < 8; ++i) {
        PORT &= (uint8_t)~BIT_CLK;            // falling edge
        uint8_t pins = static_cast<uint8_t>(PORT);  // whole-port Read-Pin
        uint8_t b = (uint8_t)((pins & BIT_DIO) ? 1u : 0u);
        value = (uint8_t)((value << 1) | b);
        PORT |= BIT_CLK;
    }
    PORT |= BIT_CS;
    return value;
}

// 4-wire mode: DI on P1.0, DO on P1.4 (separate pins).
uint8_t adc_read_4wire(uint8_t channel) {
    mcs51_trap_reset();
    mcs51_adc0832_init(/*CS*/1,2, /*CLK*/1,1, /*DI*/1,0, /*DO*/1,4);
    reset_pins();

    WinkSfr PORT(P1_ADDR);
    WinkSbit CS(PORT ^ 2), CLK(PORT ^ 1), DI(PORT ^ 0), DO(PORT ^ 4);

    CS = 0;
    CLK = 0;
    DI = 1;  CLK = 1; CLK = 0;
    DI = 1;  CLK = 1; CLK = 0;
    DI = channel ? 1u : 0u;
    CLK = 1;

    uint8_t value = 0;
    for (int i = 0; i < 8; ++i) {
        CLK = 0;
        value = (uint8_t)((value << 1) | (static_cast<uint8_t>(DO) & 1u));
        CLK = 1;
    }
    CS = 1;
    return value;
}

void test_both_styles_match() {
    // MSB-first patterns exercising every bit position and a value with MSB 0.
    static const uint8_t patterns[] = {0xA5, 0x5A, 0xFF, 0x00, 0x81, 0x7E};
    for (uint8_t ch = 0; ch < 2; ++ch) {
        for (uint8_t want : patterns) {
            mcs51_adc0832_set_value(ch, want);
            uint8_t a = adc_read_sbit(ch);
            mcs51_adc0832_set_value(ch, want);
            uint8_t b = adc_read_rmw(ch);
            mcs51_adc0832_set_value(ch, want);
            uint8_t c = adc_read_4wire(ch);
            check(a == want, "sbit drive reads injected value");
            check(b == want, "whole-port RMW drive reads injected value");
            check(c == want, "4-wire DO reads injected value");
            if (a != want || b != want || c != want) {
                printf("[mcs51]   ch%u want=0x%02X sbit=0x%02X rmw=0x%02X "
                       "4wire=0x%02X\n", (unsigned)ch, (unsigned)want,
                       (unsigned)a, (unsigned)b, (unsigned)c);
            }
        }
    }
}

void test_channel_mux() {
    mcs51_adc0832_set_value(0, 0x3C);
    mcs51_adc0832_set_value(1, 0xC3);
    check(adc_read_sbit(0) == 0x3Cu, "CH0 select returns CH0 value");
    check(adc_read_sbit(1) == 0xC3u, "CH1 select returns CH1 value");
}

void test_bus_release_absorbed() {
    // During PHASE_OUTPUT the MCU's DIO=1 release write must not disturb the
    // driven bit: drive a value with MSB 0 (0x40) — the first data read must
    // be 0 even though the latch bit was written 1 for bus release.
    mcs51_adc0832_set_value(0, 0x40);
    uint8_t v = adc_read_sbit(0);
    check(v == 0x40u, "bus-release DIO=1 absorbed (MSB 0 read correctly)");
}

void test_protocol_abort() {
    // Start bit must be 1: a 0 on the first rising edge aborts to IDLE, and
    // the read-back must NOT be the injected value.
    mcs51_trap_reset();
    mcs51_adc0832_set_value(0, 0x96);
    mcs51_adc0832_init(1,2, 1,1, 1,0, 1,0);
    reset_pins();

    WinkSfr PORT(P1_ADDR);
    WinkSbit CS(PORT ^ 2), CLK(PORT ^ 1), DIO(PORT ^ 0);
    CS = 0; CLK = 0;
    DIO = 0;  CLK = 1; CLK = 0;   // rise 1: start bit = 0 -> abort to IDLE
    DIO = 1;  CLK = 1; CLK = 0;
    DIO = 0;  CLK = 1;

    uint8_t value = 0;
    for (int i = 0; i < 8; ++i) {
        CLK = 0;
        value = (uint8_t)((value << 1) | (static_cast<uint8_t>(DIO) & 1u));
        CLK = 1;
    }
    check(value == 0xFFu, "bad start bit aborts: DO reads released-high 0xFF");
}

}  // namespace

extern "C" void wink_mcs51_user_main(void) {}
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

int main(void) {
    test_both_styles_match();
    test_channel_mux();
    test_bus_release_absorbed();
    test_protocol_abort();

    if (g_fails != 0) {
        printf("[mcs51] FAIL: ADC0832 DIO test had %d failures\n", g_fails);
        return 1;
    }
    printf("[mcs51] PASS: ADC0832 3-wire DIO — sbit+RMW drive parity, CH0/CH1 "
           "mux, null-bit alignment, bus-release absorb, start-bit abort\n");
    return 0;
}
