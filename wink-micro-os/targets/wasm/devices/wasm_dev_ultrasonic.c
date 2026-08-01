/**
 * @file wasm_dev_ultrasonic.c
 * @brief Wasm 仿真侧 HC-SR04 超声波虚拟外设模型 (C-side Model)。
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

    float distance_cm = -1.0f;

    // 1. 优先尝试直接从目标引脚 (echo_pin) 读取物理注入的距离
    if (s_virtual_ultrasonic_distance[pin] >= 0.0f) {
        distance_cm = s_virtual_ultrasonic_distance[pin];
    }
    // 2. 双向容错：检查前一个引脚 (解决前端把距离注入到 trig_pin 的情况, 如 trig=4, echo=5)
    else if (pin > 0 && s_virtual_ultrasonic_distance[pin - 1] >= 0.0f) {
        distance_cm = s_virtual_ultrasonic_distance[pin - 1];
    }
    // 3. 双向容错：检查后一个引脚 (如 trig=6, echo=5)
    else if (pin < WASM_SIM_MAX_PINS - 1 && s_virtual_ultrasonic_distance[pin + 1] >= 0.0f) {
        distance_cm = s_virtual_ultrasonic_distance[pin + 1];
    }
    // 4. 通道模型检索：尝试插件通道获取距离
    else {
        distance_cm = js_sim_get_plugin_channel("front_radar", "distanceCm");
        if (distance_cm < 0.0f) {
            distance_cm = js_sim_get_plugin_channel("ultrasonic:0", "distanceCm");
        }
    }

    if (distance_cm < 0.0f) {
        return 0; // 未注入，返回 0 模拟无回波
    }

    // HC-SR04 超声波测距公式: 脉宽 (us) = 距离 (cm) * 58.0f
    // 限制在合理范围内 (最远 400cm，对应约 23200us)
    if (distance_cm <= 2.0f) {
        return 116; // 最小限制约 2cm
    }
    if (distance_cm >= 400.0f) {
        return 0; // 超出范围返回 0，模拟超时
    }

    return (uint32_t)(distance_cm * 58.0f);
}
