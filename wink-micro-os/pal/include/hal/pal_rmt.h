/**
 * @file pal_rmt.h
 * @brief PAL 通用脉冲捕获（pulse capture）API。
 *
 * 提供一个硬件无关的非阻塞脉冲宽度测量原语。原名 "pal_rmt" 保留自
 * ESP32 上的 RMT (Remote Control Transceiver) 外设——RMT 目前是本 API
 * 在真机端的实现后端，但语义已泛化为通用 pulse capture：给定一个数字输入
 * 引脚和触发沿类型，测量下一次沿-到-反向沿的脉冲宽度（微秒）。
 *
 * 典型用途：
 *   - 超声波（HC-SR04）ECHO 脉宽（PAL_RMT_EDGE_RISING）
 *   - 红外接收器（IR receiver）解码
 *   - 增量编码器脉冲宽度（占空比测量）
 *
 * ⚠️ 单实例语义：当前实现只支持同时存在一路 pulse-capture 通道
 *   （静态单例设计）。如需并发多路捕获，需在未来 ADR 中扩展。
 *
 * ⚠️ 平台支持矩阵：
 *   - ESP32: 实现完整（RMT RX channel）
 *   - Wasm/Host: 当前无独立实现；pulse 捕获在这两个 target 上直接经由
 *     pal_gpio_pulse_in 完成，此 API 在这两个 target 上以 stub 返回
 *     WINK_ERR_UNSUPPORTED（is_active 返回 false）。
 */

#ifndef PAL_RMT_H
#define PAL_RMT_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"
#include "pal_hal.h"      /* wink_pin_t */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 脉冲捕获起始沿方向。
 *
 * 决定 pulse capture 通道以哪种电平跳变作为脉宽测量的起点：
 *   - RISING  : 上升沿开始计时，到下一次下降沿结束（测量高电平脉宽）
 *   - FALLING : 下降沿开始计时，到下一次上升沿结束（测量低电平脉宽）
 */
typedef enum {
    PAL_RMT_EDGE_RISING  = 0,   /* 上升沿起，测量到下一次下降沿——高电平脉宽 */
    PAL_RMT_EDGE_FALLING = 1,   /* 下降沿起，测量到下一次上升沿——低电平脉宽 */
} pal_rmt_edge_t;

/**
 * @brief 初始化 pulse-capture 通道并绑定到指定输入引脚。
 *
 * @param pin        脉冲输入引脚编号（wink_pin_t，含 -1 无效值语义）
 * @param start_edge 起始沿类型（RISING / FALLING）
 * @return WINK_OK 成功；其它错误码失败
 *
 * @note 单实例：若通道已绑定到同一 pin，返回 WINK_OK（幂等）；若绑定到
 *   其它 pin，本次调用会先 deinit 旧绑定再重建到新 pin。
 * @note 平台限制：某些后端（如 ESP32 RMT v5.x）目前不直接暴露"起始沿选择"
 *   的硬件配置，实现层可能忽略 start_edge 并按上升沿起（同 HC-SR04 惯例）
 *   处理——此为 TODO，未来 ADR 化后再修正。调用方仍应按语义正确传入。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_rmt_pulse_capture_init(wink_pin_t pin, pal_rmt_edge_t start_edge);

/**
 * @brief 等待一次脉冲捕获完成并返回脉宽（微秒）。
 *
 * 该函数不驱动信号源，仅被动等待起始沿→反向沿事件完成；发送方（如
 * HC-SR04 的 TRIG 或 IR 发射端）需由调用方另行触发。
 *
 * @param timeout_us    超时时间（微秒）；超时未捕获返回 WINK_ERR_TIMEOUT
 * @param pulse_us_out  输出脉宽（微秒），入口置 0，失败时保持 0
 * @return WINK_OK 测量成功；WINK_ERR_TIMEOUT 超时；WINK_ERR_INVALID_ARG
 *   通道未初始化或参数非法；其它错误码硬件失败
 *
 * @note 阻塞语义：本函数会阻塞调用线程直至捕获完成或超时；在真机上依赖
 *   FreeRTOS 信号量，不消耗 CPU。10ms tick 上下文应避免长 timeout_us。
 *
 * 使用方法（以 HC-SR04 为例）：
 *   pal_gpio_write(trig_pin, true);
 *   pal_os_busy_wait_us(10);
 *   pal_gpio_write(trig_pin, false);
 *   uint32_t pulse_us;
 *   if (pal_rmt_pulse_capture_wait(30000, &pulse_us) == WINK_OK) {
 *       float distance_mm = (float)pulse_us * 0.343f / 2.0f;
 *   }
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_rmt_pulse_capture_wait(uint32_t timeout_us, uint32_t *pulse_us_out);

/**
 * @brief 反初始化 pulse-capture 通道并释放其占用的外设资源。
 *
 * 幂等：未初始化时调用是 no-op。
 */
void pal_rmt_pulse_capture_deinit(void);

/**
 * @brief 查询 pulse-capture 通道当前是否已初始化。
 *
 * @return true 已初始化；false 未初始化
 *
 * @note 用于 pal_gpio_pulse_in 侧的 fast-path 判定（决定是否需要走 RMT
 *   路径还是 busy-wait 回退）。
 */
bool pal_rmt_pulse_capture_is_active(void);

#ifdef __cplusplus
}
#endif

#endif /* PAL_RMT_H */
