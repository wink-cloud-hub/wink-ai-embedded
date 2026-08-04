#include "unity.h"
#include "pal_hal.h"
#include "internal/pal_test_loopback.h"
#include "pal_osal.h"
#include "pal_pwm_router.h"
#include "pal_resource.h"
#include "host_test_ctrl.h"


/* ADR-0017 层 1 例外：本 TU 合法调用 WINK_BLOCKING API。抑制
 * -Wdeprecated-declarations 使 -Werror 下仍能编译；严格模式
 * (-DWINK_STRICT_NONBLOCKING=1) 下相关 API 声明直接消失，本 TU 会链接失败——那是设计意图。 */
#include "compat/wink_test_compat.h"
WINK_TEST_ALLOW_DEPRECATED

void setUp(void) {
    sim_reset_time();
    pal_pwm_router_reset();
    pal_resource_reset();
}
void tearDown(void) {}

void test_delay_advances_virtual_time(void) {
    /* fixup 计划 F3 H2：pal_os_sleep_ms 在 SIMULATION 分支已强制走调度器；
     * 非 SIM host 分支保持"直接推进虚拟时钟" 的合法路径。此测试用 host_sim_advance_to
     * 更直接、更可读地表达"legacy 虚拟时钟推进" 意图。 */
    extern void host_sim_advance_to(uint64_t us);
    host_sim_advance_to(5000u);
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
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 5, "test"));
    sim_set_echo_pin(5);
    sim_set_echo_timing(100, 5882);   /* rise@100us, high 5882us */
    uint32_t pulse = 0;
    wink_status_t st = pal_gpio_pulse_in(5, true, 30000u, &pulse);
    TEST_ASSERT_EQUAL_INT(WINK_OK, st);
    TEST_ASSERT_EQUAL_UINT32(5882u, pulse);
}

void test_pulse_in_timeout_when_echo_late(void) {
    extern wink_status_t pal_gpio_pulse_in(wink_pin_t pin, bool level, uint32_t timeout_us, uint32_t *pulse_us);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 5, "test"));
    sim_set_echo_pin(5);
    sim_set_echo_timing(100000, 1000);   /* rise 100000us > timeout 30000us */
    uint32_t pulse = 0;
    wink_status_t st = pal_gpio_pulse_in(5, true, 30000u, &pulse);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_TIMEOUT, st);
}

void test_echo_timing_stored(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 5, "test"));
    sim_set_echo_pin(5);
    sim_set_echo_timing(100, 5882);
    /* 验证 host_echo_pin/rise/high 经 pal_gpio_read 协作推进（见 dal 测试，此处只验注入生效） */
    extern wink_status_t pal_gpio_read(wink_pin_t pin, bool *out_level);
    bool lvl = false;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_read(5, &lvl));
    TEST_ASSERT_TRUE(lvl);   /* 首次读推进到 rise，返回高 */
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

/* ---- P1-P5-5：pal_os_ringbuf_used 语义门禁（host/wasm 共享实现） ---- */

void test_ringbuf_used_reports_zero_when_empty(void) {
    pal_os_ringbuf_handle_t rb = pal_os_ringbuf_create(64);
    TEST_ASSERT_NOT_NULL(rb);
    TEST_ASSERT_EQUAL_UINT32(0, pal_os_ringbuf_used(rb));
    pal_os_ringbuf_destroy(rb);
}

void test_ringbuf_used_tracks_push_pop(void) {
    pal_os_ringbuf_handle_t rb = pal_os_ringbuf_create(64);
    TEST_ASSERT_NOT_NULL(rb);

    const uint8_t payload[16] = {0xA5};
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_os_ringbuf_push(rb, payload, sizeof(payload)));
    TEST_ASSERT_EQUAL_UINT32(16, pal_os_ringbuf_used(rb));

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_os_ringbuf_push(rb, payload, 8));
    TEST_ASSERT_EQUAL_UINT32(24, pal_os_ringbuf_used(rb));

    uint8_t out[10] = {0};
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_os_ringbuf_pop(rb, out, 10));
    TEST_ASSERT_EQUAL_UINT32(14, pal_os_ringbuf_used(rb));

    pal_os_ringbuf_destroy(rb);
}

void test_ringbuf_used_null_handle_returns_zero(void) {
    /* NULL-safe contract: used(NULL) must not crash and must not lie about capacity. */
    TEST_ASSERT_EQUAL_UINT32(0, pal_os_ringbuf_used(NULL));
}

void test_pal_gpio_loopback_level(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 4, "test"));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 5, "test"));

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_test_enable_hardware_loopback(4, 5));

    bool level = false;
    // Write HIGH and read
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_write(4, true));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_read(5, &level));
    TEST_ASSERT_TRUE(level);

    // Write LOW and read
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_write(4, false));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_read(5, &level));
    TEST_ASSERT_FALSE(level);

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_test_disable_hardware_loopback(4, 5));
}

void test_pal_gpio_loopback_pulse(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 4, "test"));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 5, "test"));

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_test_enable_hardware_loopback(4, 5));

    // Channel 1 corresponds to pin 4
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pwm_init(1, 50));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pwm_set_duty(1, 7.5f)); // 1.5ms pulse

    uint32_t pulse_us = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_pulse_in(5, true, 30000u, &pulse_us));
    TEST_ASSERT_UINT32_WITHIN(10, 1500, pulse_us);

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_test_disable_hardware_loopback(4, 5));
    pal_pwm_deinit(1);
}

void test_pal_gpio_loopback_invalid_args(void) {
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, pal_test_enable_hardware_loopback(-1, 5));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, pal_test_enable_hardware_loopback(4, 100));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, pal_test_disable_hardware_loopback(-1, 5));
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
    RUN_TEST(test_ringbuf_used_reports_zero_when_empty);
    RUN_TEST(test_ringbuf_used_tracks_push_pop);
    RUN_TEST(test_ringbuf_used_null_handle_returns_zero);
    RUN_TEST(test_pal_gpio_loopback_level);
    RUN_TEST(test_pal_gpio_loopback_pulse);
    RUN_TEST(test_pal_gpio_loopback_invalid_args);
    return UNITY_END();
}
