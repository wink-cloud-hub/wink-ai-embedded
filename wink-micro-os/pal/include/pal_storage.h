// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_storage.h
 * @brief Key-Value Non-Volatile Storage Abstraction (ADR-0008).
 *
 * Implementations (Compile-time static dispatch):
 *   - Host : In-memory single slot (for unit tests, non-persistent)
 *   - ESP32: NVS (Namespace "wink")
 *   - Wasm : No-op, read returns WINK_ERR_UNSUPPORTED
 *
 * API Contract:
 *   - read: Reads value blob of key into buf[cap], outputs actual length to *out_len.
 *           If key does not exist, returns WINK_ERR_EMPTY (caller degrades to default).
 *           If storage unsupported (Wasm), returns WINK_ERR_UNSUPPORTED.
 *           If buf capacity is insufficient, returns WINK_ERR_INVALID_ARG.
 *   - write: Atomic overwrite of key with buf[len].
 *   - erase: Removes key (no-op if key does not exist).
 *   - reset: Clears storage to initial empty state (Host test only).
 */
#ifndef PAL_STORAGE_H
#define PAL_STORAGE_H

#include <stdint.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read binary blob from non-volatile storage key
 * @param[in] key Key string identifier
 * @param[out] buf Output buffer pointer
 * @param[in] cap Output buffer capacity in bytes
 * @param[out] out_len Pointer to store actual read length in bytes
 * @return WINK_OK on success, WINK_ERR_EMPTY if key missing, WINK_ERR_UNSUPPORTED on Wasm, WINK_ERR_INVALID_ARG if parameters invalid
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_storage_read(const char *key, uint8_t *buf, uint16_t cap, uint16_t *out_len);

/**
 * @brief Write binary blob to non-volatile storage key
 * @param[in] key Key string identifier
 * @param[in] buf Input data buffer pointer
 * @param[in] len Length of input data in bytes
 * @return WINK_OK on success, WINK_ERR_UNSUPPORTED on Wasm, WINK_ERR_INVALID_ARG if parameters invalid
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_storage_write(const char *key, const uint8_t *buf, uint16_t len);

/**
 * @brief Erase non-volatile storage key
 * @param[in] key Key string identifier
 * @return WINK_OK on success, WINK_ERR_UNSUPPORTED on Wasm
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_storage_erase(const char *key);

/** @brief Clear non-volatile storage to initial empty state (Host test only; no-op on ESP32/Wasm) */
void pal_storage_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* PAL_STORAGE_H */
