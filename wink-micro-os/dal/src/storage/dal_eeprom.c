#include "dal_eeprom.h"
#include "pal_hal.h"
#include "pal_osal.h"
#include <string.h>

wink_status_t dal_eeprom_init(dal_eeprom_t *dev, const dal_eeprom_config_t *cfg) {
    if (dev == NULL || cfg == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->i2c_addr == 0 || cfg->i2c_addr > 0x7Fu) { return WINK_ERR_INVALID_ARG; }

    /* TODO: 真实 I2C 初始化 + EEPROM 存在性探测 */
    memcpy(&dev->config, cfg, sizeof(dal_eeprom_config_t));
    dev->initialized = true;
    return WINK_OK;
}

wink_status_t dal_eeprom_read(dal_eeprom_t *dev, uint16_t addr, uint8_t *buf, uint16_t len) {
    if (dev == NULL || buf == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    if (addr + len > dev->config.capacity_bytes) { return WINK_ERR_OUT_OF_RANGE; }

    /* TODO: 真实 I2C 读取实现 */
    (void)addr;
    memset(buf, 0xFF, len);  /* 未初始化 EEPROM 默认值 */
    return WINK_OK;
}

wink_status_t dal_eeprom_write(dal_eeprom_t *dev, uint16_t addr, const uint8_t *buf, uint16_t len) {
    if (dev == NULL || buf == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    if (addr + len > dev->config.capacity_bytes) { return WINK_ERR_OUT_OF_RANGE; }

    /* TODO: 真实 I2C 写入 + 跨页自动拆分 + 写入等待 */
    (void)addr;
    (void)buf;
    (void)len;
    return WINK_OK;
}
