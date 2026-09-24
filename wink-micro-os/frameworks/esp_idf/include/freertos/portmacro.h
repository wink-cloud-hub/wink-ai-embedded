/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef PORTMACRO_H
#define PORTMACRO_H

#include <stdint.h>
#include <stddef.h>
#include "freertos/FreeRTOSConfig.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t BaseType_t;
typedef uint32_t UBaseType_t;
typedef uint32_t TickType_t;

#define portMAX_DELAY           (TickType_t) 0xffffffffUL
#define portTICK_PERIOD_MS      ((TickType_t) 1000 / configTICK_RATE_HZ)
#define portNUM_PROCESSORS      1

#define portNOP()               ((void)0)

#ifdef __cplusplus
}
#endif

#endif /* PORTMACRO_H */
