#ifndef WINK_BAL_OPTS_H
#define WINK_BAL_OPTS_H

/**
 * @file wink_bal_opts.h
 * @brief BAL common types — core-affinity enum + unified BAL options
 *        struct + default-value macros.
 *
 * This is the ONLY public BAL header that defines types shared across
 * all BAL helpers.  Per ADR-0023 §2/§3 layering red-line:
 *
 *   * This file MUST NOT include any pal_*.h header.  Core affinity is
 *     isolated to our own enum; BAL .c files map it to pal_os_core_id_t
 *     internally.  App code never sees pal_os_core_id_t.
 *   * Zero / INVALID field values mean "use BAL default" — so a
 *     zero- or WINK_BAL_OPTS_DEFAULT-initialized struct never
 *     accidentally overrides BAL defaults with bogus values.
 *
 * Copyright (c) 2026 Wink-AI.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Core-affinity enum (BAL-isolated — does NOT depend on pal_os_core_id_t).
 *
 * Single-core targets (host/wasm/baremetal) map CORE_0/CORE_1 to ANY in
 * the internal map_core() helper ("best effort" rather than "must pin").
 */
typedef enum {
    WINK_BAL_CORE_ANY      = 0,  /**< Let scheduler decide (default). */
    WINK_BAL_CORE_0        = 1,  /**< Pin to core 0 (dual-core targets only). */
    WINK_BAL_CORE_1        = 2,  /**< Pin to core 1 (dual-core targets only). */
    WINK_BAL_CORE_INVALID  = -1, /**< Sentinel: "use BAL default" (never pass as explicit pin). */
} wink_bal_core_t;

/**
 * Unified BAL options struct — shared by ALL _start_ex() variants.
 *
 * Each field uses a sentinel value meaning "use BAL default":
 *   stack_bytes == 0 → use BAL default stack
 *   priority    <  0 → use BAL default priority
 *   core_id     == WINK_BAL_CORE_INVALID → use BAL default core
 *   flags       == 0 → use BAL default flags (LIGHT/MAY_BLOCK choice)
 *
 * Passing opts=NULL to _start_ex() is equivalent to _start() (all defaults).
 */
typedef struct {
    uint32_t        stack_bytes;  /**< 0 = use BAL default. */
    int32_t         priority;     /**< <0 = use BAL default (higher = more urgent, FreeRTOS-style). */
    wink_bal_core_t core_id;      /**< WINK_BAL_CORE_INVALID = use BAL default. */
    uint32_t        flags;        /**< Bitwise OR of WINK_PERIODIC_LIGHT / WINK_PERIODIC_MAY_BLOCK; 0 = use BAL default. */
} wink_bal_opts_t;

/**
 * Default-initializer for wink_bal_opts_t.
 *
 * Use this at declaration sites to avoid accidentally zero-initializing
 * priority/core_id to meaningful values (0 priority / ANY core are valid
 * settings that can silently override BAL defaults).  Example:
 *
 *     wink_bal_opts_t opts = WINK_BAL_OPTS_DEFAULT;
 *     opts.priority = 8;                       // override one field
 *     wink_ultrasonic_poll_start_ex(&ultrasonic, 50, &opts);
 */
#define WINK_BAL_OPTS_DEFAULT \
    ((wink_bal_opts_t){ .stack_bytes = 0u, .priority = -1, .core_id = WINK_BAL_CORE_INVALID, .flags = 0u })

/**
 * Explicit-value initializer (convenience wrapper).
 *
 * Leaves flags at 0 (use BAL default flags).  Example:
 *
 *     wink_bal_opts_t opts = WINK_BAL_OPTS(3072, 5, WINK_BAL_CORE_ANY);
 */
#define WINK_BAL_OPTS(stack, prio, core) \
    ((wink_bal_opts_t){ .stack_bytes = (stack), .priority = (prio), .core_id = (core), .flags = 0u })

#ifdef __cplusplus
}
#endif

#endif /* WINK_BAL_OPTS_H */
