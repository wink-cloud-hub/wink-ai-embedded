// SPDX-License-Identifier: Apache-2.0
#include "dal_eeprom.h"
#include "hal/pal_i2c.h"
#include "pal_osal.h"
#include "pal_resource.h"
#include "wink_pt_debug.h"
#include <string.h>

#define EEPROM_INTERNAL_BUF_MAX 128
static uint8_t s_eeprom_rx_buf[EEPROM_INTERNAL_BUF_MAX];

/* Non-blocking init (always compiled; STRICT-safe) */
wink_status_t dal_eeprom_init(dal_eeprom_t *dev, const dal_eeprom_config_t *cfg) {
    if (dev == NULL || cfg == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->owner == NULL) { return WINK_ERR_INVALID_ARG; }
    if (dev->initialized) { return WINK_ERR_ALREADY_INITIALIZED; }

    uint32_t res_id = pal_resource_i2c_id(cfg->i2c_port, cfg->i2c_addr);
    wink_status_t rs = pal_resource_claim(PAL_RESOURCE_I2C_ADDR, res_id, cfg->owner);
    if (rs != WINK_OK) {
        return rs;
    }

    memset(dev, 0, sizeof(dal_eeprom_t));
    dev->config = *cfg;
    dev->state = DAL_EEPROM_IDLE;
    dev->initialized = true;
    return WINK_OK;
}

wink_status_t dal_eeprom_request_read(dal_eeprom_t *dev, uint32_t addr, uint32_t len) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    if (addr + len > dev->config.capacity_bytes || len == 0 || len > EEPROM_INTERNAL_BUF_MAX) {
        return WINK_ERR_OUT_OF_RANGE;
    }
    if (dev->state == DAL_EEPROM_BUSY) { return WINK_ERR_BUSY; }

    dev->req_addr = addr;
    dev->req_len = len;

    uint8_t addr_buf[2];
    addr_buf[0] = (uint8_t)((addr >> 8) & 0xFF);
    addr_buf[1] = (uint8_t)(addr & 0xFF);

    uint8_t *dst = (dev->req_buf != NULL) ? dev->req_buf : s_eeprom_rx_buf;
    wink_status_t st = pal_i2c_transfer(dev->config.i2c_port, dev->config.i2c_addr,
                                        addr_buf, 2, dst, len);

    dev->last_status = st;
    dev->state = (st == WINK_OK) ? DAL_EEPROM_READY : DAL_EEPROM_ERROR;
    return st;
}

wink_status_t dal_eeprom_request_write(dal_eeprom_t *dev, uint32_t addr,
                                        const uint8_t *buf, uint32_t len) {
    if (dev == NULL || buf == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    if (addr + len > dev->config.capacity_bytes || len == 0) { return WINK_ERR_OUT_OF_RANGE; }
    if (dev->state == DAL_EEPROM_BUSY) { return WINK_ERR_BUSY; }

    uint8_t tx_buf[EEPROM_INTERNAL_BUF_MAX + 2];
    if (len + 2 > sizeof(tx_buf)) return WINK_ERR_OUT_OF_RANGE;

    tx_buf[0] = (uint8_t)((addr >> 8) & 0xFF);
    tx_buf[1] = (uint8_t)(addr & 0xFF);
    memcpy(&tx_buf[2], buf, len);

    wink_status_t st = pal_i2c_transfer(dev->config.i2c_port, dev->config.i2c_addr,
                                        tx_buf, len + 2, NULL, 0);
    dev->last_status = st;
    dev->state = (st == WINK_OK) ? DAL_EEPROM_READY : DAL_EEPROM_ERROR;
    return st;
}

wink_status_t dal_eeprom_poll(dal_eeprom_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    return WINK_OK;
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
    if (dev->state != DAL_EEPROM_READY) { return WINK_ERR_INVALID_STATE; }

    uint32_t copy_len = (len < dev->req_len) ? len : dev->req_len;
    uint8_t *src = (dev->req_buf != NULL) ? dev->req_buf : s_eeprom_rx_buf;
    if (buf != src) {
        memcpy(buf, src, copy_len);
    }
    dev->state = DAL_EEPROM_IDLE;
    return WINK_OK;
}

#ifndef WINK_STRICT_NONBLOCKING

wink_status_t dal_eeprom_read_blocking(dal_eeprom_t *dev, uint32_t addr,
                                        uint8_t *buf, uint32_t len) {
    if (dev == NULL || buf == NULL) { return WINK_ERR_INVALID_ARG; }
    WINK_ASSERT_NONBLOCKING();

    dev->req_buf = buf;
    wink_status_t st = dal_eeprom_request_read(dev, addr, len);
    dev->req_buf = NULL;
    if (st != WINK_OK) return st;
    dev->state = DAL_EEPROM_IDLE;
    return WINK_OK;
}

wink_status_t dal_eeprom_write_blocking(dal_eeprom_t *dev, uint32_t addr,
                                         const uint8_t *buf, uint32_t len) {
    if (dev == NULL || buf == NULL) { return WINK_ERR_INVALID_ARG; }
    WINK_ASSERT_NONBLOCKING();

    return dal_eeprom_request_write(dev, addr, buf, len);
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
