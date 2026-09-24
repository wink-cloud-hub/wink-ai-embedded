/* SPDX-License-Identifier: LGPL-3.0-only */
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_log.h"

TimerHandle_t xTimerCreate(const char *const pcTimerName,
                           const TickType_t xTimerPeriodInTicks,
                           const UBaseType_t uxAutoReload,
                           void *const pvTimerID,
                           TimerCallbackFunction_t pxCallbackFunction) {
    (void)pcTimerName;
    (void)xTimerPeriodInTicks;
    (void)uxAutoReload;
    (void)pvTimerID;
    (void)pxCallbackFunction;
    ESP_LOGE("FREERTOS", "xTimerCreate not supported in M1 simulation shim (deferred to M2+)");
    return NULL;
}

BaseType_t xTimerStart(TimerHandle_t xTimer, const TickType_t xTicksToWait) {
    (void)xTimer;
    (void)xTicksToWait;
    ESP_LOGE("FREERTOS", "xTimerStart not supported in M1 simulation shim");
    return pdFAIL;
}

BaseType_t xTimerStop(TimerHandle_t xTimer, const TickType_t xTicksToWait) {
    (void)xTimer;
    (void)xTicksToWait;
    ESP_LOGE("FREERTOS", "xTimerStop not supported in M1 simulation shim");
    return pdFAIL;
}

BaseType_t xTimerChangePeriod(TimerHandle_t xTimer, const TickType_t xNewPeriod, const TickType_t xTicksToWait) {
    (void)xTimer;
    (void)xNewPeriod;
    (void)xTicksToWait;
    ESP_LOGE("FREERTOS", "xTimerChangePeriod not supported in M1 simulation shim");
    return pdFAIL;
}

BaseType_t xTimerDelete(TimerHandle_t xTimer, const TickType_t xTicksToWait) {
    (void)xTimer;
    (void)xTicksToWait;
    ESP_LOGE("FREERTOS", "xTimerDelete not supported in M1 simulation shim");
    return pdFAIL;
}

BaseType_t xTimerReset(TimerHandle_t xTimer, const TickType_t xTicksToWait) {
    (void)xTimer;
    (void)xTicksToWait;
    ESP_LOGE("FREERTOS", "xTimerReset not supported in M1 simulation shim");
    return pdFAIL;
}

BaseType_t xTimerStartFromISR(TimerHandle_t xTimer, BaseType_t *pxHigherPriorityTaskWoken) {
    (void)xTimer;
    if (pxHigherPriorityTaskWoken != NULL) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }
    ESP_LOGE("FREERTOS", "xTimerStartFromISR not supported in M1 simulation shim");
    return pdFAIL;
}

BaseType_t xTimerStopFromISR(TimerHandle_t xTimer, BaseType_t *pxHigherPriorityTaskWoken) {
    (void)xTimer;
    if (pxHigherPriorityTaskWoken != NULL) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }
    ESP_LOGE("FREERTOS", "xTimerStopFromISR not supported in M1 simulation shim");
    return pdFAIL;
}

BaseType_t xTimerResetFromISR(TimerHandle_t xTimer, BaseType_t *pxHigherPriorityTaskWoken) {
    (void)xTimer;
    if (pxHigherPriorityTaskWoken != NULL) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }
    ESP_LOGE("FREERTOS", "xTimerResetFromISR not supported in M1 simulation shim");
    return pdFAIL;
}

BaseType_t xTimerChangePeriodFromISR(TimerHandle_t xTimer, const TickType_t xNewPeriod, BaseType_t *pxHigherPriorityTaskWoken) {
    (void)xTimer;
    (void)xNewPeriod;
    if (pxHigherPriorityTaskWoken != NULL) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }
    ESP_LOGE("FREERTOS", "xTimerChangePeriodFromISR not supported in M1 simulation shim");
    return pdFAIL;
}

BaseType_t xTimerIsTimerActive(TimerHandle_t xTimer) {
    (void)xTimer;
    return pdFALSE;
}

void *pvTimerGetTimerID(const TimerHandle_t xTimer) {
    (void)xTimer;
    return NULL;
}
