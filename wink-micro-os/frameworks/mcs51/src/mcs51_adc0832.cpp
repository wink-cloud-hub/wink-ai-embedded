// SPDX-License-Identifier: Apache-2.0
// ADC0832 external 8-bit ADC — instant Level-2 trap state machine (AD-15).
//
// Instruction-edge driven (NOT time driven): every state transition happens
// synchronously inside the MCU's own pin write/read statement, in the same
// interception point. Conversion takes 0 us — the analog code value is pulled
// from the channel-3 rail (mcs51_adc_get_value → js_pal_adc_read_norm / test
// injection) at the 3rd CLK rising edge. Trap discipline: no delay, no yield,
// pure state machine, no virtual-time advance.
//
// Deviation from timing SSOT §3.3 (documented in the M4 plan): the reference
// snippet mirrors DO output bits into wink_mcs51_sfr_shadow. We do NOT — the
// data-plane Read-Latch golden rule forbids peripheral writes to the latch
// (they would corrupt RMW instructions on the DIO pin and double-fire
// js_pal_gpio_write). Whole-port reads reconstruct the DO level through the
// on_read trap instead (WinkSfr::operator uint8_t per-bit reconstruction).
#include "ADC0832.H"

#include <stdint.h>

#include "mcs51_adc.h"
#include "mcs51_trap.h"

namespace {

enum AdcPhase { PHASE_IDLE = 0, PHASE_INPUT, PHASE_OUTPUT };

struct Adc0832State {
    uint8_t cs_port, cs_bit;
    uint8_t clk_port, clk_bit;
    uint8_t di_port, di_bit;
    uint8_t do_port, do_bit;
    bool is_dio_shared;

    uint8_t phase;         // AdcPhase
    uint8_t rise_count;    // CLK rising edges since CS fall
    uint8_t fall_count;    // CLK falling edges in PHASE_OUTPUT
    uint8_t channel_cfg;   // [1]=SGL/DIF, [0]=ODD/SIGN
    uint8_t shift_data;    // 8-bit conversion result, MSB first
    uint8_t out_bit;       // current DO drive level (1 = released/high)
};

Adc0832State s_adc;

inline uint8_t sfr_addr_for(uint8_t port) {
    return (uint8_t)(0x80u + (port << 4));  // P0=0x80 … P3=0xB0
}

// CS edge: fall (1->0) starts a conversion; rise (0->1) aborts to IDLE.
void on_cs_write(void *ctx, uint8_t level) {
    (void)ctx;
    if (level == 0u) {
        s_adc.phase = PHASE_INPUT;
        s_adc.rise_count = 0u;
        s_adc.fall_count = 0u;
        s_adc.channel_cfg = 0u;
        s_adc.shift_data = 0u;
        s_adc.out_bit = 1u;  // DO released (bus high) during config input
    } else {
        s_adc.phase = PHASE_IDLE;
        s_adc.out_bit = 1u;
    }
}

// CLK edge: the sole state-machine clock.
void on_clk_write(void *ctx, uint8_t level) {
    (void)ctx;
    if (s_adc.phase == PHASE_IDLE) {
        return;
    }

    if (level == 1u) {
        // ── CLK rising edge ──────────────────────────────────────────────
        if (s_adc.phase != PHASE_INPUT) {
            return;  // output-phase rising edges: MCU samples DO, nothing to do
        }
        // DI is sampled from the MCU's LATCH (config phase: MCU drives DIO).
        const uint8_t di_val =
            (uint8_t)((wink_mcs51_sfr_shadow[sfr_addr_for(s_adc.di_port)]
                       >> s_adc.di_bit) & 1u);
        ++s_adc.rise_count;

        if (s_adc.rise_count == 1u) {
            // Start bit must be 1; anything else is a protocol abort.
            if (di_val != 1u) {
                s_adc.phase = PHASE_IDLE;
            }
        } else if (s_adc.rise_count == 2u) {
            s_adc.channel_cfg = (uint8_t)(di_val << 1);  // SGL/DIF
        } else if (s_adc.rise_count == 3u) {
            s_adc.channel_cfg |= di_val;                 // ODD/SIGN
            // Channel locked (single-ended: ODD/SIGN selects CH0/CH1).
            const uint8_t ch = (uint8_t)(s_adc.channel_cfg & 0x01u);
            // 0 us instant conversion: pull the 8-bit code value right now.
            s_adc.shift_data = (uint8_t)(mcs51_adc_get_value(ch) & 0xFFu);
            s_adc.phase = PHASE_OUTPUT;
            s_adc.fall_count = 0u;
            // Leading Null bit window: DO drives 0 until the 3rd falling edge.
            // (In 3-wire mode the MCU is still driving DIO itself here, so this
            // level is only observable in 4-wire mode or on DO-read races.)
            s_adc.out_bit = 0u;
        }
    } else {
        // ── CLK falling edge: DO presents the next bit BEFORE the MCU reads ─
        if (s_adc.phase != PHASE_OUTPUT) {
            return;
        }
        ++s_adc.fall_count;
        if (s_adc.fall_count <= 8u) {
            // Fall #1 (overall #3) -> MSB (bit7) … fall #8 -> bit0.
            s_adc.out_bit =
                (uint8_t)((s_adc.shift_data >> (8u - s_adc.fall_count)) & 1u);
        } else {
            s_adc.out_bit = 1u;  // all 8 bits shifted: release bus (high)
        }
    }
}

// DI/DIO write.
void on_di_write(void *ctx, uint8_t level) {
    (void)ctx;
    (void)level;
    // PHASE_OUTPUT: the MCU writing DIO=1 is the quasi-bidirectional port's
    // input-enable / bus-release gesture — absorb it completely. Config bits
    // are sampled from the latch at CLK rising edges, so INPUT writes need no
    // action here either.
}

// DO/DIO read: external pin level reconstruction (Read-Pin).
uint8_t on_do_read(void *ctx) {
    (void)ctx;
    if (s_adc.phase == PHASE_OUTPUT) {
        return s_adc.out_bit;  // chip drives the converted bit
    }
    if (s_adc.phase == PHASE_INPUT && s_adc.is_dio_shared) {
        // 3-wire mode: the MCU itself drives DIO with the config bits, so a
        // read-back sees the driven latch level.
        return (uint8_t)((wink_mcs51_sfr_shadow[sfr_addr_for(s_adc.do_port)]
                          >> s_adc.do_bit) & 1u);
    }
    return 1u;  // IDLE, or 4-wire DO: chip high-Z, bus pulled high
}

}  // namespace

extern "C" void mcs51_adc0832_init(uint8_t cs_port,  uint8_t cs_bit,
                                   uint8_t clk_port, uint8_t clk_bit,
                                   uint8_t di_port,  uint8_t di_bit,
                                   uint8_t do_port,  uint8_t do_bit) {
    s_adc.cs_port = cs_port;   s_adc.cs_bit = cs_bit;
    s_adc.clk_port = clk_port; s_adc.clk_bit = clk_bit;
    s_adc.di_port = di_port;   s_adc.di_bit = di_bit;
    s_adc.do_port = do_port;   s_adc.do_bit = do_bit;
    s_adc.is_dio_shared = (di_port == do_port && di_bit == do_bit);
    s_adc.phase = PHASE_IDLE;
    s_adc.rise_count = 0u;
    s_adc.fall_count = 0u;
    s_adc.channel_cfg = 0u;
    s_adc.shift_data = 0u;
    s_adc.out_bit = 1u;

    mcs51_trap_register_write(cs_port, cs_bit, &on_cs_write, nullptr);
    mcs51_trap_register_write(clk_port, clk_bit, &on_clk_write, nullptr);
    mcs51_trap_register_write(di_port, di_bit, &on_di_write, nullptr);
    mcs51_trap_register_read(do_port, do_bit, &on_do_read, nullptr);
}
