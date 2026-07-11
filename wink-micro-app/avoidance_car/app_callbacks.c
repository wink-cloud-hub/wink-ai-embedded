/**
 * @file app_callbacks.c
 * @brief avoidance_car 业务逻辑 + 回调工厂。
 *        简化版（无 dal_led，仅 radar+servo）：雷达探测近障则扫舵机。
 *
 * ADR-0008 override: device_tree.c 保留手写（含 Flash 覆写表），不使用 codegen 生成。
 * 本 TU 仅：业务事件回调 + 手动设备 init/safe-off 注册 + 非阻塞测量调度。
 */
#define LOG_TAG "avoidance_car"

#include "device_tree.h"
#include "wink_app.h"
#include "wink_trace.h"
#include "wink_actuator_registry.h"
#include "wink_status.h"
#include "pal_log.h"

#define OBSTACLE_THRESHOLD_CM 20.0f
#define FAULT_FRONT_RADAR     7001u
#define FAULT_SERVO_INIT      7002u
#define FAULT_RADAR_INIT      7003u

/* ── on_boot: log reset reason ─────────────────────────────────────────── */
static void app_on_boot(const wink_boot_info_t *info)
{
    if (info->abnormal_boot_count > 0) {
        LOG_I("abnormal boot (count=%lu, reason=%d)",
              (unsigned long)info->abnormal_boot_count, (int)info->reset_reason);
    }
}

/* ── Actuator safe-off thunk (file scope — macro expands to function def) ─ */
WINK_DEFINE_ACTUATOR_THUNK(neck_servo_safe_off, dal_servo_safe_off, dal_servo_t)

/* ── init: apply Flash overrides → init devices → register safe-off ────── */
static wink_status_t app_init_status(void)
{
    /* ADR-0008: apply Flash overrides before init (failure silently degrades
     * to compile-time defaults). */
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

    wink_status_t a = dal_servo_set_angle(&neck_servo, 90.0f);  /* safe position */
    (void)a;

    LOG_I("avoidance_car initialized");
    return WINK_OK;
}

/* ── Non-blocking measurement scheduling ──────────────────────────────── */
typedef enum { RADAR_NEED_TRIGGER, RADAR_WAITING } radar_phase_t;
static radar_phase_t s_radar_phase = RADAR_NEED_TRIGGER;

/* ── loop (10ms tick): trigger measurement → check result → move servo ── */
static void app_loop(void)
{
    float distance = 0.0f;

    if (s_radar_phase == RADAR_NEED_TRIGGER) {
        wink_status_t r = dal_ultrasonic_request_measurement(&front_radar);
        if (wink_status_is_error(r) && r != WINK_ERR_BUSY) {
            wink_trace_fault(FAULT_FRONT_RADAR);
        }
        s_radar_phase = RADAR_WAITING;
    }

    wink_status_t s = dal_ultrasonic_get_cached_distance(&front_radar, &distance);
    if (s == WINK_OK) {
        s_radar_phase = RADAR_NEED_TRIGGER;
        if (distance > 0.0f && distance < OBSTACLE_THRESHOLD_CM) {
            wink_status_t mv = dal_servo_set_angle(&neck_servo, 180.0f);
            (void)mv;
        } else {
            wink_status_t mv = dal_servo_set_angle(&neck_servo, 90.0f);
            (void)mv;
        }
    } else if (s == WINK_ERR_BUSY) {
        /* measurement in progress: hold last safe output */
    } else {
        wink_trace_fault(FAULT_FRONT_RADAR);
    }
}

/* ── Fault callback: return OK=recovered, LOCKED=halt for WDT ─────────── */
static wink_status_t app_on_fault_status(uint32_t fault_code)
{
    wink_trace_fault(fault_code);
    wink_status_t r = dal_servo_set_angle(&neck_servo, 90.0f);  /* safe position */
    (void)r;
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
