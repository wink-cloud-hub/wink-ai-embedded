/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_I2C_TYPES_H
#define WINK_H_GUARD_HAL_I2C_TYPES_H
#ifndef __WINK_HARVESTED_HAL_I2C_TYPES_H__
#define __WINK_HARVESTED_HAL_I2C_TYPES_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>

#include "hal/hal_utils.h"
#include "soc/clk_tree_defs.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    I2C_NUM_0 = 0,
    I2C_NUM_1 = 1,
    I2C_NUM_MAX = 2,
} i2c_port_t;
typedef enum {
    I2C_ADDR_BIT_LEN_7 = 0,
    I2C_ADDR_BIT_LEN_10 = 1,
} i2c_addr_bit_len_t;
typedef enum {
    I2C_MODE_SLAVE = 0,
    I2C_MODE_MASTER = 1,
    I2C_MODE_MAX = 2,
} i2c_mode_t;
typedef enum {
    I2C_MASTER_WRITE = 0,
    I2C_MASTER_READ = 1,
} i2c_rw_t;
typedef enum {
    I2C_DATA_MODE_MSB_FIRST = 0,
    I2C_DATA_MODE_LSB_FIRST = 1,
    I2C_DATA_MODE_MAX = 2,
} i2c_trans_mode_t;
typedef enum {
    I2C_ADDR_BIT_7 = 0,
    I2C_ADDR_BIT_10 = 1,
    I2C_ADDR_BIT_MAX = 2,
} i2c_addr_mode_t;
typedef enum {
    I2C_MASTER_ACK = 0,
    I2C_MASTER_NACK = 1,
    I2C_MASTER_LAST_NACK = 2,
    I2C_MASTER_ACK_MAX = 3,
} i2c_ack_type_t;
typedef enum {
    I2C_SLAVE_STRETCH_CAUSE_ADDRESS_MATCH = 0,
    I2C_SLAVE_STRETCH_CAUSE_TX_EMPTY = 1,
    I2C_SLAVE_STRETCH_CAUSE_RX_FULL = 2,
    I2C_SLAVE_STRETCH_CAUSE_SENDING_ACK = 3,
} i2c_slave_stretch_cause_t;
typedef enum {
    I2C_SLAVE_WRITE_BY_MASTER = 0,
    I2C_SLAVE_READ_BY_MASTER = 1,
} i2c_slave_read_write_status_t;
typedef enum {
    I2C_BUS_MODE_MASTER = 0,
    I2C_BUS_MODE_SLAVE = 1,
} i2c_bus_mode_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    uint16_t clkm_div;
    uint16_t scl_low;
    uint16_t scl_high;
    uint16_t scl_wait_high;
    uint16_t sda_hold;
    uint16_t sda_sample;
    uint16_t setup;
    uint16_t hold;
    uint16_t tout;
} i2c_hal_clk_config_t;
typedef soc_periph_i2c_clk_src_t i2c_clock_source_t;



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_I2C_TYPES_H__ */
#endif /* WINK_H_GUARD_HAL_I2C_TYPES_H */
