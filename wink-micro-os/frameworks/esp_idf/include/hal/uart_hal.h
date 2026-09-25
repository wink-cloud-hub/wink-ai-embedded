/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_UART_HAL_H
#define WINK_H_GUARD_HAL_UART_HAL_H
#ifndef __WINK_HARVESTED_HAL_UART_HAL_H__
#define __WINK_HARVESTED_HAL_UART_HAL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "hal/uart_types.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef uart_hal_clr_intsts_mask
#define uart_hal_clr_intsts_mask(hal, mask) uart_ll_clr_intsts_mask((hal)->dev, mask)
#endif
#ifndef uart_hal_disable_intr_mask
#define uart_hal_disable_intr_mask(hal, mask) uart_ll_disable_intr_mask((hal)->dev, mask)
#endif
#ifndef uart_hal_ena_intr_mask
#define uart_hal_ena_intr_mask(hal, mask) uart_ll_ena_intr_mask((hal)->dev, mask)
#endif
#ifndef uart_hal_enable_glitch_filt
#define uart_hal_enable_glitch_filt(hal, enable) uart_ll_enable_glitch_filt((hal)->dev, enable)
#endif
#ifndef uart_hal_get_at_cmd_char
#define uart_hal_get_at_cmd_char(hal, cmd_char, char_num) uart_ll_get_at_cmd_char((hal)->dev, cmd_char, char_num)
#endif
#ifndef uart_hal_get_high_pulse_cnt
#define uart_hal_get_high_pulse_cnt(hal) uart_ll_get_high_pulse_cnt((hal)->dev)
#endif
#ifndef uart_hal_get_intr_ena_status
#define uart_hal_get_intr_ena_status(hal) uart_ll_get_intr_ena_status((hal)->dev)
#endif
#ifndef uart_hal_get_intr_status_reg
#define uart_hal_get_intr_status_reg(hal) uart_ll_get_intr_status_reg((hal)->dev)
#endif
#ifndef uart_hal_get_intraw_mask
#define uart_hal_get_intraw_mask(hal) uart_ll_get_intraw_mask((hal)->dev)
#endif
#ifndef uart_hal_get_intsts_mask
#define uart_hal_get_intsts_mask(hal) uart_ll_get_intsts_mask((hal)->dev)
#endif
#ifndef uart_hal_get_low_pulse_cnt
#define uart_hal_get_low_pulse_cnt(hal) uart_ll_get_low_pulse_cnt((hal)->dev)
#endif
#ifndef uart_hal_get_neg_pulse_cnt
#define uart_hal_get_neg_pulse_cnt(hal) uart_ll_get_neg_pulse_cnt((hal)->dev)
#endif
#ifndef uart_hal_get_pos_pulse_cnt
#define uart_hal_get_pos_pulse_cnt(hal) uart_ll_get_pos_pulse_cnt((hal)->dev)
#endif
#ifndef uart_hal_get_rx_tout_thr
#define uart_hal_get_rx_tout_thr(hal) uart_ll_get_rx_tout_thr((hal)->dev)
#endif
#ifndef uart_hal_get_rxd_edge_cnt
#define uart_hal_get_rxd_edge_cnt(hal) uart_ll_get_rxd_edge_cnt((hal)->dev)
#endif
#ifndef uart_hal_get_rxfifo_len
#define uart_hal_get_rxfifo_len(hal) uart_ll_get_rxfifo_len((hal)->dev)
#endif
#ifndef uart_hal_get_txfifo_len
#define uart_hal_get_txfifo_len(hal) uart_ll_get_txfifo_len((hal)->dev)
#endif
#ifndef uart_hal_is_tx_idle
#define uart_hal_is_tx_idle(hal) uart_ll_is_tx_idle((hal)->dev)
#endif
#ifndef uart_hal_set_autobaud_en
#define uart_hal_set_autobaud_en(hal, enable) uart_ll_set_autobaud_en((hal)->dev, enable)
#endif
#ifndef uart_hal_set_baudrate
#define uart_hal_set_baudrate(hal, baud_rate, sclk_freq) uart_ll_set_baudrate((hal)->dev, baud_rate, sclk_freq)
#endif
#ifndef uart_hal_set_glitch_filt_thrd
#define uart_hal_set_glitch_filt_thrd(hal, glitch_filt_thrd, sclk_freq) uart_ll_set_glitch_filt_thrd((hal)->dev, glitch_filt_thrd, sclk_freq)
#endif
#ifndef uart_hal_set_rts
#define uart_hal_set_rts(hal, active_level) uart_ll_set_rts_active_level((hal)->dev, active_level)
#endif
#ifndef uart_hal_set_sclk
#define uart_hal_set_sclk(hal, sclk) uart_ll_set_sclk((hal)->dev, sclk);
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    uart_dev_t * dev;
} uart_hal_context_t;

void uart_hal_get_baudrate(uart_hal_context_t *hal, uint32_t *baud_rate, uint32_t sclk_freq);
void uart_hal_get_data_bit_num(uart_hal_context_t *hal, uart_word_length_t *data_bit);
void uart_hal_get_hw_flow_ctrl(uart_hal_context_t *hal, uart_hw_flowcontrol_t *flow_ctrl);
uint16_t uart_hal_get_max_rx_timeout_thrd(uart_hal_context_t *hal);
void uart_hal_get_parity(uart_hal_context_t *hal, uart_parity_t *parity_mode);
void uart_hal_get_sclk(uart_hal_context_t *hal, soc_module_clk_t *sclk);
void uart_hal_get_stop_bits(uart_hal_context_t *hal, uart_stop_bits_t *stop_bit);
uint8_t uart_hal_get_symb_len(uart_hal_context_t *hal);
void uart_hal_get_wakeup_edge_thrd(uart_hal_context_t *hal, uint32_t *wakeup_thrd);
void uart_hal_init(uart_hal_context_t *hal, uart_port_t uart_num);
void uart_hal_inverse_signal(uart_hal_context_t *hal, uint32_t inv_mask);
bool uart_hal_is_hw_rts_en(uart_hal_context_t *hal);
void uart_hal_read_rxfifo(uart_hal_context_t *hal, uint8_t *buf, int *inout_rd_len);
void uart_hal_rxfifo_rst(uart_hal_context_t *hal);
void uart_hal_set_at_cmd_char(uart_hal_context_t *hal, uart_at_cmd_t *at_cmd);
void uart_hal_set_data_bit_num(uart_hal_context_t *hal, uart_word_length_t data_bit);
void uart_hal_set_dtr(uart_hal_context_t *hal, int active_level);
void uart_hal_set_hw_flow_ctrl(uart_hal_context_t *hal, uart_hw_flowcontrol_t flow_ctrl, uint8_t rx_thresh);
void uart_hal_set_loop_back(uart_hal_context_t *hal, bool loop_back_en);
void uart_hal_set_mode(uart_hal_context_t *hal, uart_mode_t mode);
void uart_hal_set_parity(uart_hal_context_t *hal, uart_parity_t parity_mode);
void uart_hal_set_rx_timeout(uart_hal_context_t *hal, const uint8_t tout);
void uart_hal_set_rxfifo_full_thr(uart_hal_context_t *hal, uint32_t full_thrhd);
void uart_hal_set_stop_bits(uart_hal_context_t *hal, uart_stop_bits_t stop_bit);
void uart_hal_set_sw_flow_ctrl(uart_hal_context_t *hal, uart_sw_flowctrl_t *flow_ctrl, bool sw_flow_ctrl_en);
void uart_hal_set_tx_idle_num(uart_hal_context_t *hal, uint16_t idle_num);
void uart_hal_set_txfifo_empty_thr(uart_hal_context_t *hal, uint32_t empty_thrhd);
void uart_hal_set_wakeup_edge_thrd(uart_hal_context_t *hal, uint32_t wakeup_thrd);
void uart_hal_tx_break(uart_hal_context_t *hal, uint32_t break_num);
void uart_hal_txfifo_rst(uart_hal_context_t *hal);
void uart_hal_write_txfifo(uart_hal_context_t *hal, const uint8_t *buf, uint32_t data_size, uint32_t *write_size);


#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_UART_HAL_H__ */
#endif /* WINK_H_GUARD_HAL_UART_HAL_H */
