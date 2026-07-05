/**
 * @file selftest_i2c_scan.c
 * @brief S6: I2C 总线扫描。
 *
 * 扫描 [0x03, 0x77] 地址范围，返回 ACK 位图：
 * - 裸板（无外设）期望 bitmap 为 0，pal_i2c_scan 返回 WINK_OK → PASS；
 * - 有外设时 bitmap 非 0 → PASS（metric 报告 ACK 数）；
 * - pal_i2c_scan 返回 WINK_ERR_UNSUPPORTED → SKIP（该 target 未实现 I2C）；
 * - 其他错误 → FAIL。
 *
 * NACK 不算 FAIL —— 与原 smoke_check_i2c_bus 的"NACK expected"语义一致。
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
    /* 128-bit bitmap: bit n = address n ACK'd. Little-endian byte order
     * (byte k holds bits for addresses 8*k .. 8*k+7, bit0 = addr 8*k). */
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

    /* 统计 ACK 数 */
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
    /* 扫描 API 本身工作正常（驱动 init+probe 不崩） → PASS，
     * 哪怕裸板空无设备。这正是 bring-up 场景的期望："驱动就绪，没有 panic"。 */
    return WINK_OK;
}
