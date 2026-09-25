/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_DRIVER_UHCI_TYPES_H
#define WINK_H_GUARD_DRIVER_UHCI_TYPES_H
#ifndef __WINK_HARVESTED_DRIVER_UHCI_TYPES_H__
#define __WINK_HARVESTED_DRIVER_UHCI_TYPES_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "hal/uart_types.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct uhci_controller_t * uhci_controller_handle_t;
typedef struct {
    uint8_t * buffer;
    size_t sent_size;
} uhci_tx_done_event_data_t;
typedef bool (*uhci_tx_done_callback_t)(uhci_controller_handle_t uhci_ctrl, const uhci_tx_done_event_data_t *edata, void *user_ctx);
typedef struct {
const uint8_t *data;           
    size_t recv_size;              
    struct {
        uint32_t totally_received: 1;     
    } flags;
} uhci_rx_event_data_t;
typedef bool (*uhci_rx_event_callback_t)(uhci_controller_handle_t uhci_ctrl, const uhci_rx_event_data_t *edata, void *user_ctx);



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_DRIVER_UHCI_TYPES_H__ */
#endif /* WINK_H_GUARD_DRIVER_UHCI_TYPES_H */
