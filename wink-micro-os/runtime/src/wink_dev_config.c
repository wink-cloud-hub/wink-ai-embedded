// SPDX-License-Identifier: Apache-2.0
#include "wink_dev_config.h"
#include <string.h>

uint32_t wink_dev_config_crc32(const uint8_t *data, uint16_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint32_t)data[i];
        for (uint8_t b = 0; b < 8u; b++) {
            if (crc & 1u) {
                crc = (crc >> 1) ^ 0xEDB88320u;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

wink_status_t wink_dev_config_apply(const uint8_t *buf, uint16_t len,
                                    const wink_dev_override_entry_t *registry,
                                    uint16_t registry_count) {
    if (buf == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (registry_count > 0u && registry == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (len < (WINK_DEV_CONFIG_HEADER_SIZE + WINK_DEV_CONFIG_CRC_SIZE)) {
        return WINK_ERR_INVALID_ARG;
    }

    uint32_t magic;
    uint16_t version;
    uint16_t count;
    memcpy(&magic,   buf + 0u, 4u);
    memcpy(&version, buf + 4u, 2u);
    memcpy(&count,   buf + 6u, 2u);

    if (magic != WINK_DEV_CONFIG_MAGIC) {
        return WINK_ERR_CONFIG_CORRUPT_DEGRADED;
    }
    if (version != WINK_DEV_CONFIG_VERSION) {
        return WINK_ERR_CONFIG_CORRUPT_DEGRADED;
    }

    uint32_t expected = WINK_DEV_CONFIG_HEADER_SIZE
                      + (uint32_t)count * WINK_DEV_CONFIG_ITEM_SIZE
                      + WINK_DEV_CONFIG_CRC_SIZE;
    if ((uint32_t)len != expected) {
        return WINK_ERR_CONFIG_CORRUPT_DEGRADED;
    }

    uint16_t body_len = (uint16_t)(len - WINK_DEV_CONFIG_CRC_SIZE);
    uint32_t stored_crc;
    memcpy(&stored_crc, buf + body_len, 4u);
    if (wink_dev_config_crc32(buf, body_len) != stored_crc) {
        return WINK_ERR_CHECKSUM;
    }

    for (uint16_t i = 0u; i < count; i++) {
        uint16_t off = (uint16_t)(WINK_DEV_CONFIG_HEADER_SIZE
                                  + (uint32_t)i * WINK_DEV_CONFIG_ITEM_SIZE);
        uint32_t device_id;
        memcpy(&device_id, buf + off, 4u);
        const uint8_t *params = buf + off + 4u;

        for (uint16_t r = 0u; r < registry_count; r++) {
            if (registry[r].device_id == device_id) {
                wink_status_t s = registry[r].apply(registry[r].dev,
                                                    params,
                                                    (uint16_t)WINK_DEV_CONFIG_PARAMS_SIZE);
                (void)s;
                break;
            }
        }
    }
    return WINK_OK;
}
