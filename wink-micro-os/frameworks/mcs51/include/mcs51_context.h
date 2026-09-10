// SPDX-License-Identifier: Apache-2.0
// Task R2: MCS-51 Mcu51Context runtime core container.
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "wink_event.h"
#include "mcs51_trap.h"

#ifdef __cplusplus
extern "C" {
#endif

// ── Timing Policy (ADR-0079 / Task T2 preview) ─────────────────────────────
#define MCS51_TIMING_NATIVE_0CYCLE 0u
#define MCS51_TIMING_ISS_SCHEDULED 1u

// MCU family ids for silicon reset seeds (GAP-04/GAP-13). mcs51_context_reset
// applies family-specific seeds (CKCON reset value, power-on Fosc).
#define MCS51_FAMILY_CLASSIC   0u  // AT89C52/STC89 and other classic 12T parts
#define MCS51_FAMILY_CMS8S78XX 1u  // Cmsemicon CMS8S78xx

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

// ── Standard Core MCU Context Container ────────────────────────────────────
// sizeof(Mcu51Context) ~ 66 KB (with 64 KB XDATA shadow).
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
