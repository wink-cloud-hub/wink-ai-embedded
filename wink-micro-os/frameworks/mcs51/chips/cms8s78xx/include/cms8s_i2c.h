// SPDX-License-Identifier: LGPL-3.0-only
// CMS8S78xx on-chip I2C master model (T1.3 of
// PLAN-20260921-CMS8S78XX-I2C-SPI-DEADLOCK).
//
// Phase 1 scope: the 0xF5 I2CMCR command state machine (START/SEND/RECEIVE/
// STOP per vendor StdDriver i2c.c), 0xF5 write/read split, ADDR_NACKED
// reservation mapping (ADR-0086 §2) and SCL-formula charging. All address and
// data phases are mocked as ACK (§5.3.4); AT24C256 tWR/ACK-polling semantics
// belong to the Phase 2 UniSim plugin.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Mcu51Context;

// Register the I2C SFR read+write hooks (0xF2/0xF4/0xF5/0xF6/0xF7). Both
// entry points reinstall the hooks because a context reset wipes the hook
// table (S4-H2); init delegates to reset.
void cms8s_i2c_init(struct Mcu51Context* ctx);
void cms8s_i2c_reset(struct Mcu51Context* ctx);

// Test observability: executed commands, START opens, repeated STARTs, STOPs,
// address-phase NACKs, illegal/skipped commands, the last command charge and
// the ADR-0086 session-ABI failure count (bus-level errors).
uint32_t cms8s_i2c_cmd_count(void);
uint32_t cms8s_i2c_start_count(void);
uint32_t cms8s_i2c_restart_count(void);
uint32_t cms8s_i2c_stop_count(void);
uint32_t cms8s_i2c_addr_nack_count(void);
uint32_t cms8s_i2c_illegal_cmd_count(void);
uint32_t cms8s_i2c_last_charge_us(void);
uint32_t cms8s_i2c_abi_error_count(void);

// Phase 1 mock injection, used only by the host in-chip fallback (the wasm
// path always routes through the ADR-0086 engine). Defaults: every address
// phase ACKs and the mock receive byte is 0x00. A reset restores both.
void    cms8s_i2c_set_mock_addr_ack(bool ack);
void    cms8s_i2c_set_mock_rx_value(uint8_t value);
uint8_t cms8s_i2c_mock_rx_value(void);

#ifdef __cplusplus
}
#endif
