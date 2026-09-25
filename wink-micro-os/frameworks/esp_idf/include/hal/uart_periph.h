/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_UART_PERIPH_H
#define WINK_H_GUARD_HAL_UART_PERIPH_H
#ifndef __WINK_HARVESTED_HAL_UART_PERIPH_H__
#define __WINK_HARVESTED_HAL_UART_PERIPH_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef UART_PERIPH_SIGNAL
#define UART_PERIPH_SIGNAL(IDX, PIN) (uart_periph_signal[(IDX)].pins[(PIN)].signal)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    SOC_UART_PERIPH_SIGNAL_TX = 0,
    SOC_UART_PERIPH_SIGNAL_RX = 1,
    SOC_UART_PERIPH_SIGNAL_RTS = 2,
    SOC_UART_PERIPH_SIGNAL_CTS = 3,
    SOC_UART_PERIPH_SIGNAL_DTR = 4,
    SOC_UART_PERIPH_SIGNAL_DSR = 5,
    SOC_UART_PERIPH_SIGNAL_MAX = 6,
} soc_uart_periph_signal_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    int32_t default_gpio : 15;
    int32_t iomux_func : 4;
    uint32_t input : 1;
    uint32_t signal : 12;
} uart_periph_sig_t;
typedef struct {
    const uart_periph_sig_t pins[SOC_UART_PERIPH_SIGNAL_MAX];
    const int irq;
} uart_signal_conn_t;
typedef struct {
    const periph_retention_module_t module;
    const regdma_entries_config_t * regdma_entry_array;
    uint32_t array_size;
} uart_reg_retention_info_t;



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_UART_PERIPH_H__ */
#endif /* WINK_H_GUARD_HAL_UART_PERIPH_H */
