// SPDX-License-Identifier: GPL-3.0-only
// Task F4: Channel 1b Soft PWM duty cycle measurement meter implementation.
#include "wink_mcs51_pwm_meter.h"

#include <cstring>
#include "mcs51_context.h"
#include "wink_mcs51_clock.h"

// M2: meters live in Mcu51Context::pwm_meters (Mcs51PwmMeter, see
// mcs51_context.h); the file-static table shared measurements across
// contexts. All entry points below resolve the active context.
extern "C" {

void wink_mcs51_pwm_meter_start(uint16_t pin) {
    if (pin >= 32u) return;
    Mcu51Context* ctx = mcs51_get_context();
    Mcs51PwmMeter& m = ctx->pwm_meters[pin];
    m.active = true;
    m.last_flip_us = wink_mcs51_virtual_us();
    m.high_time_us = 0;
    m.low_time_us = 0;
    m.transitions = 0;

    // Read current pin latch level
    uint8_t port = static_cast<uint8_t>(pin >> 3);
    uint8_t bit  = static_cast<uint8_t>(pin & 7u);
    uint8_t sfr_addr = 0x80u + (port * 0x10u);
    m.current_level = (ctx->sfr_shadow[sfr_addr] >> bit) & 1u;
}

void wink_mcs51_pwm_meter_stop(uint16_t pin) {
    if (pin >= 32u) return;
    Mcs51PwmMeter& m = mcs51_get_context()->pwm_meters[pin];
    if (!m.active) return;

    uint64_t now_us = wink_mcs51_virtual_us();
    if (now_us > m.last_flip_us) {
        uint64_t dt = now_us - m.last_flip_us;
        if (m.current_level == 1u) {
            m.high_time_us += dt;
        } else {
            m.low_time_us += dt;
        }
        m.last_flip_us = now_us;
    }
    m.active = false;
}

void wink_mcs51_pwm_meter_reset(uint16_t pin) {
    if (pin >= 32u) return;
    Mcs51PwmMeter& m = mcs51_get_context()->pwm_meters[pin];
    std::memset(&m, 0, sizeof(Mcs51PwmMeter));
}

void wink_mcs51_pwm_meter_update(uint16_t pin, uint8_t level, uint64_t timestamp_us) {
    if (pin >= 32u) return;
    Mcs51PwmMeter& m = mcs51_get_context()->pwm_meters[pin];
    if (!m.active) return;

    if (timestamp_us >= m.last_flip_us) {
        uint64_t dt = timestamp_us - m.last_flip_us;
        if (m.current_level == 1u) {
            m.high_time_us += dt;
        } else {
            m.low_time_us += dt;
        }
    }
    m.current_level = level ? 1u : 0u;
    m.last_flip_us = timestamp_us;
    ++m.transitions;
}

float wink_mcs51_pwm_meter_get_duty_cycle(uint16_t pin) {
    if (pin >= 32u) return 0.0f;
    Mcs51PwmMeter& m = mcs51_get_context()->pwm_meters[pin];

    uint64_t high = m.high_time_us;
    uint64_t low  = m.low_time_us;

    if (m.active) {
        uint64_t now_us = wink_mcs51_virtual_us();
        if (now_us > m.last_flip_us) {
            uint64_t dt = now_us - m.last_flip_us;
            if (m.current_level == 1u) {
                high += dt;
            } else {
                low += dt;
            }
        }
    }

    uint64_t total = high + low;
    if (total == 0) {
        return m.current_level ? 100.0f : 0.0f;
    }

    return static_cast<float>(static_cast<double>(high) / static_cast<double>(total) * 100.0);
}

uint32_t wink_mcs51_pwm_meter_get_transitions(uint16_t pin) {
    if (pin >= 32u) return 0;
    return mcs51_get_context()->pwm_meters[pin].transitions;
}

} // extern "C"
