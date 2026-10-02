/* SPDX-License-Identifier: GPL-3.0-only */
#include "unity.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

extern void pal_wasm_target_clear_pending_reset(void);

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

void test_nvs_stale_handle_cannot_access_or_close_reused_slot(void) {
    nvs_handle_t old_handle = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_open("old_ns", NVS_READWRITE, &old_handle));
    nvs_close(old_handle);

    nvs_handle_t current = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_open("new_ns", NVS_READWRITE, &current));
    TEST_ASSERT_NOT_EQUAL(old_handle, current);
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG,
        nvs_set_u32(old_handle, "value", 1));
    nvs_close(old_handle);
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_set_u32(current, "value", 2));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, nvs_commit(old_handle));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_commit(current));
    nvs_close(current);
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

    /* Fill the selected simulation profile's entry pool. */
    for (int i = 0; i < CONFIG_NVS_MAX_ENTRIES; i++) {
        char key[16];
        snprintf(key, sizeof(key), "k%d", i);
        TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_set_u32(h, key, (uint32_t)i));
    }

    /* The first entry beyond the configured pool must fail. */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NVS_NOT_ENOUGH_SPACE, nvs_set_u32(h, "overflow_k", 999));

    nvs_close(h);
}

void test_nvs_commit_survives_soft_reset_but_handles_do_not(void) {
    nvs_handle_t old_handle = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_open("reset_persist", NVS_READWRITE, &old_handle));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_set_u32(old_handle, "persisted", 0x51A7));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_commit(old_handle));

    pal_wasm_target_clear_pending_reset();
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG,
        nvs_get_u32(old_handle, "persisted", &(uint32_t){0}));

    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_flash_init());
    nvs_handle_t new_handle = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_open("reset_persist", NVS_READWRITE, &new_handle));
    TEST_ASSERT_NOT_EQUAL(old_handle, new_handle);
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG,
        nvs_set_u32(old_handle, "persisted", 0));
    uint32_t value = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_get_u32(new_handle, "persisted", &value));
    TEST_ASSERT_EQUAL_UINT32(0x51A7, value);
    nvs_close(new_handle);
}

void test_nvs_uncommitted_discarded_on_deinit_reinit(void) {
    nvs_handle_t h1 = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_open("uncommit_test", NVS_READWRITE, &h1));

    /* Key 1: committed to persistent backing store */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_set_u32(h1, "committed_k", 12345));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_commit(h1));

    /* Key 2: uncommitted, only in RAM cache */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_set_u32(h1, "uncommitted_k", 99999));
    nvs_close(h1);

    /* Simulate power-cycle / soft-reset by deinit then init (RAM cleared, disk preserved) */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_flash_deinit());
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_flash_init());

    nvs_handle_t h2 = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_open("uncommit_test", NVS_READWRITE, &h2));

    /* Committed key must be present */
    uint32_t val1 = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_get_u32(h2, "committed_k", &val1));
    TEST_ASSERT_EQUAL_UINT32(12345, val1);

    /* Uncommitted key must have been discarded! */
    uint32_t val2 = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NVS_NOT_FOUND, nvs_get_u32(h2, "uncommitted_k", &val2));

    nvs_close(h2);
}

void test_nvs_iterators_and_type_metadata(void) {
    nvs_handle_t h = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_open("iter_ns", NVS_READWRITE, &h));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_set_i32(h, "counter", 42));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_set_str(h, "message", "hello_nvs"));

    nvs_type_t t = NVS_TYPE_ANY;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_find_key(h, "counter", &t));
    TEST_ASSERT_EQUAL_INT(NVS_TYPE_I32, t);
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_find_key(h, "message", &t));
    TEST_ASSERT_EQUAL_INT(NVS_TYPE_STR, t);

    nvs_iterator_t it = NULL;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_entry_find("nvs", "iter_ns", NVS_TYPE_ANY, &it));
    TEST_ASSERT_NOT_NULL(it);

    int count = 0;
    while (it != NULL) {
        nvs_entry_info_t info;
        TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_entry_info(it, &info));
        TEST_ASSERT_EQUAL_STRING("iter_ns", info.namespace_name);
        if (strcmp(info.key, "counter") == 0) {
            TEST_ASSERT_EQUAL_INT(NVS_TYPE_I32, info.type);
            count++;
        } else if (strcmp(info.key, "message") == 0) {
            TEST_ASSERT_EQUAL_INT(NVS_TYPE_STR, info.type);
            count++;
        }
        esp_err_t res = nvs_entry_next(&it);
        if (res != ESP_OK) {
            TEST_ASSERT_EQUAL_INT32(ESP_ERR_NVS_NOT_FOUND, res);
            TEST_ASSERT_NULL(it);
        }
    }
    TEST_ASSERT_EQUAL_INT(2, count);
    nvs_release_iterator(it);
    nvs_close(h);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_nvs_handle_lifecycle_and_limit);
    RUN_TEST(test_nvs_stale_handle_cannot_access_or_close_reused_slot);
    RUN_TEST(test_nvs_all_primitive_types);
    RUN_TEST(test_nvs_erase_and_not_found);
    RUN_TEST(test_nvs_entry_limit);
    RUN_TEST(test_nvs_commit_survives_soft_reset_but_handles_do_not);
    RUN_TEST(test_nvs_uncommitted_discarded_on_deinit_reinit);
    RUN_TEST(test_nvs_iterators_and_type_metadata);
    return UNITY_END();
}
