// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_spinlock.h
 * @brief Cross-target fine-grained spinlock abstraction.
 *
 * Header-only static inline implementation providing fine-grained spinlocks across:
 * - ESP32: portMUX_TYPE with configASSERT debug guards against ISR misuse.
 * - Host/Wasm: signal barrier degradation with consistent API semantics.
 *
 * Critical Section Rules:
 * - Thread/Task context: must use pal_spinlock_lock / pal_spinlock_unlock.
 * - ISR context: must use pal_spinlock_lock_isr / pal_spinlock_unlock_isr.
 */
#ifndef PAL_SPINLOCK_H
#define PAL_SPINLOCK_H

#include "wink_compiler.h"

#if defined(ESP_PLATFORM)
  #include "freertos/FreeRTOS.h"
  #include "soc/cpu.h"

  typedef portMUX_TYPE pal_spinlock_t;
  #define PAL_SPINLOCK_INITIALIZER   portMUX_INITIALIZER_UNLOCKED

  static inline void pal_spinlock_init(pal_spinlock_t *l) {
      vPortCPUInitializeMutex(l);
  }

  static inline void pal_spinlock_lock(pal_spinlock_t *l) {
      /* Debug guard: pal_spinlock_lock is forbidden in ISR context;
       * ISR must use pal_spinlock_lock_isr, otherwise FreeRTOS critical
       * nesting count mismatch leads to assert/crash. */
      configASSERT(!xPortInIsrContext());
      taskENTER_CRITICAL(l);
  }

  static inline void pal_spinlock_unlock(pal_spinlock_t *l) {
      configASSERT(!xPortInIsrContext());
      taskEXIT_CRITICAL(l);
  }

  /* ISR side variants (must and can only be called in ISR context) */
  static inline void pal_spinlock_lock_isr(pal_spinlock_t *l) {
      taskENTER_CRITICAL_ISR(l);
  }

  static inline void pal_spinlock_unlock_isr(pal_spinlock_t *l) {
      taskEXIT_CRITICAL_ISR(l);
  }

#elif defined(__wasm__) || defined(__unix__) || defined(__APPLE__) || defined(_WIN32)
  typedef struct { char _dummy; } pal_spinlock_t;
  #define PAL_SPINLOCK_INITIALIZER {0}

  static inline void pal_spinlock_init(pal_spinlock_t *l) {
      (void)l;
  }

  static inline void pal_spinlock_lock(pal_spinlock_t *l) {
      (void)l;
#if defined(__GNUC__) || defined(__clang__)
      __atomic_signal_fence(__ATOMIC_ACQUIRE);
#endif
  }

  static inline void pal_spinlock_unlock(pal_spinlock_t *l) {
      (void)l;
#if defined(__GNUC__) || defined(__clang__)
      __atomic_signal_fence(__ATOMIC_RELEASE);
#endif
  }

  static inline void pal_spinlock_lock_isr(pal_spinlock_t *l) {
      pal_spinlock_lock(l);
  }

  static inline void pal_spinlock_unlock_isr(pal_spinlock_t *l) {
      pal_spinlock_unlock(l);
  }
#else
  #error "Define pal_spinlock for this target"
#endif

#endif /* PAL_SPINLOCK_H */
