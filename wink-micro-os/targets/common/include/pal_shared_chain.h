/**
 * @file pal_shared_chain.h
 * @brief target-private 共享中断责任链算法层（三 target 通用）
 *
 * ─────────────────────────────────────────────────────────────────────────────
 *  ADR-0004 边界解释（R-3 红线）
 * ─────────────────────────────────────────────────────────────────────────────
 *  本头文件定义的 `pal_shared_chain_sync_ops_t` 是**同步原语（synchronization
 *  primitive）vtable**，用于向算法层注入平台相关的临界区/synchronize 语义：
 *
 *    - enter_critical / exit_critical：SMP 自旋锁的 enter/exit（或单线程 NULL）
 *    - synchronize                    ：`pal_irq_synchronize` 的桥接（或 NULL）
 *
 *  ⚠️ **本 vtable 不是 ADR-0004 禁止的"外设 device_ops 运行期虚表"。**
 *
 *  ADR-0004 禁止的是**对外设实例做运行期多态**（例如 `struct servo_ops`、
 *  `struct display_ops` 之类的成员函数指针，因为它们在同一 target 上会以多份
 *  实例存在，且需要 `container_of` 下转型才能定位派生数据）。本 vtable：
 *
 *    1. **不涉及外设语义** —— 抽象的是同步原语；
 *    2. **每 target 编译期只挂一份实例** —— 三 target 各自静态定义一个
 *       `static const pal_shared_chain_sync_ops_t s_xxx_shared_sync_ops = {...}`；
 *    3. **不涉及派生结构与下转型** —— 与 `pal_pwm_pin_map` 的 weak override
 *       性质等同（一份静态数据 + 编译期注入）。
 *
 *  参考：
 *    - `docs/design/decisions/0004-static-dispatch-vs-runtime-ops.md`
 *    - `docs/design/implementation-plans/2026-07-01-pal-target-p1-maintainability-plan.md` §3.3 R-3
 *
 * ─────────────────────────────────────────────────────────────────────────────
 *  头文件可见性（target-private）
 * ─────────────────────────────────────────────────────────────────────────────
 *  本头文件位于 `targets/common/include/`，**仅供三 target 内部实现引用**，
 *  **不进入** `WINK_CORE_INCLUDE_DIRS`，因此 DAL / runtime 层看不到此头文件，
 *  避免了内部实现细节泄漏到跨 target 公开命名空间。
 */

#ifndef PAL_SHARED_CHAIN_H
#define PAL_SHARED_CHAIN_H

#include <stdbool.h>
#include <stdint.h>
#include "wink_status.h"
#include "pal_irq.h"   /* pal_irq_shared_handler_t */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PAL_SHARED_CHAIN_MAX_HANDLERS
#define PAL_SHARED_CHAIN_MAX_HANDLERS 4
#endif

/**
 * @brief 同步策略回调（每 target 各注入一份实例）
 *
 * 参见文件头 ADR-0004 边界解释：本 vtable 是"同步原语抽象"，非外设多态。
 *
 * - ESP32：三个字段全部有值（portMUX 自旋锁 + pal_irq_synchronize）
 * - wasm / host：整个 ops 指针传 NULL（单线程简化路径）
 */
typedef struct pal_shared_chain_sync_ops {
    void (*enter_critical)(void *ctx);       /**< SMP: 自旋锁 enter；单线程: NULL */
    void (*exit_critical)(void *ctx);        /**< SMP: 自旋锁 exit；单线程: NULL */
    void (*synchronize)(uint32_t irq_num);   /**< SMP: 忙等 in-flight → 0；单线程: NULL */
    void *critical_ctx;                      /**< SMP: 指向 portMUX_TYPE；单线程: NULL */
} pal_shared_chain_sync_ops_t;

/**
 * @brief 责任链单个 entry（POD）
 */
typedef struct {
    pal_irq_shared_handler_t handler;
    void                    *arg;
} pal_shared_chain_entry_t;

/**
 * @brief 责任链 POD（三 target 布局一致）
 */
typedef struct {
    pal_shared_chain_entry_t entries[PAL_SHARED_CHAIN_MAX_HANDLERS];
    uint8_t count;
} pal_shared_chain_t;

/**
 * @brief 向 chain 追加 handler（RCU 写路径），线程/SMP 安全由 ops 提供。
 *
 * 语义（R-1 / R-5 红线）：
 *   1. `enter_critical(ctx)`
 *   2. 若 `*slot == NULL`：malloc 新 chain（count=1）；
 *      否则：检查 count < MAX；malloc + memcpy 新 chain（count+=1）
 *   3. 原子替换 `*slot = new_chain`
 *   4. `exit_critical(ctx)`
 *   5. `synchronize(irq_num)`（NULL 则跳过）
 *   6. `free(old_chain)`
 *
 * 步骤 4/5/6 顺序不可颠倒 —— 否则会破坏 ESP32 侧读端无锁的 RCU 语义
 * （synchronize 之前 free old_chain 会造成 SMP UAF）。
 *
 * @param slot     指向 chain 指针存储位置（每 irq_num 一个 slot）
 * @param ops      同步策略；NULL 表示单线程简化路径（跳过 enter/exit/synchronize）
 * @param irq_num  逻辑中断号（透传给 ops->synchronize）
 * @param handler  非空
 * @param arg      任意
 * @param out_became_first 输出参数（可为 NULL）：本次是否是该 irq 的首个 handler；
 *                         为 true 表示调用者需要向底层注册 wrapper
 * @return WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NO_MEM
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_shared_chain_append(pal_shared_chain_t **slot,
                                       const pal_shared_chain_sync_ops_t *ops,
                                       uint32_t irq_num,
                                       pal_irq_shared_handler_t handler,
                                       void *arg,
                                       bool *out_became_first);

/**
 * @brief 遍历 chain 调用所有 handler（v2.0 语义：不提前终止）。
 *
 * 由 target 侧 ISR wrapper 调用；本函数**不获取任何锁**——依赖 RCU 读端安全
 * （ESP32：调用方前面已经原子读取过 chain 指针；wasm/host：单线程无并发）。
 *
 * @param chain 责任链指针（NULL 时返回 0）
 * @return 认领次数（供 wrapper 统计使用）
 */
uint32_t pal_shared_chain_dispatch(const pal_shared_chain_t *chain);

#ifdef __cplusplus
}
#endif

#endif /* PAL_SHARED_CHAIN_H */
