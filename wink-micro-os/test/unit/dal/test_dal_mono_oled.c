// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_dal_mono_oled.c
 * @brief DAL monochrome OLED display driver unit tests.
 */
#include "unity.h"
#include "wink_status.h"
#include "dal_mono_oled.h"
#include "pal_resource.h"
#include "host_test_ctrl.h"

void setUp(void) {
    pal_resource_reset();
    sim_reset_time();
}
void tearDown(void) {}

void test_init_null_returns_invalid_arg(void) {
    static dal_mono_oled_t dev = {0};
    dal_mono_oled_config_t cfg = { .i2c_port = 0, .i2c_addr = 0x3C,
                                  .width = 128, .height = 64, .owner = "oled0" };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_mono_oled_init(NULL, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_mono_oled_init(&dev, NULL));
}

void test_init_null_owner_returns_invalid_arg(void) {
    static dal_mono_oled_t dev = {0};
    dal_mono_oled_config_t cfg = { .i2c_port = 0, .i2c_addr = 0x3C,
                                  .width = 128, .height = 64, .owner = NULL };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_mono_oled_init(&dev, &cfg));
}

void test_init_valid_claims_addr_and_sends_init(void) {
    static dal_mono_oled_t dev = {0};
    dal_mono_oled_config_t cfg = { .i2c_port = 0, .i2c_addr = 0x3C,
                                  .width = 128, .height = 64, .owner = "oled0" };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_mono_oled_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.initialized);
    TEST_ASSERT_EQUAL_INT(0x3C, sim_last_i2c_addr());
    TEST_ASSERT_EQUAL_INT(1, sim_i2c_transfer_count());
}

void test_init_addr_conflict_returns_busy(void) {
    static dal_mono_oled_t dev0 = {0};
    static dal_mono_oled_t dev1 = {0};
    dal_mono_oled_config_t cfg0 = { .i2c_port = 0, .i2c_addr = 0x3C,
                                   .width = 128, .height = 64, .owner = "oled0" };
    dal_mono_oled_config_t cfg1 = { .i2c_port = 0, .i2c_addr = 0x3C,
                                   .width = 128, .height = 64, .owner = "oled1" };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_mono_oled_init(&dev0, &cfg0));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, dal_mono_oled_init(&dev1, &cfg1));
}

void test_init_rejects_invalid_width(void) {
    static dal_mono_oled_t dev = {0};
    dal_mono_oled_config_t cfg64 = { .i2c_port = 0, .i2c_addr = 0x3C,
                                    .width = 64, .height = 64, .owner = "oled_bad_w" };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_mono_oled_init(&dev, &cfg64));
    TEST_ASSERT_FALSE(dev.initialized);
}

void test_init_rejects_invalid_height(void) {
    static dal_mono_oled_t dev = {0};
    dal_mono_oled_config_t cfg48 = { .i2c_port = 0, .i2c_addr = 0x3C,
                                    .width = 128, .height = 48, .owner = "oled_bad_h" };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_mono_oled_init(&dev, &cfg48));
    TEST_ASSERT_FALSE(dev.initialized);

    dal_mono_oled_config_t cfg128 = { .i2c_port = 0, .i2c_addr = 0x3C,
                                     .width = 128, .height = 128, .owner = "oled_bad_h2" };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_mono_oled_init(&dev, &cfg128));
}

void test_init_rejects_invalid_i2c_addr(void) {
    static dal_mono_oled_t dev = {0};
    dal_mono_oled_config_t cfg0 = { .i2c_port = 0, .i2c_addr = 0x00,
                                   .width = 128, .height = 64, .owner = "oled_bad_addr0" };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_mono_oled_init(&dev, &cfg0));
    dal_mono_oled_config_t cfg7f = { .i2c_port = 0, .i2c_addr = 0x7F,
                                    .width = 128, .height = 64, .owner = "oled_bad_addr7f" };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_mono_oled_init(&dev, &cfg7f));
}

void test_init_128x32_ok_and_flush_transfers_4_pages(void) {
    static dal_mono_oled_t dev = {0};
    dal_mono_oled_config_t cfg = { .i2c_port = 0, .i2c_addr = 0x3C,
                                  .width = 128, .height = 32, .owner = "oled32" };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_mono_oled_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.initialized);
    TEST_ASSERT_EQUAL_UINT16(128, dev.config.width);
    TEST_ASSERT_EQUAL_UINT16(32, dev.config.height);
    TEST_ASSERT_EQUAL_UINT8(4, dev.pages);

    uint32_t before = sim_i2c_transfer_count();
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_mono_oled_flush(&dev));
    uint32_t after = sim_i2c_transfer_count();
    TEST_ASSERT_EQUAL_INT(5, (int)(after - before));
}

void test_clear_zeros_framebuffer(void) {
    static dal_mono_oled_t dev = {0};
    dal_mono_oled_config_t cfg = { .i2c_port = 0, .i2c_addr = 0x3C,
                                  .width = 128, .height = 64, .owner = "oled0" };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_mono_oled_init(&dev, &cfg));
    dev.framebuffer[10] = 0xFF;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_mono_oled_clear(&dev));
    TEST_ASSERT_EQUAL_INT(0x00, dev.framebuffer[10]);
}

void test_draw_text_modifies_framebuffer(void) {
    static dal_mono_oled_t dev = {0};
    dal_mono_oled_config_t cfg = { .i2c_port = 0, .i2c_addr = 0x3C,
                                  .width = 128, .height = 64, .owner = "oled0" };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_mono_oled_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_mono_oled_clear(&dev));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_mono_oled_draw_text(&dev, 0, 0, "0"));
    TEST_ASSERT_NOT_EQUAL(0x00, dev.framebuffer[0]);
}

void test_draw_text_ascii_upper_letter_b(void) {
    static dal_mono_oled_t dev = {0};
    dal_mono_oled_config_t cfg = { .i2c_port = 0, .i2c_addr = 0x3C,
                                  .width = 128, .height = 64, .owner = "oled0" };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_mono_oled_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_mono_oled_clear(&dev));

#if defined(WINK_MONO_OLED_FONT_MINIMAL)
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_mono_oled_draw_text(&dev, 0, 0, "B"));
    TEST_ASSERT_EQUAL_INT(0x00, dev.framebuffer[0]);
#else
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_mono_oled_draw_text(&dev, 0, 0, "B"));
    TEST_ASSERT_EQUAL_INT(0x7F, dev.framebuffer[0]);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_mono_oled_draw_text(&dev, 6, 0, "z"));
    TEST_ASSERT_EQUAL_INT(0x61, dev.framebuffer[6]);
#endif
}

void test_flush_generates_i2c_transfers(void) {
    static dal_mono_oled_t dev = {0};
    dal_mono_oled_config_t cfg = { .i2c_port = 0, .i2c_addr = 0x3C,
                                  .width = 128, .height = 64, .owner = "oled0" };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_mono_oled_init(&dev, &cfg));

    uint32_t count_before = sim_i2c_transfer_count();
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_mono_oled_flush(&dev));
    uint32_t count_after = sim_i2c_transfer_count();

    TEST_ASSERT_EQUAL_INT(9, (int)(count_after - count_before));
    TEST_ASSERT_EQUAL_INT(0x3C, sim_last_i2c_addr());
}

void test_ops_before_init_returns_not_initialized(void) {
    static dal_mono_oled_t dev = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_mono_oled_clear(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED,
                          dal_mono_oled_draw_text(&dev, 0, 0, "A"));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_mono_oled_flush(&dev));
}

void test_deinit_hardening(void) {
    static dal_mono_oled_t dev = {0};
    dal_mono_oled_config_t cfg = { .i2c_port = 0, .i2c_addr = 0x3C,
                                  .width = 128, .height = 64, .owner = "oled0" };

    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_mono_oled_deinit(NULL));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_mono_oled_deinit(&dev));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_mono_oled_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.initialized);
    uint32_t res_id = pal_resource_i2c_id(0, 0x3C);
    TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_I2C_ADDR, res_id));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_mono_oled_deinit(&dev));
    TEST_ASSERT_FALSE(dev.initialized);
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_I2C_ADDR, res_id));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_mono_oled_deinit(&dev));
}

void test_deinit_loop_i2c_client_no_resource_leak(void) {
    static dal_mono_oled_t dev = {0};
    const dal_mono_oled_config_t cfg = {
        .i2c_port = 0, .i2c_addr = 0x3C,
        .width = 128, .height = 64, .owner = "oled_loop",
    };
    for (int round = 0; round < 10; round++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_mono_oled_init(&dev, &cfg));
        TEST_ASSERT_TRUE(dev.initialized);
        uint32_t res_id = pal_resource_i2c_id(0, 0x3C);
        TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_I2C_ADDR, res_id));
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_mono_oled_deinit(&dev));
        TEST_ASSERT_FALSE(dev.initialized);
        TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_I2C_ADDR, res_id));
    }
}

void test_mono_oled_sh1106_variant_unsupported(void)
{
    static dal_mono_oled_t dev = {0};
    dal_mono_oled_config_t cfg = {
        .owner = "t",
        .i2c_addr = 0x3C,
        .width = 128,
        .height = 64,
        .i2c_port = 0,
        .variant = DAL_MONO_OLED_VARIANT_SH1106,
    };
    TEST_ASSERT_EQUAL(WINK_ERR_UNSUPPORTED, dal_mono_oled_init(&dev, &cfg));
    TEST_ASSERT_FALSE(dev.initialized);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_init_null_returns_invalid_arg);
    RUN_TEST(test_init_null_owner_returns_invalid_arg);
    RUN_TEST(test_init_valid_claims_addr_and_sends_init);
    RUN_TEST(test_init_addr_conflict_returns_busy);
    RUN_TEST(test_init_rejects_invalid_width);
    RUN_TEST(test_init_rejects_invalid_height);
    RUN_TEST(test_init_rejects_invalid_i2c_addr);
    RUN_TEST(test_init_128x32_ok_and_flush_transfers_4_pages);
    RUN_TEST(test_clear_zeros_framebuffer);
    RUN_TEST(test_draw_text_modifies_framebuffer);
    RUN_TEST(test_draw_text_ascii_upper_letter_b);
    RUN_TEST(test_flush_generates_i2c_transfers);
    RUN_TEST(test_ops_before_init_returns_not_initialized);
    RUN_TEST(test_deinit_hardening);
    RUN_TEST(test_deinit_loop_i2c_client_no_resource_leak);
    RUN_TEST(test_mono_oled_sh1106_variant_unsupported);
    return UNITY_END();
}
