/* wink_test_compat.h
 * Test compatibility macros for contract-guard tests calling deprecated
 * blocking APIs (e.g. pal_os_sleep_ms, pal_os_mutex_lock).
 *
 *   WINK_TEST_ALLOW_DEPRECATED        (file-level, ON for whole TU)
 *     Suppresses -Wdeprecated-declarations (GCC/Clang) or C4996 (MSVC)
 *     for the rest of the translation unit.
 *
 *   WINK_TEST_ALLOW_DEPRECATED_BEGIN / _END   (scope-level push/pop)
 *     For use around a specific call site when only one call should be
 *     permitted to trigger the deprecation.
 *
 * Spec: spec v3.5.0 §11.3 "Test Exemption from Blocking Deprecation"
 *       (proposed).
 */
#ifndef WINK_TEST_COMPAT_H
#define WINK_TEST_COMPAT_H

#if defined(_MSC_VER)
#  define WINK_TEST_ALLOW_DEPRECATED \
        __pragma(warning(disable: 4996))
#  define WINK_TEST_ALLOW_DEPRECATED_BEGIN \
        __pragma(warning(push))             \
        __pragma(warning(disable: 4996))
#  define WINK_TEST_ALLOW_DEPRECATED_END   \
        __pragma(warning(pop))
#elif defined(__GNUC__) || defined(__clang__)
#  define WINK_TEST_ALLOW_DEPRECATED \
        _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#  define WINK_TEST_ALLOW_DEPRECATED_BEGIN \
        _Pragma("GCC diagnostic push")      \
        _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#  define WINK_TEST_ALLOW_DEPRECATED_END   \
        _Pragma("GCC diagnostic pop")
#else
#  define WINK_TEST_ALLOW_DEPRECATED
#  define WINK_TEST_ALLOW_DEPRECATED_BEGIN
#  define WINK_TEST_ALLOW_DEPRECATED_END
#endif

#endif /* WINK_TEST_COMPAT_H */
