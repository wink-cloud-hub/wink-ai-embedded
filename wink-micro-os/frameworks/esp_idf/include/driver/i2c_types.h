/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_DRIVER_I2C_TYPES_H
#define WINK_H_GUARD_DRIVER_I2C_TYPES_H
#ifndef __WINK_HARVESTED_DRIVER_I2C_TYPES_H__
#define __WINK_HARVESTED_DRIVER_I2C_TYPES_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>

#include "hal/i2c_types.h"
#include "sdkconfig.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    I2C_STATUS_READ = 0,
    I2C_STATUS_READ_ALL = 1,
    I2C_STATUS_WRITE = 2,
    I2C_STATUS_START = 3,
    I2C_STATUS_STOP = 4,
    I2C_STATUS_IDLE = 5,
    I2C_STATUS_ACK_ERROR = 6,
    I2C_STATUS_DONE = 7,
    I2C_STATUS_TIMEOUT = 8,
} i2c_master_status_t;
typedef enum {
    I2C_EVENT_ALIVE = 0,
    I2C_EVENT_DONE = 1,
    I2C_EVENT_NACK = 2,
    I2C_EVENT_TIMEOUT = 3,
} i2c_master_event_t;
typedef enum {
    I2C_MASTER_CMD_START = 0,
    I2C_MASTER_CMD_WRITE = 1,
    I2C_MASTER_CMD_READ = 2,
    I2C_MASTER_CMD_STOP = 3,
} i2c_master_command_t;
typedef enum {
    I2C_ACK_VAL = 0,
    I2C_NACK_VAL = 1,
} __attribute__((packed)) i2c_ack_value_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef int i2c_port_num_t;
typedef struct i2c_master_bus_t * i2c_master_bus_handle_t;
typedef struct i2c_master_dev_t * i2c_master_dev_handle_t;
typedef struct i2c_slave_dev_t * i2c_slave_dev_handle_t;
typedef struct {
    i2c_master_event_t event;
} i2c_master_event_data_t;
typedef bool (*i2c_master_callback_t)(i2c_master_dev_handle_t i2c_dev, const i2c_master_event_data_t *evt_data, void *arg);
typedef struct {
    uint8_t * buffer;
    uint32_t length;
} i2c_slave_rx_done_event_data_t;
typedef bool (*i2c_slave_received_callback_t)(i2c_slave_dev_handle_t i2c_slave, const i2c_slave_rx_done_event_data_t *evt_data, void *arg);
typedef struct {
} i2c_slave_request_event_data_t;
typedef bool (*i2c_slave_request_callback_t)(i2c_slave_dev_handle_t i2c_slave, const i2c_slave_request_event_data_t *evt_data, void *arg);



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_DRIVER_I2C_TYPES_H__ */
#endif /* WINK_H_GUARD_DRIVER_I2C_TYPES_H */
