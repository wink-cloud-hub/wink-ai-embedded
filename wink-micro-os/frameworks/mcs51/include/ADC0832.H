// SPDX-License-Identifier: Apache-2.0
// ADC0832 external 8-bit SAR ADC — virtual instant peripheral (AD-15).
//
// Keil-style header name (user .c projects often ship their own ADC0832.H with
// sbit pin definitions; the interception layer itself needs no user changes).
// This header declares the simulation-side state machine init, called once
// from the framework board init (mcs51_board_config.h pin constants) or from
// tests. C-safe (boundary ③ C ABI).
//
// Wiring modes (auto-detected):
//   * 3-wire DIO: DI and DO physically tied to one MCU pin (di == do). The DIO
//     pin gets BOTH an on_write trap (absorbs the MCU bus-release write during
//     the output phase) and an on_read trap (drives the converted bit back).
//   * 4-wire: DI and DO on separate pins.
//
// Timing model (instruction-edge driven, 0 us conversion — trap red lines):
//   CS fall (1->0)            -> PHASE_INPUT, counters reset
//   CLK rise #1               -> sample Start bit (must be 1, else IDLE)
//   CLK rise #2               -> sample SGL/DIF
//   CLK rise #3               -> sample ODD/SIGN, latch channel, PHASE_OUTPUT;
//                                DO enters the leading-Null window (drives 0)
//   CLK fall #3               -> MSB (bit 7) driven
//   CLK fall #4 .. #10        -> bits 6..0 driven
//   CLK fall #11              -> bus release (DO reads 1)
//   CS rise (0->1)            -> IDLE
// The Null bit occupies the half-cycle between the 3rd rising and 3rd falling
// edge (in 3-wire mode the MCU is driving DIO itself in that window, so it is
// never observed); MSB is valid from falling #3, which aligns BOTH canonical
// Keil bit-bang idioms — read-while-CLK-high (8 reads over clocks 4..11) and
// read-after-falling (8 reads after falls #3..#10) — with no off-by-one.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Bind an ADC0832 instance to MCU pins and register its Level-2 traps.
// port: 0=P0 … 3=P3; bit: 0..7. Out-of-range pins are ignored. Re-calling
// re-binds (test isolation: mcs51_trap_reset() clears all traps first).
void mcs51_adc0832_init(uint8_t cs_port,  uint8_t cs_bit,
                        uint8_t clk_port, uint8_t clk_bit,
                        uint8_t di_port,  uint8_t di_bit,
                        uint8_t do_port,  uint8_t do_bit);

#ifdef __cplusplus
}
#endif
