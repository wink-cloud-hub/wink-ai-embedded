/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef INC_QUEUE_H
#define INC_QUEUE_H

#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/portmacro.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *QueueHandle_t;
typedef void *QueueSetHandle_t;
typedef void *QueueSetMemberHandle_t;

QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize);
void vQueueDelete(QueueHandle_t xQueue);
BaseType_t xQueueSend(QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait);
BaseType_t xQueueSendToFront(QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait);
BaseType_t xQueueReceive(QueueHandle_t xQueue, void *pvBuffer, TickType_t xTicksToWait);
BaseType_t xQueuePeek(QueueHandle_t xQueue, void *pvBuffer, TickType_t xTicksToWait);

/* xQueueSendToBack is identical to xQueueSend (tail enqueue, FreeRTOS standard) */
#define xQueueSendToBack(xQueue, pvItemToQueue, xTicksToWait) \
    xQueueSend((xQueue), (pvItemToQueue), (xTicksToWait))
UBaseType_t uxQueueMessagesWaiting(const QueueHandle_t xQueue);
UBaseType_t uxQueueSpacesAvailable(const QueueHandle_t xQueue);
BaseType_t xQueueReset(QueueHandle_t xQueue);

BaseType_t xQueueSendFromISR(QueueHandle_t xQueue, const void *pvItemToQueue, BaseType_t *pxHigherPriorityTaskWoken);
BaseType_t xQueueSendToFrontFromISR(QueueHandle_t xQueue, const void *pvItemToQueue, BaseType_t *pxHigherPriorityTaskWoken);
BaseType_t xQueueReceiveFromISR(QueueHandle_t xQueue, void *pvBuffer, BaseType_t *pxHigherPriorityTaskWoken);
BaseType_t xQueuePeekFromISR(QueueHandle_t xQueue, void *pvBuffer);

/* xQueueSendToBackFromISR is identical to xQueueSendFromISR */
#define xQueueSendToBackFromISR(xQueue, pvItemToQueue, pxHigherPriorityTaskWoken) \
    xQueueSendFromISR((xQueue), (pvItemToQueue), (pxHigherPriorityTaskWoken))

#ifdef __cplusplus
}
#endif

#endif /* INC_QUEUE_H */
