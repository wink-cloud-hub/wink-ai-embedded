// SPDX-License-Identifier: LGPL-3.0-only
// MCS51 framework view of the UniSim CH2 bus ABI (ADR-0085/0086/0087, plan
// T2.3-D).
//
// MIRROR of targets/wasm/wasm_bridge.h, which stays the ABI SSOT (hash,
// catalog and TS parity are enforced there). The on-chip controller models
// and the host compat bridge compile for host CTest as well as wasm, and the
// host build does not put targets/wasm on the include path, so the handful of
// imports they call are declared here. Any signature change must be made in
// wasm_bridge.h first (seven-step sync) and mirrored here verbatim.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

// 8-byte structured I2C result (ADR-0085); layout frozen by wasm_bridge.h.
typedef struct {
    uint32_t nack_bits;
    uint16_t stretch_us;
    uint8_t  addr_nack;
    uint8_t  flags;
} pal_i2c_result_t;

_Static_assert(sizeof(pal_i2c_result_t) == 8, "pal_i2c_result_t ABI size");
_Static_assert(offsetof(pal_i2c_result_t, nack_bits) == 0, "ABI layout");
_Static_assert(offsetof(pal_i2c_result_t, stretch_us) == 4, "ABI layout");
_Static_assert(offsetof(pal_i2c_result_t, addr_nack) == 6, "ABI layout");
_Static_assert(offsetof(pal_i2c_result_t, flags) == 7, "ABI layout");

#define PAL_I2C_SESSION_INVALID 0xFFu
#define PAL_I2C_ACK_ALL         1u
#define PAL_I2C_NACK_ALL        2u
#define PAL_SPI_SESSION_INVALID 0xFFu

// -- ADR-0085/0086 I2C status + session stream ---------------------------

wink_status_t js_pal_i2c_transfer_ex(uint8_t port, uint16_t dev_addr,
                                     const uint8_t *write_buf,
                                     uint32_t write_len, uint8_t *read_buf,
                                     uint32_t read_len,
                                     pal_i2c_result_t *result);

wink_status_t js_pal_i2c_session_open(uint8_t port, uint16_t dev_addr,
                                      uint8_t direction,
                                      uint8_t *out_session,
                                      pal_i2c_result_t *result);

wink_status_t js_pal_i2c_session_restart(uint8_t session_id,
                                         uint16_t dev_addr,
                                         uint8_t direction,
                                         pal_i2c_result_t *result);

wink_status_t js_pal_i2c_session_write(uint8_t session_id,
                                       const uint8_t *buf, uint32_t len,
                                       pal_i2c_result_t *result);

wink_status_t js_pal_i2c_session_read(uint8_t session_id, uint8_t *buf,
                                      uint32_t len, uint8_t ack_mode,
                                      pal_i2c_result_t *result);

wink_status_t js_pal_i2c_session_close(uint8_t session_id);

// -- ADR-0087 SPI status + session stream --------------------------------

wink_status_t js_pal_spi_transfer_ex(uint8_t port, uint16_t device_id,
                                     const uint8_t *tx_buf, uint32_t len,
                                     uint8_t *rx_buf, uint8_t mode,
                                     uint32_t sck_hz);

wink_status_t js_pal_spi_session_open(uint8_t port, uint16_t device_id,
                                      uint8_t mode, uint32_t sck_hz,
                                      uint8_t *out_session);

wink_status_t js_pal_spi_session_transfer(uint8_t session_id,
                                          const uint8_t *tx_buf,
                                          uint8_t *rx_buf, uint32_t len);

wink_status_t js_pal_spi_session_close(uint8_t session_id);

#ifdef __cplusplus
}  // extern "C"
#endif
