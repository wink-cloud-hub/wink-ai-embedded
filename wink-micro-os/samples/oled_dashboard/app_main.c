/**
 * @file app_main.c
 * @brief OLED Dashboard 业务逻辑：按钮输入 → OLED 文本 + LED 指示。
 *
 * 真机行为：
 *   - 按钮按下（active_low，内部上拉）→ OLED 显示 "Hi!"，LED 点亮。
 *   - 按钮释放 → OLED 清屏，LED 熄灭。
 *
 * host e2e 行为：
 *   - host pal_gpio_read 对非 echo pin 恒返回 false。
 *   - active_low=true 按钮经 3 次 poll 去抖后稳定为 pressed。
 *   - 故 host 下 LED 恒亮、OLED 恒显 "Hi!"（可接受仿真保真）。
 */
#include "device_tree.h"
#include "wink_app.h"
#include "wink_trace.h"
#include "wink_actuator_registry.h"
#include "wink_status.h"

#define FAULT_BUTTON_INIT 8001u
#define FAULT_LED_INIT    8002u
#define FAULT_OLED_INIT   8003u

/* safe-off thunk：LED 安全态 = 熄灭 */
static wink_status_t led_safe_off_thunk(void *ctx) {
    return dal_led_off((dal_led_t *)ctx);
}

static void app_init(void) {
    const dal_button_config_t btn_cfg = { .pin = 10, .active_low = true };
    wink_status_t s = dal_button_init(&user_button, &btn_cfg);
    if (wink_status_is_error(s)) { wink_trace_fault(FAULT_BUTTON_INIT); }

    const dal_led_config_t led_cfg = { .pin = 2, .active_high = true };
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
}

static void app_loop(void) {
    wink_status_t s = dal_button_poll(&user_button);
    (void)s;

    bool pressed = false;
    s = dal_button_is_pressed(&user_button, &pressed);
    (void)s;

    if (pressed) {
        s = dal_led_on(&status_led); (void)s;
        s = dal_ssd1306_clear(&status_oled); (void)s;
        s = dal_ssd1306_draw_text(&status_oled, 0, 0, "Hi!"); (void)s;
        s = dal_ssd1306_flush(&status_oled); (void)s;
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
