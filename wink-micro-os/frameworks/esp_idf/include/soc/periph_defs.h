/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_SOC_PERIPH_DEFS_H
#define WINK_H_GUARD_SOC_PERIPH_DEFS_H
#ifndef __WINK_HARVESTED_SOC_PERIPH_DEFS_H__
#define __WINK_HARVESTED_SOC_PERIPH_DEFS_H__
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
    PERIPH_UART1_MODULE = 0,
    PERIPH_UART2_MODULE = 1,
    PERIPH_I2S0_MODULE = 2,
    PERIPH_TIMG0_MODULE = 3,
    PERIPH_TIMG1_MODULE = 4,
    PERIPH_UHCI0_MODULE = 5,
    PERIPH_SPI_MODULE = 6,
    PERIPH_HSPI_MODULE = 7,
    PERIPH_VSPI_MODULE = 8,
    PERIPH_RNG_MODULE = 9,
    PERIPH_WIFI_MODULE = 10,
    PERIPH_BT_MODULE = 11,
    PERIPH_WIFI_BT_COMMON_MODULE = 12,
    PERIPH_BT_BASEBAND_MODULE = 13,
    PERIPH_PHY_CALIBRATION_MODULE = 14,
    PERIPH_MODULE_MAX = 15,
} shared_periph_module_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_SOC_PERIPH_DEFS_H__ */
#endif /* WINK_H_GUARD_SOC_PERIPH_DEFS_H */
