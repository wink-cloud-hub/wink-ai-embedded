/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_MODEM_CLOCK_TYPES_H
#define WINK_H_GUARD_HAL_MODEM_CLOCK_TYPES_H
#ifndef __WINK_HARVESTED_HAL_MODEM_CLOCK_TYPES_H__
#define __WINK_HARVESTED_HAL_MODEM_CLOCK_TYPES_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    MODEM_CLOCK_DOMAIN_MODEM_APB = 0,
    MODEM_CLOCK_DOMAIN_MODEM_PERIPH = 1,
    MODEM_CLOCK_DOMAIN_WIFI = 2,
    MODEM_CLOCK_DOMAIN_BT = 3,
    MODEM_CLOCK_DOMAIN_MODEM_FE = 4,
    MODEM_CLOCK_DOMAIN_LP_APB = 5,
    MODEM_CLOCK_DOMAIN_I2C_MASTER = 6,
    MODEM_CLOCK_DOMAIN_COEX = 7,
    MODEM_CLOCK_DOMAIN_WIFIPWR = 8,
    MODEM_CLOCK_DOMAIN_MAX = 9,
} modem_clock_domain_t;
typedef enum {
    MODEM_CLOCK_LPCLK_SRC_INVALID = -1,
    MODEM_CLOCK_LPCLK_SRC_RC_SLOW = 0,
    MODEM_CLOCK_LPCLK_SRC_RC_FAST = 1,
    MODEM_CLOCK_LPCLK_SRC_MAIN_XTAL = 2,
    MODEM_CLOCK_LPCLK_SRC_RC32K = 3,
    MODEM_CLOCK_LPCLK_SRC_XTAL32K = 4,
    MODEM_CLOCK_LPCLK_SRC_EXT32K = 5,
    MODEM_CLOCK_LPCLK_SRC_MAX = 6,
} modem_clock_lpclk_src_t;
typedef enum {
    MODEM_CLOCK_32K_SRC_XTAL32K = 0,
    MODEM_CLOCK_32K_SRC_RC32K = 1,
    MODEM_CLOCK_32K_SRC_EXT32K = 2,
} modem_clock_32k_clk_src_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_MODEM_CLOCK_TYPES_H__ */
#endif /* WINK_H_GUARD_HAL_MODEM_CLOCK_TYPES_H */
