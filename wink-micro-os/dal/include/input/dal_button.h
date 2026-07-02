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
 * @brief 按钮配置结构体（标准化 config_t 模式，便于 Codegen 设备树生成）
 *
 * Phase 2 标准化：所有 DAL 外设统一采用 dal_xxx_config_t + dal_xxx_init(dev, cfg) 模式。
 * 便于代码生成器（app_codegen.py）输出结构化的初始化数据。
 *
 * 成员按对齐降序排列（uint16_t → bool）：自然对齐，无填充。
 */
typedef struct {
    const char *owner;       /* 资源占用 owner 静态字符串（device_tree 实例名，静态存储） */
    uint16_t pin;            /* 逻辑 GPIO 引脚 */
    bool active_low;         /* true: 按下为低电平（常见上拉按钮）；false: 按下为高电平 */
} dal_button_config_t;

/**
 * @brief 按钮逻辑句柄（POD，ADR-0004 静态分发）
 *
 * 内嵌 config 副本，便于：
 *   1. Flash 动态覆写（ADR-0008）：从 Flash blob 读取 → 写入 config → dal_xxx_apply_override
 *   2. 运行时诊断：可直接打印当前生效的配置
 *
 * 成员按对齐降序排列（config_t → bool ×3 → uint8_t）：自然对齐，无填充。
 */
typedef struct {
    dal_button_config_t config; /* 配置副本（pin, active_low），由 init 从 cfg 拷贝 */
    bool stable_pressed;     /* 去抖后的稳定按下状态 */
    bool last_reported;      /* 上次 was_pressed 报告过的状态（边沿消抖） */
    bool initialized;        /* init 成功后置 true */
    uint8_t debounce_counter;/* 连续一致采样计数器 */
} dal_button_t;

/**
 * @brief 初始化按钮：校验引脚、按 active_low 配置上拉/下拉输入、置 initialized。
 *
 * Phase 2 标准化：统一采用 config_t 模式，简化 Codegen 设备树生成。
 * 旧 API（pin + active_low 分离参数）已迁移至此。
 *
 * @param dev 按钮实例句柄
 * @param cfg 配置结构体指针（内部深拷贝到 dev->config）
 * @return wink_status_t
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；cfg 非 NULL。
 *   - Blocking: No。
 *   - Thread-safe: No; ISR-safe: No.
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG(NULL) / 透传 PAL 错误。
 *   - Postconditions: WINK_OK 时 dev->initialized=true；GPIO 方向已配置（真机）；
 *                     cfg 的内容已深拷贝到 dev->config。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_init(dal_button_t *dev, const dal_button_config_t *cfg);

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
