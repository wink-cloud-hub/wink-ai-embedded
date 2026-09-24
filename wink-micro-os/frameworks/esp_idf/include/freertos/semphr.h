/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef INC_SEMAPHORE_H
#define INC_SEMAPHORE_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef QueueHandle_t SemaphoreHandle_t;

#define semBINARY_SEMAPHORE_QUEUE_LENGTH    ((UBaseType_t) 1)
#define semSEMAPHORE_QUEUE_ITEM_LENGTH      ((UBaseType_t) 0)

SemaphoreHandle_t xSemaphoreCreateBinary(void);
SemaphoreHandle_t xSemaphoreCreateCounting(UBaseType_t uxMaxCount, UBaseType_t uxInitialCount);
SemaphoreHandle_t xSemaphoreCreateMutex(void);
SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void);

BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xBlockTime);
BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore);
BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t xMutex, TickType_t xBlockTime);
BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t xMutex);
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t xSemaphore, BaseType_t *pxHigherPriorityTaskWoken);
BaseType_t xSemaphoreTakeFromISR(SemaphoreHandle_t xSemaphore, BaseType_t *pxHigherPriorityTaskWoken);
void vSemaphoreDelete(SemaphoreHandle_t xSemaphore);
UBaseType_t uxSemaphoreGetCount(SemaphoreHandle_t xSemaphore);

#ifdef __cplusplus
}
#endif

#endif /* INC_SEMAPHORE_H */
