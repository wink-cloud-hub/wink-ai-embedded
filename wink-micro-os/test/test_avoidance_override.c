/**
 * @file test_avoidance_override.c
 * @brief ADR-0008 avoidance_car 设备树覆写端到端（host 内存存储）。
 *
 * 验证 device_tree_apply_flash_config() 经 pal_storage 读 blob → 注册表派发 →
 * 真实 DAL apply_override 改写全局 neck_servo/front_radar 字段；空/损坏静默降级保默认。
 * 链 device_tree.c + dal + pal_host（含 pal_storage_host + wink_dev_config）。
 */
#include "unity.h"
#include "device_tree.h"
#include "wink_dev_config.h"
#include "pal_storage.h"

#include <string.h>

void setUp(void) {
    pal_storage_reset();
    /* 重置全局到编译期默认，保证每条用例起点确定 */
    neck_servo = (dal_servo_t){
        .pwm_channel = 0, .current_angle = 90.0f,
        .min_pulse_ms = 0.5f, .max_pulse_ms = 2.5f
    };
    front_radar = (dal_ultrasonic_t){
        .trig_pin = 4, .echo_pin = 5, .last_distance = 0.0f
    };
}
void tearDown(void) {}

/* 构造覆写 blob（servo + radar 两 item，含正确 CRC） */
static uint16_t build_override_blob(uint8_t *buf) {
    uint16_t off = 0;
    uint32_t magic = WINK_DEV_CONFIG_MAGIC;
    uint16_t version = (uint16_t)WINK_DEV_CONFIG_VERSION;
    uint16_t count = 2;
    memcpy(buf + off, &magic, 4);   off += 4;
    memcpy(buf + off, &version, 2); off += 2;
    memcpy(buf + off, &count, 2);   off += 2;

    /* item 0: servo → ch3 / min0.6 / max2.4 */
    uint32_t id0 = DEV_ID_NECK_SERVO;
    uint8_t  sp[16] = {0};
    uint8_t  ch = 3;
    float    mn = 0.6f, mx = 2.4f;
    memcpy(sp + 0, &ch, 1);
    memcpy(sp + 1, &mn, 4);
    memcpy(sp + 5, &mx, 4);
    memcpy(buf + off, &id0, 4); off += 4;
    memcpy(buf + off, sp, 16);  off += 16;

    /* item 1: radar → trig6 / echo7 */
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
    TEST_ASSERT_EQUAL_UINT8(3u, neck_servo.pwm_channel);
    TEST_ASSERT_EQUAL_FLOAT(0.6f, neck_servo.min_pulse_ms);
    TEST_ASSERT_EQUAL_FLOAT(2.4f, neck_servo.max_pulse_ms);
    TEST_ASSERT_EQUAL_UINT16(6, front_radar.trig_pin);
    TEST_ASSERT_EQUAL_UINT16(7, front_radar.echo_pin);
}

void test_apply_flash_config_empty_degrades(void) {
    /* 未写入 → apply 返错（EMPTY），字段保持编译期默认 */
    wink_status_t s = device_tree_apply_flash_config();
    TEST_ASSERT_TRUE(wink_status_is_error(s));
    TEST_ASSERT_EQUAL_UINT8(0u, neck_servo.pwm_channel);
    TEST_ASSERT_EQUAL_FLOAT(0.5f, neck_servo.min_pulse_ms);
    TEST_ASSERT_EQUAL_UINT16(4, front_radar.trig_pin);
    TEST_ASSERT_EQUAL_UINT16(5, front_radar.echo_pin);
}

void test_apply_flash_config_corrupt_degrades(void) {
    uint8_t blob[64];
    uint16_t len = build_override_blob(blob);
    blob[8] ^= 0xFFu;   /* 破坏 body → CRC 失败 */
    wink_status_t w = pal_storage_write(WINK_DEV_CONFIG_KEY, blob, len);
    TEST_ASSERT_EQUAL_INT(WINK_OK, w);

    wink_status_t s = device_tree_apply_flash_config();
    TEST_ASSERT_EQUAL_INT(WINK_ERR_CHECKSUM, s);
    TEST_ASSERT_EQUAL_UINT8(0u, neck_servo.pwm_channel);   /* 字段保持默认 */
    TEST_ASSERT_EQUAL_UINT16(4, front_radar.trig_pin);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_apply_flash_config_overrides_globals);
    RUN_TEST(test_apply_flash_config_empty_degrades);
    RUN_TEST(test_apply_flash_config_corrupt_degrades);
    return UNITY_END();
}
