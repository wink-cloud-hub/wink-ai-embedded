/**
 * @file wasm_dev_ultrasonic.c
 * @brief Wasm 仿真侧 HC-SR04 超声波虚拟外设模型 (C-side Model)。
 */
#include "wasm_sim_registry.h"
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#define WASM_SIM_MAX_PINS 40

// 记录每个 GPIO 引脚作为 Echo 引脚时的虚拟物理距离 (cm)
static float s_virtual_ultrasonic_distance[WASM_SIM_MAX_PINS];

void wasm_dev_ultrasonic_reset(void) {
    for (int i = 0; i < WASM_SIM_MAX_PINS; i++) {
        s_virtual_ultrasonic_distance[i] = -1.0f; // -1.0f 表示未注入，走 JS 旁路 fallback
    }
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

    float distance_cm = s_virtual_ultrasonic_distance[pin];
    if (distance_cm < 0.0f) {
        return 0; // 未注入，返回 0 触发 JS fallback
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
