/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_DRIVER_UHCI_H
#define WINK_H_GUARD_DRIVER_UHCI_H
#ifndef __WINK_HARVESTED_DRIVER_UHCI_H__
#define __WINK_HARVESTED_DRIVER_UHCI_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
uart_port_t uart_port;                                
    size_t tx_trans_queue_depth;                          
    size_t max_transmit_size;                             
    size_t max_transmit_buffer_count;                     
    size_t max_receive_internal_mem;                      
    size_t dma_burst_size;                                
    size_t max_packet_receive;                            

    struct {
        uint16_t rx_brk_eof: 1;                           
        uint16_t idle_eof: 1;                             
        uint16_t length_eof: 1;                           
    } rx_eof_flags;
} uhci_controller_config_t;
typedef struct {
    uhci_rx_event_callback_t on_rx_trans_event;
    uhci_tx_done_callback_t on_tx_trans_done;
} uhci_event_callbacks_t;
typedef struct {
    const uint8_t * write_buffer;
    size_t buffer_size;
} uhci_transmit_buffer_info_t;



#if defined(__WINK_SIM__)
esp_err_t uhci_del_controller(uhci_controller_handle_t uhci_ctrl) WINK_SLA_ERROR("Wink SLA Violation: uhci_del_controller out of Core 8 scope.");
#else
esp_err_t uhci_del_controller(uhci_controller_handle_t uhci_ctrl);
#endif

#if defined(__WINK_SIM__)
esp_err_t uhci_multi_buffer_transmit(uhci_controller_handle_t uhci_ctrl, const uhci_transmit_buffer_info_t *buffer_info_array, size_t array_size) WINK_SLA_ERROR("Wink SLA Violation: uhci_multi_buffer_transmit out of Core 8 scope.");
#else
esp_err_t uhci_multi_buffer_transmit(uhci_controller_handle_t uhci_ctrl, const uhci_transmit_buffer_info_t *buffer_info_array, size_t array_size);
#endif

#if defined(__WINK_SIM__)
esp_err_t uhci_new_controller(const uhci_controller_config_t *config, uhci_controller_handle_t *ret_uhci_ctrl) WINK_SLA_ERROR("Wink SLA Violation: uhci_new_controller out of Core 8 scope.");
#else
esp_err_t uhci_new_controller(const uhci_controller_config_t *config, uhci_controller_handle_t *ret_uhci_ctrl);
#endif

#if defined(__WINK_SIM__)
esp_err_t uhci_receive(uhci_controller_handle_t uhci_ctrl, uint8_t *read_buffer, size_t buffer_size) WINK_SLA_ERROR("Wink SLA Violation: uhci_receive out of Core 8 scope.");
#else
esp_err_t uhci_receive(uhci_controller_handle_t uhci_ctrl, uint8_t *read_buffer, size_t buffer_size);
#endif

#if defined(__WINK_SIM__)
esp_err_t uhci_register_event_callbacks(uhci_controller_handle_t uhci_ctrl, const uhci_event_callbacks_t *cbs, void *user_data) WINK_SLA_ERROR("Wink SLA Violation: uhci_register_event_callbacks out of Core 8 scope.");
#else
esp_err_t uhci_register_event_callbacks(uhci_controller_handle_t uhci_ctrl, const uhci_event_callbacks_t *cbs, void *user_data);
#endif

#if defined(__WINK_SIM__)
esp_err_t uhci_transmit(uhci_controller_handle_t uhci_ctrl, uint8_t *write_buffer, size_t write_size) WINK_SLA_ERROR("Wink SLA Violation: uhci_transmit out of Core 8 scope.");
#else
esp_err_t uhci_transmit(uhci_controller_handle_t uhci_ctrl, uint8_t *write_buffer, size_t write_size);
#endif

#if defined(__WINK_SIM__)
esp_err_t uhci_wait_all_tx_transaction_done(uhci_controller_handle_t uhci_ctrl, int timeout_ms) WINK_SLA_ERROR("Wink SLA Violation: uhci_wait_all_tx_transaction_done out of Core 8 scope.");
#else
esp_err_t uhci_wait_all_tx_transaction_done(uhci_controller_handle_t uhci_ctrl, int timeout_ms);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_DRIVER_UHCI_H__ */
#endif /* WINK_H_GUARD_DRIVER_UHCI_H */
