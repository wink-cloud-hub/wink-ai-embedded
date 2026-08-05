// SPDX-License-Identifier: Apache-2.0
/**
 * @file sim_ctx_emscripten_fiber.c
 * @brief Emscripten Fiber based simulation context implementation for WASM target.
 */
#include "sim_ctx.h"
#include "pal_osal.h"
#include "wink_sim_scheduler.h"
#include <emscripten/fiber.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct sim_ctx {
    emscripten_fiber_t fiber;
    char* stack;              /* Data stack */
    char* asyncify_stack;     /* Asyncify unwind stack */
    size_t stack_bytes;
    size_t async_bytes;
    void (*entry)(void*);
    void* arg;
    bool  is_main;            /* Main ctx created via from_current without independent stack malloc */
};

static void fiber_trampoline(void* p) {
    struct sim_ctx* c = (struct sim_ctx*)p;
    c->entry(c->arg);
    /* Auto-delete task after completion via pal_os_task_delete(NULL) */
    pal_os_task_delete(NULL);
}

sim_ctx_t* sim_ctx_from_current(void) {
    struct sim_ctx* c = calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->is_main = true;
    /* Round up async stack size to 16-byte alignment */
    c->async_bytes = (4u * 1024u + 15u) & ~(size_t)15u;
    c->asyncify_stack = aligned_alloc(16, c->async_bytes);
    if (!c->asyncify_stack) {
        free(c);
        return NULL;
    }
    emscripten_fiber_init_from_current_context(
        &c->fiber, c->asyncify_stack, c->async_bytes);
    return c;
}

sim_ctx_t* sim_ctx_create(void (*entry)(void*), void* arg, size_t stack_bytes) {
    struct sim_ctx* c = calloc(1, sizeof(*c));
    if (!c) return NULL;
    /* Round up stack size to 16-byte alignment */
    c->stack_bytes = (stack_bytes + 15u) & ~(size_t)15u;
    size_t base_async = WINK_SIM_ASYNCIFY_MIN > 4096u ? WINK_SIM_ASYNCIFY_MIN : 4096u;
    c->async_bytes = (base_async + 15u) & ~(size_t)15u;
    /* 16-byte aligned memory allocation */
    c->stack = aligned_alloc(16, c->stack_bytes);
    c->asyncify_stack = aligned_alloc(16, c->async_bytes);
    if (!c->stack || !c->asyncify_stack) {
        free(c->stack); free(c->asyncify_stack); free(c);
        return NULL;
    }
    c->entry = entry;
    c->arg = arg;
    c->is_main = false;
    emscripten_fiber_init(&c->fiber, fiber_trampoline, c,
                          c->stack, c->stack_bytes,
                          c->asyncify_stack, c->async_bytes);
    return c;
}

void sim_ctx_switch(sim_ctx_t* from, sim_ctx_t* to) {
    assert(from != NULL && "sim_ctx_switch: from must be non-null (contract v2)");
    assert(to != NULL && "sim_ctx_switch: to must be non-null");
    emscripten_fiber_swap(&from->fiber, &to->fiber);
}

void sim_ctx_destroy(sim_ctx_t* ctx) {
    if (!ctx) return;
    if (!ctx->is_main) free(ctx->stack);
    free(ctx->asyncify_stack);
    free(ctx);
}
