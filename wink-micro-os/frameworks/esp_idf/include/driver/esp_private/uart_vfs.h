/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_DRIVER_ESP_PRIVATE_UART_VFS_H
#define WINK_H_GUARD_DRIVER_ESP_PRIVATE_UART_VFS_H
#ifndef __WINK_HARVESTED_DRIVER_ESP_PRIVATE_UART_VFS_H__
#define __WINK_HARVESTED_DRIVER_ESP_PRIVATE_UART_VFS_H__
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

const esp_vfs_fs_ops_t * esp_vfs_uart_get_vfs(void);
void uart_vfs_dev_port_deinit(const esp_console_dev_uart_config_t *config);
esp_err_t uart_vfs_dev_port_init(const esp_console_dev_uart_config_t *config,
                                 esp_line_endings_t rx_mode,
                                 esp_line_endings_t tx_mode);


#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_DRIVER_ESP_PRIVATE_UART_VFS_H__ */
#endif /* WINK_H_GUARD_DRIVER_ESP_PRIVATE_UART_VFS_H */
