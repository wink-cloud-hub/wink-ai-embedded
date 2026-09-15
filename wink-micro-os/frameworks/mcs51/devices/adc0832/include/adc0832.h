// SPDX-License-Identifier: LGPL-3.0-only
// ADC0832 external 8-bit SAR ADC — virtual instant peripheral (AD-15).
//
// Board-level device package (Stage3 S3-2 Step 3, decision B): fully
// independent of chip families. Renamed from the Keil-style ADC0832.H to
// the canonical lowercase name (Linux CI is case-sensitive); the old path
// keeps a one-stage forwarding shim (deleted in stage7).
//
// This header declares the simulation-side state machine attach, called from
// the board's own post-init hook (mcs51_framework_set_post_init_hook) or
// from tests. C-safe (boundary ③ C ABI).
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
// edge (in 3-wire mode the MCU is still driving DIO itself here, so it is
// never observed); MSB is valid from falling #3, which aligns BOTH canonical
// Keil bit-bang idioms — read-while-CLK-high (8 reads over clocks 4..11) and
// read-after-falling (8 reads after falls #3..#10) — with no off-by-one.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Mcu51Context;

// Default board-channel rail keys (Stage1 dual-space partition): the board
// fabric owns keys 32~63; an ADC0832 channel with no explicit net binding
// keeps its historical 32+ch key, so frontends driving channel 32 and all
// existing tests observe zero behavior change.
#define ADC0832_DEFAULT_CH0_NET_ID 32u
#define ADC0832_DEFAULT_CH1_NET_ID 33u

// Per-instance trap state machine (S2-1, scheme A) lives in the device TU
// (mcs51_adc0832.cpp, BSS pool indexed by instance_index) — S3-H5: internal
// run state is hidden from this public header (interface segregation; no
// external TU names it). The pool slot layout is a TU-private detail.

// Attach configuration (POD, passed by pointer — keeps the API at 2 params).
typedef struct {
    uint8_t cs_port, cs_bit;
    uint8_t clk_port, clk_bit;
    uint8_t di_port, di_bit;
    uint8_t do_port, do_bit;
    uint8_t ch0_net_id;    // board-channel rail key for CH0
    uint8_t ch1_net_id;    // board-channel rail key for CH1
} Adc0832Config;

// Bind an ADC0832 instance to MCU pins + board-channel net ids and register
// its Level-2 traps on ctx (null ctx falls back to the active context).
// Out-of-range pins are ignored. Re-calling re-binds (test isolation:
// mcs51_trap_reset() clears all traps first).
void adc0832_device_attach(struct Mcu51Context* ctx, const Adc0832Config* cfg);

// Resolve channel ch (0/1) to its board-channel rail key on ctx (unbound
// slots fall back to ADC0832_DEFAULT_CHx_NET_ID).
uint8_t adc0832_ch_key(struct Mcu51Context* ctx, uint8_t ch);

// Bind an ADC0832 instance to MCU pins and register its Level-2 traps.
// port: 0=P0 … 3=P3; bit: 0..7. Out-of-range pins are ignored. Re-calling
// re-binds (test isolation: mcs51_trap_reset() clears all traps first).
// Compatibility wrapper (decision B): identical pins-only signature as
// before, channels default to ADC0832_DEFAULT_CHx_NET_ID (32/33).
void mcs51_adc0832_init(uint8_t cs_port,  uint8_t cs_bit,
                        uint8_t clk_port, uint8_t clk_bit,
                        uint8_t di_port,  uint8_t di_bit,
                        uint8_t do_port,  uint8_t do_bit);

// Channel data API (moved here from the core mcs51_adc.h shim, S3-2):
// inject/read the deterministic 8-bit code for channel ch on the ACTIVE
// context, routed through this device's net-id mapping (defaults 32+ch).
void mcs51_adc0832_set_value(uint8_t ch, uint8_t val);
uint8_t mcs51_adc0832_get_value(uint8_t ch);

#ifdef __cplusplus
}
#endif
