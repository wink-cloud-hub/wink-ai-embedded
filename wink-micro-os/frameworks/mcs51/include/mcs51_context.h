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

// On-chip XDATA aperture by family (GAP-09, datasheet §2.2.3):
//  - CMS8S78xx: 1 KB internal XRAM (0x0000..0x03FF).
//  - Classic 8052: no on-chip XRAM; external MOVX RAM size is board-specific
//    and declared per app, so the sim keeps the configurable
//    WINK_MCS51_XDATA_SIZE aperture (8 KB default) for that family.
#define MCS51_XRAM_SIZE_CMS8S78XX 1024u
#define MCS51_XRAM_WINDOW_BASE     0xF000u  // CMS8S extended-SFR MOVX window

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

typedef struct {
    Mcu51ExtIntLine   lines[2];
    Mcu51PortPinState port_pins[4][8];
    uint64_t          port_last_sample_us;
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

// ── Per-instance model states (maintainability M2) ─────────────────────────
// These POD states were file-static globals (one instance per process);
// they now live in Mcu51Context so two contexts / two families never share
// silicon state. Owner model noted per struct. Process-level *diagnostic*
// counters (OOB/unmodeled-XSFR/UART-notready/unsupported/duplicate-vector)
// deliberately stay file-static: they are build-time diagnostics, not silicon.

// Owner: mcs51_adc0832.cpp (external ADC0832 trap state machine).
typedef struct {
    uint8_t cs_port, cs_bit;
    uint8_t clk_port, clk_bit;
    uint8_t di_port, di_bit;
    uint8_t do_port, do_bit;
    bool    is_dio_shared;
    uint8_t phase;         // 0=IDLE, 1=INPUT, 2=OUTPUT
    uint8_t rise_count;    // CLK rising edges since CS fall
    uint8_t fall_count;    // CLK falling edges in OUTPUT phase
    uint8_t channel_cfg;   // [1]=SGL/DIF, [0]=ODD/SIGN
    uint8_t shift_data;    // 8-bit conversion result, MSB first
    uint8_t out_bit;       // current DO drive level (1 = released/high)
} Mcs51Adc0832State;

// Owner: cms8s_sys.cpp (TA protection window + WDT coarse model).
typedef struct {
    // 0 = waiting 0xAA, 1 = got 0xAA waiting 0x55, 2 = unlocked (next
    // protected write passes and consumes the window).
    uint8_t ta_phase;
    // virtual_us when 0xAA was accepted (TA window timeout, GAP-07 coarse).
    uint64_t ta_aa_us;
    // virtual_us of the last WDT start/feed (WDTRE 0->1 or WDTCLR strobe).
    uint64_t wdt_last_feed_us;
    // Overflow already counted for the current arming (one count per
    // episode until the next feed; Release warn-once latch lives with the
    // file-static diagnostic counter).
    uint8_t wdt_overflow_latched;
} Mcs51SysProtState;

// Owner: cms8s_buzzer.cpp.
typedef struct {
    bool     running;
    uint8_t  pin_level;
    uint32_t half_period_us;
    uint64_t next_toggle_us;
    uint32_t toggle_count;
} Mcs51BuzzerState;

// Owner: cms8s_adc.cpp (on-chip 12-bit ADC + ADET trigger).
typedef struct {
    uint32_t conversion_count;
    uint8_t  last_channel;
} Mcs51Cms8sAdcState;

typedef struct {
    uint16_t last_pin;
    uint8_t  last_level;
    bool     have_sample;
} Mcs51AdetPinState;

typedef struct {
    Mcs51Cms8sAdcState adc;
    Mcs51AdetPinState  adet;
    bool               in_poll;
} Mcs51Cms8sAdcPriv;

// Owner: mcs51_pwm_meter.cpp (host-side soft-PWM measurement).
typedef struct {
    bool     active;
    uint8_t  current_level;
    uint64_t last_flip_us;
    uint64_t high_time_us;
    uint64_t low_time_us;
    uint32_t transitions;
} Mcs51PwmMeter;

// Owner: mcs51_xdata.cpp + mcs51_gpio.cpp (classic external MOVX bus,
// GAP-24). Parts without on-chip XRAM drive P0 (AD0-7), P2 (A8-15),
// P3.6 (/WR) and P3.7 (/RD) on every MOVX access, so firmware that mixes
// XBYTE traffic with GPIO use of those pins is in silicon conflict.
// xbus_used latches "MOVX seen"; gpio_bus_mask latches which bus pins were
// touched as GPIO (P0: bits 0-7, P2: bits 8-15, P3.6/7: bits 16-17).
// Cleared by context reset (memset); conflict counting is process-level
// (file-static, M4) in mcs51_xdata.cpp.
typedef struct {
    uint8_t  xbus_used;
    uint32_t gpio_bus_mask;
} Mcs51ClassicBusState;

// ── Standard Core MCU Context Container ────────────────────────────────────
// sizeof(Mcu51Context) = 75752 B post-stage1 (MinGW GCC-measured, S2-0 probe:
// 75648 master baseline + 8 caps_cache + 96 rail-64; 64 KB is XDATA shadow).
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
    void*              soc_priv;

    // 6. Timed edge queue (Task F1)
    McuEdgeEvent edge_queue[64];
    uint8_t      edge_head;
    uint8_t      edge_tail;

    // 7. Low power OSAL wake event (Task R6)
    wink_event_t wake_event;

    // 8. MCU family + per-instance model states (M1/M2). `family` is loaded
    // from the process selector at reset; mechanism files branch on the
    // family *descriptor* (mcs51_family.h), never on an id comparison.
    // Stage0 (v2 schema): `caps_cache` snapshots desc->capabilities at
    // reset/set_family; hot paths read ONLY this cache (ADR-0004 static
    // dispatch, zero function-pointer cost on standard parts).
    uint8_t            family;
    uint32_t           caps_cache;
    Mcs51Adc0832State  adc0832;
    Mcs51SysProtState  sysProt;
    Mcs51BuzzerState   buzzer;
    Mcs51Cms8sAdcPriv  cms8sAdc;
    uint16_t           adc_injected[MCS51_ADC_MAX_RAIL_KEYS];
    uint8_t            adc_inject_flag[MCS51_ADC_MAX_RAIL_KEYS];
    // A-02 ADC reference rail (GAP-05): Vref/Vrail in mV, set by the chip
    // layer through the generic mcs51_adc_set_vref/vrail_mv rail parameters
    // (core holds no ADCLDO knowledge). Ratio Vrail/Vref scales the
    // Pull-track norm->raw conversion. Defaults 3000/3000 (ratio 1.0,
    // zero regression) are seeded on context reset.
    uint16_t           adc_vref_mv;
    uint16_t           adc_vrail_mv;
    Mcs51PwmMeter      pwm_meters[32];
    // GAP-24 classic external MOVX bus occupancy (per-instance silicon
    // state; zeroed by context reset via memset).
    Mcs51ClassicBusState classicBus;
} Mcu51Context;

// ── Active context pointer and accessors ───────────────────────────────────
extern Mcu51Context* g_active_mcu_context;

static inline Mcu51Context* mcs51_get_context(void) {
    return g_active_mcu_context;
}

static inline void mcs51_set_active_context(Mcu51Context* ctx) {
    g_active_mcu_context = ctx;
}

// Reset MCU context to silicon seeds (P0..P3=0xFF, SP=0x07, PS_ADET=0x7F, etc.).
// Preserves registered ISRs in isr_table. Family-specific seeds (CKCON/Fosc,
// GAP-04/GAP-13) are applied at the end per the selected MCU family.
void mcs51_context_reset(Mcu51Context* ctx);

// Select MCU family at runtime and apply that family's silicon seeds
// immediately (test seam; production builds get the compile-time default
// from the WINK_MCU_* define).
void mcs51_context_set_family(uint8_t family);
uint8_t mcs51_context_get_family(void);

#ifdef __cplusplus
}
#endif
