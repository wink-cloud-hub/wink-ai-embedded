// SPDX-License-Identifier: Apache-2.0
/**
 * @file wink_actuator_registry.h
 * @brief Actuator Safe-Off Registry interface (static table, zero dynamic allocation).
 */
#ifndef WINK_ACTUATOR_REGISTRY_H
#define WINK_ACTUATOR_REGISTRY_H

#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Registry capacity (statically allocated) */
#ifndef WINK_ACTUATOR_REGISTRY_CAPACITY
#define WINK_ACTUATOR_REGISTRY_CAPACITY 16
#endif

/** @brief Actuator safe-off callback prototype */
typedef wink_status_t (*wink_actuator_safe_off_fn)(void *ctx);

/**
 * @brief Register an actuator safe-off callback
 *
 * @param[in] fn Callback function pointer.
 * @param[in] ctx Context pointer (instance handle).
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_actuator_register(wink_actuator_safe_off_fn fn, void *ctx);

/**
 * @brief Unregister an actuator safe-off callback
 *
 * @param[in] fn Callback function pointer.
 * @param[in] ctx Context pointer.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_actuator_unregister(wink_actuator_safe_off_fn fn, void *ctx);

/**
 * @brief Reset and clear actuator registry
 */
void wink_actuator_registry_reset(void);

/**
 * @brief Safely turn off all registered actuators
 */
void wink_actuator_safe_off_all(void);

/**
 * @brief Define a type-adapting thunk for registering DAL actuator off
 *        functions with the registry.
 */
#define WINK_DEFINE_ACTUATOR_THUNK(thunk_name, fn, dev_type) \
    static wink_status_t thunk_name(void *_ctx) { return fn((dev_type *)_ctx); }

#ifdef __cplusplus
}
#endif

#endif /* WINK_ACTUATOR_REGISTRY_H */
