/**
 * @file pal_shared_chain.c
 * @brief target-private 共享中断责任链算法实现（三 target 通用）
 *
 * 见 pal_shared_chain.h 顶部 ADR-0004 边界解释与实施计划
 * `docs/design/implementation-plans/2026-07-01-pal-target-p1-maintainability-plan.md`
 * §3.3 红线 R-1 / R-3 / R-5。
 */
#include "pal_shared_chain.h"

#include <stdlib.h>
#include <string.h>

static inline void s_enter(const pal_shared_chain_sync_ops_t *ops)
{
    if (ops != NULL && ops->enter_critical != NULL) {
        ops->enter_critical(ops->critical_ctx);
    }
}

static inline void s_exit(const pal_shared_chain_sync_ops_t *ops)
{
    if (ops != NULL && ops->exit_critical != NULL) {
        ops->exit_critical(ops->critical_ctx);
    }
}

static inline void s_sync(const pal_shared_chain_sync_ops_t *ops, uint32_t irq_num)
{
    if (ops != NULL && ops->synchronize != NULL) {
        ops->synchronize(irq_num);
    }
}

wink_status_t pal_shared_chain_append(pal_shared_chain_t **slot,
                                       const pal_shared_chain_sync_ops_t *ops,
                                       uint32_t irq_num,
                                       pal_irq_shared_handler_t handler,
                                       void *arg,
                                       bool *out_became_first)
{
    if (slot == NULL || handler == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    /* ✅ RCU 写路径 step 1：进入临界区（ESP32 portMUX；单线程 NULL 跳过） */
    s_enter(ops);

    pal_shared_chain_t *old_chain = *slot;
    pal_shared_chain_t *new_chain = NULL;

    if (old_chain == NULL) {
        /* 第一个 handler：创建新链 */
        new_chain = (pal_shared_chain_t *)malloc(sizeof(*new_chain));
        if (new_chain == NULL) {
            s_exit(ops);
            return WINK_ERR_NO_MEM;
        }
        memset(new_chain, 0, sizeof(*new_chain));
    } else {
        /* 已有 handler：先容量检查，再复制旧链 */
        if (old_chain->count >= PAL_SHARED_CHAIN_MAX_HANDLERS) {
            s_exit(ops);
            return WINK_ERR_NO_MEM;
        }
        new_chain = (pal_shared_chain_t *)malloc(sizeof(*new_chain));
        if (new_chain == NULL) {
            s_exit(ops);
            return WINK_ERR_NO_MEM;
        }
        memcpy(new_chain, old_chain, sizeof(*new_chain));
    }

    /* 追加新 handler */
    new_chain->entries[new_chain->count].handler = handler;
    new_chain->entries[new_chain->count].arg     = arg;
    new_chain->count++;

    /* ✅ RCU 关键点：原子指针替换（ISR 读端可能正握着 old_chain，安全） */
    *slot = new_chain;
    const bool became_first = (old_chain == NULL);

    /* ✅ RCU step 4：退出临界区 */
    s_exit(ops);

    /* ✅ RCU step 5：等所有旧 ISR 退出（SMP 关键；单线程 no-op）
     * 此步顺序必须在 exit 之后、free old 之前 —— 违反即 SMP UAF。 */
    s_sync(ops, irq_num);

    /* ✅ RCU step 6：现在释放旧链是安全的（老 ISR 都已经退出了） */
    free(old_chain);

    if (out_became_first != NULL) {
        *out_became_first = became_first;
    }
    return WINK_OK;
}

uint32_t pal_shared_chain_dispatch(const pal_shared_chain_t *chain)
{
    if (chain == NULL) {
        return 0;
    }
    uint32_t claimed = 0;
    /* ✅ v2.0 语义：始终遍历调用所有 handler，不提前终止 */
    for (uint8_t i = 0; i < chain->count; i++) {
        if (chain->entries[i].handler != NULL) {
            if (chain->entries[i].handler(chain->entries[i].arg)) {
                claimed++;
            }
        }
    }
    return claimed;
}
