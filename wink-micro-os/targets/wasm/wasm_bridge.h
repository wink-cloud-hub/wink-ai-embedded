// SPDX-License-Identifier: Apache-2.0
/**
 * @file wasm_bridge.h
 * @brief Wasm-JS bridge contract single source of truth (SSOT).
 */
#ifndef WASM_BRIDGE_H
#define WASM_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void js_pal_gpio_write(uint16_t pin, bool level);
extern bool js_pal_gpio_read(uint16_t pin);
extern float js_pal_adc_read_norm(uint16_t pin);

enum {
    JS_GPIO_STATE_LOW      = 0,
    JS_GPIO_STATE_HIGH     = 1,
    JS_GPIO_STATE_HIZ      = 2,
    JS_GPIO_STATE_CONFLICT = 3,
};
extern uint8_t js_pal_gpio_read_state(uint16_t pin);
extern void js_pal_gpio_drive_ideal(uint16_t pin, bool level);
extern void js_pal_gpio_release_ideal(uint16_t pin);
extern void js_pal_gpio_release_mcu(uint16_t pin);
extern void js_pal_pwm_set_duty(uint8_t channel, float duty_cycle_percent);
extern bool js_pal_i2c_transfer(uint8_t port, uint16_t dev_addr,
                                const uint8_t *write_buf, uint32_t write_len,
                                uint8_t *read_buf, uint32_t read_len);
extern bool js_pal_spi_transfer(uint8_t port, uint16_t device_id,
                                const uint8_t *tx_buf, uint32_t len,
                                uint8_t *rx_buf, uint8_t mode, uint32_t sck_hz);
extern void js_pal_uart_write(uint8_t port, const uint8_t *buf, uint32_t len);

extern void js_pal_register_interrupt(uint16_t pin, uint32_t callback_index, uint32_t arg_ptr);
extern void js_pal_deregister_interrupt(uint16_t pin);
extern bool js_pal_poll_interrupt(uint32_t *out_callback_index, uint32_t *out_arg_ptr);

extern void js_pal_os_sleep_ms(uint32_t ms);
extern void js_pal_os_busy_wait_us(uint32_t us);

extern void js_pal_log(uint8_t level, const char *msg);

extern void     js_pal_gpio_on_write(uint8_t pin, uint8_t level);
extern float    js_sim_get_plugin_channel(const char *instance_id, const char *channel_name);

extern void     pal_wasm_advance_virtual_clock(uint64_t us);
extern void     pal_wasm_set_bounce_us(uint32_t us);
extern void     pal_wasm_set_warmup_us(uint32_t us);
extern void     pal_wasm_set_sample_interval_us(uint32_t us);
extern void     pal_wasm_set_adc_noise_v(float v);
extern void     pal_wasm_set_rc_tau_s(float s);
extern void     pal_wasm_set_i2c_drop_permil(uint16_t permil);
extern void     pal_wasm_set_prng_seed(uint32_t seed);
extern uint32_t pal_wasm_get_prng_state(void);
extern void     pal_wasm_set_prng_state(uint32_t state);
extern uint32_t pal_wasm_get_abi_hash(void);
extern void     pal_wasm_reset_physical(void);

extern bool     pal_wasm_is_clock_warning_fired(void);
extern uint64_t pal_wasm_get_virtual_clock_us(void);

extern uint64_t pal_os_get_us(void);
extern uint64_t pal_os_get_ms(void);

extern bool     pal_wasm_gpio_read(uint16_t pin);
extern bool     pal_wasm_i2c_transfer(uint8_t port, uint16_t dev_addr,
                                      const uint8_t *write_buf, uint32_t write_len,
                                      uint8_t *read_buf, uint32_t read_len);

extern uint32_t pal_wasm_get_fault_log_count(void);
extern void     pal_wasm_reset_fault_log(void);
extern uint64_t pal_wasm_fault_event_get_timestamp(uint32_t index);
extern uint8_t  pal_wasm_fault_event_get_type(uint32_t index);
extern uint16_t pal_wasm_fault_event_get_pin_or_bus(uint32_t index);
extern uint32_t pal_wasm_fault_event_get_sequence(uint32_t index);

extern bool   pal_wasm_is_faulted(void);
extern void   pal_wasm_host_fault(uint32_t code, const char* msg_cstr);

struct wasm_pin_power_model_t;
#include "wink_status.h"
extern wink_status_t pal_wasm_set_pin_power_model(uint8_t pin,
                                                  const struct wasm_pin_power_model_t *model);
extern uint64_t      pal_wasm_get_total_energy_mj(void);

extern void           pal_wasm_sim_reset_all_devices(void);
extern float          pal_wasm_get_servo_angle(uint8_t channel);
extern float          pal_wasm_get_pwm_duty_percent(uint8_t channel);
extern void           pal_wasm_push_pin_event(uint8_t pin, uint64_t delay_us, uint8_t level);
extern void           pal_wasm_set_ultrasonic_distance(uint8_t pin, float distance_cm);
extern void           pal_wasm_set_gpio_input(uint8_t pin, bool level);
extern bool           pal_wasm_get_gpio_output(uint8_t pin);

extern void           pal_wasm_set_sim_mode(uint32_t mode);
extern uint32_t       pal_wasm_get_sim_mode(void);

#ifdef __cplusplus
}
#endif

#endif /* WASM_BRIDGE_H */
