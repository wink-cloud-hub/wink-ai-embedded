/**
 * @file wink_selftest_internal.h
 * @brief Selftest 模块内部声明：每个 selftest 条目的函数原型。
 *
 * 本头不对外暴露；selftest 源文件 include 本头，runtime include 目录下只有
 * <wink_selftest.h> 作为公共 API。
 */
#ifndef WINK_SELFTEST_INTERNAL_H
#define WINK_SELFTEST_INTERNAL_H

#include "wink_selftest.h"

/* ADR-0017 层 1 例外：selftest 是 OS 内部 bring-up 测试代码，合法调用
 * WINK_BLOCKING API（sleep/task_create/sem_take/i2c_scan/rmt_wait）。
 * 抑制 -Wdeprecated-declarations 使 -Werror 下仍能编译；严格模式
 * (-DWINK_STRICT_NONBLOCKING=1) 下 blocking API 声明直接消失，本文件
 * 会链接失败——那是设计意图（selftest 不进 STRICT 非阻塞镜像）。*/
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* 每个 selftest 条目的函数签名：
 *   入参：result 指针（name/metric/note 由条目自己填写，status 由返回值推断）
 *   返回：WINK_OK=PASS, WINK_ERR_UNSUPPORTED=SKIP, 其他=FAIL
 *   约定：调用方已将 result->name 指向注册表中的字符串名字；
 *         条目只填 metric/note 字段即可。*/
typedef wink_status_t (*wink_selftest_fn)(wink_selftest_result_t *result);

/* ── 各 selftest 条目原型（与 registry.def 对应） ── */
wink_status_t wink_selftest_pwm_router_freq_isolation(wink_selftest_result_t *r);
wink_status_t wink_selftest_i2c_bus_scan(wink_selftest_result_t *r);
wink_status_t wink_selftest_smp_resource_stress(wink_selftest_result_t *r);
wink_status_t wink_selftest_gpio_isr_roundtrip(wink_selftest_result_t *r);
wink_status_t wink_selftest_rmt_self_loopback(wink_selftest_result_t *r);

#ifdef __cplusplus
}
#endif

#endif /* WINK_SELFTEST_INTERNAL_H */
