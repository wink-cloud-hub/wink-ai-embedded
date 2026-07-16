/**
 * @file app_callbacks.c
 * @brief OLED Dashboard — L1 beginner gold sample (Role API only).
 *
 * Audience: A (low-code) / B (AI) default path. Prefer `{name}_{verb}` Role
 * APIs from device_tree.h. Expert (C) escape hatch (&dev + dal_*) is
 * documented in wink-micro-app/README.md — do not mix into this sample.
 *
 * Behavior (host / wasm / ESP32 identical):
 *   - Press (active_low, pull-up) → OLED "HELLO WORLD N" (N starts at 1;
 *     MVP glyph set: uppercase + digits), LOG_I "Hello World N".
 *   - Hold: display stable; release → clear OLED, LED off.
 *   - Each new press edge increments N.
 */
#define LOG_TAG "oled_dashboard"

#include "device_tree.h"
#include "wink_app.h"
#include "wink_log.h"
#include "wink_trace.h"
#include "wink_fault.h"
#include <stdint.h>
#include <stdio.h>

static uint32_t s_press_count = 1;

static wink_status_t app_init_status(void)
{
    WINK_TRY(wink_device_tree_init());
    WINK_TRY(user_button_enable_events());

    status_oled_clear();
    status_oled_flush();

    s_press_count = 1;

    LOG_I("init done");
    return WINK_OK;
}
/*
static void app_loop(void)
{
    // no-op — events drive this sample
}
*/
static void app_on_event(const wink_event_t *evt)
{
    if (evt->device == &user_button) {
        if (evt->type == WINK_EVENT_BUTTON_PRESSED) {
            LOG_I("Hello World %lu", (unsigned long)s_press_count);
            char line[32];
            (void)snprintf(line, sizeof(line), "HELLO WORLD %lu",
                           (unsigned long)s_press_count);

            status_led_activate();
            status_oled_clear();
            status_oled_draw_text(0, 0, line);
            status_oled_flush();

            if (s_press_count < UINT32_MAX) {
                s_press_count++;
            }
        } else if (evt->type == WINK_EVENT_BUTTON_RELEASED) {
            status_led_deactivate();
            status_oled_clear();
            status_oled_flush();
        }
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
        /*.loop            = app_loop,*/
        .on_event        = app_on_event,
        .on_fault_status = app_on_fault_status,
        /* on_boot omitted (NULL): L1 samples need no reset-reason handling */
    };
    return &cb;
}
