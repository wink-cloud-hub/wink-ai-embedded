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

#if defined(ESP_PLATFORM)
#  include "esp_attr.h"
#  define PAL_IRAM_TEXT     IRAM_ATTR        /* ISR 函数 */
#  define PAL_IRAM_DATA     IRAM_DATA_ATTR   /* ISR 读写数据（非 DMA） */
#  define PAL_IRAM_RODATA   IRAM_DATA_ATTR   /* ISR 只读常量表 */
#  define PAL_DMA_ATTR      WORD_ALIGNED_ATTR DRAM_ATTR /* DMA 描述符 */
#  define PAL_DMA_BUF_ATTR  WORD_ALIGNED_ATTR DRAM_ATTR /* DMA 数据缓冲 */
#else
#  define PAL_IRAM_TEXT
#  define PAL_IRAM_DATA
#  define PAL_IRAM_RODATA
#  define PAL_DMA_ATTR
#  define PAL_DMA_BUF_ATTR
#endif

#endif /* WINK_COMPILER_H */
