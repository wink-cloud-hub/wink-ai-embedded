#include "unity.h"
#include "pal_osal.h"
#include "pal_pwm_router.h"
#include "pal_resource.h"
#include "host_test_ctrl.h"

void setUp(void) {
    sim_reset_time();
    pal_pwm_router_reset();
    pal_resource_reset();
}
void tearDown(void) {}

void test_delay_advances_virtual_time(void) {
    pal_os_sleep_ms(5);
    TEST_ASSERT_EQUAL_UINT64(5000u, pal_os_get_us());
    pal_os_busy_wait_us(300);
    TEST_ASSERT_EQUAL_UINT64(5300u, pal_os_get_us());
}

void test_pwm_duty_recorded(void) {
    /* pal_pwm_set_duty 在 targets/host 提供；经声明直接调（Phase 3 起 status 化） */
    extern wink_status_t pal_pwm_init(uint8_t, uint32_t);
    extern wink_status_t pal_pwm_set_duty(uint8_t, float);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pwm_init(2, 50));
    wink_status_t st = pal_pwm_set_duty(2, 7.5f);
    TEST_ASSERT_EQUAL_INT(WINK_OK, st);
    TEST_ASSERT_EQUAL_FLOAT(7.5f, sim_last_pwm_duty(2));
}

void test_pwm_set_duty_rejects_invalid_channel(void) {
    /* Phase 3：host pal_pwm_init/set_duty 补 channel 校验（PWM_CHANNELS=8），非法 channel → INVALID_ARG */
    extern wink_status_t pal_pwm_set_duty(uint8_t channel, float duty);
    extern wink_status_t pal_pwm_init(uint8_t channel, uint32_t frequency_hz);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, pal_pwm_set_duty(8, 7.5f));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, pal_pwm_init(8, 50));
}

void test_pulse_in_reads_echo_width(void) {
    /* Phase 4 Task 4-2：pal_gpio_pulse_in 直接读 echo 脉宽（不经 pal_gpio_read 协作推进） */
    extern wink_status_t pal_gpio_pulse_in(wink_pin_t pin, bool level, uint32_t timeout_us, uint32_t *pulse_us);
    sim_set_echo_pin(5);
    sim_set_echo_timing(100, 5882);   /* rise@100us, high 5882us */
    uint32_t pulse = 0;
    wink_status_t st = pal_gpio_pulse_in(5, true, 30000u, &pulse);
    TEST_ASSERT_EQUAL_INT(WINK_OK, st);
    TEST_ASSERT_EQUAL_UINT32(5882u, pulse);
}

void test_pulse_in_timeout_when_echo_late(void) {
    extern wink_status_t pal_gpio_pulse_in(wink_pin_t pin, bool level, uint32_t timeout_us, uint32_t *pulse_us);
    sim_set_echo_pin(5);
    sim_set_echo_timing(100000, 1000);   /* rise 100000us > timeout 30000us */
    uint32_t pulse = 0;
    wink_status_t st = pal_gpio_pulse_in(5, true, 30000u, &pulse);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_TIMEOUT, st);
}

void test_echo_timing_stored(void) {
    sim_set_echo_pin(5);
    sim_set_echo_timing(100, 5882);
    /* 验证 host_echo_pin/rise/high 经 pal_gpio_read 协作推进（见 dal 测试，此处只验注入生效） */
    extern bool pal_gpio_read(wink_pin_t pin);
    TEST_ASSERT_TRUE(pal_gpio_read(5));   /* 首次读推进到 rise，返回高 */
}

void test_pwm_deinit_then_reinit(void) {
    extern wink_status_t pal_pwm_init(uint8_t, uint32_t);
    extern wink_status_t pal_pwm_set_duty(uint8_t, float);
    extern void pal_pwm_deinit(uint8_t);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pwm_init(3, 1000));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pwm_set_duty(3, 50.0f));
    pal_pwm_deinit(3);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pwm_init(3, 2000));   /* different freq OK after deinit */
    pal_pwm_deinit(3);
}

void test_pwm_reinit_different_freq_returns_busy(void) {
    extern wink_status_t pal_pwm_init(uint8_t, uint32_t);
    extern void pal_pwm_deinit(uint8_t);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pwm_init(0, 50));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, pal_pwm_init(0, 1000));  /* not deinit'd → BUSY */
    pal_pwm_deinit(0);
}

void test_pwm_deinit_uninit_is_noop(void) {
    extern void pal_pwm_deinit(uint8_t);
    pal_pwm_deinit(5);   /* uninitialized: must not crash */
    TEST_PASS();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_delay_advances_virtual_time);
    RUN_TEST(test_pwm_duty_recorded);
    RUN_TEST(test_pwm_set_duty_rejects_invalid_channel);
    RUN_TEST(test_pulse_in_reads_echo_width);
    RUN_TEST(test_pulse_in_timeout_when_echo_late);
    RUN_TEST(test_echo_timing_stored);
    RUN_TEST(test_pwm_deinit_then_reinit);
    RUN_TEST(test_pwm_reinit_different_freq_returns_busy);
    RUN_TEST(test_pwm_deinit_uninit_is_noop);
    return UNITY_END();
}
