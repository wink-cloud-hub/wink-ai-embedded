// SPDX-License-Identifier: Apache-2.0
/**
 * @file wink_selftest.h
 * @brief OS built-in selftest framework interface.
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
 * @brief Single selftest execution result struct
 */
typedef struct {
    const char    *name;    /**< Dotted name string, e.g. "pwm_router.freq_isolation" */
    wink_status_t  status;  /**< WINK_OK=PASS, WINK_ERR_UNSUPPORTED=SKIP, other=FAIL */
    uint32_t       metric;  /**< Optional numerical result */
    const char    *note;    /**< Static note string (may be NULL) */
} wink_selftest_result_t;

/**
 * @brief Run matching selftests
 *
 * @param[in] name_glob Glob pattern (e.g. "*", "pwm*").
 * @param[out] results Array buffer for results.
 * @param[in] cap Capacity of results array.
 * @param[out] out_count Pointer to store executed test count.
 * @return WINK_OK if all matching tests pass or skip, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_selftest_run(const char *name_glob,
                                wink_selftest_result_t *results,
                                size_t cap,
                                size_t *out_count);

/**
 * @brief Return total number of registered selftests
 * @return Total test count
 */
size_t wink_selftest_count(void);

#ifdef __cplusplus
}
#endif

#endif /* WINK_SELFTEST_H */
