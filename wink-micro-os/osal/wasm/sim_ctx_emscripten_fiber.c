#include "sim_ctx.h"
#include "pal_osal.h"
#include "wink_sim_scheduler.h"
#include <emscripten/fiber.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct sim_ctx {
    emscripten_fiber_t fiber;
    char* stack;              /* 数据栈 */
    char* asyncify_stack;     /* Asyncify unwind 栈 */
    size_t stack_bytes;
    size_t async_bytes;
    void (*entry)(void*);
    void* arg;
    bool  is_main;            /* main ctx 用 from_current，无独立 stack malloc */
};

static void fiber_trampoline(void* p) {
    struct sim_ctx* c = (struct sim_ctx*)p;
    c->entry(c->arg);
    /* 用户函数执行完成后，自动调用 pal_os_task_delete(NULL) 进行自删和挂起切换，
     * 对齐 Host 侧自适应三段式收尾机制，杜绝协程直接 return 引发 runtime 崩溃 */
    pal_os_task_delete(NULL);
}

sim_ctx_t* sim_ctx_from_current(void) {
    struct sim_ctx* c = calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->is_main = true;
    /* R8：async stack 大小向上舍入到 16 字节倍数（aligned_alloc C11 要求） */
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
    /* R8：向上舍入到 16 字节倍数，满足 aligned_alloc C11 要求（size 必须是 alignment 的整数倍） */
    c->stack_bytes = (stack_bytes + 15u) & ~(size_t)15u;
    size_t base_async = WINK_SIM_ASYNCIFY_MIN > 4096u ? WINK_SIM_ASYNCIFY_MIN : 4096u;
    c->async_bytes = (base_async + 15u) & ~(size_t)15u;
    /* 显式 16 字节对齐内存分配 */
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
    /* 契约 v2（fixup 计划红线 12）：from 必须非空，且必须为当前正在运行的 ctx。
     * emscripten_fiber_swap 会读 from 保存当前上下文；NULL 会 deref 崩。 */
    assert(from != NULL && "sim_ctx_switch: from must be non-null (contract v2)");
    assert(to != NULL && "sim_ctx_switch: to must be non-null");
    emscripten_fiber_swap(&from->fiber, &to->fiber);
}

void sim_ctx_destroy(sim_ctx_t* ctx) {
    if (!ctx) return;
    if (!ctx->is_main) free(ctx->stack);   /* main 无独立 stack */
    free(ctx->asyncify_stack);
    free(ctx);
}
