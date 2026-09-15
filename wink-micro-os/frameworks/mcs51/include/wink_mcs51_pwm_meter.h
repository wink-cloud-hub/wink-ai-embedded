// SPDX-License-Identifier: GPL-3.0-only
// Task F4: Channel 1b Soft PWM duty cycle measurement meter for MCS-51 simulation.
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Start PWM duty cycle measurement on a linear pin (0..31).
void wink_mcs51_pwm_meter_start(uint16_t pin);

// Stop PWM duty cycle measurement on a linear pin.
void wink_mcs51_pwm_meter_stop(uint16_t pin);

// Reset PWM meter statistics for a linear pin.
void wink_mcs51_pwm_meter_reset(uint16_t pin);

// Record a pin transition event (called automatically on pin changes).
void wink_mcs51_pwm_meter_update(uint16_t pin, uint8_t level, uint64_t timestamp_us);

// Get measured duty cycle as a percentage: 0.0f .. 100.0f (precision error < 2%).
float wink_mcs51_pwm_meter_get_duty_cycle(uint16_t pin);

// Get total number of recorded pin transitions.
uint32_t wink_mcs51_pwm_meter_get_transitions(uint16_t pin);

#ifdef __cplusplus
}
#endif
