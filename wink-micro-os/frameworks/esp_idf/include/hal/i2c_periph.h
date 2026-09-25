/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_I2C_PERIPH_H
#define WINK_H_GUARD_HAL_I2C_PERIPH_H
#ifndef __WINK_HARVESTED_HAL_I2C_PERIPH_H__
#define __WINK_HARVESTED_HAL_I2C_PERIPH_H__
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

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    const char * module_name;
    const uint8_t sda_out_sig;
    const uint8_t sda_in_sig;
    const uint8_t scl_out_sig;
    const uint8_t scl_in_sig;
    const uint8_t iomux_func;
    const uint8_t irq;
} i2c_signal_conn_t;
typedef struct {
    const regdma_entries_config_t * link_list;
    uint32_t link_num;
    periph_retention_module_t module_id;
} i2c_reg_ctx_link_t;



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_I2C_PERIPH_H__ */
#endif /* WINK_H_GUARD_HAL_I2C_PERIPH_H */
