/* SPDX-License-Identifier: GPL-3.0-only */
#include "unity.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>

#include "sim_responder.h"
#include "esp_partition.h"
#include "esp_partition_sim.h"
#include "esp_vfs_ram.h"

void setUp(void) {
    sim_responder_reset_all();
    esp_partition_sim_reset();
    esp_vfs_ram_reset();
}

void tearDown(void) {
    sim_responder_reset_all();
    esp_partition_sim_reset();
    esp_vfs_ram_reset();
}

void test_sim_responder_at24c02_eeprom(void) {
    static sim_i2c_eeprom_at24c02_t eeprom;
    wink_status_t st = sim_i2c_eeprom_at24c02_init(&eeprom, 0u, 0x50u);
    TEST_ASSERT_EQUAL(WINK_OK, st);

    /* 1. Probe transaction */
    st = sim_responder_dispatch(SIM_BUS_TYPE_I2C, 0u, 0x50u, NULL, 0u, NULL, 0u);
    TEST_ASSERT_EQUAL(WINK_OK, st);

    /* 2. Unregistered device probe returns NOT_FOUND */
    st = sim_responder_dispatch(SIM_BUS_TYPE_I2C, 0u, 0x51u, NULL, 0u, NULL, 0u);
    TEST_ASSERT_EQUAL(WINK_ERR_NOT_FOUND, st);

    /* 3. Write data to EEPROM at address 0x10 */
    uint8_t write_payload[] = { 0x10u, 0xDEu, 0xADu, 0xBEu, 0xEFu };
    st = sim_responder_dispatch(SIM_BUS_TYPE_I2C, 0u, 0x50u,
                                write_payload, sizeof(write_payload), NULL, 0u);
    TEST_ASSERT_EQUAL(WINK_OK, st);

    /* 4. Set read address pointer to 0x10 and read 4 bytes */
    uint8_t addr_select[] = { 0x10u };
    uint8_t read_back[4] = { 0 };
    st = sim_responder_dispatch(SIM_BUS_TYPE_I2C, 0u, 0x50u,
                                addr_select, sizeof(addr_select), read_back, sizeof(read_back));
    TEST_ASSERT_EQUAL(WINK_OK, st);
    TEST_ASSERT_EQUAL_HEX8(0xDE, read_back[0]);
    TEST_ASSERT_EQUAL_HEX8(0xAD, read_back[1]);
    TEST_ASSERT_EQUAL_HEX8(0xBE, read_back[2]);
    TEST_ASSERT_EQUAL_HEX8(0xEF, read_back[3]);
}

void test_esp_partition_sim_in_memory_crud(void) {
    /* 1. Find NVS partition */
    const esp_partition_t *nvs = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                          ESP_PARTITION_SUBTYPE_DATA_NVS,
                                                          "nvs");
    TEST_ASSERT_NOT_NULL(nvs);
    TEST_ASSERT_EQUAL_STRING("nvs", nvs->label);
    TEST_ASSERT_EQUAL_UINT32(0x6000, nvs->size);

    /* 2. Unwritten partition reads as 0xFF */
    uint8_t buf[8] = { 0 };
    esp_err_t err = esp_partition_read(nvs, 0, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(ESP_OK, err);
    for (size_t i = 0; i < sizeof(buf); i++) {
        TEST_ASSERT_EQUAL_HEX8(0xFF, buf[i]);
    }

    /* 3. Write test pattern */
    uint8_t test_data[] = { 0x12, 0x34, 0x56, 0x78 };
    err = esp_partition_write(nvs, 4096, test_data, sizeof(test_data));
    TEST_ASSERT_EQUAL(ESP_OK, err);

    /* 4. Read back */
    uint8_t read_data[4] = { 0 };
    err = esp_partition_read(nvs, 4096, read_data, sizeof(read_data));
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(test_data, read_data, sizeof(test_data));

    /* 5. Erase sector */
    err = esp_partition_erase_range(nvs, 4096, 4096);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = esp_partition_read(nvs, 4096, read_data, sizeof(read_data));
    TEST_ASSERT_EQUAL(ESP_OK, err);
    for (size_t i = 0; i < sizeof(read_data); i++) {
        TEST_ASSERT_EQUAL_HEX8(0xFF, read_data[i]);
    }
}

void test_esp_vfs_ram_pure_memory_sandbox(void) {
    /* 1. Create directory */
    int res = esp_vfs_ram_mkdir("/spiffs", 0755);
    TEST_ASSERT_EQUAL(0, res);

    /* Duplicate mkdir fails with EEXIST */
    res = esp_vfs_ram_mkdir("/spiffs", 0755);
    TEST_ASSERT_EQUAL(-1, res);

    /* 2. Open file with O_CREAT */
    int fd = esp_vfs_ram_open("/spiffs/test.txt", O_CREAT | O_RDWR, 0644);
    TEST_ASSERT_GREATER_OR_EQUAL(3, fd);

    /* 3. Write data */
    const char msg[] = "WinkMicroOS VFS Sandbox";
    ssize_t written = esp_vfs_ram_write(fd, msg, strlen(msg));
    TEST_ASSERT_EQUAL(strlen(msg), (size_t)written);

    /* 4. Seek to start and read */
    off_t pos = esp_vfs_ram_lseek(fd, 0, SEEK_SET);
    TEST_ASSERT_EQUAL(0, pos);

    char read_buf[32] = { 0 };
    ssize_t n_read = esp_vfs_ram_read(fd, read_buf, sizeof(read_buf));
    TEST_ASSERT_EQUAL(strlen(msg), (size_t)n_read);
    TEST_ASSERT_EQUAL_STRING(msg, read_buf);

    /* 5. Close file */
    res = esp_vfs_ram_close(fd);
    TEST_ASSERT_EQUAL(0, res);

    /* 6. Stat file */
    struct stat st;
    res = esp_vfs_ram_stat("/spiffs/test.txt", &st);
    TEST_ASSERT_EQUAL(0, res);
    TEST_ASSERT_EQUAL(strlen(msg), (size_t)st.st_size);

    /* 7. Unlink */
    res = esp_vfs_ram_unlink("/spiffs/test.txt");
    TEST_ASSERT_EQUAL(0, res);

    /* 8. Re-open fails with ENOENT */
    fd = esp_vfs_ram_open("/spiffs/test.txt", O_RDONLY, 0);
    TEST_ASSERT_EQUAL(-1, fd);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sim_responder_at24c02_eeprom);
    RUN_TEST(test_esp_partition_sim_in_memory_crud);
    RUN_TEST(test_esp_vfs_ram_pure_memory_sandbox);
    return UNITY_END();
}
