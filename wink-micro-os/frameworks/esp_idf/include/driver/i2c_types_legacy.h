/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef DRIVER_I2C_TYPES_LEGACY_H_
#define DRIVER_I2C_TYPES_LEGACY_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "soc/soc_caps.h"
#include "hal/i2c_types.h"
#include "hal/gpio_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define I2C_SCLK_SRC_FLAG_FOR_NOMAL   (0)
#define I2C_INTERNAL_STRUCT_SIZE      (24)

typedef struct {
    i2c_mode_t mode;
    gpio_num_t sda_io_num;
    gpio_num_t scl_io_num;
    bool sda_pullup_en;
    bool scl_pullup_en;
    union {
        struct {
            uint32_t clk_speed;
        } master;
        struct {
            uint8_t addr_10bit_en;
            uint16_t slave_addr;
            uint32_t maximum_speed;
        } slave;
    };
    uint32_t clk_flags;
} i2c_config_t;

typedef void *i2c_cmd_handle_t;

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_I2C_TYPES_LEGACY_H_ */
