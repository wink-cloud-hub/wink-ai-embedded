/**
 * @file app_callbacks.c
 * @brief Avoidance car — L1 sample (Role API + distance events, ADR-0033).
 *
 * Audience: A (low-code) / B (AI) default path — same shape as oled_dashboard.
 *
 * Behavior (host / wasm / ESP32 identical):
 *   - front_radar posts WINK_EVENT_DISTANCE_READY each completed measurement.
 *   - Distance < 20 cm → neck_servo 180°; otherwise 90°.
 */
#define LOG_TAG "avoidance_car"

#include "device_tree.h"
#include "wink_app.h"
#include "wink_event.h"
#include "wink_fault.h"
#include "wink_log.h"
#include "wink_trace.h"

#include <stdint.h>

#define OBSTACLE_THRESHOLD_CM 20.0f

static wink_status_t app_init_status(void)
{
    WINK_TRY(wink_device_tree_init());
    WINK_TRY(front_radar_enable_distance_events());
    neck_servo_set_angle(90.0f);

    LOG_I("init done");
    return WINK_OK;
}

static void app_on_event(const wink_event_t *evt)
{
    float cm;

    if (evt->device != &front_radar) {
        return;
    }
    if (evt->type != WINK_EVENT_DISTANCE_READY) {
        return;
    }

    cm = (float)evt->param / 10.0f;
    if (cm > 0.0f && cm < OBSTACLE_THRESHOLD_CM) {
        neck_servo_set_angle(180.0f);
    } else {
        neck_servo_set_angle(90.0f);
    }
}

static wink_status_t app_on_fault_status(uint32_t fault_code)
{
    wink_trace_fault(fault_code);
    neck_servo_set_angle(90.0f);
    return WINK_OK;
}

const wink_app_callbacks_t *wink_app_get_callbacks(void)
{
    static const wink_app_callbacks_t cb = {
        .init_status     = app_init_status,
        .on_event        = app_on_event,
        .on_fault_status = app_on_fault_status,
        /* loop / on_boot omitted (NULL): event-driven L1 */
    };
    return &cb;
}
