/**
 * @file app_callbacks.c
 * @brief OLED Dashboard 业务逻辑：按钮输入 → OLED 文本 + LED 指示。
 *
 * 真机行为：
 *   - 按钮按下（active_low，内部上拉）→ OLED 显示 "HELLO WORLD N"（N 初值 1；
 *     仅 SSD1306 MVP 字库支持的大写/数字），LOG_I 输出完整 "Hello World N"。
 *   - 按钮释放 → OLED 清屏，LED 熄灭。
 *
 * host e2e 行为：
 *   - host pal_gpio_read 对非 echo pin 恒返回 false。
 *   - active_low=true 按钮经 3 次 poll 去抖后稳定为 pressed。
 *   - 故 host 下 LED 恒亮、OLED 恒显 "Hello World N"（可接受仿真保真）。
 */
#define LOG_TAG "oled_dashboard"

#include "device_tree.h"
#include "wink_app.h"
#include "wink_log.h"
#include "wink_trace.h"
#include "wink_actuator_registry.h"
#include "wink_status.h"
#include <stdint.h>
#include <stdio.h>

#define FAULT_BUTTON_INIT 8001u
#define FAULT_LED_INIT    8002u
#define FAULT_OLED_INIT   8003u

/* safe-off thunk：LED 安全态 = 熄灭 */
static wink_status_t led_safe_off_thunk(void *ctx) {
    return dal_led_off((dal_led_t *)ctx);
}

static uint32_t s_press_count = 1;
static bool s_prev_pressed = false;

static void app_init(void) {
    const dal_button_config_t btn_cfg = { .owner = "user_button", .pin = 10, .active_low = true };
    wink_status_t s = dal_button_init(&user_button, &btn_cfg);
    if (wink_status_is_error(s)) { wink_trace_fault(FAULT_BUTTON_INIT); }

    const dal_led_config_t led_cfg = { .owner = "status_led", .pin = 2, .active_high = true };
    s = dal_led_init(&status_led, &led_cfg);
    if (wink_status_is_error(s)) { wink_trace_fault(FAULT_LED_INIT); }

    const dal_ssd1306_config_t oled_cfg = {
        .i2c_port = 0, .i2c_addr = 0x3C,
        .width = 128, .height = 64,
        .owner = "status_oled"
    };
    s = dal_ssd1306_init(&status_oled, &oled_cfg);
    if (wink_status_is_error(s)) { wink_trace_fault(FAULT_OLED_INIT); }

    wink_status_t ar = wink_actuator_register(led_safe_off_thunk, &status_led);
    if (wink_status_is_error(ar)) { wink_trace_fault(FAULT_LED_INIT); }

    /* 初始安全态：熄灭 + 清屏 */
    s = dal_led_off(&status_led); (void)s;
    s = dal_ssd1306_clear(&status_oled); (void)s;
    s = dal_ssd1306_flush(&status_oled); (void)s;

    s_press_count = 1;
    s_prev_pressed = false;
}

static void app_loop(void) {
    wink_status_t s = dal_button_poll(&user_button);
    (void)s;

    bool pressed = false;
    s = dal_button_is_pressed(&user_button, &pressed);
    (void)s;

    const bool rising_edge = pressed && !s_prev_pressed;
    s_prev_pressed = pressed;

    if (pressed) {
        char line[32];
        (void)snprintf(line, sizeof(line), "HELLO WORLD %lu", (unsigned long)s_press_count);

        s = dal_led_on(&status_led); (void)s;
        s = dal_ssd1306_clear(&status_oled); (void)s;
        s = dal_ssd1306_draw_text(&status_oled, 0, 0, line); (void)s;
        s = dal_ssd1306_flush(&status_oled); (void)s;

        if (rising_edge) {
            LOG_I("Hello World %lu", (unsigned long)s_press_count);
            if (s_press_count < UINT32_MAX) {
                s_press_count++;
            }
        }
    } else {
        s = dal_led_off(&status_led); (void)s;
        s = dal_ssd1306_clear(&status_oled); (void)s;
        s = dal_ssd1306_flush(&status_oled); (void)s;
    }
}

static void app_on_fault(uint32_t fault_code) {
    wink_trace_fault(fault_code);
    wink_status_t s = dal_led_off(&status_led);
    (void)s;
}

const wink_app_callbacks_t *wink_app_get_callbacks(void) {
    static const wink_app_callbacks_t cb = { app_init, app_loop, app_on_fault };
    return &cb;
}
