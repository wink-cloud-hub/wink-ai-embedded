/**
 * @file wasm_sim_registry.c
 * @brief Wasm 侧虚拟外设拦截分发器实现。
 */
#include "wasm_sim_registry.h"
#include "wasm_bridge.h"
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#define WASM_SIM_MAX_PINS 40

// 外部虚拟外设声明（SSD1306 C Model 已退役 → Unisim plugin / I2CBus）
void wasm_dev_servo_reset(void);
void wasm_dev_servo_set_duty(uint8_t channel, float duty_cycle_percent);

void     wasm_dev_ultrasonic_reset(void);
uint32_t wasm_dev_ultrasonic_get_pulse_us(uint8_t pin);
#ifdef __EMSCRIPTEN__
void     wasm_sim_pin_events_reset(void);
#endif

// 通用 GPIO 输入/输出虚拟状态
static bool s_gpio_inputs[WASM_SIM_MAX_PINS];
static bool s_gpio_input_set[WASM_SIM_MAX_PINS];
static bool s_gpio_outputs[WASM_SIM_MAX_PINS];

// 统一复位接口
void wasm_sim_devices_reset(void) {
    wasm_dev_servo_reset();
    wasm_dev_ultrasonic_reset();
#ifdef __EMSCRIPTEN__
    wasm_sim_pin_events_reset();
#endif
    memset(s_gpio_inputs, 0, sizeof(s_gpio_inputs));
    memset(s_gpio_input_set, 0, sizeof(s_gpio_input_set));
    memset(s_gpio_outputs, 0, sizeof(s_gpio_outputs));
}

// 供 JS 侧重置所有仿真状态的导出接口
EMSCRIPTEN_KEEPALIVE void pal_wasm_sim_reset_all_devices(void) {
    wasm_sim_devices_reset();
}

// I2C 拦截判断 — Phase 4 T5: SSD1306 moved to Unisim TS plugin on I2CBus.
// Scheme-A address short-circuit retired; all I2C goes through js_pal_i2c_transfer.
bool wasm_sim_i2c_dev_exists(uint16_t dev_addr) {
    (void)dev_addr;
    return false;
}

// I2C 拦截处理分发 (no C-side I2C devices remain)
wink_status_t wasm_sim_i2c_dev_transfer(uint8_t port, uint16_t dev_addr,
                                        const uint8_t *write_buf, uint32_t write_len,
                                        uint8_t *read_buf, uint32_t read_len) {
    (void)port;
    (void)dev_addr;
    (void)write_buf;
    (void)write_len;
    (void)read_buf;
    (void)read_len;
    return WINK_ERR_UNSUPPORTED;
}

// PWM 拦截判断
bool wasm_sim_pwm_channel_exists(uint8_t channel) {
    return (channel < 16);
}

// PWM 拦截处理分发
void wasm_sim_pwm_set_duty(uint8_t channel, float duty_cycle_percent) {
    wasm_dev_servo_set_duty(channel, duty_cycle_percent);
}

// GPIO 状态获取/写入
void wasm_sim_gpio_set_input(uint8_t pin, bool level) {
    if (pin < WASM_SIM_MAX_PINS) {
        s_gpio_inputs[pin] = level;
        s_gpio_input_set[pin] = true;
    }
}

bool wasm_sim_gpio_input_is_set(uint8_t pin, bool *out_level) {
    if (pin < WASM_SIM_MAX_PINS && s_gpio_input_set[pin]) {
        if (out_level != NULL) {
            *out_level = s_gpio_inputs[pin];
        }
        return true;
    }
    return false;
}

bool wasm_sim_gpio_get_input(uint8_t pin) {
    if (pin < WASM_SIM_MAX_PINS) {
        return s_gpio_inputs[pin];
    }
    return false;
}

bool wasm_sim_gpio_get_output(uint8_t pin) {
    if (pin < WASM_SIM_MAX_PINS) {
        return s_gpio_outputs[pin];
    }
    return false;
}

void wasm_sim_gpio_write(uint8_t pin, bool level) {
    if (pin < WASM_SIM_MAX_PINS) {
        s_gpio_outputs[pin] = level;
    }
}

// 供 JS 侧注入虚拟 GPIO 输入电平 (如用户点击虚拟开关/按钮)
// P3: Arbiter-only — drive ideal:ui:{N}; C shadow is unused by pal_gpio_read.
EMSCRIPTEN_KEEPALIVE void pal_wasm_set_gpio_input(uint8_t pin, bool level) {
    js_pal_gpio_drive_ideal((uint16_t)pin, level);
}

#ifndef __EMSCRIPTEN__
/* Host unit-test link stub (real import is JS under emscripten). */
void js_pal_gpio_drive_ideal(uint16_t pin, bool level) {
    (void)pin;
    (void)level;
}
#endif

// 供 JS 侧同步获取虚拟 GPIO 输出电平 (如 LED 灯渲染)
EMSCRIPTEN_KEEPALIVE bool pal_wasm_get_gpio_output(uint8_t pin) {
    return wasm_sim_gpio_get_output(pin);
}
