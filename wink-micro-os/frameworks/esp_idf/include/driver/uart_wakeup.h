/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_DRIVER_UART_WAKEUP_H
#define WINK_H_GUARD_DRIVER_UART_WAKEUP_H
#ifndef __WINK_HARVESTED_DRIVER_UART_WAKEUP_H__
#define __WINK_HARVESTED_DRIVER_UART_WAKEUP_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "esp_attr.h"
#include "esp_err.h"
#include "hal/uart_types.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    uart_wakeup_mode_t wakeup_mode;
    uint16_t rx_edge_threshold;
} uart_wakeup_cfg_t;

esp_err_t uart_wakeup_clear(uart_port_t uart_num, uart_wakeup_mode_t wakeup_mode);
esp_err_t uart_wakeup_setup(uart_port_t uart_num, const uart_wakeup_cfg_t *cfg);


#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_DRIVER_UART_WAKEUP_H__ */
#endif /* WINK_H_GUARD_DRIVER_UART_WAKEUP_H */
