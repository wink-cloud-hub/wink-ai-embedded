/**
 * @file wasm_sim_registry.h
 * @brief Wasm 侧虚拟外设拦截分发器头文件。
 */
#ifndef WASM_SIM_REGISTRY_H
#define WASM_SIM_REGISTRY_H

#include "wink_status.h"
#include <stdint.h>
#include <stdbool.h>

// I2C 仿真设备分发
bool          wasm_sim_i2c_dev_exists(uint16_t dev_addr);
wink_status_t wasm_sim_i2c_dev_transfer(uint8_t port, uint16_t dev_addr,
                                        const uint8_t *write_buf, uint32_t write_len,
                                        uint8_t *read_buf, uint32_t read_len);

// PWM 仿真通道分发
bool wasm_sim_pwm_channel_exists(uint8_t channel);
void wasm_sim_pwm_set_duty(uint8_t channel, float duty_cycle_percent);

// GPIO 仿真与注入
void wasm_sim_gpio_set_input(uint8_t pin, bool level);
bool wasm_sim_gpio_get_input(uint8_t pin);
bool wasm_sim_gpio_get_output(uint8_t pin);
void wasm_sim_gpio_write(uint8_t pin, bool level);
uint32_t wasm_dev_ultrasonic_get_pulse_us(uint8_t pin);

// 统一仿真复位
void wasm_sim_devices_reset(void);

#endif /* WASM_SIM_REGISTRY_H */
