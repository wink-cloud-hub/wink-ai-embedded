// SPDX-License-Identifier: LGPL-3.0-only
#pragma once

#include "api/ArduinoAPI.h"
#include "hal/pal_hal.h"
#include "osal/pal_osal.h"
#include "WinkHardwareSerial.h"

// Auto-generated board pin aliases (D0..D13, A0..A5, LED_BUILTIN, ...).
// Emitted by wink-tools codegen into the build tree's generated/ include
// directory from boards/<board>.json headers + onboard_devices; see
// boards/README.md §3.5 (wink_board_pins.h generation contract).
// The __has_include guard keeps the framework self-contained when the
// header is absent (older codegen / host-only builds). The generated
// header wraps every macro in #ifndef, so a sketch that defines
// LED_BUILTIN itself still wins.
#if defined(__has_include)
#  if __has_include("wink_board_pins.h")
#    include "wink_board_pins.h"
#  endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Board Support Package (BSP) pin mapping registry
extern const wink_pin_t arduino_pin_map[];
extern const size_t arduino_pin_map_size;

// Wink-specific Arduino lifecycle hooks
void wink_arduino_init(void);

#ifdef __cplusplus
}
#endif
