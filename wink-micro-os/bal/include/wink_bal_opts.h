// SPDX-License-Identifier: Apache-2.0
#ifndef WINK_BAL_OPTS_H
#define WINK_BAL_OPTS_H

/**
 * @file wink_bal_opts.h
 * @brief BAL common types - core-affinity enum + unified BAL options
 *        struct + default-value macros.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Core-affinity enum (BAL-isolated - does NOT depend on pal_os_core_id_t).
 */
typedef enum {
    WINK_BAL_CORE_ANY      = 0,  /**< Let scheduler decide (default). */
    WINK_BAL_CORE_0        = 1,  /**< Pin to core 0 (dual-core targets only). */
    WINK_BAL_CORE_1        = 2,  /**< Pin to core 1 (dual-core targets only). */
    WINK_BAL_CORE_INVALID  = -1, /**< Sentinel: "use BAL default". */
} wink_bal_core_t;

/**
 * Unified BAL options struct - shared by ALL _start_ex() variants.
 */
typedef struct {
    uint32_t        stack_bytes;  /**< 0 = use BAL default. */
    int32_t         priority;     /**< <0 = use BAL default. */
    wink_bal_core_t core_id;      /**< WINK_BAL_CORE_INVALID = use BAL default. */
    uint32_t        flags;        /**< Bitwise OR of flags; 0 = use BAL default. */
} wink_bal_opts_t;

/**
 * Default-initializer for wink_bal_opts_t.
 */
#define WINK_BAL_OPTS_DEFAULT \
    ((wink_bal_opts_t){ .stack_bytes = 0u, .priority = -1, .core_id = WINK_BAL_CORE_INVALID, .flags = 0u })

/**
 * Explicit-value initializer (convenience wrapper).
 */
#define WINK_BAL_OPTS(stack, prio, core) \
    ((wink_bal_opts_t){ .stack_bytes = (stack), .priority = (prio), .core_id = (core), .flags = 0u })

#ifdef __cplusplus
}
#endif

#endif /* WINK_BAL_OPTS_H */
