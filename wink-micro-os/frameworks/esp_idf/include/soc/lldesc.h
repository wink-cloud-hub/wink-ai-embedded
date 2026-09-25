/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_SOC_LLDESC_H
#define WINK_H_GUARD_SOC_LLDESC_H
#ifndef __WINK_HARVESTED_SOC_LLDESC_H__
#define __WINK_HARVESTED_SOC_LLDESC_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef LLDESC_MAX_NUM_PER_DESC
#define LLDESC_MAX_NUM_PER_DESC (4096-4)
#endif
#ifndef LLDESC_MAX_NUM_PER_DESC_16B_ALIGNED
#define LLDESC_MAX_NUM_PER_DESC_16B_ALIGNED (4096 - 16)
#endif
#ifndef LLDESC_MAX_NUM_PER_DESC_32B_ALIGNED
#define LLDESC_MAX_NUM_PER_DESC_32B_ALIGNED (4096 - 32)
#endif
#ifndef lldesc_get_required_num
#define lldesc_get_required_num(data_size) lldesc_get_required_num_constrained(data_size, LLDESC_MAX_NUM_PER_DESC)
#endif
#ifndef lldesc_setup_link
#define lldesc_setup_link(out_desc_array, buffer, size, isrx) lldesc_setup_link_constrained(out_desc_array, buffer, size, LLDESC_MAX_NUM_PER_DESC, isrx)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
int lldesc_get_received_len(lldesc_t* head, lldesc_t** out_next) WINK_SLA_ERROR("Wink SLA Violation: lldesc_get_received_len out of Core 8 scope.");
#else
int lldesc_get_received_len(lldesc_t* head, lldesc_t** out_next);
#endif

#if defined(__WINK_SIM__)
int lldesc_get_required_num_constrained(int data_size, int max_desc_size) WINK_SLA_ERROR("Wink SLA Violation: lldesc_get_required_num_constrained out of Core 8 scope.");
#else
int lldesc_get_required_num_constrained(int data_size, int max_desc_size);
#endif

#if defined(__WINK_SIM__)
void lldesc_setup_link_constrained(lldesc_t *out_desc_array, const void *buffer, int size, int max_desc_size, bool isrx) WINK_SLA_ERROR("Wink SLA Violation: lldesc_setup_link_constrained out of Core 8 scope.");
#else
void lldesc_setup_link_constrained(lldesc_t *out_desc_array, const void *buffer, int size, int max_desc_size, bool isrx);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_SOC_LLDESC_H__ */
#endif /* WINK_H_GUARD_SOC_LLDESC_H */
