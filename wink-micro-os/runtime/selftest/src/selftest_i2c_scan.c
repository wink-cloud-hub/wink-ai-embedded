// SPDX-License-Identifier: Apache-2.0
/**
 * @file selftest_i2c_scan.c
 * @brief I2C bus scan selftest.
 */
#define LOG_TAG "selftest.i2c"

#include "wink_selftest.h"
#include "wink_selftest_internal.h"
#include "wink_status.h"
#include "wink_log.h"
#include "pal_hal.h"

#include <stdint.h>
#include <string.h>

wink_status_t wink_selftest_i2c_bus_scan(wink_selftest_result_t *r)
{
    uint8_t bitmap[16];
    memset(bitmap, 0, sizeof(bitmap));

    wink_status_t st = pal_i2c_scan(0 /* port */, 0x03, 0x77, bitmap, sizeof(bitmap));
    if (st == WINK_ERR_UNSUPPORTED) {
        r->note = "i2c not supported on this target";
        r->metric = 0;
        return WINK_ERR_UNSUPPORTED;
    }
    if (wink_status_is_error(st)) {
        r->note = "pal_i2c_scan returned error";
        r->metric = (uint32_t)st;
        return st;
    }

    uint32_t ack_count = 0;
    for (size_t i = 0; i < sizeof(bitmap); i++) {
        uint8_t b = bitmap[i];
        while (b) { ack_count += (b & 1u); b >>= 1; }
    }

    r->metric = ack_count;
    if (ack_count == 0) {
        r->note = "empty bus (all NACK) — driver healthy";
    } else {
        r->note = "devices detected on bus";
    }
    return WINK_OK;
}
