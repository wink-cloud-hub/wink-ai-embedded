// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_wasm_hwtimer.h
 * @brief Wasm target hardware timer virtual clock drain interface.
 */
#ifndef PAL_WASM_HWTIMER_H
#define PAL_WASM_HWTIMER_H

#include "hal/pal_hwtimer.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PAL_WASM_HWTIMER_MAX_CATCHUP 64

/**
 * @brief Drain all pending hardware timers whose fire timestamps have arrived.
 */
void pal_wasm_hwtimer_drain(void);

#ifdef __cplusplus
}
#endif

#endif /* PAL_WASM_HWTIMER_H */
