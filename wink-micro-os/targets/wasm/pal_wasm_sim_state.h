// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_wasm_sim_state.h
 * @brief WASM simulation target pure POD state aggregation for deterministic replay & zero-leak reset.
 */
#ifndef PAL_WASM_SIM_STATE_H
#define PAL_WASM_SIM_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_sim_physical.h"

#define WASM_SIM_MAX_PINS 50
#define WASM_SIM_MAX_UART_PORTS 2
#define WASM_SIM_UART_FIFO_SIZE 256
#define WASM_SIM_MAX_PENDING_IRQ 64

/**
 * Pure POD UART FIFO inline buffer (value semantics, zero heap pointer)
 */
typedef struct {
    uint8_t buf[WASM_SIM_UART_FIFO_SIZE];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    bool inited;
    uint8_t _pad[3];
} pal_wasm_sim_uart_fifo_t;

/**
 * Pure POD Pending IRQ record
 */
typedef struct {
    uint32_t irq_num;
} pal_wasm_sim_pending_irq_t;

/**
 * Aggregated WASM simulation state struct.
 * HARD CONSTRAINT: MUST BE PURE POD VALUE SEMANTICS.
 * ABSOLUTELY NO NAKED HEAP POINTERS OR HANDLES ALLOWED.
 */
typedef struct {
    /* PRNG & Fault degradation state */
    uint32_t prng_state;
    uint8_t fidelity_level;
    uint8_t _padding[3];
    wink_sim_faults_t faults;
    wink_phys_debounce_ctx_t debounce_ctx[WASM_SIM_MAX_PINS];

    /* IRQ pending queue */
    pal_wasm_sim_pending_irq_t pending_irq_queue[WASM_SIM_MAX_PENDING_IRQ];
    uint32_t pending_irq_head;
    uint32_t pending_irq_count;
    uint32_t pending_irq_overflow_count;
    uint32_t irq_lock_nest_count;

    /* UART inline RX FIFOs */
    pal_wasm_sim_uart_fifo_t uart_rx_fifo[WASM_SIM_MAX_UART_PORTS];

    /* Fault domain & latch flags */
    bool is_faulted;
    uint8_t active_fault_mask;
    uint8_t _reserved[2];
} sim_state_t;

extern sim_state_t g_sim;

#endif /* PAL_WASM_SIM_STATE_H */
