/**
 * @file app_callbacks.c
 * @brief OLED Dashboard 业务逻辑：按钮输入 → OLED 文本 + LED 指示。
 *
 * 真机 / 仿真一致行为：
 *   - 按钮按下（active_low，内部上拉）→ OLED 显示 "HELLO WORLD N"（N 初值 1；
 *     仅 SSD1306 MVP 字库支持的大写/数字），LOG_I 输出完整 "Hello World N"。
 *   - 按住期间显示序号不变；松开 → OLED 清屏，LED 熄灭。
 *   - 每次新的按下边沿递增 N（下次按下显示 N+1）。
 */
#define LOG_TAG "oled_dashboard"

#include "device_tree.h"
#include "wink_app.h"
#include "wink_log.h"
#include "wink_trace.h"
#include "wink_fault.h"
#include "wink_status.h"
#include "wink_button_helper.h"
#include "wink_blocking_region.h"
#include "pal_log.h"
#include <stdint.h>
#include <stdio.h>

#define FAULT_BUTTON_HELPER 8001u

static uint32_t s_press_count = 1;
static uint32_t s_shown_count = 0;

static void app_on_boot(const wink_boot_info_t *info)
{
    (void)info;
}

static wink_status_t app_init_status(void)
{
    WINK_TRY(wink_device_tree_init());

#ifdef USER_BUTTON_AUTO_POLL_MS
    WINK_TRY(user_button_start_auto_poll(USER_BUTTON_AUTO_POLL_MS));
#endif

    status_oled_clear();
    status_oled_flush();

    s_press_count = 1;
    s_shown_count = 0;

    LOG_I("init done");
    return WINK_OK;
}

static void app_loop(void)
{
    const bool pressed = user_button_is_active();
    const bool rising_edge = user_button_was_active();

    if (rising_edge) {
        s_shown_count = s_press_count;
        LOG_I("Hello World %lu", (unsigned long)s_shown_count);
        if (s_press_count < UINT32_MAX) {
            s_press_count++;
        }
    }

    if (pressed && s_shown_count > 0u) {
        char line[32];
        (void)snprintf(line, sizeof(line), "HELLO WORLD %lu", (unsigned long)s_shown_count);

        status_led_activate();
        status_oled_clear();
        status_oled_draw_text(0, 0, line);
        status_oled_flush();
    } else {
        s_shown_count = 0;
        status_led_deactivate();
        status_oled_clear();
        status_oled_flush();
    }
}

static wink_status_t app_on_fault_status(uint32_t fault_code)
{
    wink_trace_fault(fault_code);
    status_led_deactivate();
    return WINK_OK;
}

const wink_app_callbacks_t *wink_app_get_callbacks(void)
{
    static const wink_app_callbacks_t cb = {
        .init_status     = app_init_status,
        .loop            = app_loop,
        .on_fault_status = app_on_fault_status,
        .on_boot         = app_on_boot,
    };
    return &cb;
}
