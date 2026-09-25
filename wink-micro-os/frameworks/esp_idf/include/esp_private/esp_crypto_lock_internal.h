/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_ESP_CRYPTO_LOCK_INTERNAL_H
#define WINK_H_GUARD_ESP_PRIVATE_ESP_CRYPTO_LOCK_INTERNAL_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_ESP_CRYPTO_LOCK_INTERNAL_H__
#define __WINK_HARVESTED_ESP_PRIVATE_ESP_CRYPTO_LOCK_INTERNAL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef AES_RCC_ATOMIC
#define AES_RCC_ATOMIC() PERIPH_RCC_ATOMIC()
#endif
#ifndef DS_RCC_ATOMIC
#define DS_RCC_ATOMIC() PERIPH_RCC_ATOMIC()
#endif
#ifndef ECC_RCC_ATOMIC
#define ECC_RCC_ATOMIC() PERIPH_RCC_ATOMIC()
#endif
#ifndef ECDSA_RCC_ATOMIC
#define ECDSA_RCC_ATOMIC() PERIPH_RCC_ATOMIC()
#endif
#ifndef HMAC_RCC_ATOMIC
#define HMAC_RCC_ATOMIC() PERIPH_RCC_ATOMIC()
#endif
#ifndef KEY_MANAGER_RCC_ATOMIC
#define KEY_MANAGER_RCC_ATOMIC() PERIPH_RCC_ATOMIC()
#endif
#ifndef MPI_RCC_ATOMIC
#define MPI_RCC_ATOMIC() PERIPH_RCC_ATOMIC()
#endif
#ifndef SHA_RCC_ATOMIC
#define SHA_RCC_ATOMIC() PERIPH_RCC_ATOMIC()
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_ESP_CRYPTO_LOCK_INTERNAL_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_ESP_CRYPTO_LOCK_INTERNAL_H */
