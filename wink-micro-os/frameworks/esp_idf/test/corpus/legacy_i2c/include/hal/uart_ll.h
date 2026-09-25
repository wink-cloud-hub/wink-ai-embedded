/* SPDX-License-Identifier: CC0-1.0 */
#ifndef HAL_UART_LL_H_
#define HAL_UART_LL_H_

#include <stdbool.h>
#include <stdint.h>
#include "soc/uart_struct.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline void uart_ll_enable_bus_clock(int uart_num, bool enable) { (void)uart_num; (void)enable; }
static inline void uart_ll_reset_register(int uart_num) { (void)uart_num; }
static inline void uart_ll_disable_intr_mask(uart_dev_t *hw, uint32_t mask) { (void)hw; (void)mask; }
static inline void uart_ll_clr_intsts_mask(uart_dev_t *hw, uint32_t mask) { (void)hw; (void)mask; }
static inline void uart_ll_set_autobaud_en(uart_dev_t *hw, bool enable) { (void)hw; (void)enable; }
static inline int uart_ll_get_rxd_edge_cnt(uart_dev_t *hw) { (void)hw; return 542; }
static inline int uart_ll_get_pos_pulse_cnt(uart_dev_t *hw) { (void)hw; return 100; }
static inline int uart_ll_get_neg_pulse_cnt(uart_dev_t *hw) { (void)hw; return 100; }
static inline int uart_ll_get_high_pulse_cnt(uart_dev_t *hw) { (void)hw; return 100; }
static inline int uart_ll_get_low_pulse_cnt(uart_dev_t *hw) { (void)hw; return 100; }

#ifdef __cplusplus
}
#endif

#endif /* HAL_UART_LL_H_ */
