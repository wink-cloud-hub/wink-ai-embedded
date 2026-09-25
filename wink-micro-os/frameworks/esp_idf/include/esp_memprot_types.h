/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_MEMPROT_TYPES_H
#define WINK_H_GUARD_ESP_MEMPROT_TYPES_H
#ifndef __WINK_HARVESTED_ESP_MEMPROT_TYPES_H__
#define __WINK_HARVESTED_ESP_MEMPROT_TYPES_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef MEMPROT_OP_EXEC
#define MEMPROT_OP_EXEC 0x00000004
#endif
#ifndef MEMPROT_OP_INVALID
#define MEMPROT_OP_INVALID 0x80000000
#endif
#ifndef MEMPROT_OP_NONE
#define MEMPROT_OP_NONE 0x00000000
#endif
#ifndef MEMPROT_OP_READ
#define MEMPROT_OP_READ 0x00000001
#endif
#ifndef MEMPROT_OP_WRITE
#define MEMPROT_OP_WRITE 0x00000002
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    MEMPROT_PMS_WORLD_NONE = 0,
    MEMPROT_PMS_WORLD_0 = 1,
    MEMPROT_PMS_WORLD_1 = 2,
    MEMPROT_PMS_WORLD_2 = 4,
    MEMPROT_PMS_WORLD_ALL = 2147483647,
    MEMPROT_PMS_WORLD_INVALID = 2147483648,
} esp_mprot_pms_world_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_MEMPROT_TYPES_H__ */
#endif /* WINK_H_GUARD_ESP_MEMPROT_TYPES_H */
