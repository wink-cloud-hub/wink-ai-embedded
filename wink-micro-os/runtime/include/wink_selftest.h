/**
 * @file wink_selftest.h
 * @brief OS 内建自检框架：一行跑完所有板级 bring-up 自检。
 *
 * 设计原则（ADR-0002 / 2026-07-05 Wave 4）：
 * - selftest 是 OS 子模块（不进 DAL，不进 app 业务层），只依赖 PAL + DAL + Runtime。
 * - 每个自检返回 WINK_OK=PASS、WINK_ERR_UNSUPPORTED=SKIP、其他错误=FAIL。
 * - 通过 glob 模式（"*" / "pwm*" / "*.freq_isolation"）筛选运行。
 * - 命名规范："<subsystem>.<test>"，例如 "pwm_router.freq_isolation"、"rmt.self_loopback"。
 * - 空总线 / 不支持的硬件 → UNSUPPORTED（SKIP），不算 FAIL；
 *   只有"应该支持但结果错"才是 FAIL。
 *
 * 链接说明：selftest 条目通过 X-macro 注册表静态链接（wink_selftest_registry.def），
 * 不需要构造函数或链接器段，ESP32 / host / wasm 三 target 同源。
 */
#ifndef WINK_SELFTEST_H
#define WINK_SELFTEST_H

#include "wink_status.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 单个自检结果。
 */
typedef struct {
    const char    *name;    /**< 点分命名，如 "pwm_router.freq_isolation"。*/
    wink_status_t  status;  /**< WINK_OK=PASS, WINK_ERR_UNSUPPORTED=SKIP, 其他=FAIL。*/
    uint32_t       metric;  /**< 可选数值结果（迭代次数 / 捕获脉宽 / ACK 位图等）。*/
    const char    *note;    /**< 静态字符串说明（可 NULL）。*/
} wink_selftest_result_t;

/**
 * @brief 运行匹配 glob 的自检。
 *
 * @param name_glob   简单通配："*"=全部；"pwm*"=前缀；"*.loopback"=后缀；
 *                    "pwm_router.freq_isolation"=精确匹配；不支持 `?` 和字符类。
 * @param results     调用方提供的结果数组（可为 NULL 且 cap=0 表示只计数）。
 * @param cap         results 容量。
 * @param out_count   [out] 实际匹配并执行的条目数（即使 cap=0 也计数），可 NULL。
 * @return WINK_OK 当所有非 SKIP 条目都 PASS；WINK_ERR_INVALID_ARG 当 name_glob==NULL；
 *         否则返回首个 FAIL 条目的状态码（但仍会继续运行所有匹配项）。
 *
 * @note 本函数是阻塞调用（SMP 自检内部会 spawn 后台任务并等待完成）。
 *       从 app_init 调用，耗时 ESP32 约 60-90s（smp_stress=60s），host/wasm 约 100ms。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_selftest_run(const char *name_glob,
                                wink_selftest_result_t *results,
                                size_t cap,
                                size_t *out_count);

/**
 * @brief 返回总自检条目数（不筛选），便于 app 分配数组。
 */
size_t wink_selftest_count(void);

#ifdef __cplusplus
}
#endif

#endif /* WINK_SELFTEST_H */
