// SPDX-License-Identifier: Apache-2.0
#ifndef WINK_BLOCKING_REGION_H
#define WINK_BLOCKING_REGION_H

/**
 * @file wink_blocking_region.h
 * @brief Deprecated-warning suppression macros for legitimate blocking-call regions (ADR-0025).
 */

#ifdef __cplusplus
extern "C" {
#endif

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

#define WINK_INIT_BLOCKING_REGION_BEGIN  WINK_INTERNAL_BLOCKING_REGION_BEGIN
#define WINK_INIT_BLOCKING_REGION_END    WINK_INTERNAL_BLOCKING_REGION_END

#ifdef __cplusplus
}
#endif

#endif /* WINK_BLOCKING_REGION_H */
