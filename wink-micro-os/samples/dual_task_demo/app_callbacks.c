/**
 * @file app_callbacks.c
 * @brief dual_task_demo — two periodic callbacks at different rates.
 *
 * Expert-mode sample: demonstrates wink_periodic_start_ex() when BAL helpers
 * (blink/button/sonar) don't fit.  A 20 ms "sensor" callback generates mock
 * distance data; a 30 ms "motor" callback reads the latest sample and drives
 * the servo.  No ringbuf or pal_os_sleep_ms — the runtime schedules callbacks
 * at the declared period.
 *
 * ADR-0008 override: device_tree.c is hand-written (Flash override table);
 * codegen wink-app.json exists for documentation only.
 */
#define LOG_TAG "dual_task"

#include "device_tree.h"
#include "wink_app.h"
#include "wink_trace.h"
#include "wink_fault.h"
#include "wink_actuator_registry.h"
#include "wink_status.h"
#include "wink_tasks.h"
#include "pal_log.h"
#include <stdbool.h>

#define FAULT_SERVO_INIT  7002u
#define FAULT_RADAR_INIT  7003u

/* ── Actuator safe-off thunk (file scope — macro expands to function def) ─ */
WINK_DEFINE_ACTUATOR_THUNK(neck_servo_safe_off, dal_servo_safe_off, dal_servo_t)

/* ── Shared state between periodic callbacks ── */
static volatile float s_latest_dist = 50.0f;
static float s_mock_dist = 50.0f;
static float s_dir = -2.0f;

volatile bool g_servo_was_180 = false;

/* ── Periodic: sensor tick (20 ms) — generate mock distance ── */
static void sensor_tick(void *ctx)
{
    (void)ctx;
    s_mock_dist += s_dir;
    if (s_mock_dist <= 10.0f) {
        s_mock_dist = 10.0f;
        s_dir = 2.0f;
    } else if (s_mock_dist >= 50.0f) {
        s_mock_dist = 50.0f;
        s_dir = -2.0f;
    }
    s_latest_dist = s_mock_dist;
}

/* ── Periodic: motor tick (30 ms) — read latest distance, drive servo ── */
static void motor_tick(void *ctx)
{
    (void)ctx;
    float dist = s_latest_dist;
    float angle = (dist < 20.0f) ? 180.0f : 90.0f;
    if (angle == 180.0f) {
        g_servo_was_180 = true;
    }
    wink_status_t s = dal_servo_set_angle(&neck_servo, angle);
    (void)s;
}

/* ── on_boot: log reset reason ─────────────────────────────────────────── */
static void app_on_boot(const wink_boot_info_t *info)
{
    if (info->abnormal_boot_count > 0) {
        LOG_I("abnormal boot (count=%lu, reason=%d)",
              (unsigned long)info->abnormal_boot_count, (int)info->reset_reason);
    }
}

/* ── init: apply Flash overrides → init devices → register safe-off →
 *                    start two periodic callbacks at different rates. ───── */
static wink_status_t app_init_status(void)
{
    /* ADR-0008: apply Flash overrides before init. */
    wink_status_t cfg = device_tree_apply_flash_config();
    (void)cfg;

    /* servo: config may have been overridden by Flash, so rebuild cfg from
     * instance fields (dal_servo.h documents this redundancy). */
    const dal_servo_config_t servo_cfg = {
        .owner        = "neck_servo",
        .pwm_channel  = neck_servo.config.pwm_channel,
        .min_pulse_ms = neck_servo.config.min_pulse_ms,
        .max_pulse_ms = neck_servo.config.max_pulse_ms,
    };
    wink_status_t s = dal_servo_init(&neck_servo, &servo_cfg);
    if (wink_status_is_error(s)) {
        wink_trace_fault(FAULT_SERVO_INIT);
        return s;
    }

    wink_status_t u = dal_ultrasonic_init(&front_radar, &front_radar.config);
    if (wink_status_is_error(u)) {
        wink_trace_fault(FAULT_RADAR_INIT);
        return u;
    }

    /* Register servo safe-off so fault/boot-lock path can de-energize. */
    wink_status_t ar = wink_actuator_register(neck_servo_safe_off, &neck_servo);
    if (wink_status_is_error(ar)) {
        wink_trace_fault(FAULT_SERVO_INIT);
        return ar;
    }

    wink_status_t st_angle = dal_servo_set_angle(&neck_servo, 90.0f);
    (void)st_angle;

    /* Start two periodic callbacks — expert mode: when BAL helpers don't fit
     * (asymmetric rates, custom logic), use wink_periodic_start_ex directly.
     * LIGHT path: callbacks run in tick context (cooperative, zero stack). */
    wink_periodic_handle_t h_sensor = wink_periodic_start_ex(
        "sensor", 0, 20, sensor_tick, NULL,
        WINK_PERIODIC_LIGHT, 5, PAL_OS_CORE_ANY);
    if (h_sensor < 1) {
        wink_trace_fault(FAULT_SERVO_INIT);
        return (wink_status_t)h_sensor;
    }

    wink_periodic_handle_t h_motor = wink_periodic_start_ex(
        "motor", 0, 30, motor_tick, NULL,
        WINK_PERIODIC_LIGHT, 5, PAL_OS_CORE_ANY);
    if (h_motor < 1) {
        wink_trace_fault(FAULT_SERVO_INIT);
        return (wink_status_t)h_motor;
    }

    LOG_I("dual_task_demo initialized: 2 periodic callbacks started");
    return WINK_OK;
}

/* ── loop (10ms tick): no-op — periodic callbacks handle everything. ──── */
static void app_loop(void)
{
}

/* ── Fault callback: return OK=recovered, LOCKED=halt for WDT ─────────── */
static wink_status_t app_on_fault_status(uint32_t fault_code)
{
    wink_trace_fault(fault_code);
    wink_status_t s = dal_servo_set_angle(&neck_servo, 90.0f);
    (void)s;
    return WINK_OK;
}

/* ── Callback factory ─────────────────────────────────────────────────── */
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
