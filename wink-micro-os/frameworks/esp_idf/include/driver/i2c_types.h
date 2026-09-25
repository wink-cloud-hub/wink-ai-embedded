/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef DRIVER_I2C_TYPES_H_
#define DRIVER_I2C_TYPES_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "hal/i2c_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int i2c_port_num_t;

typedef struct i2c_master_bus_t *i2c_master_bus_handle_t;
typedef struct i2c_master_dev_t *i2c_master_dev_handle_t;

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_I2C_TYPES_H_ */
