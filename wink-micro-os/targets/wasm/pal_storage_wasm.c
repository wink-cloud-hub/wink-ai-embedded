/**
 * @file pal_storage_wasm.c
 * @brief wasm no-op 存储实现（ADR-0008）。
 *
 * read 恒返 WINK_ERR_UNSUPPORTED → 调用方降级到编译期默认。保持 wasm 构建绿。
 * Wasm JS 桥对等（虚拟 FS 覆写）属后置范围（见 plan「显式后置」）。
 */
#include "pal_storage.h"

void pal_storage_reset(void) {
    /* no-op */
}

wink_status_t pal_storage_read(const char *key, uint8_t *buf, uint16_t cap, uint16_t *out_len) {
    (void)key; (void)buf; (void)cap; (void)out_len;
    return WINK_ERR_UNSUPPORTED;   /* wasm 无持久存储 → 降级 */
}

wink_status_t pal_storage_write(const char *key, const uint8_t *buf, uint16_t len) {
    (void)key; (void)buf; (void)len;
    return WINK_ERR_UNSUPPORTED;
}

wink_status_t pal_storage_erase(const char *key) {
    (void)key;
    return WINK_ERR_UNSUPPORTED;
}
