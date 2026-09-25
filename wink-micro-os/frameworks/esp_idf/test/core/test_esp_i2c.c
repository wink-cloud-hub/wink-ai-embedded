/* SPDX-License-Identifier: GPL-3.0-only */
#include "unity.h"
#include "driver/i2c.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_idf_wink.h"
#include "soc/soc_caps.h"

void setUp(void) {
    esp_i2c_legacy_reset();
    esp_i2c_master_reset();
}

void tearDown(void) {
    esp_i2c_legacy_reset();
    esp_i2c_master_reset();
}

void test_legacy_i2c_slave_mode_rejected(void) {
    /* ADR-0012: Slave mode must Fail-Loud */
    i2c_config_t conf = {
        .mode = I2C_MODE_SLAVE,
        .sda_io_num = 21,
        .scl_io_num = 22,
        .sda_pullup_en = true,
        .scl_pullup_en = true
    };
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NOT_SUPPORTED, i2c_param_config(I2C_NUM_0, &conf));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NOT_SUPPORTED,
        i2c_driver_install(I2C_NUM_0, I2C_MODE_SLAVE, 512, 512, 0));
}

void test_legacy_i2c_master_config_and_folding(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 21,
        .scl_io_num = 22,
        .sda_pullup_en = true,
        .scl_pullup_en = true,
        .master = { .clk_speed = 100000 }
    };
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_param_config(I2C_NUM_0, &conf));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    TEST_ASSERT_NOT_NULL(cmd);

    /* Write: START -> Addr+W -> Reg -> Repeated START -> Addr+R -> Read 2 -> STOP */
    uint8_t write_data[2] = { (0x68 << 1) | I2C_MASTER_WRITE, 0x75 };
    uint8_t read_addr = (0x68 << 1) | I2C_MASTER_READ;
    uint8_t rx_buf[2] = { 0 };

    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_master_start(cmd));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_master_write(cmd, write_data, sizeof(write_data), true));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_master_start(cmd));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_master_write_byte(cmd, read_addr, true));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_master_read(cmd, rx_buf, sizeof(rx_buf), I2C_MASTER_LAST_NACK));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_master_stop(cmd));

    esp_err_t err = i2c_master_cmd_begin(I2C_NUM_0, cmd, 100);
    /* In host simulation without external devices attached, transfer might succeed or timeout/fail, but must execute safely */
    TEST_ASSERT_TRUE(err == ESP_OK || err == ESP_FAIL || err == ESP_ERR_TIMEOUT);

    i2c_cmd_link_delete(cmd);
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_driver_delete(I2C_NUM_0));
}

void test_legacy_i2c_empty_cmd_link_rejected(void) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    TEST_ASSERT_NOT_NULL(cmd);

    /* Empty link executed should return ESP_ERR_INVALID_ARG */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_master_cmd_begin(I2C_NUM_0, cmd, 100));

    i2c_cmd_link_delete(cmd);
}

void test_modern_i2c_master_bus_lifecycle(void) {
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = 21,
        .scl_io_num = 22,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = { .enable_internal_pullup = true }
    };
    i2c_master_bus_handle_t bus_handle = NULL;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_new_master_bus(&bus_cfg, &bus_handle));
    TEST_ASSERT_NOT_NULL(bus_handle);

    /* Re-opening same port without deleting should return INVALID_STATE */
    i2c_master_bus_handle_t bus_handle2 = NULL;
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_STATE, i2c_new_master_bus(&bus_cfg, &bus_handle2));

    /* Add device */
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x68,
        .scl_speed_hz = 400000
    };
    i2c_master_dev_handle_t dev_handle = NULL;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));
    TEST_ASSERT_NOT_NULL(dev_handle);

    /* Transmit / Receive checks */
    uint8_t tx[2] = { 0x6B, 0x00 };
    uint8_t rx[2] = { 0 };
    (void)i2c_master_transmit(dev_handle, tx, sizeof(tx), 100);
    (void)i2c_master_receive(dev_handle, rx, sizeof(rx), 100);
    (void)i2c_master_transmit_receive(dev_handle, tx, 1, rx, 1, 100);
    (void)i2c_master_probe(bus_handle, 0x68, 50);

    /* Cleanup device and bus */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_master_bus_rm_device(dev_handle));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_del_master_bus(bus_handle));
}

void test_modern_i2c_device_pool_limit(void) {
#if SOC_HP_I2C_NUM < 2
    TEST_IGNORE_MESSAGE("SoC exposes a single HP I2C controller; port 1 case is covered by test_esp_soc_matrix");
#else
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_1,
        .sda_io_num = 18,
        .scl_io_num = 19,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = { .enable_internal_pullup = true }
    };
    i2c_master_bus_handle_t bus_handle = NULL;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_new_master_bus(&bus_cfg, &bus_handle));

    i2c_master_dev_handle_t devs[8];
    for (int i = 0; i < 8; i++) {
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = (uint16_t)(0x10 + i),
            .scl_speed_hz = 100000
        };
        TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_master_bus_add_device(bus_handle, &dev_cfg, &devs[i]));
    }

    /* 9th device must fail with ESP_ERR_NO_MEM */
    i2c_device_config_t dev_extra = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x20,
        .scl_speed_hz = 100000
    };
    i2c_master_dev_handle_t dev_extra_handle = NULL;
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NO_MEM,
        i2c_master_bus_add_device(bus_handle, &dev_extra, &dev_extra_handle));

    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_del_master_bus(bus_handle));
#endif /* SOC_HP_I2C_NUM >= 2 */
}

void test_legacy_i2c_multi_transaction_rejected(void) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    TEST_ASSERT_NOT_NULL(cmd);

    uint8_t write_data[2] = { (0x68 << 1) | I2C_MASTER_WRITE, 0x01 };
    /* First transaction: START -> WRITE -> STOP */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_master_start(cmd));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_master_write(cmd, write_data, sizeof(write_data), true));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_master_stop(cmd));

    /* Second transaction in same link: START -> WRITE -> STOP */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_master_start(cmd));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_master_write(cmd, write_data, sizeof(write_data), true));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_master_stop(cmd));

    /* Should fail loud with ESP_ERR_NOT_SUPPORTED */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NOT_SUPPORTED, i2c_master_cmd_begin(I2C_NUM_0, cmd, 100));

    i2c_cmd_link_delete(cmd);
}

void test_legacy_i2c_cmd_link_edges(void) {
    uint8_t data[2] = { 0x01, 0x02 };
    uint8_t rx[2] = { 0 };

    /* NULL handle / NULL buffer paths */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_master_start(NULL));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_master_write(NULL, data, 1, true));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_master_write_byte(NULL, 0x10, true));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_master_read(NULL, rx, 1, I2C_MASTER_ACK));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_master_read_byte(NULL, rx, I2C_MASTER_ACK));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_master_stop(NULL));
    i2c_cmd_link_delete(NULL); /* no-op */

    /* cmd link pool exhaustion (MAX_CMD_LINKS) */
    i2c_cmd_handle_t links[8];
    int n = 0;
    for (; n < 8; ++n) {
        links[n] = i2c_cmd_link_create();
        if (!links[n]) {
            break;
        }
    }
    TEST_ASSERT_EQUAL_INT(4, n);
    for (int i = 0; i < n; ++i) {
        i2c_cmd_link_delete(links[i]);
    }

    /* Fold guard: START without address/data is rejected */
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_master_cmd_begin(I2C_NUM_0, cmd, 0));
    i2c_cmd_link_delete(cmd);

    /* Address-only transaction with ticks == 0 (default timeout path) */
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (0x68 << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(I2C_NUM_0, cmd, 0);
    TEST_ASSERT_TRUE(err == ESP_OK || err == ESP_FAIL || err == ESP_ERR_TIMEOUT);
    i2c_cmd_link_delete(cmd);

    /* read_byte happy path through i2c_master_read */
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (0x68 << 1) | I2C_MASTER_READ, true);
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_master_read_byte(cmd, rx, I2C_MASTER_ACK));
    i2c_master_stop(cmd);
    i2c_cmd_link_delete(cmd);
}

void test_legacy_i2c_misc_apis(void) {
    i2c_port_t bad = (i2c_port_t)SOC_HP_I2C_NUM;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 21,
        .scl_io_num = 22,
        .master = { .clk_speed = 100000 }
    };
    int hp = 0, lp = 0;

    /* Invalid port paths */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG,
        i2c_driver_install(bad, I2C_MODE_MASTER, 0, 0, 0));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_driver_delete(bad));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_param_config(bad, &conf));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_set_pin(bad, 21, 22, true, true, 0));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_reset_tx_fifo(bad));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_reset_rx_fifo(bad));

    /* set_pin validation */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_set_pin(I2C_NUM_0, 21, 22, true, true, 0));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_set_pin(I2C_NUM_0, 21, 21, true, true, 0));

    /* FIFO resets on a valid port */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_reset_tx_fifo(I2C_NUM_0));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_reset_rx_fifo(I2C_NUM_0));

    /* Slave-buffer APIs are Fail-Loud */
    TEST_ASSERT_EQUAL_INT(-1, i2c_slave_write_buffer(I2C_NUM_0, (const uint8_t *)"x", 1, 0));
    TEST_ASSERT_EQUAL_INT(-1, i2c_slave_read_buffer(I2C_NUM_0, (uint8_t *)"x", 1, 0));

    /* Timing getters/setters: invalid args + valid round-trip */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_set_period(bad, 10, 10));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_set_period(I2C_NUM_0, 0, 10));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_set_period(I2C_NUM_0, 0x400, 10));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_set_period(I2C_NUM_0, 10, 20));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_get_period(I2C_NUM_0, &hp, &lp));
    TEST_ASSERT_EQUAL_INT(10, hp);
    TEST_ASSERT_EQUAL_INT(20, lp);
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_get_period(I2C_NUM_0, NULL, NULL));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_get_period(bad, &hp, &lp));

    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_set_start_timing(I2C_NUM_0, 1, 2));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_get_start_timing(I2C_NUM_0, &hp, &lp));
    TEST_ASSERT_EQUAL_INT(1, hp);
    TEST_ASSERT_EQUAL_INT(2, lp);
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_get_start_timing(I2C_NUM_0, NULL, NULL));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_set_start_timing(bad, 1, 2));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_get_start_timing(bad, &hp, &lp));

    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_set_stop_timing(I2C_NUM_0, 3, 4));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_get_stop_timing(I2C_NUM_0, &hp, &lp));
    TEST_ASSERT_EQUAL_INT(3, hp);
    TEST_ASSERT_EQUAL_INT(4, lp);
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_get_stop_timing(I2C_NUM_0, NULL, NULL));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_set_stop_timing(bad, 1, 2));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_get_stop_timing(bad, &hp, &lp));

    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_set_data_timing(I2C_NUM_0, 5, 6));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_get_data_timing(I2C_NUM_0, &hp, &lp));
    TEST_ASSERT_EQUAL_INT(5, hp);
    TEST_ASSERT_EQUAL_INT(6, lp);
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_get_data_timing(I2C_NUM_0, NULL, NULL));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_set_data_timing(bad, 1, 2));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_get_data_timing(bad, &hp, &lp));

    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_set_timeout(I2C_NUM_0, 100));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_get_timeout(I2C_NUM_0, &hp));
    TEST_ASSERT_EQUAL_INT(100, hp);
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_get_timeout(I2C_NUM_0, NULL));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_set_timeout(bad, 100));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_get_timeout(bad, &hp));
}

void test_modern_i2c_master_invalid_args(void) {
    i2c_master_bus_handle_t bus = NULL;
    i2c_master_dev_handle_t dev = NULL;
    uint8_t buf[2] = { 0 };

    /* NULL argument paths */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_new_master_bus(NULL, &bus));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_master_bus_add_device(NULL, NULL, &dev));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_master_transmit(NULL, buf, sizeof(buf), 10));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_master_receive(NULL, buf, sizeof(buf), 10));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG,
        i2c_master_transmit_receive(NULL, buf, 1, buf, 1, 10));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_master_probe(NULL, 0x68, 10));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_del_master_bus(NULL));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_master_bus_rm_device(NULL));

    /* Out-of-range port */
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = (i2c_port_t)SOC_HP_I2C_NUM,
        .sda_io_num = 21,
        .scl_io_num = 22,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = { .enable_internal_pullup = true }
    };
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, i2c_new_master_bus(&bus_cfg, &bus));

    /* Auto port selection (-1) + device add/remove on the auto-selected bus */
    bus_cfg.i2c_port = (i2c_port_t)-1;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_new_master_bus(&bus_cfg, &bus));
    TEST_ASSERT_NOT_NULL(bus);

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x33,
        .scl_speed_hz = 100000
    };
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_master_bus_add_device(bus, &dev_cfg, &dev));
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_master_bus_rm_device(dev));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, i2c_del_master_bus(bus));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_legacy_i2c_slave_mode_rejected);
    RUN_TEST(test_legacy_i2c_master_config_and_folding);
    RUN_TEST(test_legacy_i2c_empty_cmd_link_rejected);
    RUN_TEST(test_legacy_i2c_multi_transaction_rejected);
    RUN_TEST(test_legacy_i2c_cmd_link_edges);
    RUN_TEST(test_legacy_i2c_misc_apis);
    RUN_TEST(test_modern_i2c_master_bus_lifecycle);
    RUN_TEST(test_modern_i2c_device_pool_limit);
    RUN_TEST(test_modern_i2c_master_invalid_args);
    return UNITY_END();
}
