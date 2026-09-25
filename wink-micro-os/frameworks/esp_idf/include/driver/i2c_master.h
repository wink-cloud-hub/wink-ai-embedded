/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef DRIVER_I2C_MASTER_H_
#define DRIVER_I2C_MASTER_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "driver/i2c_types.h"
#include "hal/gpio_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    i2c_port_num_t i2c_port;
    gpio_num_t sda_io_num;
    gpio_num_t scl_io_num;
    i2c_clock_source_t clk_source;
    uint8_t glitch_ignore_cnt;
    int intr_priority;
    size_t trans_queue_depth;
    struct {
        uint32_t enable_internal_pullup: 1;
        uint32_t allow_pd: 1;
    } flags;
} i2c_master_bus_config_t;

typedef struct {
    i2c_addr_bit_len_t dev_addr_length;
    uint16_t device_address;
    uint32_t scl_speed_hz;
    uint32_t scl_wait_us;
    struct {
        uint32_t disable_ack_check: 1;
    } flags;
} i2c_device_config_t;

esp_err_t i2c_new_master_bus(const i2c_master_bus_config_t *bus_config, i2c_master_bus_handle_t *ret_bus_handle);
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t bus_handle, const i2c_device_config_t *dev_config, i2c_master_dev_handle_t *ret_handle);
esp_err_t i2c_del_master_bus(i2c_master_bus_handle_t bus_handle);
esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t handle);

esp_err_t i2c_master_transmit(i2c_master_dev_handle_t handle, const uint8_t *write_buffer, size_t write_size, int xfer_timeout_ms);
esp_err_t i2c_master_receive(i2c_master_dev_handle_t handle, uint8_t *read_buffer, size_t read_size, int xfer_timeout_ms);
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t handle, const uint8_t *write_buffer, size_t write_size, uint8_t *read_buffer, size_t read_size, int xfer_timeout_ms);
esp_err_t i2c_master_probe(i2c_master_bus_handle_t bus_handle, uint16_t address, int xfer_timeout_ms);

void esp_i2c_master_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_I2C_MASTER_H_ */
