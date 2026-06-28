#ifndef DAL_BUTTON_H
#define DAL_BUTTON_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 连续一致采样阈值：达此计数后稳定态翻转（3 × tick 间隔 ≈ 30ms @ 10ms tick） */
#define DAL_BUTTON_DEBOUNCE_THRESHOLD 3

/**
 * @brief 按钮逻辑句柄（POD，ADR-0004 静态分发）
 *
 * 成员按对齐需求降序排列：uint16_t → bool ×4 → uint8_t。
 */
typedef struct {
    uint16_t pin;            /* 逻辑 GPIO 引脚 */
    bool active_low;         /* true: 按下为低电平（常见上拉按钮）；false: 按下为高电平 */
    bool stable_pressed;     /* 去抖后的稳定按下状态 */
    bool last_reported;      /* 上次 was_pressed 报告过的状态（边沿消抖） */
    bool initialized;        /* init 成功后置 true */
    uint8_t debounce_counter;/* 连续一致采样计数器 */
} dal_button_t;

/**
 * @brief 初始化按钮：校验引脚、按 active_low 配置上拉/下拉输入、置 initialized。
 * @param dev 按钮实例句柄
 * @param pin 逻辑 GPIO 引脚
 * @param active_low true=按下为低电平（常见接法，内部上拉）；false=按下为高电平（内部下拉）
 * @return wink_status_t
 * @note API Contract:
 *   - Preconditions: dev 非 NULL。
 *   - Blocking: No。
 *   - Thread-safe: No; ISR-safe: No.
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG(NULL) / 透传 PAL 错误。
 *   - Postconditions: WINK_OK 时 dev->initialized=true；GPIO 方向已配置（真机）。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_init(dal_button_t *dev, uint16_t pin, bool active_low);

/**
 * @brief 每 tick 采样并跑计数式去抖状态机（非阻塞）。
 * @note 由 App app_loop 每周期调用一次；驱动内部维护计数器，不对外暴露 poll 接口。
 *       去抖阈值 DAL_BUTTON_DEBOUNCE_THRESHOLD（≈30ms @ 10ms tick）。
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；dal_button_init() 已成功。
 *   - Blocking: No。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_poll(dal_button_t *dev);

/**
 * @brief 读取去抖后的稳定按下状态
 * @param out_pressed 输出：true=已按下；false=未按下
 * @note API Contract:
 *   - Preconditions: dev/out_pressed 非 NULL；dal_button_init() 已成功。
 *   - Blocking: No。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_is_pressed(const dal_button_t *dev, bool *out_pressed);

/**
 * @brief 检测「按下」边沿事件（按下瞬间触发一次，读后清）。
 * @param out_was_pressed 输出：true=自上次调用后发生了按下事件；false=无新按下事件
 * @note 与 is_pressed 的区别：was_pressed 只在稳定态从「未按下」→「按下」时返回 true 一次，
 *       适用于触发单次动作（如切换模式）；is_pressed 返回当前持续状态，适用于按住动作。
 * @note API Contract:
 *   - Preconditions: dev/out_was_pressed 非 NULL；dal_button_init() 已成功。
 *   - Blocking: No。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_was_pressed(dal_button_t *dev, bool *out_was_pressed);

#ifdef __cplusplus
}
#endif

#endif /* DAL_BUTTON_H */
