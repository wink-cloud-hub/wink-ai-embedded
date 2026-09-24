/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef INC_TIMERS_H
#define INC_TIMERS_H

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *TimerHandle_t;
typedef void (*TimerCallbackFunction_t)(TimerHandle_t xTimer);

TimerHandle_t xTimerCreate(const char *const pcTimerName,
                           const TickType_t xTimerPeriodInTicks,
                           const UBaseType_t uxAutoReload,
                           void *const pvTimerID,
                           TimerCallbackFunction_t pxCallbackFunction);

BaseType_t xTimerStart(TimerHandle_t xTimer, const TickType_t xTicksToWait);
BaseType_t xTimerStop(TimerHandle_t xTimer, const TickType_t xTicksToWait);
BaseType_t xTimerChangePeriod(TimerHandle_t xTimer, const TickType_t xNewPeriod, const TickType_t xTicksToWait);
BaseType_t xTimerDelete(TimerHandle_t xTimer, const TickType_t xTicksToWait);
BaseType_t xTimerReset(TimerHandle_t xTimer, const TickType_t xTicksToWait);
BaseType_t xTimerStartFromISR(TimerHandle_t xTimer, BaseType_t *pxHigherPriorityTaskWoken);
BaseType_t xTimerStopFromISR(TimerHandle_t xTimer, BaseType_t *pxHigherPriorityTaskWoken);
BaseType_t xTimerResetFromISR(TimerHandle_t xTimer, BaseType_t *pxHigherPriorityTaskWoken);
BaseType_t xTimerChangePeriodFromISR(TimerHandle_t xTimer, const TickType_t xNewPeriod, BaseType_t *pxHigherPriorityTaskWoken);
BaseType_t xTimerIsTimerActive(TimerHandle_t xTimer);
void *pvTimerGetTimerID(const TimerHandle_t xTimer);

#ifdef __cplusplus
}
#endif

#endif /* INC_TIMERS_H */
