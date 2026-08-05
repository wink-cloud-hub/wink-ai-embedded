// SPDX-License-Identifier: Apache-2.0
#ifndef WINK_DEV_CONFIG_H
#define WINK_DEV_CONFIG_H

#include <stdint.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WINK_DEV_CONFIG_MAGIC        0x57494E4Bu
#define WINK_DEV_CONFIG_VERSION      1u
#define WINK_DEV_CONFIG_PARAMS_SIZE  16u
#define WINK_DEV_CONFIG_ITEM_SIZE    (4u + WINK_DEV_CONFIG_PARAMS_SIZE)
#define WINK_DEV_CONFIG_HEADER_SIZE  8u
#define WINK_DEV_CONFIG_CRC_SIZE     4u
#define WINK_DEV_CONFIG_MAX_BYTES    256u
#define WINK_DEV_CONFIG_KEY          "dtcfg"

/**
 * @brief Per-DAL override callback prototype
 */
typedef wink_status_t (*wink_dev_override_fn)(void *dev, const uint8_t *params, uint16_t len);

/**
 * @brief Override registry entry struct
 */
typedef struct {
    uint32_t             device_id;
    void                *dev;
    wink_dev_override_fn apply;
} wink_dev_override_entry_t;

/**
 * @brief Calculate CRC-32/ISO-HDLC checksum
 *
 * @param[in] data Input data buffer pointer.
 * @param[in] len Length in bytes.
 * @return 32-bit CRC checksum.
 */
WINK_WARN_UNUSED_RESULT
uint32_t wink_dev_config_crc32(const uint8_t *data, uint16_t len);

/**
 * @brief Parse and apply device configuration override blob
 *
 * @param[in] buf Binary blob buffer pointer.
 * @param[in] len Length in bytes.
 * @param[in] registry Device override registry array.
 * @param[in] registry_count Entry count in registry.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_dev_config_apply(const uint8_t *buf, uint16_t len,
                                     const wink_dev_override_entry_t *registry,
                                     uint16_t registry_count);

#ifdef __cplusplus
}
#endif

#endif /* WINK_DEV_CONFIG_H */
