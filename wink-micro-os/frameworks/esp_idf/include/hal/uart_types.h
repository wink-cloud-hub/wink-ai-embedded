/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_UART_TYPES_H
#define WINK_H_GUARD_HAL_UART_TYPES_H
#ifndef __WINK_HARVESTED_HAL_UART_TYPES_H__
#define __WINK_HARVESTED_HAL_UART_TYPES_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>

#include "esp_assert.h"
#include "soc/clk_tree_defs.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    UART_NUM_0 = 0,
    UART_NUM_1 = 1,
    UART_NUM_2 = 2,
    UART_NUM_MAX = 3,
} uart_port_t;
typedef enum {
    UART_MODE_UART = 0,
    UART_MODE_RS485_HALF_DUPLEX = 1,
    UART_MODE_IRDA = 2,
    UART_MODE_RS485_COLLISION_DETECT = 3,
    UART_MODE_RS485_APP_CTRL = 4,
} uart_mode_t;
typedef enum {
    UART_DATA_5_BITS = 0,
    UART_DATA_6_BITS = 1,
    UART_DATA_7_BITS = 2,
    UART_DATA_8_BITS = 3,
    UART_DATA_BITS_MAX = 4,
} uart_word_length_t;
typedef enum {
    UART_STOP_BITS_1 = 1,
    UART_STOP_BITS_1_5 = 2,
    UART_STOP_BITS_2 = 3,
    UART_STOP_BITS_MAX = 4,
} uart_stop_bits_t;
typedef enum {
    UART_PARITY_DISABLE = 0,
    UART_PARITY_EVEN = 2,
    UART_PARITY_ODD = 3,
} uart_parity_t;
typedef enum {
    UART_HW_FLOWCTRL_DISABLE = 0,
    UART_HW_FLOWCTRL_RTS = 1,
    UART_HW_FLOWCTRL_CTS = 2,
    UART_HW_FLOWCTRL_CTS_RTS = 3,
    UART_HW_FLOWCTRL_MAX = 4,
} uart_hw_flowcontrol_t;
typedef enum {
    UART_SIGNAL_INV_DISABLE = 0,
    UART_SIGNAL_IRDA_TX_INV = 1,
    UART_SIGNAL_IRDA_RX_INV = 2,
    UART_SIGNAL_RXD_INV = 4,
    UART_SIGNAL_CTS_INV = 8,
    UART_SIGNAL_DSR_INV = 16,
    UART_SIGNAL_TXD_INV = 32,
    UART_SIGNAL_RTS_INV = 64,
    UART_SIGNAL_DTR_INV = 128,
} uart_signal_inv_t;
typedef enum {
    UART_WK_MODE_ACTIVE_THRESH = 0,
} uart_wakeup_mode_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef soc_periph_uart_clk_src_legacy_t uart_sclk_t;
typedef struct {
    uint8_t cmd_char;
    uint8_t char_num;
    uint32_t gap_tout;
    uint32_t pre_idle;
    uint32_t post_idle;
} uart_at_cmd_t;
typedef struct {
    uint8_t xon_char;
    uint8_t xoff_char;
    uint8_t xon_thrd;
    uint8_t xoff_thrd;
} uart_sw_flowctrl_t;



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_UART_TYPES_H__ */
#endif /* WINK_H_GUARD_HAL_UART_TYPES_H */
