/**
 * @file selftest_rmt_loopback.c
 * @brief S9: RMT 硬件自环捕获测试。
 *
 * 流程（完全独立，不依赖其他 selftest 的残留状态）：
 *   1. 若 PWM ch1 在 TEST_PIN 上正运行 → deinit 它，稍后恢复；
 *   2. claim TEST_PIN (GPIO4) 为 RMT 自测独占；
 *   3. GPIO 配为 push-pull 输出，拉低；
 *   4. pal_rmt_pulse_capture_init(TEST_PIN, RISING)；
 *   5. pal_test_enable_hardware_loopback(TEST_PIN, TEST_PIN)；
 *   6. pal_rmt_pulse_capture_arm()；
 *   7. 等待 50µs 沉降，写高电平 → busy_wait 100µs → 写低电平（100µs 脉冲）；
 *   8. pal_rmt_pulse_capture_wait_armed(30ms) 等待捕获；
 *   9. 验证捕获脉宽 90..110µs → PASS，否则 FAIL；
 *  10. 按反序清理：disable loopback → deinit RMT → release pin；
 *  11. 若我们之前 deinit 了 PWM ch1 → 重新 init 为 50Hz/50% 恢复状态。
 *
 * host/wasm 上 pal_rmt_pulse_capture_init 返回 UNSUPPORTED → SKIP。
 *
 * 选 pin 4 的原因：与 smoke S9 历史一致；ESP32 DevKitC 上 GPIO4 未被 USB/flash 占用；
 * 不与 BOOT(0)/LED(2)/TRIG(18)/ECHO(19) 冲突。
 */
#define LOG_TAG "selftest.rmt"

#include "wink_selftest.h"
#include "wink_selftest_internal.h"
#include "wink_status.h"
#include "wink_log.h"
#include "pal_hal.h"
#include "pal_osal.h"
#include "pal_pwm_router.h"
#include "pal_resource.h"
#include "internal/pal_test_loopback.h"
#include "hal/pal_rmt.h"

#define RMT_TEST_PIN   4u
#define RMT_PWM_CH     1u   /* PWM ch1 绑定 GPIO4 在 smoke 的 pwm_router 测试中 */
#define RMT_PULSE_US   100u
#define RMT_PULSE_MIN  90u
#define RMT_PULSE_MAX  110u
#define RMT_TIMEOUT_US 30000u

wink_status_t wink_selftest_rmt_self_loopback(wink_selftest_result_t *r)
{
    r->note = "RMT 100us pulse self-loopback";
    r->metric = 0;

    wink_status_t final_st = WINK_OK;
    bool skip = false;

    /* 1. 检查 PWM ch1 是否运行（pwm_router 测试可能已 init）——若运行则 deinit，记录以便恢复 */
    bool pwm_was_up = pal_pwm_router_channel_ready(RMT_PWM_CH);
    if (pwm_was_up) {
        pal_pwm_deinit(RMT_PWM_CH);
    }

    /* 2. Claim pin */
    wink_status_t st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, RMT_TEST_PIN, "selftest_rmt");
    if (wink_status_is_error(st) && st != WINK_ERR_BUSY) {
        r->note = "resource claim failed";
        goto restore_pwm;
    }

    /* 3. GPIO push-pull 输出，初始低 */
    st = pal_gpio_init(RMT_TEST_PIN, PAL_GPIO_OUTPUT_PUSH_PULL);
    if (wink_status_is_error(st)) {
        r->note = "pal_gpio_init failed";
        goto release;
    }
    WINK_IGNORE_RESULT(pal_gpio_write(RMT_TEST_PIN, false));

    /* 4. Init RMT RISING 沿捕获 */
    st = pal_rmt_pulse_capture_init(RMT_TEST_PIN, PAL_RMT_EDGE_RISING);
    if (st == WINK_ERR_UNSUPPORTED) {
        r->note = "RMT not supported on this target";
        r->metric = 0;
        WINK_IGNORE_RESULT(pal_gpio_init(RMT_TEST_PIN, PAL_GPIO_INPUT_PULLUP)); /* reset to safe input */
        skip = true;
        goto release;
    }
    if (wink_status_is_error(st)) {
        r->note = "pal_rmt_pulse_capture_init failed";
        final_st = st;
        goto release;
    }

    /* 5. 使能硬件自环 */
    st = pal_test_enable_hardware_loopback(RMT_TEST_PIN, RMT_TEST_PIN);
    if (wink_status_is_error(st)) {
        r->note = "loopback enable failed";
        final_st = st;
        pal_rmt_pulse_capture_deinit();
        goto release;
    }

    /* 6. Arm RMT */
    st = pal_rmt_pulse_capture_arm();
    if (wink_status_is_error(st)) {
        r->note = "pal_rmt_pulse_capture_arm failed";
        final_st = st;
        pal_test_disable_hardware_loopback(RMT_TEST_PIN, RMT_TEST_PIN);
        pal_rmt_pulse_capture_deinit();
        goto release;
    }

    /* 7. 软件驱动 100µs 脉冲
     *    50µs 沉降 → 高电平 → 100µs 忙等 → 低电平 */
    pal_os_busy_wait_us(50);
    WINK_IGNORE_RESULT(pal_gpio_write(RMT_TEST_PIN, true));
    pal_os_busy_wait_us(RMT_PULSE_US);
    WINK_IGNORE_RESULT(pal_gpio_write(RMT_TEST_PIN, false));

    /* 8. wait_armed（阻塞） */
    uint32_t pulse_us = 0;
    st = pal_rmt_pulse_capture_wait_armed(RMT_TIMEOUT_US, &pulse_us);

    /* 9. 关闭硬件环 + deinit RMT（无论 wait 结果） */
    pal_test_disable_hardware_loopback(RMT_TEST_PIN, RMT_TEST_PIN);
    pal_rmt_pulse_capture_deinit();

    if (wink_status_is_error(st)) {
        r->note = "capture wait failed (no edge / timeout)";
        r->metric = 0;
        final_st = st;
        goto release;
    }

    r->metric = pulse_us;
    if (pulse_us < RMT_PULSE_MIN || pulse_us > RMT_PULSE_MAX) {
        r->note = "pulse width out of 90..110us range";
        final_st = WINK_ERR_HARDWARE;
        goto release;
    }

    r->note = "100us pulse captured OK";

release:
    /* 10. 释放 pin */
    WINK_IGNORE_RESULT(pal_resource_release(PAL_RESOURCE_GPIO_PIN, RMT_TEST_PIN, "selftest_rmt"));
    /* 把 GPIO 复位为高阻输入（安全）*/
    WINK_IGNORE_RESULT(pal_gpio_init(RMT_TEST_PIN, PAL_GPIO_INPUT_PULLUP));

restore_pwm:
    /* 11. 如需要，恢复 PWM ch1 到 50Hz / 50% */
    if (pwm_was_up) {
        if (pal_pwm_init(RMT_PWM_CH, 50u) == WINK_OK) {
            WINK_IGNORE_RESULT(pal_pwm_set_duty(RMT_PWM_CH, 50.0f));
        }
    }

    if (skip) return WINK_ERR_UNSUPPORTED;
    return final_st;
}
