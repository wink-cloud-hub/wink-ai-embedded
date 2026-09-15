/* SPDX-License-Identifier: GPL-3.0-only */
/*
 * Minimal pal_osal.h shim for Spike-S1 (throwaway PoC, not production code).
 * The real sim_ctx implementations only need pal_os_task_delete() from the
 * PAL; the spike harness provides its definition in s1_spike.c.
 */
#ifndef PAL_OSAL_H_SHIM
#define PAL_OSAL_H_SHIM

#include <stdint.h>

void pal_os_task_delete(void *handle);

#endif /* PAL_OSAL_H_SHIM */
