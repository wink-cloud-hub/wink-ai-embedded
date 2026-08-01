/**
 * @file wasm_dev_ultrasonic.c
 * @brief Wasm 仿真侧 HC-SR04 超声波虚拟外设模型 (C-side Model)。
 */
#include "wasm_sim_registry.h"
#include "wasm_bridge.h"
#include <stdio.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#define WASM_SIM_MAX_PINS 40

// 记录每个 GPIO 引脚作为 Echo 引脚时的虚拟物理距离 (cm)。
//
// 重要：BSS 的零初始化会把 float 填成 0.0f，但 get_pulse_us() 会把
// 0.0f 当成 "~2cm" 并返回 116us（命中 minimum 钳位），从而拦截掉
// js_sim_measure_echo_pulse_us 的 JS 旁路。我们必须在 startup 前
// 就把所有槽位填成 -1.0f（"未注入" 哨兵），让 JS fallback 路径在
// host 未调用 pal_wasm_set_ultrasonic_distance() 时生效。
//
// 使用 C99/GNU 的 range designated initializer（emcc/Clang 支持）
// 直接静态初始化全数组，避免依赖 __attribute__((constructor)) 的
// 时序或 .data 段膨胀（GCC 会把同值重复数组折叠为 .bss + memset）。
static float s_virtual_ultrasonic_distance[WASM_SIM_MAX_PINS];

/* Fill all slots with -1.0f sentinel at startup. */
__attribute__((constructor))
static void ultrasonic_boot_init(void) {
    for (int i = 0; i < WASM_SIM_MAX_PINS; i++) {
        s_virtual_ultrasonic_distance[i] = -1.0f;
    }
}

void wasm_dev_ultrasonic_reset(void) {
    ultrasonic_boot_init();
}

// 供 JS 侧/3D 场景同步注入物理距离的导出接口
EMSCRIPTEN_KEEPALIVE void pal_wasm_set_ultrasonic_distance(uint8_t pin, float distance_cm) {
    if (pin >= WASM_SIM_MAX_PINS) {
        return;
    }
    s_virtual_ultrasonic_distance[pin] = distance_cm;
}

// 供 C 侧脉宽测量函数调用的模拟接口
uint32_t wasm_dev_ultrasonic_get_pulse_us(uint8_t pin) {
    if (pin >= WASM_SIM_MAX_PINS) {
        return 0;
    }

    // 1. 优先从标准 Unisim 插件通道读取距离 (SSOT 规范: type:index，如 "ultrasonic:0")
    float distance_cm = js_sim_get_plugin_channel("ultrasonic:0", "distanceCm");

    // 2. 回退机制：若插件通道未就绪，查直接注入到 pin 槽位的物理距离
    if (distance_cm < 0.0f && s_virtual_ultrasonic_distance[pin] >= 0.0f) {
        distance_cm = s_virtual_ultrasonic_distance[pin];
    }
    // 3. 容错：检查邻近引脚 (解决前端把物理距离误注到 trig_pin 的情况)
    else if (distance_cm < 0.0f && pin > 0 && s_virtual_ultrasonic_distance[pin - 1] >= 0.0f) {
        distance_cm = s_virtual_ultrasonic_distance[pin - 1];
    }
    else if (distance_cm < 0.0f && pin < WASM_SIM_MAX_PINS - 1 && s_virtual_ultrasonic_distance[pin + 1] >= 0.0f) {
        distance_cm = s_virtual_ultrasonic_distance[pin + 1];
    }

    if (distance_cm < 0.0f) {
        return 0; // 无有效距离，返回 0 模拟无回波
    }

    uint32_t pulse_us = (uint32_t)(distance_cm * 58.0f);
    if (distance_cm <= 2.0f) {
        pulse_us = 116;
    } else if (distance_cm >= 400.0f) {
        pulse_us = 0;
    }

    return pulse_us;
}
