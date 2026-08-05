// SPDX-License-Identifier: Apache-2.0
#include "dal_eeprom.h"
#include "pal_hal.h"
#include "pal_osal.h"
#include "pal_resource.h"
#include "wink_pt_debug.h"
#include <string.h>

/* Non-blocking init (always compiled; STRICT-safe) */
wink_status_t dal_eeprom_init(dal_eeprom_t *dev, const dal_eeprom_config_t *cfg) {
    if (dev == NULL || cfg == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->owner == NULL) { return WINK_ERR_INVALID_ARG; }
    if (dev->initialized) { return WINK_ERR_ALREADY_INITIALIZED; }

    /* @experimental Stub: I2C EEPROM backend not yet implemented. */
    memset(dev, 0, sizeof(dal_eeprom_t));
    return WINK_ERR_UNSUPPORTED;
}

/* Non-blocking state machine API stubs */

wink_status_t dal_eeprom_request_read(dal_eeprom_t *dev, uint32_t addr, uint32_t len) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    if (addr + len > dev->config.capacity_bytes || len == 0) { return WINK_ERR_OUT_OF_RANGE; }
    if (dev->state == DAL_EEPROM_BUSY) { return WINK_ERR_BUSY; }
    (void)addr; (void)len;
    return WINK_ERR_UNSUPPORTED;
}

wink_status_t dal_eeprom_request_write(dal_eeprom_t *dev, uint32_t addr,
                                        const uint8_t *buf, uint32_t len) {
    if (dev == NULL || buf == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    if (addr + len > dev->config.capacity_bytes || len == 0) { return WINK_ERR_OUT_OF_RANGE; }
    if (dev->state == DAL_EEPROM_BUSY) { return WINK_ERR_BUSY; }
    (void)addr; (void)buf; (void)len;
    return WINK_ERR_UNSUPPORTED;
}

wink_status_t dal_eeprom_poll(dal_eeprom_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    return WINK_ERR_UNSUPPORTED;
}

wink_status_t dal_eeprom_get_status(const dal_eeprom_t *dev, dal_eeprom_state_t *out_state) {
    if (dev == NULL || out_state == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    *out_state = dev->state;
    return WINK_OK;
}

wink_status_t dal_eeprom_get_read_result(dal_eeprom_t *dev, uint8_t *buf, uint32_t len) {
    if (dev == NULL || buf == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    (void)len;
    return WINK_ERR_UNSUPPORTED;
}

/* Blocking convenience API (STRICT-guarded) */
#ifndef WINK_STRICT_NONBLOCKING

wink_status_t dal_eeprom_read_blocking(dal_eeprom_t *dev, uint32_t addr,
                                        uint8_t *buf, uint32_t len) {
    if (dev == NULL || buf == NULL) { return WINK_ERR_INVALID_ARG; }
    WINK_ASSERT_NONBLOCKING();
    (void)addr; (void)len;
    return WINK_ERR_UNSUPPORTED;
}

wink_status_t dal_eeprom_write_blocking(dal_eeprom_t *dev, uint32_t addr,
                                         const uint8_t *buf, uint32_t len) {
    if (dev == NULL || buf == NULL) { return WINK_ERR_INVALID_ARG; }
    WINK_ASSERT_NONBLOCKING();
    (void)addr; (void)buf; (void)len;
    return WINK_ERR_UNSUPPORTED;
}

#endif /* WINK_STRICT_NONBLOCKING */

wink_status_t dal_eeprom_deinit(dal_eeprom_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_OK; }

    uint8_t port = dev->config.i2c_port;
    uint16_t addr = dev->config.i2c_addr;
    const char *owner = dev->config.owner;

    uint32_t res_id = pal_resource_i2c_id(port, addr);
    WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_I2C_ADDR, res_id, owner));

    memset(dev, 0, sizeof(dal_eeprom_t));

    return WINK_OK;
}
