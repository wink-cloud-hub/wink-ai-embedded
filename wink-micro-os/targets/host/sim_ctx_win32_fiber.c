#include "sim_ctx.h"
#include "pal_osal.h" // 引入 pal_os_task_delete 支持自删
#include <windows.h>
#include <stdbool.h>   /* R1：显式 include，避免依赖 windows.h 传递（切换 MinGW 版本可能失败） */
#include <stdlib.h>
#include <assert.h>

struct sim_ctx {
    void*  fiber;         /* CreateFiber 返回；主 fiber 时为 ConvertThreadToFiber 返回 */
    void   (*entry)(void*);
    void*  arg;
    bool   is_main;       /* 标记是否为主协程，防止销毁时误删引发线程退出 */
};

static VOID CALLBACK fiber_trampoline(LPVOID p) {
    struct sim_ctx* c = (struct sim_ctx*)p;
    c->entry(c->arg);
    /* 用户函数执行完成后，通过 pal_os_task_delete(NULL) 自动进入 Zombie 并切回主协程，
     * 规避协程在 trampoline 顶层直接 return 引发宿主线程突发终止的崩溃风险，
     * 同时优雅规避了“任务 ID 未分配时包裹函数无法确定任务句柄”的鸡生蛋问题。 */
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
    /* 向上舍入到 16 字节对齐，防止 SIMD 等指令集错栈崩溃 */
    size_t aligned_stack = (stack_bytes + 15u) & ~15u;
    c->fiber = CreateFiber((SIZE_T)aligned_stack, fiber_trampoline, c);
    if (!c->fiber) { free(c); return NULL; }
    return c;
}

void sim_ctx_switch(sim_ctx_t* from, sim_ctx_t* to) {
    /* 契约 v2（fixup 计划红线 12）：from 必须非空，且应为当前正在运行的 ctx。
     * host Win32 SwitchToFiber 从"当前"切换，不使用 from 数据，但保留严格 assert
     * 与 wasm 侧接口语义完全对称。 */
    assert(from != NULL && "sim_ctx_switch: from must be non-null (contract v2)");
    assert(to != NULL && "sim_ctx_switch: to must be non-null");
    (void)from;
    SwitchToFiber(to->fiber);
}

void sim_ctx_destroy(sim_ctx_t* ctx) {
    if (!ctx) return;
    /* 仅在非常驻的主协程时，才能调用 DeleteFiber，防测试线程退出 */
    if (!ctx->is_main && ctx->fiber) {
        DeleteFiber(ctx->fiber);
    }
    free(ctx);
}
