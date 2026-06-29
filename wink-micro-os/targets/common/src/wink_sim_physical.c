#include <stddef.h>
#include "wink_sim_physical.h"

const wink_sim_faults_t WINK_SIM_FAULTS_IDEAL = {0};  /* 全 0 = 理想，无退化 */

float wink_phys_prng_next(uint32_t *seed) {
    if (seed == NULL) { return 0.0f; }
    *seed = (*seed * 1103515245u + 12345u) & 0x7fffffffu;
    return (float)(*seed >> 8) / 8388608.0f; /* Use top 23 bits to ensure exact FPU representation and strictly [0, 1) range */
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
            ctx->bounce_flip = false;                      /* 每次启动抖动重置翻转状态，保证测试确定性 */
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
    } else {
        ctx->in_bounce = false;                            /* 理想电平提前回弹，重置抖动标志防止后续丢失去抖保护 */
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

/* warmup/采样间隔检查（§3.2）：预热内返回 WINK_ERR_BUSY；采样过近返回 WINK_ERR_TIMEOUT；否则 OK。
 * last_sample_us=NULL → 仅检查预热；时钟回拨→强制 OK+reset last_sample。 */
wink_status_t wink_phys_warmup_check(uint64_t now_us, uint64_t power_on_us,
                                     uint32_t warmup_us, uint32_t sample_interval_us,
                                     uint64_t *last_sample_us) {
    if (now_us < power_on_us || now_us - power_on_us < warmup_us) { return WINK_ERR_BUSY; }
    if (last_sample_us != NULL && sample_interval_us > 0u) {
        if (now_us < *last_sample_us) {
            *last_sample_us = now_us; /* 时钟回拨：强制复位 */
            return WINK_OK;
        }
        if (now_us - *last_sample_us < sample_interval_us) { return WINK_ERR_TIMEOUT; }
        *last_sample_us = now_us;
    }
    return WINK_OK;
}

bool wink_phys_bus_drop(uint16_t drop_permil, uint32_t *prng_seed) {
    if (drop_permil == 0u || prng_seed == NULL) { return false; }
    if (drop_permil >= 1000u) { return true; }
    return wink_phys_prng_next(prng_seed) < ((float)drop_permil / 1000.0f);
}
