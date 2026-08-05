// SPDX-License-Identifier: Apache-2.0
/**
 * @file wink_sim_physical.h
 * @brief Physical degradation algorithm library for simulation targets.
 */
#ifndef WINK_SIM_PHYSICAL_H
#define WINK_SIM_PHYSICAL_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"

#if defined(SIM_TRACE) && SIM_TRACE
#  ifndef SIM_TRACE_DEBOUNCE
#    define SIM_TRACE_DEBOUNCE 1
#  endif
#  ifndef SIM_TRACE_RC
#    define SIM_TRACE_RC 1
#  endif
#  ifndef SIM_TRACE_WARMUP
#    define SIM_TRACE_WARMUP 1
#  endif
#  ifndef SIM_TRACE_BUS
#    define SIM_TRACE_BUS 1
#  endif
#endif

#if defined(SIM_TRACE_DEBOUNCE) && SIM_TRACE_DEBOUNCE
#  include <stdio.h>
#  define ST_DEBOUNCE_START(pin, target, now, bounce_us) \
    printf("[SIM_DEBOUNCE] pin=%d start: target=%d now=%lluus bounce=%uus\n", \
           pin, target, (unsigned long long)(now), bounce_us)
#  define ST_DEBOUNCE_IN_WINDOW(pin, flip, result) \
    printf("[SIM_DEBOUNCE] pin=%d window: flip=%d level=%d\n", pin, flip, result)
#  define ST_DEBOUNCE_SETTLED(pin, level) \
    printf("[SIM_DEBOUNCE] pin=%d SETTLED: level=%d\n", pin, level)
#else
#  define ST_DEBOUNCE_START(pin, t, n, b)
#  define ST_DEBOUNCE_IN_WINDOW(pin, f, r)
#  define ST_DEBOUNCE_SETTLED(pin, l)
#endif

#if defined(SIM_TRACE_RC) && SIM_TRACE_RC
#  include <stdio.h>
#  define ST_RC_STEP(chan, target, now, tau, out) \
    printf("[SIM_RC] chan=%d: target=%.3f now=%lluus tau=%.3fs out=%.3f\n", \
           chan, target, (unsigned long long)(now), tau, out)
#  define ST_RC_NOISE(chan, noise_amp, noise_val) \
    printf("[SIM_RC] chan=%d noise: amp=%.3f val=%.3f\n", chan, noise_amp, noise_val)
#else
#  define ST_RC_STEP(c, t, n, tau, o)
#  define ST_RC_NOISE(c, a, v)
#endif

#if defined(SIM_TRACE_WARMUP) && SIM_TRACE_WARMUP
#  include <stdio.h>
#  define ST_WARMUP_CHECK(now, power_on, warmup, elapsed, status) \
    printf("[SIM_WARMUP] now=%lluus power_on=%lluus warmup=%uus elapsed=%lluus -> %s\n", \
           (unsigned long long)(now), (unsigned long long)(power_on), warmup, \
           (unsigned long long)(elapsed), \
           (status) == WINK_ERR_BUSY ? "BUSY" : (status) == WINK_ERR_TIMEOUT ? "TIMEOUT" : "OK")
#else
#  define ST_WARMUP_CHECK(n, p, w, e, s)
#endif

#if defined(SIM_TRACE_BUS) && SIM_TRACE_BUS
#  include <stdio.h>
#  define ST_BUS_DROP(permille, rand_val, will_drop) \
    printf("[SIM_BUS] drop: permille=%d rand=%.3f -> %s\n", \
           permille, rand_val, will_drop ? "DROP" : "PASS")
#else
#  define ST_BUS_DROP(p, r, d)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Fault injection parameters struct.
 */
typedef struct {
    uint32_t bounce_us;          /**< Button bounce duration in us (0 = disabled) */
    uint32_t warmup_us;          /**< Sensor power-on warmup duration in us */
    uint32_t sample_interval_us; /**< Minimum sampling interval in us */
    float    adc_noise_v;        /**< ADC noise amplitude in V (0 = disabled) */
    float    rc_tau_s;           /* RC low-pass time constant in s (<=0 = disabled) */
    uint16_t i2c_drop_permil;    /**< I2C drop rate in permille (0 = disabled) */
    uint32_t prng_seed;          /**< Deterministic PRNG seed */
} wink_sim_faults_t;

extern const wink_sim_faults_t WINK_SIM_FAULTS_IDEAL;

/**
 * @brief Deterministic PRNG (LCG).
 *
 * @param[in,out] seed Seed pointer.
 * @return Random float value in range [0, 1).
 */
float wink_phys_prng_next(uint32_t *seed);

/**
 * @brief Debounce state context.
 */
typedef struct {
    bool     stable_level;      /**< Last settled level */
    bool     in_bounce;         /**< True if currently in bounce window */
    uint64_t bounce_start_us;   /**< Start timestamp of current bounce window */
    bool     bounce_flip;       /**< Bounce flip flag */
} wink_phys_debounce_ctx_t;

/**
 * @brief Button debounce step evaluation.
 *
 * @param[in,out] ctx Debounce context pointer.
 * @param[in] target_level Target physical logic level.
 * @param[in] now_us Current timestamp in us.
 * @param[in] bounce_us Bounce window duration in us.
 * @return Evaluated physical logic level.
 */
bool wink_phys_debounce_step(wink_phys_debounce_ctx_t *ctx,
                             bool target_level, uint64_t now_us, uint32_t bounce_us);

/**
 * @brief RC low-pass filter context.
 */
typedef struct {
    float    current;   /**< Current output value */
    uint64_t last_us;   /**< Last update timestamp */
    bool     is_initialized; /**< True if initialized */
} wink_phys_rc_ctx_t;

/**
 * @brief RC low-pass filter calculation.
 *
 * @param[in,out] ctx Filter context pointer.
 * @param[in] target Target input voltage value.
 * @param[in] now_us Current timestamp in us.
 * @param[in] tau_s Time constant in seconds.
 * @param[in] noise_v Noise amplitude in V.
 * @param[in,out] prng_seed PRNG seed pointer.
 * @return Calculated filter output voltage value.
 */
float wink_phys_rc_lowpass(wink_phys_rc_ctx_t *ctx, float target, uint64_t now_us,
                           float tau_s, float noise_v, uint32_t *prng_seed);

/**
 * @brief Check sensor warmup state and sampling interval.
 *
 * @param[in] now_us Current timestamp in us.
 * @param[in] power_on_us Power-on timestamp in us.
 * @param[in] warmup_us Warmup duration requirement in us.
 * @param[in] sample_interval_us Minimum sampling interval in us.
 * @param[in,out] last_sample_us Last sample timestamp pointer.
 * @return WINK_OK on success, WINK_ERR_BUSY during warmup, WINK_ERR_TIMEOUT if sampling too fast.
 */
wink_status_t wink_phys_warmup_check(uint64_t now_us, uint64_t power_on_us,
                                     uint32_t warmup_us, uint32_t sample_interval_us,
                                     uint64_t *last_sample_us);

/**
 * @brief Evaluate bus drop decision.
 *
 * @param[in] drop_permil Drop probability in permille.
 * @param[in,out] prng_seed PRNG seed pointer.
 * @return True if frame should be dropped, false otherwise.
 */
bool wink_phys_bus_drop(uint16_t drop_permil, uint32_t *prng_seed);

#ifdef __cplusplus
}
#endif
#endif /* WINK_SIM_PHYSICAL_H */
