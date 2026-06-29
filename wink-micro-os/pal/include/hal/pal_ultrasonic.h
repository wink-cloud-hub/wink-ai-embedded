/**
 * @file pal_ultrasonic.h
 * @brief 超声波传感器 PAL 接口（平台无关契约）
 *
 * 所有平台（Wasm / ESP32 / STM32 / host）都必须实现这些接口。
 * DAL 层只依赖这个头文件，不感知任何平台细节。
 *
 * 设计原则：
 * 1. 零平台依赖：只使用 pal_hal.h 定义的基础类型
 * 2. 最小接口集：只暴露 DAL 层真正需要的功能
 * 3. 前向兼容：新增平台不需要修改此文件
 * 4. 静态分发：各平台在各自 targets/ 目录下实现，编译期链接
 *
 * 代码复用说明（各平台实现策略）：
 * - WASM：直接委托 js_sim_* 旁路桥接函数
 * - ESP32：复用 pal_rmt_ultrasonic_* 硬件捕获（优先）或 pal_gpio_pulse_in
 * - host：使用 js_sim_host_stub.c 的注入桩函数
 */

#pragma once
#include "pal_hal.h"      /* uint16_t 等基础类型 */
#include "wink_status.h"  /* 统一错误码 WINK_OK / WINK_ERR_* */

/**
 * @brief 初始化超声波传感器硬件（可选，平台特定）
 *
 * 对于支持硬件脉冲捕获的平台（如 ESP32 RMT），此函数初始化硬件。
 * 对于不需要初始化的平台（如 WASM 仿真），此函数返回 WINK_OK 即可。
 *
 * @param echo_pin 回波引脚号
 * @return wink_status_t WINK_OK 表示成功
 */
wink_status_t pal_hal_ultrasonic_init(uint16_t echo_pin);

/**
 * @brief 触发超声波测量（TRIG 时序）
 *
 * 向 TRIG 引脚输出 10us 高电平脉冲，启动超声波发射。
 * WASM 仿真下委托给 js_sim_trigger_ultrasonic()。
 *
 * @param trigger_pin 触发引脚号
 * @return wink_status_t WINK_OK 表示成功
 */
wink_status_t pal_hal_ultrasonic_trigger(uint16_t trigger_pin);

/**
 * @brief 测量 ECHO 回波脉宽（同步阻塞）
 *
 * 测量 ECHO 引脚的高电平持续时间（微秒）。测量失败或超时时
 * 返回 0。各平台实现策略：
 * - ESP32：RMT 硬件捕获（优先）或 GPIO busy-wait
 * - WASM：委托 js_sim_measure_echo_pulse_us() 物理模拟旁路
 * - host：js_sim_host_stub.c 注入返回预设值
 *
 * @param echo_pin 回波引脚号
 * @param timeout_us 超时时间（微秒），建议 30000us
 * @param pulse_us 输出参数，返回测量到的脉宽（微秒）
 * @return wink_status_t WINK_OK 表示测量成功，WINK_ERR_TIMEOUT 表示超时
 */
wink_status_t pal_hal_ultrasonic_measure_pulse_us(
    uint16_t echo_pin,
    uint32_t timeout_us,
    uint32_t *pulse_us
);

/**
 * @brief 反初始化超声波硬件（可选）
 *
 * 释放硬件资源（如 RMT 通道）。不需要硬件反初始化的平台
 * 此函数可留空。
 */
void pal_hal_ultrasonic_deinit(void);
