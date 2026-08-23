// SPDX-License-Identifier: Apache-2.0
#include "dal_gps.h"
#include "hal/pal_uart.h"
#include "pal_resource.h"
#include "pal_osal.h"
#include <string.h>

wink_status_t dal_gps_init(dal_gps_t *dev, const dal_gps_config_t *cfg) {
    if (dev == NULL || cfg == NULL) return WINK_ERR_INVALID_ARG;
    if (cfg->owner == NULL) return WINK_ERR_INVALID_ARG;
    if (dev->initialized) return WINK_ERR_ALREADY_INITIALIZED;

    uint32_t baud = (cfg->baudrate > 0) ? cfg->baudrate : 9600;
    wink_status_t st = pal_uart_init(cfg->uart_port, -1, -1, baud);
    if (st != WINK_OK) {
        return st;
    }

    memset(dev, 0, sizeof(dal_gps_t));
    dev->config = *cfg;
    dev->initialized = true;
    return WINK_OK;
}

#ifndef WINK_STRICT_NONBLOCKING
wink_status_t dal_gps_init_blocking(dal_gps_t *dev, const dal_gps_config_t *cfg) {
    return dal_gps_init(dev, cfg);
}
#endif

wink_status_t dal_gps_poll(dal_gps_t *dev) {
    if (dev == NULL || !dev->initialized) return WINK_ERR_NOT_INITIALIZED;

    uint8_t rx_buf[32];
    uint32_t actual = 0;
    wink_status_t st = pal_uart_read(dev->config.uart_port, rx_buf, sizeof(rx_buf), &actual);
    if (st == WINK_OK && actual > 0) {
        dev->last_position.timestamp_ms = (uint32_t)pal_os_get_ms();
        dev->last_fix_time_ms = dev->last_position.timestamp_ms;
    }
    return WINK_OK;
}

wink_status_t dal_gps_get_position(const dal_gps_t *dev, dal_gps_position_t *pos) {
    if (dev == NULL || pos == NULL) return WINK_ERR_INVALID_ARG;
    if (!dev->initialized) return WINK_ERR_NOT_INITIALIZED;

    *pos = dev->last_position;
    return WINK_OK;
}

wink_status_t dal_gps_deinit(dal_gps_t *dev) {
    if (dev == NULL) return WINK_ERR_INVALID_ARG;
    if (!dev->initialized) return WINK_OK;

    pal_uart_deinit(dev->config.uart_port);
    dev->initialized = false;
    return WINK_OK;
}
