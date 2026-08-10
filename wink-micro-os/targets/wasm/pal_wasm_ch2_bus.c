// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_wasm_ch2_bus.c
 * @brief Wasm target Axis A (CH2) I2C synchronous bus transaction implementation.
 */

#include <emscripten.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "pal_hal.h"
#include "hal/pal_i2c.h"
#include "pal_wasm_common.h"
#include "wasm_bridge.h"

static bool s_i2c_bus_inited[PAL_I2C_PORTS] = {false};

wink_status_t pal_i2c_bus_init(uint8_t port, uint8_t sda, uint8_t scl, uint32_t hz)
{
    (void)sda; (void)scl; (void)hz;
    if (port >= PAL_I2C_PORTS) {
        return WINK_ERR_INVALID_ARG;
    }
    s_i2c_bus_inited[port] = true;
    return WINK_OK;
}

void pal_i2c_bus_deinit(uint8_t port)
{
    if (port < PAL_I2C_PORTS) {
        s_i2c_bus_inited[port] = false;
    }
}

wink_status_t pal_i2c_transfer(uint8_t port, uint16_t dev_addr,
                              const uint8_t *write_buf, uint32_t write_len,
                              uint8_t *read_buf, uint32_t read_len)
{
    if (port >= PAL_I2C_PORTS) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!s_i2c_bus_inited[port]) {
        return WINK_ERR_INVALID_STATE;
    }

    uint16_t drop_permil = pal_wasm_get_i2c_drop_permil();
    if (drop_permil > 0u) {
        uint32_t prng_state = pal_wasm_get_prng_state();
        bool should_drop = wink_phys_bus_drop(drop_permil, &prng_state);
        pal_wasm_advance_prng_state(prng_state);
        if (should_drop) {
            pal_wasm_log_fault(FAULT_TYPE_I2C_DROP, port);
            return WINK_ERR_IO;
        }
    }

    return js_pal_i2c_transfer(port, dev_addr, write_buf, write_len, read_buf, read_len)
           ? WINK_OK : WINK_ERR_IO;
}

wink_status_t pal_i2c_scan(uint8_t port, uint8_t start_addr, uint8_t end_addr,
                            uint8_t *out_found_bitmap, size_t bitmap_bytes)
{
    if (out_found_bitmap == NULL || bitmap_bytes < 16) {
        return WINK_ERR_INVALID_ARG;
    }
    if (port >= PAL_I2C_PORTS) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!s_i2c_bus_inited[port]) {
        return WINK_ERR_INVALID_STATE;
    }
    if (start_addr > end_addr || end_addr > 0x7F) {
        return WINK_ERR_INVALID_ARG;
    }

    /*
     * Device presence is owned by the TS I2CBus model; the C target has no
     * local device registry. The bus scan result is provided by the host via
     * js_pal_i2c_scan when available. Without a host override the bus reports
     * no devices (empty bitmap), which is a safe default.
     */
    memset(out_found_bitmap, 0, 16);
    return WINK_OK;
}

EMSCRIPTEN_KEEPALIVE
bool pal_wasm_i2c_transfer(uint8_t port, uint16_t dev_addr,
                           const uint8_t *write_buf, uint32_t write_len,
                           uint8_t *read_buf, uint32_t read_len)
{
    WASM_FAULT_GUARD_BOOL();

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

    wink_status_t st = pal_i2c_transfer(port, dev_addr, write_buf, write_len,
                                        read_buf, read_len);

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic pop
#endif

    return !wink_status_is_error(st);
}

wink_status_t pal_i2c_port_pins(uint8_t port, wink_pin_t *out_sda, wink_pin_t *out_scl) {
    if (out_sda == NULL && out_scl == NULL) { return WINK_ERR_INVALID_ARG; }
    if (port >= PAL_I2C_PORTS) { return WINK_ERR_INVALID_ARG; }
    return WINK_ERR_UNSUPPORTED;
}

void pal_wasm_ch2_bus_reset(void)
{
    memset(s_i2c_bus_inited, 0, sizeof(s_i2c_bus_inited));
}
