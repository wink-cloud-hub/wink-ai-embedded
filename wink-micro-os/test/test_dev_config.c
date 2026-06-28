/**
 * @file test_dev_config.c
 * @brief ADR-0008 覆写 blob 解析器 + CRC32 单测（host，target 无关共享核心）。
 *
 * 覆盖：CRC32 golden vector / 合法解析 / count==0 / magic·version·长度·CRC 降级 /
 *       buffer 过小 / 未命中 id 跳过 / apply 失败仅降级该项。
 *       apply 用独立 capturing stub（不依赖 DAL），DAL apply_override 在 test_dal_* 另测。
 */
#include "unity.h"
#include "wink_dev_config.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ---------- CRC32 golden vectors（CRC-32/ISO-HDLC 权威校验值） ---------- */
void test_crc32_known_vectors(void) {
    /* check value of "123456789" — zlib/PNG canonical */
    static const unsigned char ascii[] = "123456789";
    TEST_ASSERT_EQUAL_UINT32(0xCBF43926u, wink_dev_config_crc32(ascii, 9));
    /* empty input → 0 */
    TEST_ASSERT_EQUAL_UINT32(0u, wink_dev_config_crc32((const uint8_t *)"", 0));
}

/* ---------- apply capturing stub（独立于 DAL） ---------- */
typedef struct {
    uint8_t       params[WINK_DEV_CONFIG_PARAMS_SIZE];
    uint16_t      len_seen;
    int           call_count;
    wink_status_t forced_result;
} apply_capture_t;

static wink_status_t capturing_apply(void *dev, const uint8_t *params, uint16_t len) {
    apply_capture_t *cap = (apply_capture_t *)dev;
    cap->call_count++;
    cap->len_seen = len;
    memcpy(cap->params, params,
           len < WINK_DEV_CONFIG_PARAMS_SIZE ? len : WINK_DEV_CONFIG_PARAMS_SIZE);
    return cap->forced_result;
}

/* ---------- blob builder：生成含正确 CRC 的合法 blob，返回总长 ---------- */
static uint16_t build_valid_blob(uint8_t *buf,
                                 const uint32_t *ids,
                                 const uint8_t params[][WINK_DEV_CONFIG_PARAMS_SIZE],
                                 uint16_t count) {
    uint16_t off = 0;
    uint32_t magic = WINK_DEV_CONFIG_MAGIC;
    uint16_t version = (uint16_t)WINK_DEV_CONFIG_VERSION;
    memcpy(buf + off, &magic, 4);   off += 4;
    memcpy(buf + off, &version, 2); off += 2;
    memcpy(buf + off, &count, 2);   off += 2;
    for (uint16_t i = 0; i < count; i++) {
        memcpy(buf + off, &ids[i], 4); off += 4;
        memcpy(buf + off, params[i], WINK_DEV_CONFIG_PARAMS_SIZE);
        off += WINK_DEV_CONFIG_PARAMS_SIZE;
    }
    uint32_t crc = wink_dev_config_crc32(buf, off);
    memcpy(buf + off, &crc, 4); off += 4;
    return off;
}

/* 重算并回填 CRC（用于单独触发 magic/version/长度 校验、隔离 CRC 干扰） */
static void recompute_crc(uint8_t *buf, uint16_t len) {
    uint32_t crc = wink_dev_config_crc32(buf, (uint16_t)(len - WINK_DEV_CONFIG_CRC_SIZE));
    memcpy(buf + len - WINK_DEV_CONFIG_CRC_SIZE, &crc, 4);
}

/* ---------- 用例 ---------- */
void test_apply_valid_blob_hits_registry(void) {
    uint8_t blob[64];
    uint32_t ids[1] = { 42u };
    static const uint8_t params[1][WINK_DEV_CONFIG_PARAMS_SIZE] = {
        { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 }
    };
    uint16_t len = build_valid_blob(blob, ids, params, 1);

    apply_capture_t cap;
    memset(&cap, 0, sizeof cap);
    cap.forced_result = WINK_OK;
    wink_dev_override_entry_t reg[1] = { { 42u, &cap, capturing_apply } };

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_dev_config_apply(blob, len, reg, 1));
    TEST_ASSERT_EQUAL_INT(1, cap.call_count);
    TEST_ASSERT_EQUAL_UINT16(WINK_DEV_CONFIG_PARAMS_SIZE, cap.len_seen);
    TEST_ASSERT_EQUAL_UINT8(7u, cap.params[6]);
}

void test_apply_count_zero_is_noop_success(void) {
    uint8_t blob[16];
    uint16_t len = build_valid_blob(blob, NULL, NULL, 0);  /* header+crc = 12 */
    TEST_ASSERT_EQUAL_UINT16(WINK_DEV_CONFIG_HEADER_SIZE + WINK_DEV_CONFIG_CRC_SIZE, len);

    apply_capture_t cap;
    memset(&cap, 0, sizeof cap);
    wink_dev_override_entry_t reg[1] = { { 1u, &cap, capturing_apply } };

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_dev_config_apply(blob, len, reg, 1));
    TEST_ASSERT_EQUAL_INT(0, cap.call_count);
}

void test_apply_bad_magic_degrades(void) {
    uint8_t blob[64];
    uint32_t ids[1] = { 1u };
    static const uint8_t params[1][WINK_DEV_CONFIG_PARAMS_SIZE] = { {0} };
    uint16_t len = build_valid_blob(blob, ids, params, 1);
    blob[0] ^= 0xFFu;   /* 破坏 magic */

    apply_capture_t cap;
    memset(&cap, 0, sizeof cap);
    wink_dev_override_entry_t reg[1] = { { 1u, &cap, capturing_apply } };

    TEST_ASSERT_EQUAL_INT(WINK_ERR_CONFIG_CORRUPT_DEGRADED,
                          wink_dev_config_apply(blob, len, reg, 1));
    TEST_ASSERT_EQUAL_INT(0, cap.call_count);   /* 未写任何字段 */
}

void test_apply_bad_crc_returns_checksum(void) {
    uint8_t blob[64];
    uint32_t ids[1] = { 1u };
    static const uint8_t params[1][WINK_DEV_CONFIG_PARAMS_SIZE] = { {0} };
    uint16_t len = build_valid_blob(blob, ids, params, 1);
    blob[8] ^= 0xFFu;   /* 破坏 item0 的 device_id 字节（body 内，CRC 外） */

    apply_capture_t cap;
    memset(&cap, 0, sizeof cap);
    wink_dev_override_entry_t reg[1] = { { 1u, &cap, capturing_apply } };

    TEST_ASSERT_EQUAL_INT(WINK_ERR_CHECKSUM, wink_dev_config_apply(blob, len, reg, 1));
    TEST_ASSERT_EQUAL_INT(0, cap.call_count);
}

void test_apply_bad_version_degrades(void) {
    uint8_t blob[64];
    uint32_t ids[1] = { 1u };
    static const uint8_t params[1][WINK_DEV_CONFIG_PARAMS_SIZE] = { {0} };
    uint16_t len = build_valid_blob(blob, ids, params, 1);
    uint16_t bad_ver = 999u;
    memcpy(blob + 4, &bad_ver, 2);   /* version 字段 != 1 */
    recompute_crc(blob, len);         /* 重算 CRC 以隔离 version 校验 */

    apply_capture_t cap;
    memset(&cap, 0, sizeof cap);
    wink_dev_override_entry_t reg[1] = { { 1u, &cap, capturing_apply } };

    TEST_ASSERT_EQUAL_INT(WINK_ERR_CONFIG_CORRUPT_DEGRADED,
                          wink_dev_config_apply(blob, len, reg, 1));
    TEST_ASSERT_EQUAL_INT(0, cap.call_count);
}

void test_apply_length_mismatch_degrades(void) {
    uint8_t blob[64];
    uint32_t ids[1] = { 1u };
    static const uint8_t params[1][WINK_DEV_CONFIG_PARAMS_SIZE] = { {0} };
    uint16_t len = build_valid_blob(blob, ids, params, 1);
    uint16_t bad_count = 2u;          /* 声称 2 个 item，实际只 1 个 */
    memcpy(blob + 6, &bad_count, 2);
    recompute_crc(blob, len);

    apply_capture_t cap;
    memset(&cap, 0, sizeof cap);
    wink_dev_override_entry_t reg[1] = { { 1u, &cap, capturing_apply } };

    TEST_ASSERT_EQUAL_INT(WINK_ERR_CONFIG_CORRUPT_DEGRADED,
                          wink_dev_config_apply(blob, len, reg, 1));
}

void test_apply_buffer_too_small_invalid_arg(void) {
    uint8_t blob[8] = {0};   /* < 12 */
    apply_capture_t cap;
    memset(&cap, 0, sizeof cap);
    wink_dev_override_entry_t reg[1] = { { 1u, &cap, capturing_apply } };

    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_dev_config_apply(blob, 8, reg, 1));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_dev_config_apply(NULL, 12, reg, 1));
}

void test_apply_unmatched_id_skipped_others_apply(void) {
    uint8_t blob[64];
    uint32_t ids[2] = { 1u, 999u };   /* 999 不在注册表 */
    static const uint8_t params[2][WINK_DEV_CONFIG_PARAMS_SIZE] = { {1}, {2} };
    uint16_t len = build_valid_blob(blob, ids, params, 2);

    apply_capture_t cap;
    memset(&cap, 0, sizeof cap);
    cap.forced_result = WINK_OK;
    wink_dev_override_entry_t reg[1] = { { 1u, &cap, capturing_apply } };

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_dev_config_apply(blob, len, reg, 1));
    TEST_ASSERT_EQUAL_INT(1, cap.call_count);   /* 仅 id=1 应用；999 跳过、不崩 */
}

void test_apply_failed_apply_degrades_item_only(void) {
    uint8_t blob[64];
    uint32_t ids[1] = { 1u };
    static const uint8_t params[1][WINK_DEV_CONFIG_PARAMS_SIZE] = { {0} };
    uint16_t len = build_valid_blob(blob, ids, params, 1);

    apply_capture_t cap;
    memset(&cap, 0, sizeof cap);
    cap.forced_result = WINK_ERR_INVALID_ARG;   /* 模拟 DAL apply 拒绝非法 params */
    wink_dev_override_entry_t reg[1] = { { 1u, &cap, capturing_apply } };

    /* 该 item 降级，但整体解析仍 OK（不因单 item 失败而中断） */
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_dev_config_apply(blob, len, reg, 1));
    TEST_ASSERT_EQUAL_INT(1, cap.call_count);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_crc32_known_vectors);
    RUN_TEST(test_apply_valid_blob_hits_registry);
    RUN_TEST(test_apply_count_zero_is_noop_success);
    RUN_TEST(test_apply_bad_magic_degrades);
    RUN_TEST(test_apply_bad_crc_returns_checksum);
    RUN_TEST(test_apply_bad_version_degrades);
    RUN_TEST(test_apply_length_mismatch_degrades);
    RUN_TEST(test_apply_buffer_too_small_invalid_arg);
    RUN_TEST(test_apply_unmatched_id_skipped_others_apply);
    RUN_TEST(test_apply_failed_apply_degrades_item_only);
    return UNITY_END();
}
