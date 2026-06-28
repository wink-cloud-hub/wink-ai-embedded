#ifndef PAL_STORAGE_H
#define PAL_STORAGE_H

#include <stdint.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file pal_storage.h
 * @brief 键值式非易失存储抽象（ADR-0008 设备树覆写 blob 存取）。
 *
 * 实现（编译期静态绑定，无运行期多态）：
 *   - host ：进程内内存单槽（测试用，非持久）
 *   - esp32：NVS（namespace "wink"）
 *   - wasm ：no-op，read 返 WINK_ERR_UNSUPPORTED → 调用方降级
 *
 * @note API Contract:
 *   - read：读 key 的 blob 到 buf[cap]，输出实际长度 *out_len。
 *     key 空/不存在 → WINK_ERR_EMPTY（调用方据此降级到编译期默认）；
 *     存储不支持（wasm）→ WINK_ERR_UNSUPPORTED；buf 过小 → WINK_ERR_INVALID_ARG。
 *   - write：原子覆写 key 为 buf[len]（NVS 按 key 整体替换；torn write 由读侧 CRC 兜底）。
 *   - erase：删除 key（不存在为 no-op）；后续 read 返 EMPTY。
 *   - reset：仅 host 测试用——清空到初始空状态；esp32/wasm 为 no-op。
 *   - Blocking: No（host/esp32 NVS 均 O(1)，不阻塞）；Thread-safe: No; ISR-safe: No.
 *   - Error-codes: WINK_OK / WINK_ERR_EMPTY / WINK_ERR_UNSUPPORTED /
 *     WINK_ERR_INVALID_ARG(NULL/过小) / 透传 IO 错误（esp32）。
 */

WINK_WARN_UNUSED_RESULT
wink_status_t pal_storage_read(const char *key, uint8_t *buf, uint16_t cap, uint16_t *out_len);

WINK_WARN_UNUSED_RESULT
wink_status_t pal_storage_write(const char *key, const uint8_t *buf, uint16_t len);

WINK_WARN_UNUSED_RESULT
wink_status_t pal_storage_erase(const char *key);

/** @brief 仅 host 测试用：清空存储到初始空状态。esp32/wasm 为 no-op。 */
void pal_storage_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* PAL_STORAGE_H */
