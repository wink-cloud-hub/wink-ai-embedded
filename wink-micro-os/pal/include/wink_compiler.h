// SPDX-License-Identifier: Apache-2.0
/**
 * @file wink_compiler.h
 * @brief Cross-compiler abstraction macros.
 *
 *   WINK_WEAK
 *     GCC/Clang: __attribute__((weak))  - real weak symbol override.
 *     MSVC:      __declspec(selectany)  - COMDAT folding.
 *
 *   WINK_WEAK_ALIAS(weak_func, default_func)
 *     GCC/Clang: no-op (weak_func is its own weak symbol; user may override).
 *     MSVC:      emits a linker /alternatename directive.
 *
 * Spec: ADR-0059 cross-platform weak symbol convention.
 */
#ifndef WINK_COMPILER_H
#define WINK_COMPILER_H

#if defined(__GNUC__) || defined(__clang__)
#  define WINK_WEAK __attribute__((weak))
#  define WINK_WEAK_ALIAS(weak_func, default_func) /* no-op on GCC/Clang */
#elif defined(_MSC_VER)
#  define WINK_WEAK
#  define WINK_WEAK_ALIAS(weak_func, default_func)                  \
        __pragma(comment(linker,                                    \
            "/alternatename:" #weak_func "=" #default_func))
#else
#  define WINK_WEAK
#  define WINK_WEAK_ALIAS(weak_func, default_func)
#endif

#endif /* WINK_COMPILER_H */
