/* wink_compiler.h
 * Cross-compiler abstraction macros.
 *
 *   WINK_WEAK
 *     GCC/Clang: __attribute__((weak))  — real weak symbol override.
 *     MSVC:      __declspec(selectany)  — COMDAT folding (linker picks one
 *                among identical definitions; NOT a true weak override).
 *                When WINK_WEAK is used on a definition that may be
 *                overridden by a strong symbol with a different body on
 *                MSVC, the override is only reliably honored if the strong
 *                symbol is referenced from a TU compiled with /INCLUDE.
 *                Most app-level overrides work via /alternatename; see
 *                WINK_WEAK_ALIAS below.
 *
 *   WINK_WEAK_ALIAS(weak_func, default_func)
 *     GCC/Clang: no-op (weak_func is its own weak symbol; user may override).
 *     MSVC:      emits a linker /alternatename directive so any unresolved
 *                reference to weak_func is redirected to default_func at
 *                link time.
 *
 * Spec: ADR-0059 cross-platform weak symbol (proposed).
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
