/**
 * @file pal_pwm_router.c
 * @brief target 无关 PWM 定时器分配状态机（纯逻辑，无锁，无硬件）。
 *
 * ADR-0034: timer 身份 = 完整 effective profile（freq + bits + clock）。
 * 非并发契约见 pal_pwm_router.h。host/wasm/esp32 三 target 共享链接。
 */
#include "pal_pwm_router.h"

typedef struct {
    pal_pwm_timer_profile_t profile;
    uint8_t                 ref_count;
} pwm_timer_slot_t;

static pwm_timer_slot_t s_timer_slots[PAL_PWM_TIMERS];
static bool             s_channel_init[PAL_PWM_CHANNELS];
static pal_pwm_timer_profile_t s_channel_profile[PAL_PWM_CHANNELS];
static uint8_t          s_channel_timer[PAL_PWM_CHANNELS];

static bool pwm_profile_valid(const pal_pwm_timer_profile_t *p)
{
    return p != NULL
        && p->freq_hz > 0u
        && p->resolution_bits > 0u
        && p->resolution_bits <= 20u
        && p->clock_source <= PAL_PWM_EFF_CLK_REF_TICK;
}

static bool pwm_profiles_equal(const pal_pwm_timer_profile_t *a,
                               const pal_pwm_timer_profile_t *b)
{
    return a->freq_hz == b->freq_hz
        && a->resolution_bits == b->resolution_bits
        && a->clock_source == b->clock_source;
}

uint32_t pal_pwm_percent_to_raw(float percent, uint8_t bits)
{
    if (bits == 0u || bits > 20u) {
        return 0u;
    }
    if (percent < 0.0f) { percent = 0.0f; }
    if (percent > 100.0f) { percent = 100.0f; }

    uint32_t max_duty = (UINT32_C(1) << bits) - UINT32_C(1);
    return (uint32_t)((percent * (float)max_duty) / 100.0f);
}

void pal_pwm_router_reset(void)
{
    for (uint8_t t = 0; t < PAL_PWM_TIMERS; t++) {
        s_timer_slots[t].profile.freq_hz = 0;
        s_timer_slots[t].profile.resolution_bits = 0;
        s_timer_slots[t].profile.clock_source = 0;
        s_timer_slots[t].ref_count = 0;
    }
    for (uint8_t c = 0; c < PAL_PWM_CHANNELS; c++) {
        s_channel_init[c] = false;
        s_channel_profile[c].freq_hz = 0;
        s_channel_profile[c].resolution_bits = 0;
        s_channel_profile[c].clock_source = 0;
        s_channel_timer[c] = 0xFF;
    }
}

static int8_t pwm_router_find_slot(const pal_pwm_timer_profile_t *profile)
{
    int8_t free_slot = -1;
    for (uint8_t t = 0; t < PAL_PWM_TIMERS; t++) {
        if (s_timer_slots[t].ref_count > 0
            && pwm_profiles_equal(&s_timer_slots[t].profile, profile)) {
            return (int8_t)t;
        }
        if (s_timer_slots[t].ref_count == 0 && free_slot < 0) {
            free_slot = (int8_t)t;
        }
    }
    return free_slot;
}

wink_status_t pal_pwm_router_acquire(uint8_t channel,
                                     const pal_pwm_timer_profile_t *profile,
                                     uint8_t *out_timer_num)
{
    if (channel >= PAL_PWM_CHANNELS || out_timer_num == NULL
        || !pwm_profile_valid(profile)) {
        return WINK_ERR_INVALID_ARG;
    }

    if (s_channel_init[channel]) {
        if (pwm_profiles_equal(&s_channel_profile[channel], profile)) {
            *out_timer_num = s_channel_timer[channel];
            return WINK_OK;
        }
        return WINK_ERR_BUSY;
    }

    int8_t slot = pwm_router_find_slot(profile);
    if (slot < 0) {
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    if (s_timer_slots[slot].ref_count == 0) {
        s_timer_slots[slot].profile = *profile;
    }
    s_timer_slots[slot].ref_count++;

    s_channel_init[channel] = true;
    s_channel_profile[channel] = *profile;
    s_channel_timer[channel] = (uint8_t)slot;
    *out_timer_num = (uint8_t)slot;
    return WINK_OK;
}

void pal_pwm_router_release(uint8_t channel)
{
    if (channel >= PAL_PWM_CHANNELS || !s_channel_init[channel]) {
        return;
    }
    uint8_t t = s_channel_timer[channel];
    if (t < PAL_PWM_TIMERS && s_timer_slots[t].ref_count > 0) {
        s_timer_slots[t].ref_count--;
        if (s_timer_slots[t].ref_count == 0) {
            s_timer_slots[t].profile.freq_hz = 0;
            s_timer_slots[t].profile.resolution_bits = 0;
            s_timer_slots[t].profile.clock_source = 0;
        }
    }
    s_channel_init[channel] = false;
    s_channel_profile[channel].freq_hz = 0;
    s_channel_profile[channel].resolution_bits = 0;
    s_channel_profile[channel].clock_source = 0;
    s_channel_timer[channel] = 0xFF;
}

bool pal_pwm_router_channel_ready(uint8_t channel)
{
    return channel < PAL_PWM_CHANNELS && s_channel_init[channel];
}

uint8_t pal_pwm_router_channel_timer(uint8_t channel)
{
    if (!pal_pwm_router_channel_ready(channel)) {
        return 0xFF;
    }
    return s_channel_timer[channel];
}
