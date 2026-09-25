/* SPDX-License-Identifier: CC0-1.0 */
#ifndef HAL_UART_PERIPH_H_
#define HAL_UART_PERIPH_H_

#define SOC_UART_PERIPH_SIGNAL_RX 0
#define UART_PERIPH_SIGNAL(port, sig) ((port) * 100 + (sig))

#endif /* HAL_UART_PERIPH_H_ */
