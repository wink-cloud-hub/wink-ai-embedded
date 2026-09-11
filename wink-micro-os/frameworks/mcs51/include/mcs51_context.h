// SPDX-License-Identifier: Apache-2.0
// Task R2: MCS-51 Mcu51Context runtime core container.
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "wink_event.h"
#include "mcs51_adc.h"
#include "mcs51_family.h"
#include "mcs51_trap.h"

#ifdef __cplusplus
extern "C" {
#endif

// ── Timing Policy (ADR-0079 / Task T2 preview) ─────────────────────────────
#define MCS51_TIMING_NATIVE_0CYCLE 0u
#define MCS51_TIMING_ISS_SCHEDULED 1u

// MCU family ids + silicon facts (MCS51_FAMILY_*, family descriptors) live
// in mcs51_family.h (maintainability M1). mcs51_context_reset applies the
// active family's seeds (CKCON reset value, power-on Fosc).
// (S2-2: on-chip XRAM facts used to live here; sunk to the chip package
// header — zero users elsewhere.)

// ── Task F1: Timed edge injection event ────────────────────────────────────
typedef struct {
    uint64_t fire_us;
    uint16_t pin;
    uint8_t  level;
} McuEdgeEvent;

// ── Timer State (Task R2 / R1) ─────────────────────────────────────────────
typedef struct {
    bool     running;
    bool     external_clk;
    uint8_t  mode;
    uint64_t next_ovf_us;
    uint8_t  tr_prev;
    uint8_t  last_pin_level;
} Mcu51TimerChannel;

typedef struct {
    Mcu51TimerChannel channels[2];
    bool     t2_running;
    uint64_t t2_next_ovf_us;
    uint8_t  t2_tr_prev;
    uint8_t  t2_last_pin_level;
    uint8_t  t2_cap_last_level[4];
    uint64_t t2_next_cmp_us[4];
    bool     t3_running;
    uint64_t t3_next_ovf_us;
    uint8_t  t3_tr_prev;
    bool     t4_running;
    uint64_t t4_next_ovf_us;
    uint8_t  t4_tr_prev;
} Mcu51TimerState;

// ── ExtInt State (Task R2 / R1) ────────────────────────────────────────────
typedef struct {
    uint16_t pin;
    uint16_t fallback_pin;
    uint16_t ps_addr;
    uint8_t  vector;
    uint8_t  it_bit;
    uint8_t  ie_bit;
    uint8_t  ex_bit;
    uint8_t  last_level;
    uint64_t last_sample_us;
    bool     have_sample;
} Mcu51ExtIntLine;

typedef struct {
    uint8_t last_level;
    bool    have_sample;
} Mcu51PortPinState;

// Standard INT0/INT1 line state only (stage4 S4-1 Step 2: full-port sampling
// moved to the chip pool; S4-D2 drops its cross-reset preserve).
typedef struct {
    Mcu51ExtIntLine   lines[2];
    bool              sample_due;
    bool              in_poll;
} Mcu51ExtIntState;

// ── UART State (Task R2 / R1) ──────────────────────────────────────────────
#define MCS51_UART_CAPTURE_CAP 4096u
#define MCS51_UART_RX_FIFO_CAP 64u

typedef struct {
    uint8_t  capture[MCS51_UART_CAPTURE_CAP];
    uint32_t count;
    uint8_t  rx_fifo[MCS51_UART_RX_FIFO_CAP];
    uint32_t rx_head;
    uint32_t rx_tail;
    uint32_t rx_dropped;
    uint64_t rx_last_deliver_us;
    bool     rx_have_delivered;
} Mcu51UartState;

// ── Per-instance model states (maintainability M2, purified S2-1) ──────────
// These POD states were file-static globals (one instance per process);
// M2 moved them into Mcu51Context, S2-1 moves them OUT again into per-owner
// BSS pools keyed by ctx->instance_index (CPL-11/12, scheme A):
//   * chip states (sys/buzzer/adc) -> chip priv pool (bound via soc_priv
//     by self-binding chip inits; the zero-extension family binds nullptr);
//   * board-device state -> device-owned pool (migrates to devices/ in
//     stage3 with its TU).
// Process-level *diagnostic* counters (OOB/unmodeled-XSFR/UART-notready/
// unsupported/duplicate-vector) deliberately stay file-static: they are
// build-time diagnostics, not silicon.

// Owner: mcs51_pwm_meter.cpp (host-side soft-PWM measurement).
typedef struct {
    bool     active;
    uint8_t  current_level;
    uint64_t last_flip_us;
    uint64_t high_time_us;
    uint64_t low_time_us;
    uint32_t transitions;
} Mcs51PwmMeter;

// Owner: mcs51_xdata.cpp + mcs51_gpio.cpp (external MOVX bus, GAP-24).
// This is a GENERIC 8051 concept (any part without on-chip XRAM drives the
// external bus on MOVX; gated by descriptor xram_size == 0), so it keeps a
// generic name and stays in core — S2-1 renames the family-named predecessor
// to extbus without touching the logic.
// Parts without on-chip XRAM drive P0 (AD0-7), P2 (A8-15),
// P3.6 (/WR) and P3.7 (/RD) on every MOVX access, so firmware that mixes
// XBYTE traffic with GPIO use of those pins is in silicon conflict.
// xbus_used latches "MOVX seen"; gpio_bus_mask latches which bus pins were
// touched as GPIO (P0: bits 0-7, P2: bits 8-15, P3.6/7: bits 16-17).
// Cleared by context reset (memset); conflict counting is process-level
// (file-static, M4) in mcs51_xdata.cpp.
typedef struct {
    uint8_t  xbus_used;
    uint32_t gpio_bus_mask;
} Mcs51ExtBusState;

// ── Standard Core MCU Context Container ────────────────────────────────────
// sizeof(Mcu51Context) = 75672 B post-S2-1 (MinGW GCC-measured: 75648 master
// baseline + 8 caps_cache + 96 rail-64 - 80 scheme-A split; 64 KB is XDATA).
// S3-2 adds the 8 B sfr_write_notify slot with ZERO net growth (absorbed by
// existing alignment padding; still 75672, locked by the budget test below).
// Locked by test_mcs51_context_budget (print + ceiling); see stage2 §4 table.
// Allocation MUST be in BSS or heap — NEVER on fiber/stack.
typedef struct Mcu51Context {
    // 1. Memory and SFR shadows
    uint8_t sfr_shadow[256];
    uint8_t xdata_shadow[65536];

    // 2. Traps and hooks (explicitly carrying ctx)
    mcs51_pin_trap_t        pin_traps[4][8];
    mcs51_sfr_read_hook_t   sfr_read_hooks[256];
    mcs51_sfr_write_hook_t  sfr_write_hooks[256];
    // S3-2: pre-dispatch SFR-write notify (chip TA window, GAP-07). The
    // bridge runs this BEFORE the per-address hook above; nullptr on
    // families without one. Installed by the owning chip init, cleared by
    // reset memset / trap reset. Per-context by construction (no file-static).
    mcs51_sfr_write_notify_fn_t sfr_write_notify;

    // 3. Interrupt subsystem state (SSOT: in_service_depth > 0)
    void (*isr_table[28])(void);
    uint32_t pending_interrupts;
    uint8_t  in_service_prio_stack[4];
    uint8_t  in_service_depth;
    bool     interrupts_enabled;
    bool     reti_suppress_one;
    uint32_t isr_dispatch_count[28];

    // 4. Clock and scheduling state
    uint64_t virtual_us;
    uint64_t slice_start_us;
    uint32_t quota_yields;
    uint32_t step_count;
    uint32_t clock_hz;
    uint32_t microstep_us;

    // 5. Standard peripheral private states
    Mcu51TimerState    timer;
    Mcu51ExtIntState   extint;
    Mcu51UartState     uart;
    // Chip-private block (S2-1, scheme A): opaque to core, bound per
    // instance by self-binding chip inits; classic binds nullptr.
    void*              soc_priv;

    // 6. Timed edge queue (Task F1)
    McuEdgeEvent edge_queue[64];
    uint8_t      edge_head;
    uint8_t      edge_tail;

    // 7. Low power OSAL wake event (Task R6)
    wink_event_t wake_event;

    // 8. MCU family + instance slot (M1/M2, purified S2-1). `family` is
    // loaded from the process selector at reset; mechanism files branch on
    // the family *descriptor* (mcs51_family.h), never on an id comparison.
    // Stage0 (v2 schema): `caps_cache` snapshots desc->capabilities at
    // reset/set_family; hot paths read ONLY this cache (ADR-0004 static
    // dispatch, zero function-pointer cost on standard parts).
    uint8_t            family;
    // S2-1: per-instance slot selecting the owner's BSS pool entry
    // (chip priv pool, device pool). BSS-zero default 0 = today's single
    // context; multi-instance callers assign via mcs51_context_init.
    // Packs with `family` (zero struct growth).
    uint8_t            instance_index;
    uint32_t           caps_cache;
    // S2-1: GPIO Trait hooks (may_drive/is_analog/pullup), per-context by
    // value. Declaration only here, mounted by chip init in stage4; memset
    // zero = unhooked standard quasi-bidirectional behavior (zero behavior
    // change this stage).
    Mcs51GpioHooks     gpio_hooks;
    // S4-2 Step 1 (S4-D3): UART source-selection hooks, per-context by
    // value. Memset zero = standard Timer1 path (zero behavior change
    // this stage for standard parts; enhanced families mount in stage4).
    Mcs51UartHooks     uart_hooks;
    uint16_t           adc_injected[MCS51_ADC_MAX_RAIL_KEYS];
    uint8_t            adc_inject_flag[MCS51_ADC_MAX_RAIL_KEYS];
    // A-02 ADC reference rail (GAP-05): Vref/Vrail in mV, set by the chip
    // layer through the generic mcs51_adc_set_vref/vrail_mv rail parameters
    // (core holds no reference-source knowledge). Ratio Vrail/Vref scales
    // the Pull-track norm->raw conversion. Defaults 3000/3000 (ratio 1.0,
    // zero regression) are injected by chip reset, not seeded here.
    uint16_t           adc_vref_mv;
    uint16_t           adc_vrail_mv;
    Mcs51PwmMeter      pwm_meters[32];
    // GAP-24 external MOVX bus occupancy (per-instance silicon
    // state; zeroed by context reset via memset).
    Mcs51ExtBusState extbus;
} Mcu51Context;

// ── Active context pointer and accessors ───────────────────────────────────
extern Mcu51Context* g_active_mcu_context;

static inline Mcu51Context* mcs51_get_context(void) {
    return g_active_mcu_context;
}

static inline void mcs51_set_active_context(Mcu51Context* ctx) {
    g_active_mcu_context = ctx;
}

// Max simultaneous context instances (S2-1, scheme A): owner BSS pools
// (chip priv, device states) are dimensioned by this. 4 = dual-context
// tests + simultaneous instances of both families + spare; pool cost per
// spare slot is ~100 B BSS (not counted in the context budget).
#define MCS51_MAX_INSTANCES 4u
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(MCS51_MAX_INSTANCES >= 2u, "need >= 2 slots for dual-context");
#endif

// Reset MCU context to silicon seeds (P0..P3=0xFF, SP=0x07, PS_ADET=0x7F, etc.).
// Preserves registered ISRs in isr_table. Family-specific seeds (CKCON/Fosc,
// GAP-04/GAP-13) are applied at the end per the selected MCU family.
// Uses ctx->instance_index (assign via mcs51_context_init for instances
// beyond the BSS-zero default 0); asserts idx < MCS51_MAX_INSTANCES.
void mcs51_context_reset(Mcu51Context* ctx);

// Assign a context to an instance slot (S2-1): selects the owner's BSS pool
// entry used after the next reset. Out-of-range idx clamps to the last slot
// (clamp is unit-tested; reset asserts the invariant as backstop).
void mcs51_context_init(Mcu51Context* ctx, uint8_t idx);

// Select MCU family at runtime and apply that family's silicon seeds
// immediately (test seam; production builds get the compile-time default
// from the WINK_MCU_* define).
void mcs51_context_set_family(uint8_t family);
uint8_t mcs51_context_get_family(void);

#ifdef __cplusplus
}
#endif
