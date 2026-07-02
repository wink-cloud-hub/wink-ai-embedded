#include "dal_eeprom.h"
#include "pal_hal.h"
#include "pal_osal.h"
#include "pal_resource.h"
#include <string.h>

wink_status_t dal_eeprom_init(dal_eeprom_t *dev, const dal_eeprom_config_t *cfg) {
    if (dev == NULL || cfg == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->owner == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->i2c_addr == 0 || cfg->i2c_addr > 0x7Fu) { return WINK_ERR_INVALID_ARG; }

    /* Track A（M1）：I2C (port,addr) 粒度冲突治理。与 dal_ssd1306 同 SSOT 模式：
     * 同 port 不同 addr 允许共享总线；同 (port,addr) 不同 owner → BUSY。 */
    uint32_t res_id = pal_resource_i2c_id(cfg->i2c_port, cfg->i2c_addr);
    wink_status_t rs = pal_resource_claim(PAL_RESOURCE_I2C_ADDR, res_id, cfg->owner);
    if (wink_status_is_error(rs)) { return rs; }

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
