/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_DRIVER_I2C_SLAVE_H
#define WINK_H_GUARD_DRIVER_I2C_SLAVE_H
#ifndef __WINK_HARVESTED_DRIVER_I2C_SLAVE_H__
#define __WINK_HARVESTED_DRIVER_I2C_SLAVE_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdint.h>

#include "driver/i2c_types.h"
#include "esp_err.h"
#include "hal/gpio_types.h"
#include "sdkconfig.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
i2c_port_num_t i2c_port;                 
    gpio_num_t sda_io_num;                   
    gpio_num_t scl_io_num;                   
    i2c_clock_source_t clk_source;           
    uint32_t send_buf_depth;                 
    uint32_t receive_buf_depth;              
    uint16_t slave_addr;                     
    i2c_addr_bit_len_t addr_bit_len;         
    int intr_priority;                       
    struct {
        uint32_t allow_pd:    1;  


        uint32_t enable_internal_pullup: 1;  

        uint32_t broadcast_en: 1;            

    } flags;
} i2c_slave_config_t;
typedef struct {
    i2c_slave_request_callback_t on_request;
    i2c_slave_received_callback_t on_receive;
} i2c_slave_event_callbacks_t;

esp_err_t i2c_new_slave_device(const i2c_slave_config_t *slave_config, i2c_slave_dev_handle_t *ret_handle);


#if defined(__WINK_SIM__)
esp_err_t i2c_del_slave_device(i2c_slave_dev_handle_t i2c_slave) WINK_SLA_ERROR("Wink SLA Violation: i2c_del_slave_device out of Core 8 scope.");
#else
esp_err_t i2c_del_slave_device(i2c_slave_dev_handle_t i2c_slave);
#endif

#if defined(__WINK_SIM__)
esp_err_t i2c_slave_register_event_callbacks(i2c_slave_dev_handle_t i2c_slave, const i2c_slave_event_callbacks_t *cbs, void *user_data) WINK_SLA_ERROR("Wink SLA Violation: i2c_slave_register_event_callbacks out of Core 8 scope.");
#else
esp_err_t i2c_slave_register_event_callbacks(i2c_slave_dev_handle_t i2c_slave, const i2c_slave_event_callbacks_t *cbs, void *user_data);
#endif

#if defined(__WINK_SIM__)
esp_err_t i2c_slave_write(i2c_slave_dev_handle_t i2c_slave, const uint8_t *data, uint32_t len, uint32_t *write_len, int timeout_ms) WINK_SLA_ERROR("Wink SLA Violation: i2c_slave_write out of Core 8 scope.");
#else
esp_err_t i2c_slave_write(i2c_slave_dev_handle_t i2c_slave, const uint8_t *data, uint32_t len, uint32_t *write_len, int timeout_ms);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_DRIVER_I2C_SLAVE_H__ */
#endif /* WINK_H_GUARD_DRIVER_I2C_SLAVE_H */
