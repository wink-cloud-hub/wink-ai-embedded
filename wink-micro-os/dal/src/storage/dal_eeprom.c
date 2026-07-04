#include "dal_eeprom.h"
#include "pal_hal.h"
#include "pal_osal.h"
#include "pal_resource.h"
#include <string.h>

wink_status_t dal_eeprom_init(dal_eeprom_t *dev, const dal_eeprom_config_t *cfg) {
    /* 参数合法性校验（必须保留） */
    if (dev == NULL || cfg == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->owner == NULL) { return WINK_ERR_INVALID_ARG; }

    /* @experimental Stub: I2C EEPROM backend not yet implemented.
     * 不 claim I2C 资源、不置 initialized=true、不做硬件探测——避免"假成功"反模式
     * （ADR-0012 契约诚实）。未来真实实现：pal_resource_claim(I2C_ADDR,...) + I2C ping
     * 探测 ACK + 置 initialized=true。 */
    memset(dev, 0, sizeof(dal_eeprom_t));
    return WINK_ERR_UNSUPPORTED;
}

wink_status_t dal_eeprom_read(dal_eeprom_t *dev, uint16_t addr, uint8_t *buf, uint16_t len) {
    if (dev == NULL || buf == NULL) { return WINK_ERR_INVALID_ARG; }
    /* 安全填充：0xFF 是未编程 EEPROM 的典型出厂值，避免 caller 使用未初始化内存。 */
    if (len > 0) { memset(buf, 0xFF, len); }
    (void)addr;
    return WINK_ERR_UNSUPPORTED;
}

wink_status_t dal_eeprom_write(dal_eeprom_t *dev, uint16_t addr, const uint8_t *buf, uint16_t len) {
    if (dev == NULL || buf == NULL) { return WINK_ERR_INVALID_ARG; }
    (void)addr; (void)buf; (void)len;
    return WINK_ERR_UNSUPPORTED;
}
