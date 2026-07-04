/**
 * @file wink_dev_config.c
 * @brief Flash 设备树覆写 blob 解析器 + CRC32（target 无关共享核心，ADR-0008）。
 *
 * 纯逻辑、无锁、无硬件、无动态分配。host/wasm/esp32 三 target 共享链接。
 * 损坏（magic/version/长度/CRC）静默降级：不写半状态、不 Panic（ADR §4.3）。
 * 逐字段 offset+memcpy 读写，禁 packed 指针强转（规避非对齐访问/严格别名 UB）。
 */
#include "wink_dev_config.h"

#include <string.h>   /* memcpy */

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
        return WINK_ERR_INVALID_ARG;                 /* 连 header+crc 都装不下 */
    }

    /* header：offset+memcpy 读取（避免非对齐/别名 UB） */
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

    /* 长度不变式：uint32 运算，防 count*20 溢出回绕导致误匹配 */
    uint32_t expected = WINK_DEV_CONFIG_HEADER_SIZE
                      + (uint32_t)count * WINK_DEV_CONFIG_ITEM_SIZE
                      + WINK_DEV_CONFIG_CRC_SIZE;
    if ((uint32_t)len != expected) {
        return WINK_ERR_CONFIG_CORRUPT_DEGRADED;
    }

    /* CRC：覆盖 header+items（= 除末尾 4B 外的全部） */
    uint16_t body_len = (uint16_t)(len - WINK_DEV_CONFIG_CRC_SIZE);
    uint32_t stored_crc;
    memcpy(&stored_crc, buf + body_len, 4u);
    if (wink_dev_config_crc32(buf, body_len) != stored_crc) {
        return WINK_ERR_CHECKSUM;
    }

    /* count==0：合法 no-op 成功（循环不进入） */
    for (uint16_t i = 0u; i < count; i++) {
        uint16_t off = (uint16_t)(WINK_DEV_CONFIG_HEADER_SIZE
                                  + (uint32_t)i * WINK_DEV_CONFIG_ITEM_SIZE);
        uint32_t device_id;
        memcpy(&device_id, buf + off, 4u);
        const uint8_t *params = buf + off + 4u;

        for (uint16_t r = 0u; r < registry_count; r++) {
            if (registry[r].device_id == device_id) {
                /* 命中：派发 apply。失败仅降级该项，不中断其它项。 */
                wink_status_t s = registry[r].apply(registry[r].dev,
                                                    params,
                                                    (uint16_t)WINK_DEV_CONFIG_PARAMS_SIZE);
                (void)s;
                break;
            }
        }
        /* 未命中 → 静默跳过（该项降级） */
    }
    return WINK_OK;
}
