/**
 * @file pal_atomic_esp32.h
 * @brief ESP32 target-private atomic helpers shared between pal_hal_gpio_esp32.c
 *        and pal_irq_esp32.c (SMP-safe wrapper for GCC __atomic_* builtins).
 *
 * ⚠️ Target-private: this header lives under wink-micro-os/targets/esp32/ and
 * MUST NOT be included from PAL public headers, DAL, runtime, or non-ESP32
 * target TUs. The IDF component includes targets/esp32/ automatically via
 * COMPONENT_DIR, so no additional include_dirs entry is required.
 *
 * Rationale: split out of pal_hal_esp32.c during PLAN-20260701-PAL-TARGET-P1-MAINT
 * Task 2 to break the file into per-subsystem TUs while preserving the
 * previously-shared file-scope Atomic_* inlines byte-for-byte.
 *
 * Also provides esp_memory_barrier(), the compiler/hardware fence used by
 * pal_irq_synchronize() after all in-flight counters drain to zero.
 */
#ifndef WINK_TARGETS_ESP32_PAL_ATOMIC_ESP32_H
#define WINK_TARGETS_ESP32_PAL_ATOMIC_ESP32_H

#include <stdint.h>

#if defined(ESP_PLATFORM)

static inline void esp_memory_barrier(void) {
#if defined(__XTENSA__)
    __asm__ __volatile__("memw" ::: "memory");
#elif defined(__riscv)
    __asm__ __volatile__("fence rw, rw" ::: "memory");
#else
    __asm__ __volatile__("" ::: "memory");
#endif
}

#ifndef Atomic_Load_u32
static inline uint32_t Atomic_Load_u32(volatile uint32_t *pulSource) {
    return *pulSource;
}
#endif

#ifndef Atomic_Increment_u32
static inline uint32_t Atomic_Increment_u32(volatile uint32_t *pulAddend) {
    return __atomic_add_fetch(pulAddend, 1, __ATOMIC_SEQ_CST);
}
#endif

#ifndef Atomic_Decrement_u32
static inline uint32_t Atomic_Decrement_u32(volatile uint32_t *pulAddend) {
    return __atomic_sub_fetch(pulAddend, 1, __ATOMIC_SEQ_CST);
}
#endif

#endif /* ESP_PLATFORM */

#endif /* WINK_TARGETS_ESP32_PAL_ATOMIC_ESP32_H */
