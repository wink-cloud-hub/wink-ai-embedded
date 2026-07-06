#include "dal_eeprom.h"
#include "pal_hal.h"
#include "pal_osal.h"
#include "pal_resource.h"
#include "wink_pt_debug.h"   /* WINK_ASSERT_NONBLOCKING() (ADR-0017 层 3 runtime hook) */
#include <string.h>

/* ADR-0017 层 2 硬隔离：本文件的三个 blocking 实现均需在严格模式下随头文件一起
 * 从翻译单元中消失（否则符号仍存在于 .o，链接时可绕过头文件门控被误引用）。 */
#ifndef WINK_STRICT_NONBLOCKING

wink_status_t dal_eeprom_init(dal_eeprom_t *dev, const dal_eeprom_config_t *cfg) {
    /* 参数合法性校验（必须保留） */
    if (dev == NULL || cfg == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->owner == NULL) { return WINK_ERR_INVALID_ARG; }

    /* ADR-0017 层 3：blocking API 契约守卫（PT 上下文误调 → assert/fault）。
     * 放在参数校验之后：invalid-arg 快速路径不触发 nonblocking 断言。 */
    WINK_ASSERT_NONBLOCKING();

    /* @experimental Stub: I2C EEPROM backend not yet implemented.
     * 不 claim I2C 资源、不置 initialized=true、不做硬件探测——避免"假成功"反模式
     * （ADR-0012 契约诚实）。未来真实实现：pal_resource_claim(I2C_ADDR,...) + I2C ping
     * 探测 ACK + 置 initialized=true。 */
    memset(dev, 0, sizeof(dal_eeprom_t));
    return WINK_ERR_UNSUPPORTED;
}

wink_status_t dal_eeprom_read(dal_eeprom_t *dev, uint16_t addr, uint8_t *buf, uint16_t len) {
    if (dev == NULL || buf == NULL) { return WINK_ERR_INVALID_ARG; }
    WINK_ASSERT_NONBLOCKING();
    /* 安全填充：0xFF 是未编程 EEPROM 的典型出厂值，避免 caller 使用未初始化内存。 */
    if (len > 0) { memset(buf, 0xFF, len); }
    (void)addr;
    return WINK_ERR_UNSUPPORTED;
}

wink_status_t dal_eeprom_write(dal_eeprom_t *dev, uint16_t addr, const uint8_t *buf, uint16_t len) {
    if (dev == NULL || buf == NULL) { return WINK_ERR_INVALID_ARG; }
    WINK_ASSERT_NONBLOCKING();
    (void)addr; (void)buf; (void)len;
    return WINK_ERR_UNSUPPORTED;
}

#endif /* WINK_STRICT_NONBLOCKING */

wink_status_t dal_eeprom_deinit(dal_eeprom_t *dev) {
    /* ADR-0024 §4 deinit — checked: 1(N/A: no actuator safe-off needed for EEPROM)/
     *   2(N/A: I2C client, SDA/SCL owned by bus-owner)/3(N/A: no GPIO ISR)/
     *   4(N/A)/5(N/A)/6(client-level: does NOT call pal_i2c_bus_deinit; releases only
     *   own I2C_ADDR claim so ssd1306 or other clients on the same bus stay alive)/
     *   7(memset clears buffer/config)/8(NULL+uninit idempotent)/
     *   9(no waits, SW-only)/10(signature unified) */
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_OK; }  /* idempotent no-op on un-init dev */

    /* Read fields before memset. */
    uint8_t port = dev->config.i2c_port;
    uint16_t addr = dev->config.i2c_addr;
    const char *owner = dev->config.owner;

    /* 6. Release only this client's I2C address claim (bus-owner managed). */
    uint32_t res_id = pal_resource_i2c_id(port, addr);
    WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_I2C_ADDR, res_id, owner));

    /* 7. Clear the instance data completely */
    memset(dev, 0, sizeof(dal_eeprom_t));

    return WINK_OK;
}
