/* SPDX-License-Identifier: GPL-3.0-only */
#include "unity.h"
#include "esp_sim_fault.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/i2c_master.h"
#include "host/ble_gap.h"
#include <string.h>

void setUp(void) {
    esp_fault_sim_reset();
    nvs_flash_deinit();
}

void tearDown(void) {
    esp_fault_sim_reset();
    nvs_flash_deinit();
}

void test_fault_injection_and_query(void) {
    TEST_ASSERT_FALSE(sim_esp_fault_is_active(ESP_FAULT_DOMAIN_I2C, ESP_FAULT_I2C_NACK));
    TEST_ASSERT_EQUAL_UINT32(0, sim_esp_fault_get_param(ESP_FAULT_DOMAIN_I2C, ESP_FAULT_I2C_NACK));

    int ret = sim_esp_fault_inject(ESP_FAULT_DOMAIN_I2C, ESP_FAULT_I2C_NACK, 0x48);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_TRUE(sim_esp_fault_is_active(ESP_FAULT_DOMAIN_I2C, ESP_FAULT_I2C_NACK));
    TEST_ASSERT_EQUAL_UINT32(0x48, sim_esp_fault_get_param(ESP_FAULT_DOMAIN_I2C, ESP_FAULT_I2C_NACK));

    /* Updating param of existing fault */
    ret = sim_esp_fault_inject(ESP_FAULT_DOMAIN_I2C, ESP_FAULT_I2C_NACK, 0x50);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_UINT32(0x50, sim_esp_fault_get_param(ESP_FAULT_DOMAIN_I2C, ESP_FAULT_I2C_NACK));

    /* Unrelated fault remains inactive */
    TEST_ASSERT_FALSE(sim_esp_fault_is_active(ESP_FAULT_DOMAIN_I2C, ESP_FAULT_I2C_TIMEOUT));
    TEST_ASSERT_FALSE(sim_esp_fault_is_active(ESP_FAULT_DOMAIN_NVS, ESP_FAULT_NVS_PARTITION_FULL));
}

void test_fault_invalid_args(void) {
    TEST_ASSERT_EQUAL_INT(-1, sim_esp_fault_inject(ESP_FAULT_DOMAIN_NONE, ESP_FAULT_I2C_NACK, 0));
    TEST_ASSERT_EQUAL_INT(-1, sim_esp_fault_inject(ESP_FAULT_DOMAIN_MAX, ESP_FAULT_I2C_NACK, 0));
    TEST_ASSERT_EQUAL_INT(-1, sim_esp_fault_inject(ESP_FAULT_DOMAIN_I2C, ESP_FAULT_TYPE_NONE, 0));
    TEST_ASSERT_EQUAL_INT(-1, sim_esp_fault_inject(ESP_FAULT_DOMAIN_I2C, ESP_FAULT_TYPE_MAX, 0));
}

void test_fault_clear_and_soft_reset(void) {
    TEST_ASSERT_EQUAL_INT(0, sim_esp_fault_inject(ESP_FAULT_DOMAIN_I2C, ESP_FAULT_I2C_NACK, 0x48));
    TEST_ASSERT_EQUAL_INT(0, sim_esp_fault_inject(ESP_FAULT_DOMAIN_NVS, ESP_FAULT_NVS_PARTITION_FULL, 1));
    TEST_ASSERT_EQUAL_INT(0, sim_esp_fault_inject(ESP_FAULT_DOMAIN_BLE, ESP_FAULT_BLE_ADV_REJECT, 0));

    TEST_ASSERT_TRUE(sim_esp_fault_is_active(ESP_FAULT_DOMAIN_I2C, ESP_FAULT_I2C_NACK));
    TEST_ASSERT_TRUE(sim_esp_fault_is_active(ESP_FAULT_DOMAIN_NVS, ESP_FAULT_NVS_PARTITION_FULL));
    TEST_ASSERT_TRUE(sim_esp_fault_is_active(ESP_FAULT_DOMAIN_BLE, ESP_FAULT_BLE_ADV_REJECT));

    /* Calling esp_fault_sim_reset() must clear everything idempotently */
    esp_fault_sim_reset();

    TEST_ASSERT_FALSE(sim_esp_fault_is_active(ESP_FAULT_DOMAIN_I2C, ESP_FAULT_I2C_NACK));
    TEST_ASSERT_FALSE(sim_esp_fault_is_active(ESP_FAULT_DOMAIN_NVS, ESP_FAULT_NVS_PARTITION_FULL));
    TEST_ASSERT_FALSE(sim_esp_fault_is_active(ESP_FAULT_DOMAIN_BLE, ESP_FAULT_BLE_ADV_REJECT));
    TEST_ASSERT_EQUAL_UINT32(0, sim_esp_fault_get_param(ESP_FAULT_DOMAIN_I2C, ESP_FAULT_I2C_NACK));
}

void test_driver_integration_nvs(void) {
    nvs_handle_t h;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_open("test_ns", NVS_READWRITE, &h));

    /* 1. Partition Full fault injection */
    sim_esp_fault_inject(ESP_FAULT_DOMAIN_NVS, ESP_FAULT_NVS_PARTITION_FULL, 0);
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NVS_NOT_ENOUGH_SPACE, nvs_set_u32(h, "key1", 12345));

    /* Clear fault: write succeeds */
    esp_fault_sim_reset();
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_set_u32(h, "key1", 12345));

    /* 2. Read Corrupt fault injection */
    sim_esp_fault_inject(ESP_FAULT_DOMAIN_NVS, ESP_FAULT_NVS_READ_CORRUPT, 0);
    uint32_t val = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NVS_CORRUPT_KEY_PART, nvs_get_u32(h, "key1", &val));

    /* Clear fault: read succeeds */
    esp_fault_sim_reset();
    TEST_ASSERT_EQUAL_INT32(ESP_OK, nvs_get_u32(h, "key1", &val));
    TEST_ASSERT_EQUAL_UINT32(12345, val);

    nvs_close(h);
}

void test_driver_integration_ble(void) {
    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));

    /* Inject BLE_ADV_REJECT fault */
    sim_esp_fault_inject(ESP_FAULT_DOMAIN_BLE, ESP_FAULT_BLE_ADV_REJECT, 0);
    int rc = ble_gap_adv_start(0, NULL, -1, &adv_params, NULL, NULL);
    TEST_ASSERT_NOT_EQUAL(0, rc);

    /* Clear fault: advertisement start succeeds */
    esp_fault_sim_reset();
    rc = ble_gap_adv_start(0, NULL, -1, &adv_params, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(0, rc);
    ble_gap_adv_stop();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_fault_injection_and_query);
    RUN_TEST(test_fault_invalid_args);
    RUN_TEST(test_fault_clear_and_soft_reset);
    RUN_TEST(test_driver_integration_nvs);
    RUN_TEST(test_driver_integration_ble);
    return UNITY_END();
}
