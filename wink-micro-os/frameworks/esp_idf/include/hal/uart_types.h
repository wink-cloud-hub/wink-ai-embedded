/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef HAL_UART_TYPES_H_
#define HAL_UART_TYPES_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UART_NUM_0 = 0,
    UART_NUM_1 = 1,
    UART_NUM_2 = 2,
    UART_NUM_MAX,
} uart_port_t;

typedef enum {
    UART_DATA_5_BITS   = 0x0,
    UART_DATA_6_BITS   = 0x1,
    UART_DATA_7_BITS   = 0x2,
    UART_DATA_8_BITS   = 0x3,
    UART_DATA_BITS_MAX = 0x4,
} uart_word_length_t;

typedef enum {
    UART_STOP_BITS_1   = 0x1,
    UART_STOP_BITS_1_5 = 0x2,
    UART_STOP_BITS_2   = 0x3,
    UART_STOP_BITS_MAX = 0x4,
} uart_stop_bits_t;

typedef enum {
    UART_PARITY_DISABLE  = 0x0,
    UART_PARITY_EVEN     = 0x2,
    UART_PARITY_ODD      = 0x3,
} uart_parity_t;

typedef enum {
    UART_HW_FLOWCTRL_DISABLE = 0x0,
    UART_HW_FLOWCTRL_RTS     = 0x1,
    UART_HW_FLOWCTRL_CTS     = 0x2,
    UART_HW_FLOWCTRL_CTS_RTS = 0x3,
    UART_HW_FLOWCTRL_MAX     = 0x4,
} uart_hw_flowcontrol_t;

typedef enum {
    UART_DATA,
    UART_BUFFER_FULL,
    UART_FIFO_OVF,
    UART_FRAME_ERR,
    UART_PARITY_ERR,
    UART_BREAK,
    UART_EVENT_MAX,
} uart_event_type_t;

typedef struct {
    uart_event_type_t type;
    size_t size;
    bool timeout_flag;
} uart_event_t;

typedef struct {
    int baud_rate;
    int data_bits;
    int parity;
    int stop_bits;
    int flow_ctrl;
    int rx_flow_ctrl_thresh;
    int source_clk;
} uart_config_t;

#ifdef __cplusplus
}
#endif

#endif /* HAL_UART_TYPES_H_ */
