/**
 * @file app_main.c
 * @brief avoidance_car 业务逻辑 + 回调工厂。
 *        简化版（无 dal_led，仅 radar+servo）：雷达探测近障则扫舵机。
 */
#include "device_tree.h"
#include "wink_app.h"
#include "wink_trace.h"
#include "wink_actuator_registry.h"
#include "wink_status.h"

#define OBSTACLE_THRESHOLD_CM 20.0f
#define FAULT_FRONT_RADAR     7001u
#define FAULT_SERVO_INIT      7002u
#define FAULT_RADAR_INIT      7003u

/* Phase 5：舵机 safe-off 适配 thunk（wink_actuator_safe_off_fn 是 void* ctx；
 * dal_servo_safe_off 是强类型 dal_servo_t*，经此 thunk 适配注册进 registry）。 */
static wink_status_t servo_safe_off_thunk(void *ctx) {
    return dal_servo_safe_off((dal_servo_t *)ctx);
}

static void app_init(void) {
    /* Phase 2 Task 2-4：显式 init 生命周期（一次性硬件配置 + initialized）。
     * 任一 init 失败须 trace，禁止 (void) 吞错（review P2-2）。 */

    /* ADR-0008：物理初始化前，从 Flash 覆写静态实例字段（失败静默降级到编译期默认）。
     * 覆写后须从结构体读配置喂 init，override 才生效。降级即默认行为，结果仅供诊断。 */
    wink_status_t cfg = device_tree_apply_flash_config();
    (void)cfg;

    /* servo config↔dev 字段重复（dal_servo.h 已注明）：override 写 dev 字段后，
     * 从 dev 字段重建 config 喂 init（多一跳，避免改 DAL API）。 */
    const dal_servo_config_t servo_cfg = {
        .pwm_channel  = neck_servo.pwm_channel,
        .min_pulse_ms = neck_servo.min_pulse_ms,
        .max_pulse_ms = neck_servo.max_pulse_ms
    };
    wink_status_t s = dal_servo_init(&neck_servo, &servo_cfg);
    if (wink_status_is_error(s)) { wink_trace_fault(FAULT_SERVO_INIT); }

    wink_status_t u = dal_ultrasonic_init(&front_radar, front_radar.trig_pin, front_radar.echo_pin);
    if (wink_status_is_error(u)) { wink_trace_fault(FAULT_RADAR_INIT); }

    /* Phase 5：注册舵机 safe-off，使 fault / boot-lock 路径可统一关断执行器（P0-4）。 */
    wink_status_t ar = wink_actuator_register(servo_safe_off_thunk, &neck_servo);
    if (wink_status_is_error(ar)) { wink_trace_fault(FAULT_SERVO_INIT); }

    wink_status_t a = dal_servo_set_angle(&neck_servo, 90.0f);   /* 初始安全位 */
    (void)a;
}

/* 非阻塞测量调度（Phase 4 Task 4-4）：NEED_TRIGGER → request → WAITING；get_cached OK 后 re-arm。
 * host 单 tick 即 ready（pal_gpio_pulse_in 同步）；真机未来异步时 WAITING 期 get_cached 返回 BUSY，
 * 保持上一安全输出——App 10ms tick 不再调用 60ms+ 的 blocking dal_ultrasonic_read（P0-2）。 */
typedef enum { RADAR_NEED_TRIGGER, RADAR_WAITING } radar_phase_t;
static radar_phase_t s_radar_phase = RADAR_NEED_TRIGGER;

static void app_loop(void) {
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
        s_radar_phase = RADAR_NEED_TRIGGER;   /* 测量完成 → 重新装填 */
        if (distance > 0.0f && distance < OBSTACLE_THRESHOLD_CM) {
            wink_status_t mv = dal_servo_set_angle(&neck_servo, 180.0f);   /* 近障：扫舵机 */
            (void)mv;
        } else {
            wink_status_t mv = dal_servo_set_angle(&neck_servo, 90.0f);    /* 复位 */
            (void)mv;
        }
    } else if (s == WINK_ERR_BUSY) {
        /* 测量进行中：保持上一安全输出（不动作） */
    } else {
        /* §6.1 约束2：DAL 只返错误码，fault 捕获+trace 在 App 回调内 */
        wink_trace_fault(FAULT_FRONT_RADAR);   /* ERROR：安全停止 + 故障上报 */
    }
}

static void app_on_fault(uint32_t fault_code) {
    wink_trace_fault(fault_code);
    wink_status_t r = dal_servo_set_angle(&neck_servo, 90.0f);   /* 安全位 */
    (void)r;
}

const wink_app_callbacks_t *wink_app_get_callbacks(void) {
    static const wink_app_callbacks_t cb = { app_init, app_loop, app_on_fault };
    return &cb;
}
