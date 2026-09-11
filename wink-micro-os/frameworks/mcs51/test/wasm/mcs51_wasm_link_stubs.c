// SPDX-License-Identifier: Apache-2.0
/**
 * @file mcs51_wasm_link_stubs.c
 * @brief Link stubs for the M1 mcs51 wasm blinky test.
 *
 * The focused mcs51 build links the fiber scheduler + runtime but deliberately
 * omits the pal_wasm channel implementations (pal_wasm_ch*.c): they pull in the
 * full GPIO/I2C/SPI/UART/ADC data plane, BAL ultrasonic event hooks, and a long
 * tail of js_ data-plane imports that the bounded blinky test never exercises.
 *
 * pal_wasm_degradation.c references each channel's *_reset() hook on the
 * degradation path; provide no-op definitions so the link closes. Mirrors the
 * adc_wasm_link_stubs.c pattern used by test/wasm/pal_adc.
 */
#include <stdint.h>

void pal_wasm_ch1_gpio_reset(void) {}
void pal_wasm_ch2_bus_reset(void) {}
void pal_wasm_ch2_uart_reset(void) {}
void pal_wasm_ch1b_pwm_reset(void) {}
void pal_wasm_ch3_adc_reset(void) {}
void pal_wasm_ch4_buffer_reset(void) {}
