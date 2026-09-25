/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef _ESP_TASK_H_
#define _ESP_TASK_H_
#ifndef __WINK_HARVESTED_ESP_TASK_H__
#define __WINK_HARVESTED_ESP_TASK_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef BT_TASK_EXTRA_STACK_SIZE
#define BT_TASK_EXTRA_STACK_SIZE TASK_EXTRA_STACK_SIZE
#endif
#ifndef ESP_TASKD_EVENT_PRIO
#define ESP_TASKD_EVENT_PRIO (ESP_TASK_PRIO_MAX - 5)
#endif
#ifndef ESP_TASKD_EVENT_STACK
#define ESP_TASKD_EVENT_STACK (CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE + TASK_EXTRA_STACK_SIZE)
#endif
#ifndef ESP_TASK_BT_CONTROLLER_PRIO
#define ESP_TASK_BT_CONTROLLER_PRIO (ESP_TASK_PRIO_MAX - 2)
#endif
#ifndef ESP_TASK_BT_CONTROLLER_STACK
#define ESP_TASK_BT_CONTROLLER_STACK (3584 + TASK_EXTRA_STACK_SIZE)
#endif
#ifndef ESP_TASK_MAIN_CORE
#define ESP_TASK_MAIN_CORE CONFIG_ESP_MAIN_TASK_AFFINITY
#endif
#ifndef ESP_TASK_MAIN_PRIO
#define ESP_TASK_MAIN_PRIO (ESP_TASK_PRIO_MIN + 1)
#endif
#ifndef ESP_TASK_MAIN_STACK
#define ESP_TASK_MAIN_STACK (CONFIG_ESP_MAIN_TASK_STACK_SIZE + TASK_EXTRA_STACK_SIZE)
#endif
#ifndef ESP_TASK_PING_STACK
#define ESP_TASK_PING_STACK (2048 + TASK_EXTRA_STACK_SIZE)
#endif
#ifndef ESP_TASK_PRIO_MAX
#define ESP_TASK_PRIO_MAX (configMAX_PRIORITIES)
#endif
#ifndef ESP_TASK_PRIO_MIN
#define ESP_TASK_PRIO_MIN (0)
#endif
#ifndef ESP_TASK_TCPIP_PRIO
#define ESP_TASK_TCPIP_PRIO (CONFIG_LWIP_TCPIP_TASK_PRIO)
#endif
#ifndef ESP_TASK_TCPIP_STACK
#define ESP_TASK_TCPIP_STACK (CONFIG_LWIP_TCPIP_TASK_STACK_SIZE + TASK_EXTRA_STACK_SIZE)
#endif
#ifndef ESP_TASK_TIMER_PRIO
#define ESP_TASK_TIMER_PRIO (ESP_TASK_PRIO_MAX - 3)
#endif
#ifndef ESP_TASK_TIMER_STACK
#define ESP_TASK_TIMER_STACK (CONFIG_ESP_TIMER_TASK_STACK_SIZE +  TASK_EXTRA_STACK_SIZE)
#endif
#ifndef TASK_EXTRA_STACK_SIZE
#define TASK_EXTRA_STACK_SIZE (512)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_TASK_H__ */
#endif /* _ESP_TASK_H_ */
