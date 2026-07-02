#include "unity.h"
#include "wink_status.h"
#include "dal_button.h"
#include "pal_resource.h"

static const char *const OWNER = "test_dal_button";

void setUp(void) { pal_resource_reset(); }
void tearDown(void) {}

/* ---- init 契约 ---- */
void test_init_null_returns_invalid_arg(void) {
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 10, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_button_init(NULL, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_button_init(NULL, NULL));
}

void test_read_before_init_returns_not_initialized(void) {
    dal_button_t dev = {0};
    bool out = false;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_button_poll(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_button_is_pressed(&dev, &out));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_button_was_pressed(&dev, &out));
}

void test_read_null_returns_invalid_arg(void) {
    bool out = false;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_button_poll(NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_button_is_pressed(NULL, &out));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_button_was_pressed(NULL, &out));
}

void test_is_pressed_null_out_returns_invalid_arg(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 10, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_button_is_pressed(&dev, NULL));
}

/* ---- host 去抖：pal_gpio_read 对非 echo pin 恒返回 false ----
 * active_low=true → raw=false 视为按下；经 3 次 poll 后稳定态翻转为 true */
void test_active_low_debounce_to_pressed(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 10, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));

    for (int i = 0; i < DAL_BUTTON_DEBOUNCE_THRESHOLD; i++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_poll(&dev));
    }

    bool pressed = false;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_is_pressed(&dev, &pressed));
    TEST_ASSERT_TRUE(pressed);
}

/* active_low=false → raw=false 视为未按下；稳定态保持 false */
void test_active_high_stays_unpressed(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 11, .active_low = false };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));

    for (int i = 0; i < DAL_BUTTON_DEBOUNCE_THRESHOLD * 2; i++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_poll(&dev));
    }

    bool pressed = true;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_is_pressed(&dev, &pressed));
    TEST_ASSERT_FALSE(pressed);
}

/* ---- was_pressed 边沿检测 ---- */
void test_was_pressed_edge_once(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 12, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));

    for (int i = 0; i < DAL_BUTTON_DEBOUNCE_THRESHOLD; i++) {
        wink_status_t s = dal_button_poll(&dev);
        (void)s;
    }

    bool event = false;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_was_pressed(&dev, &event));
    TEST_ASSERT_TRUE(event);   /* 第一次：按下事件 */

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_was_pressed(&dev, &event));
    TEST_ASSERT_FALSE(event);  /* 第二次：已消费，无新事件 */
}

/* 释放后再次按下应重新触发（手动翻转 stable_pressed 模拟释放+再按下） */
void test_was_pressed_rearm_after_release(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 13, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    dev.stable_pressed = true;
    dev.last_reported = true;

    bool event = false;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_was_pressed(&dev, &event));
    TEST_ASSERT_FALSE(event);  /* 已按下且已报告 */

    /* 模拟释放 */
    dev.stable_pressed = false;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_was_pressed(&dev, &event));
    TEST_ASSERT_FALSE(event);  /* 释放不产生 was_pressed */

    /* 模拟再次按下 */
    dev.stable_pressed = true;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_was_pressed(&dev, &event));
    TEST_ASSERT_TRUE(event);   /* 重新触发 */
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_init_null_returns_invalid_arg);
    RUN_TEST(test_read_before_init_returns_not_initialized);
    RUN_TEST(test_read_null_returns_invalid_arg);
    RUN_TEST(test_is_pressed_null_out_returns_invalid_arg);
    RUN_TEST(test_active_low_debounce_to_pressed);
    RUN_TEST(test_active_high_stays_unpressed);
    RUN_TEST(test_was_pressed_edge_once);
    RUN_TEST(test_was_pressed_rearm_after_release);
    return UNITY_END();
}
