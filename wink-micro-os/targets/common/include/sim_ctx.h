// SPDX-License-Identifier: Apache-2.0
/**
 * @file sim_ctx.h
 * @brief Fiber / coroutine context switching abstractions for simulator targets.
 */
#ifndef SIM_CTX_H
#define SIM_CTX_H

#include <stdint.h>
#include <stddef.h>

typedef struct sim_ctx sim_ctx_t;

/**
 * @brief Allocate data stack + (wasm side) asyncify stack and create fiber handle.
 *
 * @param entry Function entry point.
 * @param arg Context argument.
 * @param stack_bytes Stack size in bytes.
 * @return Allocated fiber context handle.
 */
sim_ctx_t* sim_ctx_create(void (*entry)(void*), void* arg, size_t stack_bytes);

/**
 * @brief Initialize main scheduler fiber handle from current thread context.
 *
 * @return Main fiber context handle.
 */
sim_ctx_t* sim_ctx_from_current(void);

/**
 * @brief Switch execution context from @p from to @p to.
 *
 * @param from Source context handle.
 * @param to Target context handle.
 */
void sim_ctx_switch(sim_ctx_t* from, sim_ctx_t* to);

/**
 * @brief Free context memory.
 *
 * @param ctx Context handle to destroy.
 */
void sim_ctx_destroy(sim_ctx_t* ctx);

#endif /* SIM_CTX_H */
