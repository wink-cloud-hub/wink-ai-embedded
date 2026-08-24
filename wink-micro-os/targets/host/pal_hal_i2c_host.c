// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_hal_i2c_host.c
 * @brief Host target PAL HAL I2C bus subsystem implementation.
 */
#include "hal/pal_i2c.h"
#include "hal/pal_target_caps.h"
#include "pal_resource.h"
#include "host_test_ctrl.h"
#include <stdio.h>
#include <string.h>

extern void host_record_i2c(uint8_t port, uint16_t addr, uint32_t write_len);

static bool s_i2c_bus_inited[PAL_I2C_PORTS] = {false};

wink_status_t pal_i2c_bus_init(uint8_t port, wink_pin_t sda, wink_pin_t scl, uint32_t hz) {
    (void)sda; (void)scl; (void)hz;
    if (port >= PAL_I2C_PORTS) { return WINK_ERR_INVALID_ARG; }
    s_i2c_bus_inited[port] = true;
    return WINK_OK;
}

wink_status_t pal_i2c_bus_deinit(uint8_t port) {
    if (port >= PAL_I2C_PORTS) { return WINK_ERR_INVALID_ARG; }
    s_i2c_bus_inited[port] = false;
    return WINK_OK;
}

wink_status_t pal_i2c_bus_recover(uint8_t port) {
    if (port >= PAL_I2C_PORTS) { return WINK_ERR_INVALID_ARG; }
    return WINK_OK;
}

wink_status_t pal_i2c_transfer_timeout(uint8_t port, uint16_t addr,
                                       const uint8_t *w, uint32_t wl,
                                       uint8_t *r, uint32_t rl,
                                       uint32_t timeout_ms) {
    (void)timeout_ms;
    if (port >= PAL_I2C_PORTS) { return WINK_ERR_INVALID_ARG; }
    if (!s_i2c_bus_inited[port]) {
        printf("WINK_WARN: I2C port %d transfer called before bus init, lazy initializing (deprecated path)\n", port);
        s_i2c_bus_inited[port] = true;
    }
    (void)w;
    host_record_i2c(port, addr, wl);
    if (r != NULL && rl > 0u) {
        memset(r, 0, rl);
    }
    return WINK_OK;
}

wink_status_t pal_i2c_scan(uint8_t port, uint8_t start_addr, uint8_t end_addr,
                            uint8_t *out_found_bitmap, size_t bitmap_bytes) {
    if (out_found_bitmap == NULL || bitmap_bytes < 16) {
        return WINK_ERR_INVALID_ARG;
    }
    if (port >= PAL_I2C_PORTS) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!s_i2c_bus_inited[port]) {
        printf("WINK_WARN: I2C port %d scan called before bus init, lazy initializing (deprecated path)\n", port);
        s_i2c_bus_inited[port] = true;
    }
    if (start_addr > end_addr || end_addr > 0x7F) {
        return WINK_ERR_INVALID_ARG;
    }
    memset(out_found_bitmap, 0, 16);
    return WINK_OK;
}

wink_status_t pal_i2c_port_pins(uint8_t port, wink_pin_t *out_sda, wink_pin_t *out_scl) {
    if (out_sda == NULL && out_scl == NULL) { return WINK_ERR_INVALID_ARG; }
    if (port >= PAL_I2C_PORTS) { return WINK_ERR_INVALID_ARG; }
    return WINK_ERR_UNSUPPORTED;
}
