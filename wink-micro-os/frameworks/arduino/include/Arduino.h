#pragma once

#include "api/ArduinoAPI.h"
#include "hal/pal_hal.h"
#include "osal/pal_osal.h"
#include "WinkHardwareSerial.h"

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
