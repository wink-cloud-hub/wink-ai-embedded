#include "device_tree.h"
#include "wink_app.h"
#include "wink_trace.h"
#include "wink_actuator_registry.h"
#include "pal_osal.h"
#include "pal_debug.h"


/* ADR-0017 层 1 例外：本 TU 合法调用 WINK_BLOCKING API。抑制
 * -Wdeprecated-declarations 使 -Werror 下仍能编译；严格模式
 * (-DWINK_STRICT_NONBLOCKING=1) 下相关 API 声明直接消失，本 TU 会链接失败——那是设计意图。 */
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

static pal_os_ringbuf_handle_t s_rb = NULL;
static pal_os_task_handle_t s_sensor_h = NULL;
static pal_os_task_handle_t s_motor_h = NULL;

volatile bool g_servo_was_180 = false;

static void sensor_task(void* arg) {
    (void)arg;
    float mock_dist = 50.0f;
    float dir = -2.0f;
    
    while (1) {
        /* Mock distance variation */
        mock_dist += dir;
        if (mock_dist <= 10.0f) {
            mock_dist = 10.0f;
            dir = 2.0f;
        } else if (mock_dist >= 50.0f) {
            mock_dist = 50.0f;
            dir = -2.0f;
        }

        /* Push mock distance to ringbuf */
        wink_status_t st = pal_os_ringbuf_push(s_rb, &mock_dist, sizeof(mock_dist));
        pal_debug_printf("SENSOR: dist=%f, push status=%d\n", mock_dist, st);
        (void)st;
        
        pal_os_sleep_ms(20);
    }
}

static void motor_task(void* arg) {
    (void)arg;
    float dist = 0.0f;
    
    while (1) {
        float latest_dist = -1.0f;
        /* Drain the ring buffer to get the latest measurement */
        while (pal_os_ringbuf_pop(s_rb, &dist, sizeof(dist)) == WINK_OK) {
            latest_dist = dist;
        }
        
        if (latest_dist >= 0.0f) {
            float angle = (latest_dist < 20.0f) ? 180.0f : 90.0f;
            if (angle == 180.0f) {
                g_servo_was_180 = true;
            }
            pal_debug_printf("MOTOR: latest_dist=%f, setting angle=%f\n", latest_dist, angle);
            wink_status_t st = dal_servo_set_angle(&neck_servo, angle);
            (void)st;
        } else {
            pal_debug_printf("MOTOR: ringbuf pop empty\n");
        }
        pal_os_sleep_ms(30);
    }
}

static wink_status_t servo_safe_off_thunk(void *ctx) {
    return dal_servo_safe_off((dal_servo_t *)ctx);
}

static void app_init(void) {
    /* Apply overrides */
    wink_status_t cfg = device_tree_apply_flash_config();
    (void)cfg;

    /* Initialize devices */
    const dal_servo_config_t servo_cfg = {
        .owner        = "neck_servo",
        .pwm_channel  = neck_servo.config.pwm_channel,
        .min_pulse_ms = neck_servo.config.min_pulse_ms,
        .max_pulse_ms = neck_servo.config.max_pulse_ms
    };
    wink_status_t s = dal_servo_init(&neck_servo, &servo_cfg);
    if (wink_status_is_error(s)) { wink_trace_fault(7002u); }

    wink_status_t u = dal_ultrasonic_init(&front_radar, &front_radar.config);
    if (wink_status_is_error(u)) { wink_trace_fault(7003u); }

    /* Register safe off */
    wink_status_t ar = wink_actuator_register(servo_safe_off_thunk, &neck_servo);
    if (wink_status_is_error(ar)) { wink_trace_fault(7002u); }

    wink_status_t st_angle = dal_servo_set_angle(&neck_servo, 90.0f);
    (void)st_angle;

    /* Create ring buffer */
    s_rb = pal_os_ringbuf_create(64);
    
    /* Create tasks */
    wink_status_t t1 = pal_os_task_create(sensor_task, "sensor", 32*1024, NULL, 5, PAL_OS_CORE_ANY, &s_sensor_h);
    (void)t1;
    wink_status_t t2 = pal_os_task_create(motor_task, "motor", 32*1024, NULL, 5, PAL_OS_CORE_ANY, &s_motor_h);
    (void)t2;
}

static void app_loop(void) {
    /* Dual tasks are running independently, nothing to do in main loop */
}

static void app_on_fault(uint32_t fault_code) {
    wink_trace_fault(fault_code);
    wink_status_t st_angle = dal_servo_set_angle(&neck_servo, 90.0f);
    (void)st_angle;
}

const wink_app_callbacks_t *wink_app_get_callbacks(void) {
    static const wink_app_callbacks_t cb = { app_init, app_loop, app_on_fault };
    return &cb;
}
