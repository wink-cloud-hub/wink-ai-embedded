// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_pal_log_sys_fallback.c
 * @brief Companion translation unit for test_pal_log_hardening without LOG_TAG.
 */
#include "unity.h"
#include "pal_log.h"

void test_pal_log_sys_fallback_run(void)
{
    LOG_I("sys-fallback message: %d", 1);
    LOG_W("sys-fallback warn");
    LOG_E("sys-fallback err: %s", "probe");
    LOG_D("sys-fallback debug");
    TEST_PASS();
}
