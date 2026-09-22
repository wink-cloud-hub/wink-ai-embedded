// SPDX-License-Identifier: LGPL-3.0-only
// CMS8S78xx on-chip SPI master model (T1.2 of
// PLAN-20260921-CMS8S78XX-I2C-SPI-DEADLOCK, contract ADR-0081 paradigm).
//
// Phase 1 scope: register-level behavior only — synchronous completion,
// two-step read-clear, per-byte clock charge and SSCR frame-edge recording.
// Device semantics (M95256 command FSM, WEL latch, CS commit) belong to the
// Phase 2 UniSim plugin, never to the chip package (§5.2.4).
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Mcu51Context;

// Register the SPCR/SPSR/SPDR/SSCR read+write hooks. Both entry points
// reinstall the hooks because a context reset wipes the hook table (S4-H2
// reset-rebuilds-registration contract); init delegates to reset.
void cms8s_spi_init(struct Mcu51Context* ctx);
void cms8s_spi_reset(struct Mcu51Context* ctx);

// Test observability: completed SPDR write transfers, SSCR.NSSO1 frame
// edges (1->0 start, 0->1 end), the most recent TX byte, and the per-byte
// charge applied by the last transfer.
uint32_t cms8s_spi_transfer_count(void);
uint32_t cms8s_spi_frame_start_count(void);
uint32_t cms8s_spi_frame_end_count(void);
uint8_t  cms8s_spi_last_tx(void);
uint32_t cms8s_spi_last_charge_us(void);

// Phase 1 protocol-agnostic mock receive byte: every transfer serves this
// value (MISO idle high = 0xFF by default). Tests inject a deterministic
// value; a reset restores 0xFF.
void    cms8s_spi_set_rx_value(uint8_t value);
uint8_t cms8s_spi_rx_value(void);

#ifdef __cplusplus
}
#endif
