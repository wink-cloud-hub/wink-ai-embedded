/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void nimble_port_freertos_init(void (*host_task_fn)(void *param));
void nimble_port_freertos_deinit(void);

#ifdef __cplusplus
}
#endif
