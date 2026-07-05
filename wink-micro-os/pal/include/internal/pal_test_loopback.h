#ifndef PAL_TEST_LOOPBACK_H
#define PAL_TEST_LOOPBACK_H

/**
 * @file pal_test_loopback.h
 * @brief Internal test-only API for hardware signal loopback.
 *
 * This header is NOT installed to the public include path. Only selftest
 * modules and PAL unit tests may include it. App code and DAL drivers
 * must never depend on these symbols.
 *
 * Copyright (c) 2026 Wink-AI. Internal use only.
 */

#include "wink_status.h"
#include <stdint.h>
#include "pal_hal.h"  /* for wink_pin_t */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enable hardware signal loopback (pin_out → pin_in) for test.
 *
 * On ESP32 this programs the GPIO matrix; on host/wasm it creates a
 * virtual wire in the simulation. Used by RMT self-loopback selftest
 * and GPIO ISR round-trip tests.
 *
 * @param pin_out  Source output pin
 * @param pin_in   Sink input pin
 * @return WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_UNSUPPORTED
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_test_enable_hardware_loopback(wink_pin_t pin_out, wink_pin_t pin_in);

/**
 * @brief Disable a previously-enabled loopback.
 */
wink_status_t pal_test_disable_hardware_loopback(wink_pin_t pin_out, wink_pin_t pin_in);

#ifdef __cplusplus
}
#endif

#endif /* PAL_TEST_LOOPBACK_H */
