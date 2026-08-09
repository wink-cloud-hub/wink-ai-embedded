// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_hal_wasm.c
 * @brief Wasm simulation target PAL HAL entry point.
 * @note Logic relocated to modular files per Phase 1 - Phase 4 refactoring proposal v2.0:
 *       - Channel 1 (GPIO): pal_wasm_ch1_gpio.c
 *       - Channel 2 (Bus / I2C): pal_wasm_ch2_bus.c
 *       - Channel 2u (UART RX): pal_wasm_ch2_uart.c
 *       - Channel 2b (PWM): pal_wasm_ch2b_pwm.c
 *       - Channel 3 (ADC): pal_wasm_ch3_adc.c
 *       - Channel 4 (Buffer / WS2812): pal_wasm_ch4_buffer.c
 *       File scheduled for final deprecation/deletion in Phase 5.
 */

#include "pal_hal.h"
#include "wasm_bridge.h"

_Static_assert(sizeof(void*) == 4,
    "wasm64 migration required: see wasm_bridge.h ABI contract #5 "
    "and review every (uint32_t)(uintptr_t) cast in target files / createUnisimImports.ts");
