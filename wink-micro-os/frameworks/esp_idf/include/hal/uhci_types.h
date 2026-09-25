/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_UHCI_TYPES_H
#define WINK_H_GUARD_HAL_UHCI_TYPES_H
#ifndef __WINK_HARVESTED_HAL_UHCI_TYPES_H__
#define __WINK_HARVESTED_HAL_UHCI_TYPES_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    uint8_t seper_chr;
    uint8_t sub_chr1;
    uint8_t sub_chr2;
    bool sub_chr_en;
} uhci_seper_chr_t;
typedef struct {
    uint8_t xon_chr;
    uint8_t xon_sub1;
    uint8_t xon_sub2;
    uint8_t xoff_chr;
    uint8_t xoff_sub1;
    uint8_t xoff_sub2;
    uint8_t flow_en;
} uhci_swflow_ctrl_sub_chr_t;



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_UHCI_TYPES_H__ */
#endif /* WINK_H_GUARD_HAL_UHCI_TYPES_H */
