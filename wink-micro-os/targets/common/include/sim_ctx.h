#ifndef SIM_CTX_H
#define SIM_CTX_H
#include <stdint.h>
#include <stddef.h>

typedef struct sim_ctx sim_ctx_t;   /* 前向声明，实现由 sim_ctx_*.c 定义 */

/* 语义：分配数据栈 + (wasm 侧) asyncify 栈，创建协程句柄。
 *      stack_bytes 必须 ≥ WINK_SIM_STACK_MIN；调用方（scheduler）负责 clamp+WARN。 */
sim_ctx_t* sim_ctx_create(void (*entry)(void*), void* arg, size_t stack_bytes);

/* 主调度器 fiber 初始化（从当前线程/主上下文转换而来）。全局仅调一次。 */
sim_ctx_t* sim_ctx_from_current(void);

/* 从 from 切换到 to。当前上下文挂起，to 从上次挂起处继续。 */
void       sim_ctx_switch(sim_ctx_t* from, sim_ctx_t* to);

/* 释放数据栈 + asyncify 栈。禁止对"当前正在运行"的 ctx 调用（UB）。
 * 调用方（scheduler_gc_zombies）负责在 SwitchToFiber(main) 之后再删。 */
void       sim_ctx_destroy(sim_ctx_t* ctx);

#endif
