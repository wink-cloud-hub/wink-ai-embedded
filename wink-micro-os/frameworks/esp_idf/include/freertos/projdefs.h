/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef PROJDEFS_H
#define PROJDEFS_H

#include <stdint.h>

typedef int32_t BaseType_t;
typedef uint32_t UBaseType_t;

#define pdTRUE          ((BaseType_t) 1)
#define pdFALSE         ((BaseType_t) 0)
#define pdPASS          (pdTRUE)
#define pdFAIL          (pdFALSE)

#define errQUEUE_EMPTY  ((BaseType_t) 0)
#define errQUEUE_FULL   ((BaseType_t) 0)

#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(ms)   ((TickType_t)(((uint64_t)(ms) * (uint64_t)configTICK_RATE_HZ) / 1000ULL))
#endif

#endif /* PROJDEFS_H */
