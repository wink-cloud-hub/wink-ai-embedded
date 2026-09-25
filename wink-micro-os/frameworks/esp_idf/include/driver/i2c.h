/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef DRIVER_I2C_H_
#define DRIVER_I2C_H_

#include "driver/i2c_types_legacy.h"
#include "freertos/FreeRTOS.h"

#pragma message("Notice: Legacy driver/i2c.h is deprecated in ESP-IDF v6+. Please consider migrating to driver/i2c_master.h.")

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t i2c_driver_install(i2c_port_t i2c_num, i2c_mode_t mode, size_t slv_rx_buf_len, size_t slv_tx_buf_len, int intr_alloc_flags);
esp_err_t i2c_driver_delete(i2c_port_t i2c_num);
esp_err_t i2c_param_config(i2c_port_t i2c_num, const i2c_config_t *i2c_conf);

i2c_cmd_handle_t i2c_cmd_link_create(void);
void i2c_cmd_link_delete(i2c_cmd_handle_t cmd_handle);

esp_err_t i2c_master_start(i2c_cmd_handle_t cmd_handle);
esp_err_t i2c_master_write_byte(i2c_cmd_handle_t cmd_handle, uint8_t data, bool ack_en);
esp_err_t i2c_master_write(i2c_cmd_handle_t cmd_handle, const uint8_t *data, size_t data_len, bool ack_en);
esp_err_t i2c_master_read_byte(i2c_cmd_handle_t cmd_handle, uint8_t *data, i2c_ack_type_t ack);
esp_err_t i2c_master_read(i2c_cmd_handle_t cmd_handle, uint8_t *data, size_t data_len, i2c_ack_type_t ack);
esp_err_t i2c_master_stop(i2c_cmd_handle_t cmd_handle);
esp_err_t i2c_master_cmd_begin(i2c_port_t i2c_num, i2c_cmd_handle_t cmd_handle, TickType_t ticks_to_wait);

esp_err_t i2c_set_pin(i2c_port_t i2c_num, int sda_io_num, int scl_io_num, bool sda_pullup_en, bool scl_pullup_en, i2c_mode_t mode);
esp_err_t i2c_reset_tx_fifo(i2c_port_t i2c_num);
esp_err_t i2c_reset_rx_fifo(i2c_port_t i2c_num);

int i2c_slave_write_buffer(i2c_port_t i2c_num, const uint8_t *data, int size, TickType_t ticks_to_wait);
int i2c_slave_read_buffer(i2c_port_t i2c_num, uint8_t *data, size_t max_size, TickType_t ticks_to_wait);

esp_err_t i2c_set_period(i2c_port_t i2c_num, int high_period, int low_period);
esp_err_t i2c_get_period(i2c_port_t i2c_num, int *high_period, int *low_period);

esp_err_t i2c_set_start_timing(i2c_port_t i2c_num, int setup_time, int hold_time);
esp_err_t i2c_get_start_timing(i2c_port_t i2c_num, int *setup_time, int *hold_time);

esp_err_t i2c_set_stop_timing(i2c_port_t i2c_num, int setup_time, int hold_time);
esp_err_t i2c_get_stop_timing(i2c_port_t i2c_num, int *setup_time, int *hold_time);

esp_err_t i2c_set_data_timing(i2c_port_t i2c_num, int sample_time, int hold_time);
esp_err_t i2c_get_data_timing(i2c_port_t i2c_num, int *sample_time, int *hold_time);

esp_err_t i2c_set_timeout(i2c_port_t i2c_num, int tout_cycle);
esp_err_t i2c_get_timeout(i2c_port_t i2c_num, int *tout_cycle);

void esp_i2c_legacy_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_I2C_H_ */
