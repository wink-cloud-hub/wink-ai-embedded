/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#define configTICK_RATE_HZ          100
#define configMAX_PRIORITIES        25
#define configMINIMAL_STACK_SIZE    768
#define configUSE_PREEMPTION        1
#define configUSE_TIME_SLICING      1

#ifndef configMAX_TASK_NAME_LEN
#define configMAX_TASK_NAME_LEN     16
#endif

#ifndef configGENERATE_RUN_TIME_STATS
#define configGENERATE_RUN_TIME_STATS 1
#endif

#ifndef configRUN_TIME_COUNTER_TYPE
#define configRUN_TIME_COUNTER_TYPE uint32_t
#endif

#endif /* FREERTOS_CONFIG_H */
