#define LOG_TAG "bal.chassis"

#include "control/wink_chassis_controller.h"
#include "wink_tasks.h"
#include "wink_blocking_region.h"
#include "pal_log.h"
#include "pal_osal.h"
#include <string.h>

#ifndef WINK_CHASSIS_HELPER_MAX
#  define WINK_CHASSIS_HELPER_MAX 2
#endif

#if WINK_CHASSIS_HELPER_MAX > 0

typedef struct {
    dal_motor_t             *left_motor;
    dal_encoder_t           *left_encoder;
    dal_motor_t             *right_motor;
    dal_encoder_t           *right_encoder;
    
    wink_diff_drive_params_t kinematics_params;
    
    volatile float          target_linear_v;
    volatile float          target_angular_w;
} chassis_ctx_t;

static chassis_ctx_t s_chassis_slots[WINK_CHASSIS_HELPER_MAX];

/* ── internal helpers ────────────────────────────────────────── */

static int find_free_slot(void) {
    for (int i = 0; i < WINK_CHASSIS_HELPER_MAX; i++) {
        if (s_chassis_slots[i].left_motor == NULL) {
            return i;
        }
    }
    return -1;
}

static int find_slot_by_left_motor(dal_motor_t *left_motor) {
    for (int i = 0; i < WINK_CHASSIS_HELPER_MAX; i++) {
        if (s_chassis_slots[i].left_motor == left_motor) {
            return i;
        }
    }
    return -1;
}

/* ── public API ──────────────────────────────────────────────── */

wink_status_t wink_chassis_start_ex(dal_motor_t *left_motor, dal_encoder_t *left_encoder,
                                    dal_motor_t *right_motor, dal_encoder_t *right_encoder,
                                    const wink_chassis_config_t *cfg,
                                    const wink_helper_opts_t *opts)
{
    if (left_motor == NULL || left_encoder == NULL ||
        right_motor == NULL || right_encoder == NULL || cfg == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (find_slot_by_left_motor(left_motor) >= 0) {
        return WINK_ERR_INVALID_STATE;
    }
    int free_idx = find_free_slot();
    if (free_idx < 0) {
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    chassis_ctx_t *ctx = &s_chassis_slots[free_idx];
    ctx->left_motor = left_motor;
    ctx->left_encoder = left_encoder;
    ctx->right_motor = right_motor;
    ctx->right_encoder = right_encoder;
    memcpy(&ctx->kinematics_params, &cfg->kinematics_params, sizeof(wink_diff_drive_params_t));
    ctx->target_linear_v = 0.0f;
    ctx->target_angular_w = 0.0f;

    // Start left closed-loop motor
    wink_status_t st = wink_closed_loop_motor_start_ex(left_motor, left_encoder, &cfg->left_motor_cfg, opts);
    if (wink_status_is_error(st)) {
        ctx->left_motor = NULL;
        return st;
    }

    // Start right closed-loop motor
    st = wink_closed_loop_motor_start_ex(right_motor, right_encoder, &cfg->right_motor_cfg, opts);
    if (wink_status_is_error(st)) {
        // Rollback left motor
        WINK_IGNORE_RESULT(wink_closed_loop_motor_stop(left_motor));
        ctx->left_motor = NULL;
        return st;
    }

    return WINK_OK;
}

wink_status_t wink_chassis_start(dal_motor_t *left_motor, dal_encoder_t *left_encoder,
                                 dal_motor_t *right_motor, dal_encoder_t *right_encoder,
                                 const wink_chassis_config_t *cfg)
{
    return wink_chassis_start_ex(left_motor, left_encoder, right_motor, right_encoder, cfg, NULL);
}

wink_status_t wink_chassis_stop(dal_motor_t *left_motor)
{
    if (left_motor == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    int idx = find_slot_by_left_motor(left_motor);
    if (idx < 0) {
        return WINK_OK; // Idempotent stop
    }
    chassis_ctx_t *ctx = &s_chassis_slots[idx];

    // Stop left closed-loop motor
    WINK_IGNORE_RESULT(wink_closed_loop_motor_stop(ctx->left_motor));
    // Stop right closed-loop motor
    WINK_IGNORE_RESULT(wink_closed_loop_motor_stop(ctx->right_motor));

    // Release slot
    ctx->left_motor = NULL;
    ctx->left_encoder = NULL;
    ctx->right_motor = NULL;
    ctx->right_encoder = NULL;

    return WINK_OK;
}

wink_status_t wink_chassis_set_velocity(dal_motor_t *left_motor, float linear_v, float angular_w)
{
    if (left_motor == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    int idx = find_slot_by_left_motor(left_motor);
    if (idx < 0) {
        return WINK_ERR_INVALID_STATE;
    }
    chassis_ctx_t *ctx = &s_chassis_slots[idx];

    float vl = 0.0f;
    float vr = 0.0f;
    
    // Inverse kinematics: v, w -> vl, vr (m/s)
    wink_status_t st = wink_diff_drive_to_wheel_speeds(&ctx->kinematics_params, linear_v, angular_w, &vl, &vr);
    if (wink_status_is_error(st)) {
        return st;
    }

    // Convert speed (m/s) to encoder rate (counts/s)
    float left_counts = wink_diff_drive_speed_to_counts(&ctx->kinematics_params, vl);
    float right_counts = wink_diff_drive_speed_to_counts(&ctx->kinematics_params, vr);

    PAL_CRITICAL_SECTION({
        ctx->target_linear_v = linear_v;
        ctx->target_angular_w = angular_w;
    });

    // Update targets on closed-loop motors
    st = wink_closed_loop_motor_set_speed(ctx->left_motor, left_counts);
    if (wink_status_is_error(st)) {
        return st;
    }
    st = wink_closed_loop_motor_set_speed(ctx->right_motor, right_counts);
    if (wink_status_is_error(st)) {
        return st;
    }

    return WINK_OK;
}

wink_status_t wink_chassis_get_velocity(dal_motor_t *left_motor, float *out_linear_v, float *out_angular_w)
{
    if (left_motor == NULL || out_linear_v == NULL || out_angular_w == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    int idx = find_slot_by_left_motor(left_motor);
    if (idx < 0) {
        return WINK_ERR_INVALID_STATE;
    }
    chassis_ctx_t *ctx = &s_chassis_slots[idx];

    float left_counts_s = 0.0f;
    float right_counts_s = 0.0f;

    // Get current feedback speed of motors (counts/s)
    wink_status_t st = wink_closed_loop_motor_get_speed(ctx->left_motor, &left_counts_s);
    if (wink_status_is_error(st)) {
        return st;
    }
    st = wink_closed_loop_motor_get_speed(ctx->right_motor, &right_counts_s);
    if (wink_status_is_error(st)) {
        return st;
    }

    // Convert counts/s to speed m/s
    float vl = wink_diff_drive_counts_to_speed(&ctx->kinematics_params, left_counts_s);
    float vr = wink_diff_drive_counts_to_speed(&ctx->kinematics_params, right_counts_s);

    // Forward kinematics: vl, vr -> v, w
    return wink_diff_drive_to_chassis_speeds(&ctx->kinematics_params, vl, vr, out_linear_v, out_angular_w);
}

void wink_chassis_reset(void)
{
    for (int i = 0; i < WINK_CHASSIS_HELPER_MAX; i++) {
        if (s_chassis_slots[i].left_motor != NULL) {
            WINK_IGNORE_RESULT(wink_closed_loop_motor_stop(s_chassis_slots[i].left_motor));
            WINK_IGNORE_RESULT(wink_closed_loop_motor_stop(s_chassis_slots[i].right_motor));
            s_chassis_slots[i].left_motor = NULL;
            s_chassis_slots[i].left_encoder = NULL;
            s_chassis_slots[i].right_motor = NULL;
            s_chassis_slots[i].right_encoder = NULL;
        }
    }
}

#else /* WINK_CHASSIS_HELPER_MAX == 0 */

wink_status_t wink_chassis_start_ex(dal_motor_t *left_motor, dal_encoder_t *left_encoder,
                                    dal_motor_t *right_motor, dal_encoder_t *right_encoder,
                                    const wink_chassis_config_t *cfg,
                                    const wink_helper_opts_t *opts)
{
    (void)left_motor; (void)left_encoder; (void)right_motor; (void)right_encoder; (void)cfg; (void)opts;
    return WINK_ERR_UNAVAILABLE;
}

wink_status_t wink_chassis_start(dal_motor_t *left_motor, dal_encoder_t *left_encoder,
                                 dal_motor_t *right_motor, dal_encoder_t *right_encoder,
                                 const wink_chassis_config_t *cfg)
{
    (void)left_motor; (void)left_encoder; (void)right_motor; (void)right_encoder; (void)cfg;
    return WINK_ERR_UNAVAILABLE;
}

wink_status_t wink_chassis_stop(dal_motor_t *left_motor)
{
    (void)left_motor;
    return WINK_OK;
}

wink_status_t wink_chassis_set_velocity(dal_motor_t *left_motor, float linear_v, float angular_w)
{
    (void)left_motor; (void)linear_v; (void)angular_w;
    return WINK_ERR_UNAVAILABLE;
}

wink_status_t wink_chassis_get_velocity(dal_motor_t *left_motor, float *out_linear_v, float *out_angular_w)
{
    (void)left_motor; (void)out_linear_v; (void)out_angular_w;
    return WINK_ERR_UNAVAILABLE;
}

void wink_chassis_reset(void)
{
}

#endif /* WINK_CHASSIS_HELPER_MAX > 0 */
