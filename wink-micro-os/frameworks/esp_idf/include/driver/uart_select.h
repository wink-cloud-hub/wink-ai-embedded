/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef _UART_SELECT_H_
#define _UART_SELECT_H_
#ifndef __WINK_HARVESTED_DRIVER_UART_SELECT_H__
#define __WINK_HARVESTED_DRIVER_UART_SELECT_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    UART_SELECT_READ_NOTIF = 0,
    UART_SELECT_WRITE_NOTIF = 1,
    UART_SELECT_ERROR_NOTIF = 2,
} uart_select_notif_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef void (*uart_select_notif_callback_t)(uart_port_t uart_num, uart_select_notif_t uart_select_notif, BaseType_t *task_woken);

portMUX_TYPE * uart_get_selectlock(void);
void uart_set_select_notif_callback(uart_port_t uart_num, uart_select_notif_callback_t uart_select_notif_callback);


#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_DRIVER_UART_SELECT_H__ */
#endif /* _UART_SELECT_H_ */
