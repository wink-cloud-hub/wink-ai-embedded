/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef INC_TASK_H
#define INC_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/portmacro.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *TaskHandle_t;
typedef void (*TaskFunction_t)(void *);

void vTaskDelay(const TickType_t xTicksToDelay);
void vTaskDelayUntil(TickType_t *const pxPreviousWakeTime, const TickType_t xTimeIncrement);

#ifdef __cplusplus
}
#endif

#endif /* INC_TASK_H */
