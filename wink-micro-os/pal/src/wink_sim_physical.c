#include <stddef.h>
#include "wink_sim_physical.h"

const wink_sim_faults_t WINK_SIM_FAULTS_IDEAL = {0};  /* 全 0 = 理想，无退化 */

float wink_phys_prng_next(uint32_t *seed) {
    if (seed == NULL) { return 0.0f; }
    *seed = (*seed * 1103515245u + 12345u) & 0x7fffffffu;
    return (float)*seed / 2147483647.0f; /* 字面量统一 f 后缀，避免中间运算提升为 double，保 host/wasm/esp32 确定性一致 */
}

bool wink_phys_debounce_step(wink_phys_debounce_ctx_t *ctx,
                             bool target_level, uint64_t now_us, uint32_t bounce_us) {
    if (ctx == NULL) { return target_level; }              /* 降级 */
    if (bounce_us == 0u) {
        ctx->stable_level = target_level;
        ctx->in_bounce = false;
        return target_level;
    }
    if (target_level != ctx->stable_level) {
        if (!ctx->in_bounce) {
            ctx->bounce_start_us = now_us;
            ctx->in_bounce = true;
        }
        /* 防御时钟回拨/重置 */
        if (now_us < ctx->bounce_start_us) {
            ctx->bounce_start_us = now_us;
        }
        if (now_us - ctx->bounce_start_us < bounce_us) {
            ctx->bounce_flip = !ctx->bounce_flip;          /* 强制交替：每次采样翻转（采样周期无关） */
            return ctx->bounce_flip ? target_level : !target_level;
        }
        ctx->stable_level = target_level;
        ctx->in_bounce = false;
    }
    return ctx->stable_level;
}

float wink_phys_rc_lowpass(wink_phys_rc_ctx_t *ctx, float target, uint64_t now_us,
                           float tau_s, float noise_v, uint32_t *prng_seed) {
    if (ctx == NULL) { return target; }                    /* 降级 */
    if (!ctx->is_initialized || now_us < ctx->last_us) {
        ctx->current = target;
        ctx->last_us = now_us;
        ctx->is_initialized = true;
        return target;
    }
    float dt = (float)(now_us - ctx->last_us) / 1000000.0f; /* 字面量统一 f 后缀，避免中间 double 提升，保确定性 */
    ctx->last_us = now_us;
    if (tau_s > 0.0f && dt > 0.0f) {
        float alpha = dt / tau_s;
        if (alpha > 1.0f) { alpha = 1.0f; }
        ctx->current += (target - ctx->current) * alpha;
    }
    if (noise_v > 0.0f && prng_seed != NULL) {
        float n = (wink_phys_prng_next(prng_seed) - 0.5f) * 2.0f * noise_v;
        return ctx->current + n;
    }
    return ctx->current;
}

/* warmup_check 占位实现在 Task 5；此处先加 stub 让 Task 2 编译通过 */
wink_status_t wink_phys_warmup_check(uint64_t now_us, uint64_t power_on_us,
                                     uint32_t warmup_us, uint32_t sample_interval_us,
                                     uint64_t *last_sample_us) {
    (void)now_us; (void)power_on_us; (void)warmup_us;
    (void)sample_interval_us; (void)last_sample_us;
    return WINK_OK;  /* Task 5 替换 */
}
