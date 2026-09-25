/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef HAL_I2C_TYPES_H_
#define HAL_I2C_TYPES_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    I2C_NUM_0 = 0,
#if SOC_HP_I2C_NUM >= 2
    I2C_NUM_1 = 1,
#endif
    I2C_NUM_MAX,
} i2c_port_t;

typedef enum {
    I2C_ADDR_BIT_LEN_7 = 0,
    I2C_ADDR_BIT_LEN_10 = 1,
} i2c_addr_bit_len_t;

typedef enum {
    I2C_MODE_SLAVE = 0,
    I2C_MODE_MASTER = 1,
    I2C_MODE_MAX,
} i2c_mode_t;

typedef enum {
    I2C_MASTER_WRITE = 0,
    I2C_MASTER_READ = 1,
} i2c_rw_t;

typedef enum {
    I2C_MASTER_ACK = 0x0,
    I2C_MASTER_NACK = 0x1,
    I2C_MASTER_LAST_NACK = 0x2,
    I2C_MASTER_ACK_MAX,
} i2c_ack_type_t;

typedef enum {
    I2C_CLK_SRC_DEFAULT = 0,
    I2C_CLK_SRC_XTAL,
    I2C_CLK_SRC_RC_FAST,
} i2c_clock_source_t;

#ifdef __cplusplus
}
#endif

#endif /* HAL_I2C_TYPES_H_ */
