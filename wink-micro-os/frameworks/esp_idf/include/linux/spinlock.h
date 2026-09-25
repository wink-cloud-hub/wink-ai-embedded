/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_LINUX_SPINLOCK_H
#define WINK_H_GUARD_LINUX_SPINLOCK_H
#ifndef __WINK_HARVESTED_LINUX_SPINLOCK_H__
#define __WINK_HARVESTED_LINUX_SPINLOCK_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef CORE_ID_REGVAL_XOR_SWAP
#define CORE_ID_REGVAL_XOR_SWAP (0xCDCD ^ 0xABAB)
#endif
#ifndef SPINLOCK_FREE
#define SPINLOCK_FREE 0xB33FFFFF
#endif
#ifndef SPINLOCK_INITIALIZER
#define SPINLOCK_INITIALIZER {.owner = SPINLOCK_FREE,.count = 0}
#endif
#ifndef SPINLOCK_NO_WAIT
#define SPINLOCK_NO_WAIT 0
#endif
#ifndef SPINLOCK_WAIT_FOREVER
#define SPINLOCK_WAIT_FOREVER (-1)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    uint32_t owner;
    uint32_t count;
} spinlock_t;



#if defined(__WINK_SIM__)
bool spinlock_acquire(spinlock_t *lock, int32_t timeout) WINK_SLA_ERROR("Wink SLA Violation: spinlock_acquire out of Core 8 scope.");
#else
bool spinlock_acquire(spinlock_t *lock, int32_t timeout);
#endif

#if defined(__WINK_SIM__)
void spinlock_initialize(spinlock_t *lock) WINK_SLA_ERROR("Wink SLA Violation: spinlock_initialize out of Core 8 scope.");
#else
void spinlock_initialize(spinlock_t *lock);
#endif

#if defined(__WINK_SIM__)
void spinlock_release(spinlock_t *lock) WINK_SLA_ERROR("Wink SLA Violation: spinlock_release out of Core 8 scope.");
#else
void spinlock_release(spinlock_t *lock);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_LINUX_SPINLOCK_H__ */
#endif /* WINK_H_GUARD_LINUX_SPINLOCK_H */
