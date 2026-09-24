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

typedef enum {
    eRunning = 0,
    eReady,
    eBlocked,
    eSuspended,
    eDeleted,
    eInvalid
} eTaskState;

typedef struct xTASK_STATUS {
    TaskHandle_t xHandle;
    const char *pcTaskName;
    UBaseType_t xTaskNumber;
    eTaskState eCurrentState;
    UBaseType_t uxCurrentPriority;
    UBaseType_t uxBasePriority;
    uint32_t ulRunTimeCounter;
    uint32_t usStackHighWaterMark;
} TaskStatus_t;

#define taskSCHEDULER_NOT_STARTED   0
#define taskSCHEDULER_RUNNING       1
#define taskSCHEDULER_SUSPENDED     2

#define tskNO_AFFINITY              ((BaseType_t) 0x7FFFFFFF)

#define taskENTER_CRITICAL(mux)     ((void)0)
#define taskEXIT_CRITICAL(mux)      ((void)0)
#define taskENTER_CRITICAL_ISR(mux) ((void)0)
#define taskEXIT_CRITICAL_ISR(mux)  ((void)0)

#define taskYIELD()                 vTaskDelay(0)

BaseType_t xTaskCreate(TaskFunction_t pxTaskCode,
                       const char *const pcName,
                       const uint32_t usStackDepth,
                       void *const pvParameters,
                       UBaseType_t uxPriority,
                       TaskHandle_t *const pxCreatedTask);

BaseType_t xTaskCreatePinnedToCore(TaskFunction_t pxTaskCode,
                                   const char *const pcName,
                                   const uint32_t usStackDepth,
                                   void *const pvParameters,
                                   UBaseType_t uxPriority,
                                   TaskHandle_t *const pxCreatedTask,
                                   const BaseType_t xCoreID);

void vTaskDelete(TaskHandle_t xTaskToDelete);
void vTaskDelay(const TickType_t xTicksToDelay);
void vTaskDelayUntil(TickType_t *const pxPreviousWakeTime, const TickType_t xTimeIncrement);
void vTaskSuspend(TaskHandle_t xTaskToSuspend);
void vTaskResume(TaskHandle_t xTaskToResume);
TickType_t xTaskGetTickCount(void);
TickType_t xTaskGetTickCountFromISR(void);
eTaskState eTaskGetState(TaskHandle_t xTask);
UBaseType_t uxTaskGetNumberOfTasks(void);
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t xTask);
void vTaskList(char *pcWriteBuffer);
UBaseType_t uxTaskGetSystemState(TaskStatus_t * const pxTaskStatusArray, const UBaseType_t uxArraySize, uint32_t * const pulTotalRunTime);
BaseType_t xTaskGetSchedulerState(void);
void vTaskStartScheduler(void);
BaseType_t xPortGetCoreID(void);
void vTaskPrioritySet(TaskHandle_t xTask, UBaseType_t uxNewPriority);
UBaseType_t uxTaskPriorityGet(TaskHandle_t xTask);

#ifdef __cplusplus
}
#endif

#endif /* INC_TASK_H */
