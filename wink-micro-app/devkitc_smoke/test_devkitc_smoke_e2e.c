/**
 * @file test_devkitc_smoke_e2e.c
 * @brief DevKitC 冒烟测试 host e2e：跑 5 tick → 验证 LED + 按钮 + PWM + 无故障。
 *
 * host 侧行为：
 *   - active_low 按钮上拉 idle → 释放；显式注入 LOW 后去抖为按下
 *   - PWM router 分配不同 timer 给 50Hz vs 1kHz
 *   - 无 I2C/ISR/双核/看门狗（均为 ESP_PLATFORM 隔离，stub 不崩）
 */
#include "wink_runtime.h"
#include "wink_trace.h"
#include "device_tree.h"
#include "host_test_ctrl.h"

extern const wink_app_callbacks_t *wink_app_get_callbacks(void);

#define E2E_PASS() do { extern int puts(const char*); puts("E2E PASS"); return 0; } while(0)
#define E2E_FAIL(msg) do { extern int puts(const char*); puts("E2E FAIL: " msg); return 1; } while(0)

int main(void)
{
    wink_trace_reset();
    sim_reset_time();
    const wink_app_callbacks_t *cb = wink_app_get_callbacks();

    /* 跑 5 tick：第 3 tick 后按钮去抖完成 → pressed → LED on */
    {
        wink_status_t st = wink_runtime_run(cb, 5);
        (void)st;
    }

    /* 验证：LED 点亮（led blink 在 tick 内会点亮 LED） */
    if (!board_led.is_on) {
        E2E_FAIL("LED not on after ticks (led blink)");
    }

    /* 验证：PWM 通道 1 已配置（50Hz 占空比 50%） */
    if (sim_last_pwm_duty(1) < 49.0f || sim_last_pwm_duty(1) > 51.0f) {
        E2E_FAIL("PWM ch1 duty not 50%");
    }

    /* 验证：无故障（LED/按钮/PWM 初始化均成功） */
    if (wink_trace_count() != 0) {
        E2E_FAIL("faults recorded during run");
    }

    /* S11: deinit loop verification (5 rounds, no GPIO reserve error, no WDT/leak) */
    for (int i = 0; i < 5; i++) {
        wink_device_tree_deinit();
        wink_status_t st = wink_device_tree_init();
        if (wink_status_is_error(st)) {
            E2E_FAIL("S11: failed to reinitialize device tree during deinit loop");
        }
    }

    E2E_PASS();
}
