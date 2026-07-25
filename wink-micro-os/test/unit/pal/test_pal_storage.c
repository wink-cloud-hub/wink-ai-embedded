/**
 * @file test_pal_storage.c
 * @brief ADR-0008 PAL 存储抽象 host 内存单槽实现单测。
 */
#include "unity.h"
#include "pal_storage.h"

void setUp(void) { pal_storage_reset(); }
void tearDown(void) {}

void test_read_empty_returns_empty(void) {
    uint8_t buf[16];
    uint16_t len = 999;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_EMPTY, pal_storage_read("dtcfg", buf, sizeof buf, &len));
    TEST_ASSERT_EQUAL_UINT16(999, len);   /* out_len 在 EMPTY 时不被改写 */
}

void test_write_then_read_roundtrip(void) {
    static const uint8_t payload[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_storage_write("dtcfg", payload, sizeof payload));

    uint8_t buf[16];
    uint16_t len = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_storage_read("dtcfg", buf, sizeof buf, &len));
    TEST_ASSERT_EQUAL_UINT16((uint16_t)sizeof payload, len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, buf, sizeof payload);
}

void test_write_overwrite_latest_wins(void) {
    static const uint8_t a[4] = { 1, 1, 1, 1 };
    static const uint8_t b[2] = { 9, 9 };
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_storage_write("dtcfg", a, sizeof a));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_storage_write("dtcfg", b, sizeof b));

    uint8_t buf[16];
    uint16_t len = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_storage_read("dtcfg", buf, sizeof buf, &len));
    TEST_ASSERT_EQUAL_UINT16((uint16_t)sizeof b, len);
    TEST_ASSERT_EQUAL_UINT8(9u, buf[0]);
}

void test_erase_then_read_empty(void) {
    static const uint8_t p[4] = { 0 };
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_storage_write("dtcfg", p, sizeof p));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_storage_erase("dtcfg"));

    uint8_t buf[16];
    uint16_t len = 0;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_EMPTY, pal_storage_read("dtcfg", buf, sizeof buf, &len));
}

void test_reset_clears(void) {
    static const uint8_t p[4] = { 0 };
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_storage_write("dtcfg", p, sizeof p));
    pal_storage_reset();

    uint8_t buf[16];
    uint16_t len = 0;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_EMPTY, pal_storage_read("dtcfg", buf, sizeof buf, &len));
}

void test_read_cap_too_small(void) {
    static const uint8_t p[20] = { 0 };
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_storage_write("dtcfg", p, sizeof p));

    uint8_t buf[8];
    uint16_t len = 0;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, pal_storage_read("dtcfg", buf, sizeof buf, &len));
}

void test_read_null_args(void) {
    uint16_t len = 0;
    uint8_t buf[16];
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, pal_storage_read("dtcfg", NULL, 16, &len));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, pal_storage_read("dtcfg", buf, 16, NULL));
}

void test_erase_when_empty_is_noop(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_storage_erase("dtcfg"));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_read_empty_returns_empty);
    RUN_TEST(test_write_then_read_roundtrip);
    RUN_TEST(test_write_overwrite_latest_wins);
    RUN_TEST(test_erase_then_read_empty);
    RUN_TEST(test_reset_clears);
    RUN_TEST(test_read_cap_too_small);
    RUN_TEST(test_read_null_args);
    RUN_TEST(test_erase_when_empty_is_noop);
    return UNITY_END();
}
