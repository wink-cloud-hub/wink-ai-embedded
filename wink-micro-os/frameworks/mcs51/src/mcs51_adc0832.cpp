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
// snippet mirrors DO output bits into SFR shadow. We do NOT — the
// data-plane Read-Latch golden rule forbids peripheral writes to the latch
// (they would corrupt RMW instructions on the DIO pin and double-fire
// js_pal_gpio_write). Whole-port reads reconstruct the DO level through the
// on_read trap instead (WinkSfr::operator uint8_t per-bit reconstruction).
#include "ADC0832.H"

#include <cstdint>
#include <cstring>

#include "mcs51_adc.h"
#include "mcs51_context.h"
#include "mcs51_trap.h"
#include "wink_mcs51_gpio.h"

namespace {

enum AdcPhase { PHASE_IDLE = 0, PHASE_INPUT, PHASE_OUTPUT };

// S2-1 device BSS pool (scheme A): one Adc0832State slot per context
// instance, owned by the device (migrates to devices/adc0832/ in stage3
// with this TU). Indexed by ctx->instance_index — the same key as the chip
// pool, but a separate array: board devices are orthogonal to families
// (classic + ADC0832 is the live iron_ntc combo), so they must not share
// the chip's soc_priv slot.
static Adc0832State s_adc0832_pool[MCS51_MAX_INSTANCES];

// M2: state lives per-instance (was Mcu51Context::adc0832, was file-static
// s_adc before that). Pin-trap callbacks are registered with a null cookie,
// so they resolve the active context (single-context simulation model, same
// as the CMS8S models' mcs51_get_context() fallback).
inline Adc0832State& adc_state() {
    Mcu51Context* ctx = mcs51_get_context();
    // Indexing clamps, never asserts (same rule as the chip binder: the
    // loud fuse lives in mcs51_context_reset).
    // Review hardening: null-active (no active context at all) clamps safely
    // to slot 0 instead of dereferencing nullptr.
    const uint8_t raw_idx = (ctx != nullptr) ? ctx->instance_index : 0u;
    const uint8_t idx = (raw_idx < MCS51_MAX_INSTANCES)
                            ? raw_idx
                            : (MCS51_MAX_INSTANCES - 1u);
    return s_adc0832_pool[idx];
}

inline uint8_t sfr_addr_for(uint8_t port) {
    return (uint8_t)(0x80u + (port << 4));  // P0=0x80 … P3=0xB0
}

// M3: C language linkage — these addresses are stored in the C-ABI pin
// trap table (mcs51_pin_write_fn_t / mcs51_pin_read_fn_t).
// CS edge: fall (1->0) starts a conversion; rise (0->1) aborts to IDLE.
extern "C" void on_cs_write(void *ctx, uint8_t level) {
    (void)ctx;
    if (level == 0u) {
        adc_state().phase = PHASE_INPUT;
        adc_state().rise_count = 0u;
        adc_state().fall_count = 0u;
        adc_state().channel_cfg = 0u;
        adc_state().shift_data = 0u;
        adc_state().out_bit = 1u;  // DO released (bus high) during config input
    } else {
        adc_state().phase = PHASE_IDLE;
        adc_state().out_bit = 1u;
    }
}

// CLK edge: the sole state-machine clock.
extern "C" void on_clk_write(void *ctx, uint8_t level) {
    (void)ctx;
    if (adc_state().phase == PHASE_IDLE) {
        return;
    }

    if (level == 1u) {
        // ── CLK rising edge ──────────────────────────────────────────────
        if (adc_state().phase != PHASE_INPUT) {
            return;  // output-phase rising edges: MCU samples DO, nothing to do
        }
        // DI is sampled from the MCU's LATCH (config phase: MCU drives DIO).
        const uint8_t di_val = mcs51_gpio_bit_read_latch(adc_state().di_port, adc_state().di_bit);
        ++adc_state().rise_count;

        if (adc_state().rise_count == 1u) {
            // Start bit must be 1; anything else is a protocol abort.
            if (di_val != 1u) {
                adc_state().phase = PHASE_IDLE;
            }
        } else if (adc_state().rise_count == 2u) {
            adc_state().channel_cfg = (uint8_t)(di_val << 1);  // SGL/DIF
        } else if (adc_state().rise_count == 3u) {
            adc_state().channel_cfg |= di_val;                 // ODD/SIGN
            // Channel locked (single-ended: ODD/SIGN selects CH0/CH1).
            const uint8_t ch = (uint8_t)(adc_state().channel_cfg & 0x01u);
            // 0 us instant conversion: pull the 8-bit code value right now.
            // Stage1: the board-fabric key is explicit (`32+ch`, permanent);
            // only the key expression changed, CH API and semantics unchanged.
            adc_state().shift_data = (uint8_t)(mcs51_adc_get_value((uint8_t)(32u + ch)) & 0xFFu);
            adc_state().phase = PHASE_OUTPUT;
            adc_state().fall_count = 0u;
            // Leading Null bit window: DO drives 0 until the 3rd falling edge.
            // (In 3-wire mode the MCU is still driving DIO itself here, so this
            // level is only observable in 4-wire mode or on DO-read races.)
            adc_state().out_bit = 0u;
        }
    } else {
        // ── CLK falling edge: DO presents the next bit BEFORE the MCU reads ─
        if (adc_state().phase != PHASE_OUTPUT) {
            return;
        }
        ++adc_state().fall_count;
        if (adc_state().fall_count <= 8u) {
            // Fall #1 (overall #3) -> MSB (bit7) … fall #8 -> bit0.
            adc_state().out_bit =
                (uint8_t)((adc_state().shift_data >> (8u - adc_state().fall_count)) & 1u);
        } else {
            adc_state().out_bit = 1u;  // all 8 bits shifted: release bus (high)
        }
    }
}

// DI/DIO write.
extern "C" void on_di_write(void *ctx, uint8_t level) {
    (void)ctx;
    (void)level;
    // PHASE_OUTPUT: the MCU writing DIO=1 is the quasi-bidirectional port's
    // input-enable / bus-release gesture — absorb it completely. Config bits
    // are sampled from the latch at CLK rising edges, so INPUT writes need no
    // action here either.
}

// DO/DIO read: external pin level reconstruction (Read-Pin).
extern "C" uint8_t on_do_read(void *ctx) {
    (void)ctx;
    if (adc_state().phase == PHASE_OUTPUT) {
        return adc_state().out_bit;  // chip drives the converted bit
    }
    if (adc_state().phase == PHASE_INPUT && adc_state().is_dio_shared) {
        // 3-wire mode: the MCU itself drives DIO with the config bits, so a
        // read-back sees the driven latch level.
        return mcs51_gpio_bit_read_latch(adc_state().do_port, adc_state().do_bit);
    }
    return 1u;  // IDLE, or 4-wire DO: chip high-Z, bus pulled high
}

}  // namespace

extern "C" void mcs51_adc0832_init(uint8_t cs_port,  uint8_t cs_bit,
                                   uint8_t clk_port, uint8_t clk_bit,
                                   uint8_t di_port,  uint8_t di_bit,
                                   uint8_t do_port,  uint8_t do_bit) {
    // S2-1: own the pool slot (devices have no reset-loop entry; init is
    // the lifecycle point — re-binding memsets, matching old re-init).
    std::memset(&adc_state(), 0, sizeof(Adc0832State));
    adc_state().cs_port = cs_port;   adc_state().cs_bit = cs_bit;
    adc_state().clk_port = clk_port; adc_state().clk_bit = clk_bit;
    adc_state().di_port = di_port;   adc_state().di_bit = di_bit;
    adc_state().do_port = do_port;   adc_state().do_bit = do_bit;
    adc_state().is_dio_shared = (di_port == do_port && di_bit == do_bit);
    adc_state().phase = PHASE_IDLE;
    adc_state().rise_count = 0u;
    adc_state().fall_count = 0u;
    adc_state().channel_cfg = 0u;
    adc_state().shift_data = 0u;
    adc_state().out_bit = 1u;

    mcs51_trap_register_write(cs_port, cs_bit, &on_cs_write, nullptr);
    mcs51_trap_register_write(clk_port, clk_bit, &on_clk_write, nullptr);
    mcs51_trap_register_write(di_port, di_bit, &on_di_write, nullptr);
    mcs51_trap_register_read(do_port, do_bit, &on_do_read, nullptr);
}
