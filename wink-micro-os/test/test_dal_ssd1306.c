#include "unity.h"
#include "wink_status.h"
#include "dal_ssd1306.h"
#include "pal_resource.h"
#include "host_test_ctrl.h"

void setUp(void) {
    pal_resource_reset();
    sim_reset_time();
}
void tearDown(void) {}

/* ---- init 契约 ---- */
void test_init_null_returns_invalid_arg(void) {
    static dal_ssd1306_t dev = {0};   /* static：1024B 帧缓冲移出栈，满足 -Wstack-usage 纪律；
                                       * 各测试函数的 static 局部为独立对象且只跑一次，语义不变 */
    dal_ssd1306_config_t cfg = { .i2c_port = 0, .i2c_addr = 0x3C,
                                  .width = 128, .height = 64, .owner = "oled0" };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ssd1306_init(NULL, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ssd1306_init(&dev, NULL));
}

void test_init_null_owner_returns_invalid_arg(void) {
    static dal_ssd1306_t dev = {0};
    dal_ssd1306_config_t cfg = { .i2c_port = 0, .i2c_addr = 0x3C,
                                  .width = 128, .height = 64, .owner = NULL };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ssd1306_init(&dev, &cfg));
}

void test_init_valid_claims_addr_and_sends_init(void) {
    static dal_ssd1306_t dev = {0};   /* static：1024B 帧缓冲移出栈，满足 -Wstack-usage 纪律；
                                       * 各测试函数的 static 局部为独立对象且只跑一次，语义不变 */
    dal_ssd1306_config_t cfg = { .i2c_port = 0, .i2c_addr = 0x3C,
                                  .width = 128, .height = 64, .owner = "oled0" };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ssd1306_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.initialized);
    TEST_ASSERT_EQUAL_INT(0x3C, sim_last_i2c_addr());
    TEST_ASSERT_EQUAL_INT(1, sim_i2c_transfer_count()); /* init 命令序列 1 次 transfer */
}

void test_init_addr_conflict_returns_busy(void) {
    static dal_ssd1306_t dev0 = {0};
    static dal_ssd1306_t dev1 = {0};
    dal_ssd1306_config_t cfg0 = { .i2c_port = 0, .i2c_addr = 0x3C,
                                   .width = 128, .height = 64, .owner = "oled0" };
    dal_ssd1306_config_t cfg1 = { .i2c_port = 0, .i2c_addr = 0x3C,
                                   .width = 128, .height = 64, .owner = "oled1" };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ssd1306_init(&dev0, &cfg0));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, dal_ssd1306_init(&dev1, &cfg1));
}

/* ---- P0 E3：width/height 合法性校验，防止栈缓冲溢出 ---- */
void test_init_rejects_invalid_width(void) {
    static dal_ssd1306_t dev = {0};
    /* 当前只支持 width=128：64/96/127/256 都应被拒 */
    dal_ssd1306_config_t cfg64 = { .i2c_port = 0, .i2c_addr = 0x3C,
                                    .width = 64, .height = 64, .owner = "oled_bad_w" };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ssd1306_init(&dev, &cfg64));
    TEST_ASSERT_FALSE(dev.initialized);
}

void test_init_rejects_invalid_height(void) {
    static dal_ssd1306_t dev = {0};
    /* 48/128 等非 {32,64} 高度应被拒 */
    dal_ssd1306_config_t cfg48 = { .i2c_port = 0, .i2c_addr = 0x3C,
                                    .width = 128, .height = 48, .owner = "oled_bad_h" };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ssd1306_init(&dev, &cfg48));
    TEST_ASSERT_FALSE(dev.initialized);

    dal_ssd1306_config_t cfg128 = { .i2c_port = 0, .i2c_addr = 0x3C,
                                     .width = 128, .height = 128, .owner = "oled_bad_h2" };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ssd1306_init(&dev, &cfg128));
}

void test_init_rejects_invalid_i2c_addr(void) {
    static dal_ssd1306_t dev = {0};
    /* 0x00/0x7F 保留/广播地址，应被拒 */
    dal_ssd1306_config_t cfg0 = { .i2c_port = 0, .i2c_addr = 0x00,
                                   .width = 128, .height = 64, .owner = "oled_bad_addr0" };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ssd1306_init(&dev, &cfg0));
    dal_ssd1306_config_t cfg7f = { .i2c_port = 0, .i2c_addr = 0x7F,
                                    .width = 128, .height = 64, .owner = "oled_bad_addr7f" };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ssd1306_init(&dev, &cfg7f));
}

void test_init_128x32_ok_and_flush_transfers_4_pages(void) {
    static dal_ssd1306_t dev = {0};
    dal_ssd1306_config_t cfg = { .i2c_port = 0, .i2c_addr = 0x3C,
                                  .width = 128, .height = 32, .owner = "oled32" };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ssd1306_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.initialized);
    TEST_ASSERT_EQUAL_UINT16(128, dev.width);
    TEST_ASSERT_EQUAL_UINT16(32, dev.height);
    TEST_ASSERT_EQUAL_UINT8(4, dev.pages);     /* 32/8 = 4 pages */

    uint32_t before = sim_i2c_transfer_count();
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ssd1306_flush(&dev));
    uint32_t after = sim_i2c_transfer_count();
    /* flush = 1 addr + 4 pages = 5 transfers */
    TEST_ASSERT_EQUAL_INT(5, (int)(after - before));
}

/* ---- clear / draw_text / flush ---- */
void test_clear_zeros_framebuffer(void) {
    static dal_ssd1306_t dev = {0};   /* static：1024B 帧缓冲移出栈，满足 -Wstack-usage 纪律；
                                       * 各测试函数的 static 局部为独立对象且只跑一次，语义不变 */
    dal_ssd1306_config_t cfg = { .i2c_port = 0, .i2c_addr = 0x3C,
                                  .width = 128, .height = 64, .owner = "oled0" };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ssd1306_init(&dev, &cfg));
    dev.framebuffer[10] = 0xFF;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ssd1306_clear(&dev));
    TEST_ASSERT_EQUAL_INT(0x00, dev.framebuffer[10]);
}

void test_draw_text_modifies_framebuffer(void) {
    static dal_ssd1306_t dev = {0};   /* static：1024B 帧缓冲移出栈，满足 -Wstack-usage 纪律；
                                       * 各测试函数的 static 局部为独立对象且只跑一次，语义不变 */
    dal_ssd1306_config_t cfg = { .i2c_port = 0, .i2c_addr = 0x3C,
                                  .width = 128, .height = 64, .owner = "oled0" };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ssd1306_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ssd1306_clear(&dev));

    /* 绘制数字 "0"（字体索引 1，首列 0x3E 非零） */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ssd1306_draw_text(&dev, 0, 0, "0"));
    TEST_ASSERT_NOT_EQUAL(0x00, dev.framebuffer[0]);
}

void test_flush_generates_i2c_transfers(void) {
    static dal_ssd1306_t dev = {0};   /* static：1024B 帧缓冲移出栈，满足 -Wstack-usage 纪律；
                                       * 各测试函数的 static 局部为独立对象且只跑一次，语义不变 */
    dal_ssd1306_config_t cfg = { .i2c_port = 0, .i2c_addr = 0x3C,
                                  .width = 128, .height = 64, .owner = "oled0" };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ssd1306_init(&dev, &cfg));

    uint32_t count_before = sim_i2c_transfer_count();
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ssd1306_flush(&dev));
    uint32_t count_after = sim_i2c_transfer_count();

    /* flush = 1 次地址设置 + 8 页数据 = 9 次 transfer */
    TEST_ASSERT_EQUAL_INT(9, (int)(count_after - count_before));
    TEST_ASSERT_EQUAL_INT(0x3C, sim_last_i2c_addr());
}

void test_ops_before_init_returns_not_initialized(void) {
    static dal_ssd1306_t dev = {0};   /* static：1024B 帧缓冲移出栈，满足 -Wstack-usage 纪律；
                                       * 各测试函数的 static 局部为独立对象且只跑一次，语义不变 */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_ssd1306_clear(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED,
                          dal_ssd1306_draw_text(&dev, 0, 0, "A"));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_ssd1306_flush(&dev));
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
    RUN_TEST(test_flush_generates_i2c_transfers);
    RUN_TEST(test_ops_before_init_returns_not_initialized);
    return UNITY_END();
}
