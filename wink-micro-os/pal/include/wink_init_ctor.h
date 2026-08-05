// SPDX-License-Identifier: Apache-2.0
/**
 * @file wink_init_ctor.h
 * @brief Cross-platform "run before main()" constructor macro.
 *
 *   WINK_CONSTRUCTOR(func)  -  func() runs at program startup, before main().
 *
 *   GCC/Clang: __attribute__((constructor))
 *   MSVC:      .CRT$XCU section + volatile function pointer to defeat /OPT:REF.
 *
 * Spec: ADR-0060 cross-platform constructor convention.
 */
#ifndef WINK_INIT_CTOR_H
#define WINK_INIT_CTOR_H

#if defined(_MSC_VER)
#  pragma section(".CRT$XCU", read)
#  define WINK_CONSTRUCTOR(func)                                       \
        static void func##_winkctor_impl_(void);                        \
        __declspec(allocate(".CRT$XCU"))                               \
        static void (* volatile func##_winkctor_ptr_)(void) =          \
            func##_winkctor_impl_;                                     \
        static void func##_winkctor_impl_(void)
#elif defined(__GNUC__) || defined(__clang__)
#  define WINK_CONSTRUCTOR(func)                                       \
        __attribute__((constructor)) static void func(void)
#else
#  error "WINK_CONSTRUCTOR: unsupported compiler platform"
#endif

#endif /* WINK_INIT_CTOR_H */
