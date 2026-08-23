// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_pcnt.h
 * @brief PAL PCNT (Pulse Counter) quadrature encoder hardware subsystem interface.
 *
 * Provides hardware-accelerated counting for rotary encoders, optical wheel encoders,
 * and high-frequency pulse trains without CPU interrupt overhead.
 *
 * Architecture Notes:
 * - 64-bit Accumulator Contract: ESP32 hardware PCNT has 16-bit signed registers (-32768..32767).
 *   The driver automatically manages high/low limit threshold ISRs to maintain a seamless
 *   64-bit software counter (count_out).
 * - Glitch Filter (E-001): ESP32 classic hardware filter supports pulses up to 1023 APB clock cycles (~12.75us).
 *   filter_ns will be clamped to hardware capability.
 * - Resource Arbitration: Each unit claims PAL_RESOURCE_PCNT_UNIT and pins claim PAL_RESOURCE_GPIO_PIN.
 */
#ifndef PAL_PCNT_H
#define PAL_PCNT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"
#include "hal/pal_pin_types.h"
#include "wink_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PAL_PCNT_MODE_1X = 1, /**< Count on rising edge of signal A */
    PAL_PCNT_MODE_2X = 2, /**< Count on rising & falling edges of signal A */
    PAL_PCNT_MODE_4X = 4, /**< Quadrature 4X decoding (both edges of signal A and signal B) */
} pal_pcnt_mode_t;

typedef struct {
    wink_pin_t      pin_a;         /**< Phase A / Pulse input pin */
    wink_pin_t      pin_b;         /**< Phase B / Direction input pin (WINK_PIN_NC if single pulse counter) */
    pal_pcnt_mode_t mode;          /**< Quadrature / counting mode */
    int16_t         low_limit;     /**< Hardware lower threshold (default -32768) */
    int16_t         high_limit;    /**< Hardware upper threshold (default 32767) */
    uint32_t        filter_ns;     /**< Glitch filter duration in nanoseconds (0 to disable) */
} pal_pcnt_config_t;

typedef struct pal_pcnt_unit_s *pal_pcnt_unit_handle_t;

/**
 * @brief Initialize a hardware pulse counter unit.
 * @param[in] cfg Configuration parameters
 * @param[out] out_handle Output unit handle
 * @return WINK_OK on success,
 *         WINK_ERR_RESOURCE_EXHAUSTED if all PCNT units are busy,
 *         WINK_ERR_INVALID_ARG on invalid parameters
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_pcnt_init(const pal_pcnt_config_t *cfg,
                            pal_pcnt_unit_handle_t *out_handle);

/**
 * @brief Deinitialize and release a pulse counter unit.
 * @param[in] handle Unit handle
 * @return WINK_OK on success
 */
wink_status_t pal_pcnt_deinit(pal_pcnt_unit_handle_t handle);

/**
 * @brief Read the accumulated 64-bit count.
 * @param[in] handle Unit handle
 * @param[out] count_out Pointer to receive the 64-bit signed count
 * @return WINK_OK on success
 */
wink_status_t pal_pcnt_get_count(pal_pcnt_unit_handle_t handle, int64_t *count_out);

/**
 * @brief Reset the counter value to zero.
 * @param[in] handle Unit handle
 * @return WINK_OK on success
 */
wink_status_t pal_pcnt_clear(pal_pcnt_unit_handle_t handle);

/**
 * @brief Configure or update glitch filter duration.
 * @param[in] handle Unit handle
 * @param[in] filter_ns Glitch filter duration in nanoseconds
 * @return WINK_OK on success
 */
wink_status_t pal_pcnt_set_glitch_filter(pal_pcnt_unit_handle_t handle, uint32_t filter_ns);

#ifdef __cplusplus
}
#endif

#endif /* PAL_PCNT_H */
