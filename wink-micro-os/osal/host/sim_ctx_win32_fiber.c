// SPDX-License-Identifier: Apache-2.0
/**
 * @file sim_ctx_win32_fiber.c
 * @brief Win32 Fiber based simulation context implementation for Host target.
 */
#include "sim_ctx.h"
#include "pal_osal.h"
#include <windows.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

struct sim_ctx {
    void*  fiber;         /* Returned by CreateFiber or ConvertThreadToFiber */
    void   (*entry)(void*);
    void*  arg;
    bool   is_main;       /* Flag indicating whether this is the main fiber */
};

static VOID CALLBACK fiber_trampoline(LPVOID p) {
    struct sim_ctx* c = (struct sim_ctx*)p;
    c->entry(c->arg);
    /* Auto-delete task after completion via pal_os_task_delete(NULL) */
    pal_os_task_delete(NULL);
}

sim_ctx_t* sim_ctx_from_current(void) {
    struct sim_ctx* c = (struct sim_ctx*)calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->is_main = true;
    if (IsThreadAFiber()) {
        c->fiber = GetCurrentFiber();
    } else {
        c->fiber = ConvertThreadToFiber(NULL);
    }
    return c;
}

sim_ctx_t* sim_ctx_create(void (*entry)(void*), void* arg, size_t stack_bytes) {
    struct sim_ctx* c = (struct sim_ctx*)calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->entry = entry;
    c->arg = arg;
    c->is_main = false;
    /* Round up stack size to 16-byte alignment */
    size_t aligned_stack = (stack_bytes + 15u) & ~15u;
    c->fiber = CreateFiber((SIZE_T)aligned_stack, fiber_trampoline, c);
    if (!c->fiber) { free(c); return NULL; }
    return c;
}

void sim_ctx_switch(sim_ctx_t* from, sim_ctx_t* to) {
    assert(from != NULL && "sim_ctx_switch: from must be non-null (contract v2)");
    assert(to != NULL && "sim_ctx_switch: to must be non-null");
    (void)from;
    SwitchToFiber(to->fiber);
}

void sim_ctx_destroy(sim_ctx_t* ctx) {
    if (!ctx) return;
    if (!ctx->is_main && ctx->fiber) {
        DeleteFiber(ctx->fiber);
    }
    free(ctx);
}
