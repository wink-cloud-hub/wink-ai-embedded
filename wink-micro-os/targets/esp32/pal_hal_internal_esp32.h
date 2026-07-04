/**
 * @file pal_hal_internal_esp32.h
 * @brief ESP32 target-private cross-TU declarations.
 *
 * ⚠️ Target-private: this header lives under wink-micro-os/targets/esp32/ and
 * MUST NOT be included from PAL public headers, DAL, runtime, or non-ESP32
 * target TUs. It only exposes functions used across the ESP32 sibling TUs
 * (pal_hal_esp32.c, pal_hal_gpio_esp32.c, pal_hal_pwm_esp32.c,
 * pal_hal_i2c_esp32.c, pal_irq_esp32.c) after the split introduced by
 * PLAN-20260701-PAL-TARGET-P1-MAINT Task 2.
 *
 * Motivation: pal_irq_esp32.c owns s_irq_in_flight[] and implements
 * pal_irq_synchronize(). When invoked with ~0U it must also drain GPIO ISRs
 * whose in-flight counters (s_gpio_irq_in_flight[]) live inside
 * pal_hal_gpio_esp32.c. Rather than exposing the array itself across TUs
 * (breaks encapsulation), the GPIO TU exports a single synchronise-all
 * function that the IRQ TU can call. Keeps s_gpio_irq_in_flight[] file-local
 * to pal_hal_gpio_esp32.c and preserves R-5 write-path semantics unchanged.
 */
#ifndef WINK_TARGETS_ESP32_PAL_HAL_INTERNAL_ESP32_H
#define WINK_TARGETS_ESP32_PAL_HAL_INTERNAL_ESP32_H

#include <stdint.h>

#if defined(ESP_PLATFORM)

/**
 * @brief Busy-wait until every GPIO ISR has completed on all cores.
 *
 * Iterates over the GPIO in-flight counter table maintained by
 * pal_hal_gpio_esp32.c and returns once every pin's counter has been observed
 * at 0 (or the per-pin timeout has elapsed, in which case a warning is
 * logged and iteration continues to the next pin — matches the pre-split
 * semantics of the ~0U branch in pal_irq_synchronize()).
 *
 * @param timeout_us  Per-pin busy-wait timeout in microseconds.
 */
void pal_esp32_gpio_synchronize_all(uint64_t timeout_us);

#endif /* ESP_PLATFORM */

#endif /* WINK_TARGETS_ESP32_PAL_HAL_INTERNAL_ESP32_H */
