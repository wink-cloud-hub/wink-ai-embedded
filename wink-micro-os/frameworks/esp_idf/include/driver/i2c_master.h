/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_DRIVER_I2C_MASTER_H
#define WINK_H_GUARD_DRIVER_I2C_MASTER_H
#ifndef __WINK_HARVESTED_DRIVER_I2C_MASTER_H__
#define __WINK_HARVESTED_DRIVER_I2C_MASTER_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdint.h>

#include "driver/i2c_types.h"
#include "esp_err.h"
#include "hal/gpio_types.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef I2C_DEVICE_ADDRESS_NOT_USED
#define I2C_DEVICE_ADDRESS_NOT_USED (0xffff)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
i2c_port_num_t i2c_port;              
    gpio_num_t sda_io_num;                
    gpio_num_t scl_io_num;                
    union {
        i2c_clock_source_t clk_source;        


    };
    uint8_t glitch_ignore_cnt;            
    int intr_priority;                    
    size_t trans_queue_depth;             
    struct {
        uint32_t enable_internal_pullup: 1;  
        uint32_t allow_pd:               1;  


    } flags;
} i2c_master_bus_config_t;
typedef struct {
i2c_addr_bit_len_t dev_addr_length;         
    uint16_t device_address;                    
    uint32_t scl_speed_hz;                      
    uint32_t scl_wait_us;                      
    struct {
        uint32_t disable_ack_check:      1;     
    } flags;
} i2c_device_config_t;
typedef struct {
i2c_master_command_t command; 
    union {
        




        struct {
            bool ack_check;        
            const uint8_t *data;   
            size_t total_bytes;    
        } write;
        




        struct {
            i2c_ack_value_t ack_value; 
            uint8_t *data;                    
            size_t total_bytes;               
        } read;
    };
} i2c_operation_job_t;
typedef struct {
    const uint8_t * write_buffer;
    size_t buffer_size;
} i2c_master_transmit_multi_buffer_info_t;
typedef struct {
    i2c_master_callback_t on_trans_done;
} i2c_master_event_callbacks_t;

esp_err_t i2c_del_master_bus(i2c_master_bus_handle_t bus_handle);
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t bus_handle, const i2c_device_config_t *dev_config, i2c_master_dev_handle_t *ret_handle);
esp_err_t i2c_master_bus_reset(i2c_master_bus_handle_t bus_handle);
esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t handle);
esp_err_t i2c_master_bus_wait_all_done(i2c_master_bus_handle_t bus_handle, int timeout_ms);
esp_err_t i2c_master_device_change_address(i2c_master_dev_handle_t i2c_dev, uint16_t new_device_address, int timeout_ms);
esp_err_t i2c_master_execute_defined_operations(i2c_master_dev_handle_t i2c_dev, i2c_operation_job_t *i2c_operation, size_t operation_list_num, int xfer_timeout_ms);
esp_err_t i2c_master_get_bus_handle(i2c_port_num_t port_num, i2c_master_bus_handle_t *ret_handle);
esp_err_t i2c_master_multi_buffer_transmit(i2c_master_dev_handle_t i2c_dev, i2c_master_transmit_multi_buffer_info_t *buffer_info_array, size_t array_size, int xfer_timeout_ms);
esp_err_t i2c_master_probe(i2c_master_bus_handle_t bus_handle, uint16_t address, int xfer_timeout_ms);
esp_err_t i2c_master_receive(i2c_master_dev_handle_t i2c_dev, uint8_t *read_buffer, size_t read_size, int xfer_timeout_ms);
esp_err_t i2c_master_register_event_callbacks(i2c_master_dev_handle_t i2c_dev, const i2c_master_event_callbacks_t *cbs, void *user_data);
esp_err_t i2c_master_transmit(i2c_master_dev_handle_t i2c_dev, const uint8_t *write_buffer, size_t write_size, int xfer_timeout_ms);
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t i2c_dev, const uint8_t *write_buffer, size_t write_size, uint8_t *read_buffer, size_t read_size, int xfer_timeout_ms);
esp_err_t i2c_new_master_bus(const i2c_master_bus_config_t *bus_config, i2c_master_bus_handle_t *ret_bus_handle);


#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_DRIVER_I2C_MASTER_H__ */
#endif /* WINK_H_GUARD_DRIVER_I2C_MASTER_H */
