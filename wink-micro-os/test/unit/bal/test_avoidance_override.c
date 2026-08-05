// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_avoidance_override.c
 * @brief Device tree flash configuration override end-to-end unit tests.
 */
#include "unity.h"
#include "device_tree.h"
#include "wink_dev_config.h"
#include "pal_storage.h"

#include <string.h>

void setUp(void) {
    pal_storage_reset();
    neck_servo = (dal_rc_servo_t){
        .config.pwm_channel = 0, .current_angle_ddeg = 900,
        .config.min_pulse_us = 500, .config.max_pulse_us = 2500
    };
    front_radar = (dal_ultrasonic_t){
        .config.trig_pin = 4, .config.echo_pin = 5, .last_distance = 0.0f
    };
}
void tearDown(void) {}

static uint16_t build_override_blob(uint8_t *buf) {
    uint16_t off = 0;
    uint32_t magic = WINK_DEV_CONFIG_MAGIC;
    uint16_t version = (uint16_t)WINK_DEV_CONFIG_VERSION;
    uint16_t count = 2;
    memcpy(buf + off, &magic, 4);   off += 4;
    memcpy(buf + off, &version, 2); off += 2;
    memcpy(buf + off, &count, 2);   off += 2;

    uint32_t id0 = DEV_ID_NECK_SERVO;
    uint8_t  sp[16] = {0};
    uint8_t  ch = 3;
    float    mn = 0.6f, mx = 2.4f;
    memcpy(sp + 0, &ch, 1);
    memcpy(sp + 1, &mn, 4);
    memcpy(sp + 5, &mx, 4);
    memcpy(buf + off, &id0, 4); off += 4;
    memcpy(buf + off, sp, 16);  off += 16;

    uint32_t id1 = DEV_ID_FRONT_RADAR;
    uint8_t  rp[16] = {0};
    uint16_t tg = 6, ec = 7;
    memcpy(rp + 0, &tg, 2);
    memcpy(rp + 2, &ec, 2);
    memcpy(buf + off, &id1, 4); off += 4;
    memcpy(buf + off, rp, 16);  off += 16;

    uint32_t crc = wink_dev_config_crc32(buf, off);
    memcpy(buf + off, &crc, 4); off += 4;
    return off;
}

void test_apply_flash_config_overrides_globals(void) {
    uint8_t blob[64];
    uint16_t len = build_override_blob(blob);

    wink_status_t w = pal_storage_write(WINK_DEV_CONFIG_KEY, blob, len);
    TEST_ASSERT_EQUAL_INT(WINK_OK, w);

    TEST_ASSERT_EQUAL_INT(WINK_OK, device_tree_apply_flash_config());
    TEST_ASSERT_EQUAL_UINT8(3u, neck_servo.config.pwm_channel);
    TEST_ASSERT_EQUAL_UINT16(600, neck_servo.config.min_pulse_us);
    TEST_ASSERT_EQUAL_UINT16(2400, neck_servo.config.max_pulse_us);
    TEST_ASSERT_EQUAL_UINT16(6, front_radar.config.trig_pin);
    TEST_ASSERT_EQUAL_UINT16(7, front_radar.config.echo_pin);
}

void test_apply_flash_config_empty_degrades(void) {
    wink_status_t s = device_tree_apply_flash_config();
    TEST_ASSERT_TRUE(wink_status_is_error(s));
    TEST_ASSERT_EQUAL_UINT8(0u, neck_servo.config.pwm_channel);
    TEST_ASSERT_EQUAL_UINT16(500, neck_servo.config.min_pulse_us);
    TEST_ASSERT_EQUAL_UINT16(4, front_radar.config.trig_pin);
    TEST_ASSERT_EQUAL_UINT16(5, front_radar.config.echo_pin);
}

void test_apply_flash_config_corrupt_degrades(void) {
    uint8_t blob[64];
    uint16_t len = build_override_blob(blob);
    blob[8] ^= 0xFFu;
    wink_status_t w = pal_storage_write(WINK_DEV_CONFIG_KEY, blob, len);
    TEST_ASSERT_EQUAL_INT(WINK_OK, w);

    wink_status_t s = device_tree_apply_flash_config();
    TEST_ASSERT_EQUAL_INT(WINK_ERR_CHECKSUM, s);
    TEST_ASSERT_EQUAL_UINT8(0u, neck_servo.config.pwm_channel);
    TEST_ASSERT_EQUAL_UINT16(4, front_radar.config.trig_pin);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_apply_flash_config_overrides_globals);
    RUN_TEST(test_apply_flash_config_empty_degrades);
    RUN_TEST(test_apply_flash_config_corrupt_degrades);
    return UNITY_END();
}
