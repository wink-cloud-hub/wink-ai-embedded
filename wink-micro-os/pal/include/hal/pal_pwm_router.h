#ifndef PAL_PWM_ROUTER_H
#define PAL_PWM_ROUTER_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PAL_PWM_TIMERS 4

/** @brief Effective LEDC clock source (target-resolved; ADR-0034). */
enum {
    PAL_PWM_EFF_CLK_PLATFORM_AUTO = 0,
    PAL_PWM_EFF_CLK_REF_TICK      = 1,
};

typedef struct {
    uint32_t freq_hz;
    uint8_t  resolution_bits;  /* effective (>0); not AUTO sentinel */
    uint8_t  clock_source;     /* PAL_PWM_EFF_CLK_* */
} pal_pwm_timer_profile_t;

typedef enum {
    PAL_PWM_TIMER_FREE = 0,
    PAL_PWM_TIMER_USED = 1,
} pal_pwm_timer_state_t;

/** Legacy/default effective profile: 13-bit + platform auto clock (today's pal_pwm_init). */
static inline pal_pwm_timer_profile_t pal_pwm_timer_profile_default(uint32_t freq_hz)
{
    pal_pwm_timer_profile_t p;
    p.freq_hz = freq_hz;
    p.resolution_bits = 13u;
    p.clock_source = PAL_PWM_EFF_CLK_PLATFORM_AUTO;
    return p;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_router_acquire(uint8_t channel,
                                     const pal_pwm_timer_profile_t *profile,
                                     uint8_t *out_timer_num);

void pal_pwm_router_release(uint8_t channel);

bool pal_pwm_router_channel_ready(uint8_t channel);

uint8_t pal_pwm_router_channel_timer(uint8_t channel);

void pal_pwm_router_reset(void);

/** Integer-safe percent → raw duty for `bits` resolution (shift validated). */
uint32_t pal_pwm_percent_to_raw(float percent, uint8_t bits);

#ifdef __cplusplus
}
#endif

#endif
