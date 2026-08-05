#ifndef DAL_RELAY_H
#define DAL_RELAY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 继电器电气拓扑枚举 (Topology Variant - 一等公民)
 */
typedef enum {
    DAL_RELAY_VARIANT_DIRECT_GPIO         = 0, /**< 单 GPIO 直驱/光耦隔离 (经典单线圈, 默认) */
    DAL_RELAY_VARIANT_SSR                 = 1, /**< 固态继电器 (SSR, 零机械触点) */
    DAL_RELAY_VARIANT_LATCHING_DUAL_PIN   = 2, /**< 双线圈磁保持继电器 (双脚脉冲触发, 零静态功耗) */
    DAL_RELAY_VARIANT_LATCHING_SINGLE_PIN = 3, /**< 单线圈 H 桥磁保持继电器 (正反向脉冲) */
} dal_relay_variant_t;

/**
 * @brief 继电器配置结构体 (POD config_t)
 * 成员按对齐降序排列：owner 指针 → uint16_t → int16_t → uint16_t → enum → bool 标志
 */
typedef struct {
    const char *owner;              /**< 资源占用者名称 (DAL-S-001: 必须为首成员指针) */
    uint16_t pin;                   /**< 主控制 / Set 引脚 (DAL-S-006: 必填 uint16_t) */
    int16_t reset_pin;              /**< Reset 引脚 (磁保持拓扑专用; 可选 → int16_t, -1 表示未绑定) */
    uint16_t pulse_duration_ms;     /**< 磁保持脉冲宽度 (ms, 默认 50ms) */
    dal_relay_variant_t variant;    /**< 拓扑变体枚举 (DAL-S-001) */
    bool active_low;                /**< 触发极性: false=高有效, true=低有效 */
    bool initial_state;             /**< init 后的初始状态: true=默认吸合, false=默认断开 */
} dal_relay_config_t;

/**
 * @brief 继电器句柄结构体 (POD instance_t)
 * 支持 Flash 动态覆写 (ADR-0008)
 */
typedef struct {
    dal_relay_config_t config;      /**< 配置副本 (DAL-S-011: 值副本且 offsetof == 0) */
    uint32_t pulse_start_ms;        /**< 磁保持脉冲输出起始时间 (用于非阻塞关脉冲) */
    bool is_on;                     /**< 当前逻辑开关状态: true=吸合/导通, false=断开 */
    bool pulse_active;              /**< 磁保持脉冲是否处于输出中 */
    bool initialized;               /**< 初始化状态标记 (DAL-L-004) */
    volatile wink_status_t last_status; /**< 最近一次操作错误码 (DAL-B-025 可观测性) */
} dal_relay_t;

/* DAL-S-014: 首成员偏移静态断言 */
_Static_assert(offsetof(dal_relay_t, config) == 0, "config must be the first member");

#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_relay_config_t) == 20, "ABI break: config size changed on 32-bit target");
_Static_assert(offsetof(dal_relay_t, initialized) == 26, "ABI break: initialized offset changed on 32-bit");
_Static_assert(sizeof(dal_relay_t) == 32, "ABI break: handle size changed on 32-bit target");
#else                         /* LP64 / LLP64: 64-bit Host Simulation */
_Static_assert(sizeof(dal_relay_config_t) == 24, "ABI break: config size changed on 64-bit host");
_Static_assert(offsetof(dal_relay_t, initialized) == 30, "ABI break: initialized offset changed on 64-bit host");
_Static_assert(sizeof(dal_relay_t) == 40, "ABI break: handle size changed on 64-bit host");
#endif

/**
 * @brief 初始化继电器外设并设置为初始状态
 * @param dev 继电器句柄指针
 * @param cfg 静态配置指针（内部拷贝为值副本）
 * @return WINK_OK 成功；
 *         WINK_ERR_INVALID_ARG 参数为空；
 *         WINK_ERR_ALREADY_INITIALIZED 重复初始化；
 *         WINK_ERR_BUSY GPIO 被其它外设占用
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_init(dal_relay_t *dev, const dal_relay_config_t *cfg);

/**
 * @brief 释放继电器外设资源，并自动断开线圈（安全状态）
 * @param dev 继电器句柄指针
 * @return WINK_OK 成功
 */
wink_status_t dal_relay_deinit(dal_relay_t *dev);

/**
 * @brief 设置继电器开关状态
 * @param dev 继电器句柄指针
 * @param on  true=吸合/导通, false=断开
 * @return WINK_OK 成功；WINK_ERR_NOT_INITIALIZED 未初始化
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_set_state(dal_relay_t *dev, bool on);

/**
 * @brief 吸合/导通继电器
 * @param dev 继电器句柄指针
 * @return WINK_OK 成功
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_turn_on(dal_relay_t *dev);

/**
 * @brief 断开/释放继电器
 * @param dev 继电器句柄指针
 * @return WINK_OK 成功
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_turn_off(dal_relay_t *dev);

/**
 * @brief 翻转继电器开关状态
 * @param dev 继电器句柄指针
 * @return WINK_OK 成功
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_toggle(dal_relay_t *dev);

/**
 * @brief 查询继电器当前是否处于吸合状态
 * @param dev    继电器句柄指针
 * @param out_on 输出状态指针
 * @return WINK_OK 成功
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_is_on(const dal_relay_t *dev, bool *out_on);

/**
 * @brief 轮询继电器脉冲定时器（针对磁保持拓扑，非阻塞清除脉冲）
 * @param dev 继电器句柄指针
 * @return WINK_OK 成功
 */
wink_status_t dal_relay_poll(dal_relay_t *dev);

#ifdef __cplusplus
}
#endif

#if !defined(WINK_USE_RELAY) || !WINK_USE_RELAY
#define WINK_RELAY_DISABLED_MSG \
    "Relay driver not enabled; add a \"relay\" device to wink-app.json " \
    "(or set -DWINK_USE_RELAY=ON)."
WINK_UNAVAILABLE_MSG(WINK_RELAY_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_init(dal_relay_t *dev, const dal_relay_config_t *cfg);
WINK_UNAVAILABLE_MSG(WINK_RELAY_DISABLED_MSG)
wink_status_t dal_relay_deinit(dal_relay_t *dev);
WINK_UNAVAILABLE_MSG(WINK_RELAY_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_set_state(dal_relay_t *dev, bool on);
WINK_UNAVAILABLE_MSG(WINK_RELAY_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_turn_on(dal_relay_t *dev);
WINK_UNAVAILABLE_MSG(WINK_RELAY_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_turn_off(dal_relay_t *dev);
WINK_UNAVAILABLE_MSG(WINK_RELAY_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_toggle(dal_relay_t *dev);
WINK_UNAVAILABLE_MSG(WINK_RELAY_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_is_on(const dal_relay_t *dev, bool *out_on);
WINK_UNAVAILABLE_MSG(WINK_RELAY_DISABLED_MSG)
wink_status_t dal_relay_poll(dal_relay_t *dev);

#endif /* !WINK_USE_RELAY */

#endif /* DAL_RELAY_H */
