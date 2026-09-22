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
// edges (1->0 start, 0->1 end), the most recent TX byte, the per-byte
// charge applied by the last transfer, and the ADR-0087 session-ABI failure
// count (unbound device / whole-frame-only plugin / bus-level error).
uint32_t cms8s_spi_transfer_count(void);
uint32_t cms8s_spi_frame_start_count(void);
uint32_t cms8s_spi_frame_end_count(void);
uint8_t  cms8s_spi_last_tx(void);
uint32_t cms8s_spi_last_charge_us(void);
uint32_t cms8s_spi_abi_error_count(void);

// Phase 1 protocol-agnostic mock receive byte: transfers outside an ADR-0087
// session (or on a host without a bus engine) serve this value (MISO idle
// high = 0xFF by default). Tests inject a deterministic value; a reset
// restores 0xFF.
void    cms8s_spi_set_rx_value(uint8_t value);
uint8_t cms8s_spi_rx_value(void);

// Board-bound logical device number passed to js_pal_spi_session_open
// (ADR-0087 §1). Default 0 = the first bound device; the board binding /
// tests may override it.
void cms8s_spi_set_logical_device(uint16_t device_id);

#ifdef __cplusplus
}
#endif
