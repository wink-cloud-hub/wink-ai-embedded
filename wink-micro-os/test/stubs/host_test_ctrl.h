// SPDX-License-Identifier: Apache-2.0
/**
 * @file host_test_ctrl.h
 * @brief Host test injection control API.
 */
#ifndef HOST_TEST_CTRL_H
#define HOST_TEST_CTRL_H

#include <stdint.h>
#include <stdbool.h>
#include "pal_irq.h"
#include "pal_hal.h"
#include "pal_osal.h"
#include "wink_sim_physical.h"

void sim_reset_time(void);
void sim_set_echo_pin(uint16_t pin);
void sim_set_echo_timing(uint64_t rise_us, uint64_t high_duration_us);
float sim_last_pwm_duty(uint8_t channel);
void sim_set_reset_reason(pal_os_reset_reason_t reason);

void sim_set_mono_time_us(uint64_t us);
void sim_advance_mono_time_us(uint64_t delta_us);

uint8_t  sim_last_i2c_port(void);
uint16_t sim_last_i2c_addr(void);
uint32_t sim_last_i2c_write_len(void);
uint32_t sim_i2c_transfer_count(void);

#define SIM_GPIO_IDEAL_SLOTS 4
void sim_set_gpio_ideal(uint16_t pin, bool level);
void sim_clear_gpio_ideal(void);
void sim_set_faults(const wink_sim_faults_t *faults);

bool pal_host_get_gpio_level(wink_pin_t pin, bool *out_level);
void pal_host_reset_gpio_levels(void);

/**
 * @brief Manually trigger GPIO interrupt (Host platform only)
 * @param pin GPIO pin number
 */
void pal_host_trigger_gpio_interrupt(wink_pin_t pin);

/**
 * @brief Get ISR invocation count
 * @param pin GPIO pin number
 * @return ISR call count
 */
uint32_t pal_host_get_isr_call_count(wink_pin_t pin);

/**
 * @brief Reset ISR statistics and pending queue
 */
void pal_host_reset_isr_stats(void);

/**
 * @brief Get count of pending interrupts
 * @return Number of pending interrupts
 */
uint32_t pal_host_get_pending_count(void);

/**
 * @brief Get IRQ lock nesting depth
 * @return IRQ lock depth
 */
int pal_host_get_irq_lock_depth(void);

/**
 * @brief Manually trigger logical interrupt (Host platform only)
 * @param irq_num Logical IRQ number
 */
void pal_host_trigger_logical_interrupt(uint32_t irq_num);

/**
 * @brief Get logical IRQ ISR call count
 * @param irq_num Logical IRQ number
 * @return ISR call count
 */
uint32_t pal_host_get_logical_isr_call_count(uint32_t irq_num);

/**
 * @brief Host ADC virtual data injection API
 */
void pal_host_adc_inject_raw(uint8_t ch, uint16_t raw);
void pal_host_adc_inject_mv(uint8_t ch, uint16_t mv);

#endif /* HOST_TEST_CTRL_H */
