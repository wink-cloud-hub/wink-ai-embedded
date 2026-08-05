// SPDX-License-Identifier: Apache-2.0
#include <stddef.h>
#include "wink_sim_physical.h"

const wink_sim_faults_t WINK_SIM_FAULTS_IDEAL = {0};

float wink_phys_prng_next(uint32_t *seed) {
    if (seed == NULL) { return 0.0f; }
    *seed = (*seed * 1103515245u + 12345u) & 0x7fffffffu;
    return (float)(*seed >> 8) / 8388608.0f;
}

bool wink_phys_debounce_step(wink_phys_debounce_ctx_t *ctx,
                             bool target_level, uint64_t now_us, uint32_t bounce_us) {
    if (ctx == NULL) { return target_level; }
    if (bounce_us == 0u) {
        ctx->stable_level = target_level;
        ctx->in_bounce = false;
        return target_level;
    }
    if (target_level != ctx->stable_level) {
        if (!ctx->in_bounce) {
            ctx->bounce_start_us = now_us;
            ctx->in_bounce = true;
            ctx->bounce_flip = false;
            ST_DEBOUNCE_START(-1, target_level, now_us, bounce_us);
        }
        if (now_us < ctx->bounce_start_us) {
            ctx->bounce_start_us = now_us;
        }
        if (now_us - ctx->bounce_start_us < bounce_us) {
            ctx->bounce_flip = !ctx->bounce_flip;
            bool result = ctx->bounce_flip ? target_level : !target_level;
            ST_DEBOUNCE_IN_WINDOW(-1, ctx->bounce_flip, result);
            return result;
        }
        ctx->stable_level = target_level;
        ctx->in_bounce = false;
        ST_DEBOUNCE_SETTLED(-1, target_level);
    } else {
        ctx->in_bounce = false;
    }
    return ctx->stable_level;
}

float wink_phys_rc_lowpass(wink_phys_rc_ctx_t *ctx, float target, uint64_t now_us,
                           float tau_s, float noise_v, uint32_t *prng_seed) {
    if (ctx == NULL) { return target; }
    if (!ctx->is_initialized || now_us < ctx->last_us) {
        ctx->current = target;
        ctx->last_us = now_us;
        ctx->is_initialized = true;
        ST_RC_STEP(-1, target, now_us, tau_s, target);
        return target;
    }
    if (tau_s <= 0.0f) {
        ctx->current = target;
        ctx->last_us = now_us;
    } else {
        float dt = (float)(now_us - ctx->last_us) / 1000000.0f;
        ctx->last_us = now_us;
        if (dt > 0.0f) {
            float alpha = dt / tau_s;
            if (alpha > 1.0f) { alpha = 1.0f; }
            ctx->current += (target - ctx->current) * alpha;
        }
    }
    float out = ctx->current;
    if (noise_v > 0.0f && prng_seed != NULL) {
        float n = (wink_phys_prng_next(prng_seed) - 0.5f) * 2.0f * noise_v;
        ST_RC_NOISE(-1, noise_v, n);
        out += n;
    }
    ST_RC_STEP(-1, target, now_us, tau_s, out);
    return out;
}

wink_status_t wink_phys_warmup_check(uint64_t now_us, uint64_t power_on_us,
                                     uint32_t warmup_us, uint32_t sample_interval_us,
                                     uint64_t *last_sample_us) {
    uint64_t elapsed = (now_us >= power_on_us) ? (now_us - power_on_us) : 0;
    wink_status_t status;
    if (now_us < power_on_us || now_us - power_on_us < warmup_us) {
        status = WINK_ERR_BUSY;
        ST_WARMUP_CHECK(now_us, power_on_us, warmup_us, elapsed, status);
        return status;
    }
    if (last_sample_us != NULL && sample_interval_us > 0u) {
        if (now_us < *last_sample_us) {
            *last_sample_us = now_us;
            status = WINK_OK;
            ST_WARMUP_CHECK(now_us, power_on_us, warmup_us, elapsed, status);
            return status;
        }
        elapsed = now_us - *last_sample_us;
        if (now_us - *last_sample_us < sample_interval_us) {
            status = WINK_ERR_TIMEOUT;
            ST_WARMUP_CHECK(now_us, power_on_us, warmup_us, elapsed, status);
            return status;
        }
        *last_sample_us = now_us;
    }
    status = WINK_OK;
    ST_WARMUP_CHECK(now_us, power_on_us, warmup_us, elapsed, status);
    return status;
}

bool wink_phys_bus_drop(uint16_t drop_permil, uint32_t *prng_seed) {
    if (drop_permil == 0u || prng_seed == NULL) {
        ST_BUS_DROP(drop_permil, 0.0f, false);
        return false;
    }
    if (drop_permil >= 1000u) {
        ST_BUS_DROP(drop_permil, 1.0f, true);
        return true;
    }
    float r = wink_phys_prng_next(prng_seed);
    bool will_drop = r < ((float)drop_permil / 1000.0f);
    ST_BUS_DROP(drop_permil, r, will_drop);
    return will_drop;
}
