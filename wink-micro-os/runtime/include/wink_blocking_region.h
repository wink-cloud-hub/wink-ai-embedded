#ifndef WINK_BLOCKING_REGION_H
#define WINK_BLOCKING_REGION_H

/**
 * @file wink_blocking_region.h
 * @brief Semantic deprecated-warning suppression macros for legitimate
 *        blocking-call regions (ADR-0025).
 *
 * Two macro pairs are provided, both expand to the same compiler pragma
 * sequence (push + disable-deprecated + pop). They have DISTINCT names
 * on purpose — code review and grep must immediately tell apart:
 *
 *   WINK_INTERNAL_BLOCKING_REGION_BEGIN/END
 *     — BAL/Runtime **internal** .c files (file-scope, after #includes).
 *       Use when the whole TU legitimately calls WINK_BLOCKING APIs
 *       (e.g. wink_sonar_helper.c MAY_BLOCK task body calling
 *       dal_ultrasonic_request_measurement).
 *       Mandatory accompanying comment (use C++-style // comment so the
 *       block-comment opener cannot be nested; prefix with "ADR-0017 ..."):
 *         // ADR-0017 BAL-exception: helper 内部通过 wink_periodic MAY_BLOCK
 *         //   路径调用 WINK_BLOCKING API.
 *
 *   WINK_INIT_BLOCKING_REGION_BEGIN/END
 *     — Application layer **small blocks** inside app_init_status() /
 *       app_on_fault_status() (push/pop minimal scope around a handful of
 *       statements). Use for one-off bringup/selftest/I2C-scan calls that
 *       run during synchronous startup (NOT in a PT cooperative context).
 *       Mandatory accompanying comment:
 *         // ADR-0017 init-phase exception: selftest 在同步启动阶段运行，
 *         //   不在 cooperative PT 上下文，允许阻塞调用.
 *
 * Hard rules (CI/review gated — see ADR-0025 §6):
 *   * App business callbacks (on_xxx_click, event handlers, app_loop,
 *     sensor on_data) MUST NOT use ANY of these macros — a deprecated
 *     warning there is a real bug and must be fixed by routing through
 *     BAL / wink_periodic MAY_BLOCK.
 *   * These macros MUST be placed AFTER all #include lines (file-scope
 *     variant) so the suppression does NOT leak into PAL/DAL headers.
 *   * The BEGIN/END pairs must be properly nested and matched.
 *
 * Portability: GCC/Clang use _Pragma("GCC diagnostic ..."); MSVC uses
 * __pragma(warning(push/disable:4996/pop)); other compilers get empty
 * definitions (the call still compiles; warnings on unrecognized
 * compilers are not suppressed — YAGNI for now).
 *
 * Copyright (c) 2026 Wink-AI.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ── (A) BAL/Runtime internal: file-scope suppression (top of BAL .c files) ── */
#if defined(__GNUC__) || defined(__clang__)
#  define WINK_INTERNAL_BLOCKING_REGION_BEGIN \
    _Pragma("GCC diagnostic push") \
    _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#  define WINK_INTERNAL_BLOCKING_REGION_END \
    _Pragma("GCC diagnostic pop")
#elif defined(_MSC_VER)
#  define WINK_INTERNAL_BLOCKING_REGION_BEGIN \
    __pragma(warning(push)) \
    __pragma(warning(disable:4996))
#  define WINK_INTERNAL_BLOCKING_REGION_END \
    __pragma(warning(pop))
#else
#  define WINK_INTERNAL_BLOCKING_REGION_BEGIN
#  define WINK_INTERNAL_BLOCKING_REGION_END
#endif

/* ── (B) Application layer: small init-phase exception block
 *       (wrapping selftest / I2C-scan / fault-time stop calls inside
 *       app_init_status() / app_on_fault_status()).
 *       Same expansion as (A) — separate name for code-review/grep
 *       honesty (ADR-0025 §1). ──────────────────────────────────── */
#define WINK_INIT_BLOCKING_REGION_BEGIN  WINK_INTERNAL_BLOCKING_REGION_BEGIN
#define WINK_INIT_BLOCKING_REGION_END    WINK_INTERNAL_BLOCKING_REGION_END

#ifdef __cplusplus
}
#endif

#endif /* WINK_BLOCKING_REGION_H */
