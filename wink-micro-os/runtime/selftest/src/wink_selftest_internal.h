// SPDX-License-Identifier: Apache-2.0
/**
 * @file wink_selftest_internal.h
 * @brief Internal declarations for OS selftest suite.
 */
#ifndef WINK_SELFTEST_INTERNAL_H
#define WINK_SELFTEST_INTERNAL_H

#include "wink_selftest.h"

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef wink_status_t (*wink_selftest_fn)(wink_selftest_result_t *result);

wink_status_t wink_selftest_pwm_router_freq_isolation(wink_selftest_result_t *r);
wink_status_t wink_selftest_i2c_bus_scan(wink_selftest_result_t *r);
wink_status_t wink_selftest_smp_resource_stress(wink_selftest_result_t *r);
wink_status_t wink_selftest_gpio_isr_roundtrip(wink_selftest_result_t *r);
wink_status_t wink_selftest_rmt_self_loopback(wink_selftest_result_t *r);

#ifdef __cplusplus
}
#endif

#endif /* WINK_SELFTEST_INTERNAL_H */
