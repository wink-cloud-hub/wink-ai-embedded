/* SPDX-License-Identifier: GPL-3.0-only */
#include "unity.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

void setUp(void) {
    nvs_flash_init();
    nvs_flash_erase();
}

void tearDown(void) {
    nvs_flash_erase();
    nvs_flash_deinit();
}

void test_nvs_handle_lifecycle_and_limit(void) {
    /* Invalid arguments */
    nvs_handle_t h = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, nvs_open(NULL, NVS_READWRITE, &h));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, nvs_open("test", NVS_READWRITE, NULL));

    /* Open 4 handles */
    nvs_handle_t handles[4] = { 0 };
    for (int i = 0; i < 4; i++) {
        char ns[16];
        snprintf(ns, sizeof(ns), "ns%d", i);
        TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_open(ns, NVS_READWRITE, &handles[i]));
        TEST_ASSERT_NOT_EQUAL(0, handles[i]);
    }

    /* 5th handle exceeds budget -> ESP_ERR_NVS_NOT_ENOUGH_SPACE */
    nvs_handle_t h5 = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NVS_NOT_ENOUGH_SPACE, nvs_open("ns_extra", NVS_READWRITE, &h5));

    /* Close first handle and reopen */
    nvs_close(handles[0]);
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_open("ns_new", NVS_READWRITE, &handles[0]));

    for (int i = 0; i < 4; i++) {
        nvs_close(handles[i]);
    }
}

void test_nvs_all_primitive_types(void) {
    nvs_handle_t h = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_open("storage", NVS_READWRITE, &h));

    /* u8 / i8 */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_set_u8(h, "k_u8", 0xFE));
    uint8_t v_u8 = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_get_u8(h, "k_u8", &v_u8));
    TEST_ASSERT_EQUAL_UINT8(0xFE, v_u8);

    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_set_i8(h, "k_i8", -42));
    int8_t v_i8 = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_get_i8(h, "k_i8", &v_i8));
    TEST_ASSERT_EQUAL_INT8(-42, v_i8);

    /* u16 / i16 */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_set_u16(h, "k_u16", 65500));
    uint16_t v_u16 = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_get_u16(h, "k_u16", &v_u16));
    TEST_ASSERT_EQUAL_UINT16(65500, v_u16);

    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_set_i16(h, "k_i16", -12345));
    int16_t v_i16 = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_get_i16(h, "k_i16", &v_i16));
    TEST_ASSERT_EQUAL_INT16(-12345, v_i16);

    /* u32 / i32 */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_set_u32(h, "k_u32", 3000000000U));
    uint32_t v_u32 = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_get_u32(h, "k_u32", &v_u32));
    TEST_ASSERT_EQUAL_UINT32(3000000000U, v_u32);

    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_set_i32(h, "k_i32", -999999));
    int32_t v_i32 = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_get_i32(h, "k_i32", &v_i32));
    TEST_ASSERT_EQUAL_INT32(-999999, v_i32);

    /* u64 / i64 */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_set_u64(h, "k_u64", 123456789012345ULL));
    uint64_t v_u64 = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_get_u64(h, "k_u64", &v_u64));
    TEST_ASSERT_EQUAL_UINT64(123456789012345ULL, v_u64);

    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_set_i64(h, "k_i64", -98765432109876LL));
    int64_t v_i64 = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_get_i64(h, "k_i64", &v_i64));
    TEST_ASSERT_EQUAL_INT64(-98765432109876LL, v_i64);

    /* String */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_set_str(h, "k_str", "WinkMicroOS"));
    char str_buf[32] = { 0 };
    size_t str_len = sizeof(str_buf);
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_get_str(h, "k_str", str_buf, &str_len));
    TEST_ASSERT_EQUAL_STRING("WinkMicroOS", str_buf);

    /* Blob */
    uint8_t blob_in[4] = { 1, 2, 3, 4 };
    uint8_t blob_out[4] = { 0 };
    size_t blob_len = sizeof(blob_out);
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_set_blob(h, "k_blob", blob_in, sizeof(blob_in)));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_get_blob(h, "k_blob", blob_out, &blob_len));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(blob_in, blob_out, 4);

    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_commit(h));
    nvs_close(h);
}

void test_nvs_erase_and_not_found(void) {
    nvs_handle_t h = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_open("erase_test", NVS_READWRITE, &h));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_set_u32(h, "key1", 42));

    /* Query non-existent key */
    uint32_t val = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NVS_NOT_FOUND, nvs_get_u32(h, "no_such_key", &val));

    /* Erase flash */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_flash_erase());

    /* Previously stored key should now be NOT_FOUND */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NVS_NOT_FOUND, nvs_get_u32(h, "key1", &val));

    nvs_close(h);
}

void test_nvs_entry_limit(void) {
    nvs_handle_t h = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_open("limit_ns", NVS_READWRITE, &h));

    /* Add 16 entries */
    for (int i = 0; i < 16; i++) {
        char key[16];
        snprintf(key, sizeof(key), "k%d", i);
        TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_set_u32(h, key, (uint32_t)i));
    }

    /* 17th entry should fail with ESP_ERR_NVS_NOT_ENOUGH_SPACE */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NVS_NOT_ENOUGH_SPACE, nvs_set_u32(h, "overflow_k", 999));

    nvs_close(h);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_nvs_handle_lifecycle_and_limit);
    RUN_TEST(test_nvs_all_primitive_types);
    RUN_TEST(test_nvs_erase_and_not_found);
    RUN_TEST(test_nvs_entry_limit);
    return UNITY_END();
}
