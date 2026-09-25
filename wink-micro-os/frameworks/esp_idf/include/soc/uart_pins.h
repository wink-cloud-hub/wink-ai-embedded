/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_SOC_UART_PINS_H
#define WINK_H_GUARD_SOC_UART_PINS_H
#ifndef __WINK_HARVESTED_SOC_UART_PINS_H__
#define __WINK_HARVESTED_SOC_UART_PINS_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef U0CTS_GPIO_NUM
#define U0CTS_GPIO_NUM (19)
#endif
#ifndef U0CTS_MUX_FUNC
#define U0CTS_MUX_FUNC (3)
#endif
#ifndef U0DSR_GPIO_NUM
#define U0DSR_GPIO_NUM (-1)
#endif
#ifndef U0DSR_MUX_FUNC
#define U0DSR_MUX_FUNC (-1)
#endif
#ifndef U0DTR_GPIO_NUM
#define U0DTR_GPIO_NUM (-1)
#endif
#ifndef U0DTR_MUX_FUNC
#define U0DTR_MUX_FUNC (-1)
#endif
#ifndef U0RTS_GPIO_NUM
#define U0RTS_GPIO_NUM (22)
#endif
#ifndef U0RTS_MUX_FUNC
#define U0RTS_MUX_FUNC (3)
#endif
#ifndef U0RXD_GPIO_NUM
#define U0RXD_GPIO_NUM (3)
#endif
#ifndef U0RXD_MUX_FUNC
#define U0RXD_MUX_FUNC (0)
#endif
#ifndef U0TXD_GPIO_NUM
#define U0TXD_GPIO_NUM (1)
#endif
#ifndef U0TXD_MUX_FUNC
#define U0TXD_MUX_FUNC (0)
#endif
#ifndef U1CTS_GPIO_NUM
#define U1CTS_GPIO_NUM (6)
#endif
#ifndef U1CTS_MUX_FUNC
#define U1CTS_MUX_FUNC (4)
#endif
#ifndef U1DSR_GPIO_NUM
#define U1DSR_GPIO_NUM (-1)
#endif
#ifndef U1DSR_MUX_FUNC
#define U1DSR_MUX_FUNC (-1)
#endif
#ifndef U1DTR_GPIO_NUM
#define U1DTR_GPIO_NUM (-1)
#endif
#ifndef U1DTR_MUX_FUNC
#define U1DTR_MUX_FUNC (-1)
#endif
#ifndef U1RTS_GPIO_NUM
#define U1RTS_GPIO_NUM (11)
#endif
#ifndef U1RTS_MUX_FUNC
#define U1RTS_MUX_FUNC (4)
#endif
#ifndef U1RXD_GPIO_NUM
#define U1RXD_GPIO_NUM (9)
#endif
#ifndef U1RXD_MUX_FUNC
#define U1RXD_MUX_FUNC (4)
#endif
#ifndef U1TXD_GPIO_NUM
#define U1TXD_GPIO_NUM (10)
#endif
#ifndef U1TXD_MUX_FUNC
#define U1TXD_MUX_FUNC (4)
#endif
#ifndef U2CTS_GPIO_NUM
#define U2CTS_GPIO_NUM (8)
#endif
#ifndef U2CTS_MUX_FUNC
#define U2CTS_MUX_FUNC (4)
#endif
#ifndef U2DSR_GPIO_NUM
#define U2DSR_GPIO_NUM (-1)
#endif
#ifndef U2DSR_MUX_FUNC
#define U2DSR_MUX_FUNC (-1)
#endif
#ifndef U2DTR_GPIO_NUM
#define U2DTR_GPIO_NUM (-1)
#endif
#ifndef U2DTR_MUX_FUNC
#define U2DTR_MUX_FUNC (-1)
#endif
#ifndef U2RTS_GPIO_NUM
#define U2RTS_GPIO_NUM (7)
#endif
#ifndef U2RTS_MUX_FUNC
#define U2RTS_MUX_FUNC (4)
#endif
#ifndef U2RXD_GPIO_NUM
#define U2RXD_GPIO_NUM (16)
#endif
#ifndef U2RXD_MUX_FUNC
#define U2RXD_MUX_FUNC (4)
#endif
#ifndef U2TXD_GPIO_NUM
#define U2TXD_GPIO_NUM (17)
#endif
#ifndef U2TXD_MUX_FUNC
#define U2TXD_MUX_FUNC (4)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_SOC_UART_PINS_H__ */
#endif /* WINK_H_GUARD_SOC_UART_PINS_H */
