/**
 * @file pal_hal_ultrasonic_wasm.c
 * @brief WASM 仿真平台的超声波传感器 PAL 实现。
 *
 * 由 pal_hal_ultrasonic.c 重命名（契约不变：仅加 _wasm 后缀，
 * 与 pal_hal_wasm.c / pal_irq_wasm.c 等 target-private 文件命名一致）。
 *
 * 通过 wasm_bridge.h 调用 JS 侧的物理模拟函数。此文件仅在 WASM
 * target 编译，ESP32/host 构建完全不可见（同 R-4 discipline，
 * 外层 `#if defined(__EMSCRIPTEN__)` 门控与其它 wasm 文件对齐）。
 *
 * 注意：WASM 仿真模式下引脚号没有实际硬件意义，物理模拟的引脚映射
 *       由 JS 侧统一管理。当前实现中 trigger_pin/echo_pin 参数仅
 *       透传给 JS，以支持未来的多通道虚拟寄存器路由。
 */

#include "hal/pal_ultrasonic.h"
#include "wasm_bridge.h"  /* WASM 特定 JS 导入契约 */

#if defined(__EMSCRIPTEN__)

wink_status_t pal_hal_ultrasonic_init(uint16_t echo_pin) {
    /* WASM 仿真无硬件初始化需求 */
    (void)echo_pin;
    return WINK_OK;
}

wink_status_t pal_hal_ultrasonic_trigger(uint16_t trigger_pin) {
    /* 委托 JS 侧触发超声波时序模拟 */
    js_sim_trigger_ultrasonic(trigger_pin);
    return WINK_OK;
}

wink_status_t pal_hal_ultrasonic_measure_pulse_us(
    uint16_t echo_pin,
    uint32_t timeout_us,
    uint32_t *pulse_us
) {
    if (pulse_us == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    /* 委托 JS 侧返回模拟的 ECHO 脉宽（物理量旁路点，ADR-0003）
     * 超时判定、单位换算仍在 DAL 层与真机同源 */
    (void)timeout_us;
    *pulse_us = js_sim_measure_echo_pulse_us(echo_pin);
    return WINK_OK;
}

void pal_hal_ultrasonic_deinit(void) {
    /* WASM 仿真无硬件资源需要释放 */
}

#endif /* __EMSCRIPTEN__ */
