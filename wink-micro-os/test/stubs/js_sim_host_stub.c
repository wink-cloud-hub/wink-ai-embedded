/**
 * @file js_sim_host_stub.c
 * @brief 仿真侧 js_sim_* 桩（签名抄 wasm_bridge.h / Registry）。
 *        验证「-DSIMULATION 分支同样走共享换算 dal_pulse_us_to_cm」——ADR-0003 决策2 回归守卫。
 */
#include "js_sim_host_stub.h"

static uint32_t s_injected_pulse_us = 0;

void sim_set_echo_pulse_us(uint32_t pulse_us) { s_injected_pulse_us = pulse_us; }

void js_sim_trigger_ultrasonic(uint16_t trig_pin) { (void)trig_pin; }

/* wasm_bridge.h: uint32_t js_sim_measure_echo_pulse_us(uint16_t trig_pin) —— 返回注入脉宽 */
uint32_t js_sim_measure_echo_pulse_us(uint16_t trig_pin) {
    (void)trig_pin;
    return s_injected_pulse_us;
}

/* ─────────────────────────────────────────────────────────
 * 仿真模式的自动注册器 (经由 constructor 注册到 pal_hal_host.c)
 * ───────────────────────────────────────────────────────── */
__attribute__((constructor))
static void register_sim_ultrasonic_callbacks(void) {
    extern void host_register_sim_ultrasonic(void (*trigger_fn)(uint16_t), uint32_t (*measure_fn)(uint16_t));
    host_register_sim_ultrasonic(js_sim_trigger_ultrasonic, js_sim_measure_echo_pulse_us);
}
