/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_DRIVER_UART_H
#define WINK_H_GUARD_DRIVER_UART_H
#ifndef __WINK_HARVESTED_DRIVER_UART_H__
#define __WINK_HARVESTED_DRIVER_UART_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "esp_check.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "hal/uart_types.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef UART_BITRATE_MAX
#define UART_BITRATE_MAX SOC_UART_BITRATE_MAX
#endif
#ifndef UART_HW_FIFO_LEN
#define UART_HW_FIFO_LEN(uart_num) SOC_UART_FIFO_LEN
#endif
#ifndef UART_PIN_NO_CHANGE
#define UART_PIN_NO_CHANGE (-1)
#endif
#ifndef _GET_UART_SET_PIN_FUNC_NAME
#define _GET_UART_SET_PIN_FUNC_NAME(_1, _2, _3, _4, _5, _6, _7, _FUNC_NAME, ...) _FUNC_NAME
#endif
#ifndef uart_set_pin
#define uart_set_pin(...) _GET_UART_SET_PIN_FUNC_NAME(__VA_ARGS__,  _uart_set_pin6, __uart_set_pin_invalid_args__, _uart_set_pin4,  __uart_set_pin_invalid_args__, __uart_set_pin_invalid_args__, __uart_set_pin_invalid_args__)(__VA_ARGS__)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    UART_DATA = 0,
    UART_BREAK = 1,
    UART_BUFFER_FULL = 2,
    UART_FIFO_OVF = 3,
    UART_FRAME_ERR = 4,
    UART_PARITY_ERR = 5,
    UART_DATA_BREAK = 6,
    UART_PATTERN_DET = 7,
    UART_EVENT_MAX = 8,
} uart_event_type_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
int baud_rate;                      

    uart_word_length_t data_bits;       
    uart_parity_t parity;               
    uart_stop_bits_t stop_bits;         
    uart_hw_flowcontrol_t flow_ctrl;    
    uint8_t rx_flow_ctrl_thresh;        
    uint32_t rx_glitch_filt_thresh;     
    union {
        uart_sclk_t source_clk;             


    };
    struct {
        uint32_t allow_pd: 1;               

        uint32_t backup_before_sleep: 1;    
    } flags;
} uart_config_t;
typedef struct {
    uint32_t intr_enable_mask;
    uint8_t rx_timeout_thresh;
    uint8_t txfifo_empty_intr_thresh;
    uint8_t rxfifo_full_thresh;
} uart_intr_config_t;
typedef struct {
    uart_event_type_t type;
    size_t size;
    bool timeout_flag;
} uart_event_t;
typedef struct {
    int rx_io_num;
    uart_sclk_t source_clk;
    uint32_t rx_glitch_filt_thresh;
} uart_bitrate_detect_config_t;
typedef struct {
    uint32_t low_period;
    uint32_t high_period;
    uint32_t pos_period;
    uint32_t neg_period;
    uint32_t edge_cnt;
    uint32_t clk_freq_hz;
} uart_bitrate_res_t;

esp_err_t __uart_set_pin_invalid_args__(int dummy, ...);
esp_err_t _uart_set_pin4(uart_port_t uart_num, int tx_io_num, int rx_io_num, int rts_io_num, int cts_io_num);
esp_err_t _uart_set_pin6(uart_port_t uart_num, int tx_io_num, int rx_io_num, int rts_io_num, int cts_io_num, int dtr_io_num, int dsr_io_num);
esp_err_t uart_clear_intr_status(uart_port_t uart_num, uint32_t clr_mask);
esp_err_t uart_detect_bitrate_start(uart_port_t uart_num, const uart_bitrate_detect_config_t *config);
esp_err_t uart_detect_bitrate_stop(uart_port_t uart_num, bool deinit, uart_bitrate_res_t *ret_res);
esp_err_t uart_disable_intr_mask(uart_port_t uart_num, uint32_t disable_mask);
esp_err_t uart_disable_pattern_det_intr(uart_port_t uart_num);
esp_err_t uart_disable_rx_intr(uart_port_t uart_num);
esp_err_t uart_disable_tx_intr(uart_port_t uart_num);
esp_err_t uart_driver_delete(uart_port_t uart_num);
esp_err_t uart_driver_install(uart_port_t uart_num, int rx_buffer_size, int tx_buffer_size, int queue_size, QueueHandle_t* uart_queue, int intr_alloc_flags);
esp_err_t uart_enable_intr_mask(uart_port_t uart_num, uint32_t enable_mask);
esp_err_t uart_enable_pattern_det_baud_intr(uart_port_t uart_num, char pattern_chr, uint8_t chr_num, int chr_tout, int post_idle, int pre_idle);
esp_err_t uart_enable_rx_intr(uart_port_t uart_num);
esp_err_t uart_enable_tx_intr(uart_port_t uart_num, int enable, int thresh);
esp_err_t uart_flush(uart_port_t uart_num);
esp_err_t uart_flush_input(uart_port_t uart_num);
esp_err_t uart_get_baudrate(uart_port_t uart_num, uint32_t* baudrate);
esp_err_t uart_get_buffered_data_len(uart_port_t uart_num, size_t* size);
esp_err_t uart_get_collision_flag(uart_port_t uart_num, bool* collision_flag);
esp_err_t uart_get_hw_flow_ctrl(uart_port_t uart_num, uart_hw_flowcontrol_t* flow_ctrl);
esp_err_t uart_get_parity(uart_port_t uart_num, uart_parity_t* parity_mode);
esp_err_t uart_get_sclk_freq(uart_sclk_t sclk, uint32_t* out_freq_hz);
esp_err_t uart_get_stop_bits(uart_port_t uart_num, uart_stop_bits_t* stop_bits);
esp_err_t uart_get_tx_buffer_free_size(uart_port_t uart_num, size_t *size);
esp_err_t uart_get_wakeup_threshold(uart_port_t uart_num, int* out_wakeup_threshold);
esp_err_t uart_get_word_length(uart_port_t uart_num, uart_word_length_t* data_bit);
esp_err_t uart_intr_config(uart_port_t uart_num, const uart_intr_config_t *intr_conf);
bool uart_is_driver_installed(uart_port_t uart_num);
esp_err_t uart_param_config(uart_port_t uart_num, const uart_config_t *uart_config);
int uart_pattern_get_pos(uart_port_t uart_num);
int uart_pattern_pop_pos(uart_port_t uart_num);
esp_err_t uart_pattern_queue_reset(uart_port_t uart_num, int queue_length);
int uart_read_bytes(uart_port_t uart_num, void* buf, uint32_t length, uint32_t ticks_to_wait);
void uart_set_always_rx_timeout(uart_port_t uart_num, bool always_rx_timeout_en);
esp_err_t uart_set_baudrate(uart_port_t uart_num, uint32_t baudrate);
esp_err_t uart_set_dtr(uart_port_t uart_num, int level);
esp_err_t uart_set_hw_flow_ctrl(uart_port_t uart_num, uart_hw_flowcontrol_t flow_ctrl, uint8_t rx_thresh);
esp_err_t uart_set_line_inverse(uart_port_t uart_num, uint32_t inverse_mask);
esp_err_t uart_set_loop_back(uart_port_t uart_num, bool loop_back_en);
esp_err_t uart_set_mode(uart_port_t uart_num, uart_mode_t mode);
esp_err_t uart_set_parity(uart_port_t uart_num, uart_parity_t parity_mode);
esp_err_t uart_set_rts(uart_port_t uart_num, int level);
esp_err_t uart_set_rx_full_threshold(uart_port_t uart_num, int threshold);
esp_err_t uart_set_rx_timeout(uart_port_t uart_num, const uint8_t tout_thresh);
esp_err_t uart_set_stop_bits(uart_port_t uart_num, uart_stop_bits_t stop_bits);
esp_err_t uart_set_sw_flow_ctrl(uart_port_t uart_num, bool enable,  uint8_t rx_thresh_xon,  uint8_t rx_thresh_xoff);
esp_err_t uart_set_tx_empty_threshold(uart_port_t uart_num, int threshold);
esp_err_t uart_set_tx_idle_num(uart_port_t uart_num, uint16_t idle_num);
esp_err_t uart_set_wakeup_threshold(uart_port_t uart_num, int wakeup_threshold);
esp_err_t uart_set_word_length(uart_port_t uart_num, uart_word_length_t data_bit);
int uart_tx_chars(uart_port_t uart_num, const char* buffer, uint32_t len);
esp_err_t uart_wait_tx_done(uart_port_t uart_num, uint32_t ticks_to_wait);
esp_err_t uart_wait_tx_idle_polling(uart_port_t uart_num);
int uart_write_bytes(uart_port_t uart_num, const void* src, size_t size);
int uart_write_bytes_with_break(uart_port_t uart_num, const void* src, size_t size, int brk_len);


#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_DRIVER_UART_H__ */
#endif /* WINK_H_GUARD_DRIVER_UART_H */
