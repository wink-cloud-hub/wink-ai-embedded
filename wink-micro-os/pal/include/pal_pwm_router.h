#ifndef PAL_PWM_ROUTER_H
#define PAL_PWM_ROUTER_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PAL_PWM_TIMERS 4

typedef enum {
    PAL_PWM_TIMER_FREE = 0,
    PAL_PWM_TIMER_USED = 1,
} pal_pwm_timer_state_t;

WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_router_acquire(uint8_t channel, uint32_t freq_hz, uint8_t* out_timer_num);

void pal_pwm_router_release(uint8_t channel);

bool pal_pwm_router_channel_ready(uint8_t channel);

uint8_t pal_pwm_router_channel_timer(uint8_t channel);

void pal_pwm_router_reset(void);

#ifdef __cplusplus
}
#endif

#endif
