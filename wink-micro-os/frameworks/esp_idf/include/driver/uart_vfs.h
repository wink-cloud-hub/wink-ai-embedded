/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_DRIVER_UART_VFS_H
#define WINK_H_GUARD_DRIVER_UART_VFS_H
#ifndef __WINK_HARVESTED_DRIVER_UART_VFS_H__
#define __WINK_HARVESTED_DRIVER_UART_VFS_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    int channel;
    int baud_rate;
    int tx_gpio_num;
    int rx_gpio_num;
} esp_console_dev_uart_config_t;

int uart_vfs_dev_port_set_rx_line_endings(int uart_num, esp_line_endings_t mode);
int uart_vfs_dev_port_set_tx_line_endings(int uart_num, esp_line_endings_t mode);
void uart_vfs_dev_register(void);
void uart_vfs_dev_use_driver(int uart_num);
void uart_vfs_dev_use_nonblocking(int uart_num);


#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_DRIVER_UART_VFS_H__ */
#endif /* WINK_H_GUARD_DRIVER_UART_VFS_H */
