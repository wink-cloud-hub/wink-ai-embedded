/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_MIPI_CSI_SHARE_HW_CTRL_H
#define WINK_H_GUARD_ESP_PRIVATE_MIPI_CSI_SHARE_HW_CTRL_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_MIPI_CSI_SHARE_HW_CTRL_H__
#define __WINK_HARVESTED_ESP_PRIVATE_MIPI_CSI_SHARE_HW_CTRL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    MIPI_CSI_BRG_USER_CSI = 0,
    MIPI_CSI_BRG_USER_ISP_DVP = 1,
    MIPI_CSI_BRG_USER_SHARE = 2,
} mipi_csi_brg_user_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
esp_err_t mipi_csi_brg_claim(mipi_csi_brg_user_t user, int *out_id) WINK_SLA_ERROR("Wink SLA Violation: mipi_csi_brg_claim out of Core 8 scope.");
#else
esp_err_t mipi_csi_brg_claim(mipi_csi_brg_user_t user, int *out_id);
#endif

#if defined(__WINK_SIM__)
esp_err_t mipi_csi_brg_declaim(int id) WINK_SLA_ERROR("Wink SLA Violation: mipi_csi_brg_declaim out of Core 8 scope.");
#else
esp_err_t mipi_csi_brg_declaim(int id);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_MIPI_CSI_SHARE_HW_CTRL_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_MIPI_CSI_SHARE_HW_CTRL_H */
