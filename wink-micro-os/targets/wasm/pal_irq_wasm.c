/**
 * @file pal_irq_wasm.c
 * @brief Wasm 仿真端 PAL IRQ 适配（GPIO 边沿 + 软中断 FIFO + 统一分发器 + 锁嵌套）。
 *        由 pal_hal_wasm.c 拆出（契约不变：仅物理位置调整）。
 *        HAL 见 pal_hal_wasm.c；OSAL 见 pal_osal_wasm.c；JS 契约见 wasm_bridge.h。
 *
 * 中断桥（方案 C：Poll 模型，Phase C P0-1 统一后）：
 *
 *   ┌──────────────────┬──────────────────────┬───────────────────────┐
 *   │ IRQ 源           │ 入队                 │ 分发点                │
 *   ├──────────────────┼──────────────────────┼───────────────────────┤
 *   │ GPIO 边沿        │ JS 侧 InterruptQueue │ pal_wasm_dispatch_    │
 *   │ (PinArbiter 检测)│ (FIFO, poll 模型)    │ pending_interrupts()  │
 *   ├──────────────────┼──────────────────────┼───────────────────────┤
 *   │ pal_irq_set_     │ C 侧 s_pending_queue │ pal_wasm_dispatch_    │
 *   │ pending() 软中断 │ (FIFO，无人工延迟)   │ pending_irqs()        │
 *   └──────────────────┴──────────────────────┴───────────────────────┘
 *
 * 两条分发路径都尊重 s_irq_lock_nest_count（pal_irq_save/restore 临界区）：
 *   - pal_sim_scheduler_run Phase 0 在 main 上下文调用 pal_wasm_dispatch_pending_
 *     interrupts()，该函数内部 drain 完 JS 队列后级联调用 pal_wasm_dispatch_pending_
 *     irqs() drain C 软中断 FIFO。此时不持有任何 IRQ 锁，所有 pending 正常兑现；
 *   - pal_irq_restore() 最外层 unlock（nest_count 从 1→0）时**补发**同一个入口
 *     pal_wasm_dispatch_pending_interrupts()，保证临界区内累积的 pending 在锁释
 *     放瞬间被立刻兑现，而不是拖延到下一次调度 tick，匹配 ESP32/host 目标
 *     "中断在开中断瞬间立刻派发" 的语义。
 *
 * ISR 执行流（所有路径同一入口、同一顺序 drain）：
 *   scheduler Phase 0 / restore 最外层 → pal_wasm_dispatch_pending_interrupts()
 *       ├─ drain JS InterruptQueue (GPIO 边沿等外部事件)
 *       └─ pal_wasm_dispatch_pending_irqs() → drain C 软中断 FIFO
 *
 * 派发顺序固定为 "外部 IRQ → 软中断"，与 ESP32 中断优先级语义对齐，所有调
 * 用路径统一经过同一入口保证可预测性。
 *
 * 删除的 Mechanism A legacy：
 *   - pal_wasm_gpio_level_changed()   （无任何 caller，GPIO 边沿全走 JS Poll）
 *   - s_gpio_last_level[]             （仅上述函数使用）
 *   - Pareto 延迟模型（pseudo_rand / calc_target_tick / sort_pending_by_priority）
 *     （原用于模拟"硬件 flash cache miss 长尾延迟"，与 ADR-0013 Poll 模型冲突
 *      —— Poll 模型本身在 tick 边界 drain，延迟已经是确定性的 O(1 tick)）
 */
#include "pal_hal.h"
#include "pal_osal.h"
#define WINK_ALLOW_ADVANCED_IRQ_APIS
#include "pal_irq_advanced.h"
#include "wasm_bridge.h"
#include "pal_wasm_internal.h"

#if defined(__EMSCRIPTEN__)

/* P2-4: wasm64 迁移门控 —— 本文件 js_pal_poll_interrupt 的 outCbPtr/outArgPtr
 * 以 u32 写入线性内存，pal_gpio_enable_interrupt_ex 对 callback/arg 做
 * (uint32_t)(uintptr_t) 截断；开启 wasm64 编译时此 _Static_assert 会立刻红。
 * 迁移时需同步：(1) wasm_bridge.h ABI 契约 #5 更新为 64-bit 指针 ABI；
 *              (2) JS 侧 writeU32LE → writeU64LE，BigInt 化；
 *              (3) 所有 (uint32_t)(uintptr_t) 截断改为全宽度。 */
_Static_assert(sizeof(void*) == 4,
    "wasm64 migration required: see wasm_bridge.h ABI 契约 #5 "
    "and review every (uint32_t)(uintptr_t) cast in pal_irq_wasm.c / createUnisimImports.ts");

/* ─────────────────────────────────────────────────────────
 * WASM GPIO 中断注册表
 * ───────────────────────────────────────────────────────── */
#define WASM_MAX_GPIO_PIN  50
static pal_gpio_isr_t s_gpio_isr[WASM_MAX_GPIO_PIN] = {NULL};
static void           *s_gpio_isr_arg[WASM_MAX_GPIO_PIN] = {NULL};
static pal_gpio_intr_t s_gpio_intr_type[WASM_MAX_GPIO_PIN] = {PAL_GPIO_INTR_DISABLE};

/* v2.2 G3（Phase 1.5，2026-07-01）：GPIO service 首次锁定的 prio。
 * WASM 是单线程执行环境（无 SharedArrayBuffer / pthread），无需 mutex；
 * 保留与 ESP32/host 对齐的双字段结构，语义一致。 */
static bool             s_gpio_service_initialized = false;
static pal_irq_prio_t   s_gpio_service_prio        = PAL_IRQ_PRIO_NORMAL;

/* ─────────────────────────────────────────────────────────
 * 统一 IRQ 锁（嵌套计数，GPIO/软中断共用）
 * ───────────────────────────────────────────────────────── */
static uint32_t s_irq_lock_nest_count = 0;

/* ─────────────────────────────────────────────────────────
 * C 侧软中断 pending FIFO（pal_irq_set_pending 专用）
 *
 * Phase C P0-1：移除旧 Pareto 延迟模型和 sort。软中断被 set_pending 后立刻
 * 进入 FIFO，在下次 dispatch 点（Phase 0 / restore 最外层）按入队顺序派发。
 * GPIO 边沿中断统一走 JS Poll 队列，不再进入此 C 侧队列。
 *
 * 溢出策略：环形队列满时丢最老（head 推进一格）——软中断是 level-like 语义，
 * 最新 pending 值代表"当前需要服务"的状态，覆盖过期条目是正确的（对比：
 * JS InterruptQueue 对 GPIO 边沿 event-like 中断默认 drop-newest，因为
 * 边沿事件是离散脉冲，丢新不丢老保留因果序列）。s_pending_overflow_count
 * 单调累计溢出次数，供 CI/调试诊断 "WASM_MAX_PENDING 是否需要调大"。
 * ───────────────────────────────────────────────────────── */
#define WASM_MAX_PENDING  64

typedef struct {
    uint32_t irq_num;
} wasm_pending_irq_t;

static wasm_pending_irq_t s_pending_queue[WASM_MAX_PENDING];
static uint32_t s_pending_head = 0;          /* 下一个待派发索引 */
static uint32_t s_pending_count = 0;         /* 队列中元素数 */
static uint32_t s_pending_overflow_count = 0;/* 溢出事件累计（诊断用，单调递增）*/

static inline void sw_enqueue(uint32_t irq_num) {
    if (s_pending_count >= WASM_MAX_PENDING) {
        /* 队列满：推进 head 丢弃最老条目，腾出尾部空间 */
        s_pending_head = (s_pending_head + 1) % WASM_MAX_PENDING;
        s_pending_count--;
        s_pending_overflow_count++;
    }
    uint32_t tail = (s_pending_head + s_pending_count) % WASM_MAX_PENDING;
    s_pending_queue[tail].irq_num = irq_num;
    s_pending_count++;
}

static inline bool sw_dequeue(uint32_t *out_irq) {
    if (s_pending_count == 0) return false;
    *out_irq = s_pending_queue[s_pending_head].irq_num;
    s_pending_head = (s_pending_head + 1) % WASM_MAX_PENDING;
    s_pending_count--;
    return true;
}

/* ─────────────────────────────────────────────────────────
 * GPIO 中断接口实现（WASM 平台）
 * ───────────────────────────────────────────────────────── */

wink_status_t pal_gpio_enable_interrupt_ex(wink_pin_t pin, pal_gpio_intr_t intr_type,
                                         pal_irq_prio_t prio, pal_gpio_isr_t callback, void *arg)
{
    if (pin < 0 || pin >= WASM_MAX_GPIO_PIN) {
        return WINK_ERR_INVALID_ARG;
    }
    if (callback == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (prio < PAL_IRQ_PRIO_LOW || prio > PAL_IRQ_PRIO_HIGH) {
        return WINK_ERR_INVALID_ARG;
    }


    /* v2.2 G3：GPIO service 首次锁定 prio。WASM 单线程无 mutex 需求，
     * 直接检查即可。一旦锁定，进程生命周期内不再释放（见 pal_hal.h 契约）。 */
    if (s_gpio_service_initialized) {
        if (prio != s_gpio_service_prio) {
            return WINK_ERR_INVALID_ARG;   /* G3: prio 冲突，本次拒接 */
        }
    } else {
        s_gpio_service_prio        = prio;
        s_gpio_service_initialized = true;
    }

    s_gpio_isr[pin] = callback;
    s_gpio_isr_arg[pin] = arg;
    s_gpio_intr_type[pin] = intr_type;

    /* 注册到 JS 侧（仅写映射关系，实际中断由 PinArbiter 边沿检测 push 到
     * InterruptQueue，wakeup 时由 pal_wasm_dispatch_pending_interrupts drain）。 */
    uint32_t callback_index = (uint32_t)(uintptr_t)callback;
    uint32_t arg_ptr        = (uint32_t)(uintptr_t)arg;
    js_pal_register_interrupt((uint32_t)pin, callback_index, arg_ptr);

    return WINK_OK;
}

wink_status_t pal_gpio_disable_interrupt(wink_pin_t pin)
{
    if (pin < 0 || pin >= WASM_MAX_GPIO_PIN) {
        return WINK_ERR_INVALID_ARG;
    }

    s_gpio_isr[pin] = NULL;
    s_gpio_intr_type[pin] = PAL_GPIO_INTR_DISABLE;
    js_pal_deregister_interrupt((uint32_t)pin);

    return WINK_OK;
}

/* P1-P5-10: wasm 单线程模型，disable 返回时保证无 ISR 正在执行，
 * synchronize_interrupt 语义等价于 no-op（仅做参数合法性校验）。 */
wink_status_t pal_gpio_synchronize_interrupt(wink_pin_t pin)
{
    if (pin < 0 || pin >= WASM_MAX_GPIO_PIN) {
        return WINK_ERR_INVALID_ARG;
    }
    return WINK_OK;
}

/* ─────────────────────────────────────────────────────────
 * 逻辑中断核心接口（WASM 仿真实现）
 * ───────────────────────────────────────────────────────── */

#define WASM_MAX_IRQ  32
static pal_isr_t s_wasm_irq_table[WASM_MAX_IRQ] = {NULL};
static void *s_wasm_irq_arg[WASM_MAX_IRQ] = {NULL};

wink_status_t pal_irq_enable(uint32_t irq_num, pal_irq_prio_t prio,
                              pal_isr_t handler, void *arg)
{
    /* P1-P5-9: prio 使用命名常量边界（PAL_IRQ_PRIO_LOW..HIGH）而非 magic 0，
     * 与 ESP32 / host target 保持一致。 */
    if (irq_num >= WASM_MAX_IRQ || handler == NULL ||
        prio < PAL_IRQ_PRIO_LOW || prio > PAL_IRQ_PRIO_HIGH) {
        return WINK_ERR_INVALID_ARG;
    }

    s_wasm_irq_table[irq_num] = handler;
    s_wasm_irq_arg[irq_num] = arg;
    return WINK_OK;
}

wink_status_t pal_irq_disable(uint32_t irq_num)
{
    if (irq_num >= WASM_MAX_IRQ) {
        return WINK_ERR_INVALID_ARG;
    }
    s_wasm_irq_table[irq_num] = NULL;
    s_wasm_irq_arg[irq_num] = NULL;
    return WINK_OK;
}

void pal_irq_set_pending(uint32_t irq_num)
{
    if (irq_num < WASM_MAX_IRQ && s_wasm_irq_table[irq_num] != NULL) {
        sw_enqueue(irq_num);
    }
}

void pal_irq_clear_pending(uint32_t irq_num)
{
    (void)irq_num;
    /* WASM 简化实现：FIFO 语义下没有"从中间移除"的高效操作；
     * 分发时若发现 handler 已被 pal_irq_disable 置 NULL，跳过该条目。
     * （旧注释声称"表头为 NULL 则跳过"但未实现——现已在 dispatch 中落实。） */
}

void pal_irq_synchronize(uint32_t irq_num)
{
    (void)irq_num;
    /* WASM 单线程模型，disable 后保证无 ISR 正在执行，无需同步 */
}

/* ─────────────────────────────────────────────────────────
 * 中断锁实现（WASM 仿真精确语义匹配）
 * ───────────────────────────────────────────────────────── */

uint32_t pal_irq_save(void)
{
    /* 记录锁之前的状态（支持嵌套） */
    uint32_t was_enabled = (s_irq_lock_nest_count == 0) ? 1 : 0;
    s_irq_lock_nest_count++;
    return was_enabled;
}

uint32_t pal_irq_save_rtos_safe(void)
{
    /* WASM 单线程模型下，rtos_safe 与全屏蔽行为一致
     * 真实 ESP32 上才区分优先级屏蔽，但 WASM 仿真简化处理 */
    return pal_irq_save();
}

void pal_irq_restore(uint32_t mask)
{
    if (s_irq_lock_nest_count > 0) {
        s_irq_lock_nest_count--;
        /* 只有最外层的 restore 才真正恢复中断并补发所有 pending（P0-1 修复点）。
         * mask == 1 表示 save 之前是未加锁状态。
         *
         * 统一入口：与调度器 Phase 0 使用完全相同的 pal_wasm_dispatch_pending_
         * interrupts()，该入口内部级联 drain JS FIFO → C 软中断 FIFO，保证两条
         * 路径派发顺序完全一致（外部 IRQ 先于软中断），避免顺序差异导致的时序
         * heisenbug。*/
        if (s_irq_lock_nest_count == 0 && mask) {
            pal_wasm_dispatch_pending_interrupts();
        }
    }
}

/**
 * @brief 分发 C 侧软中断 FIFO（pal_irq_set_pending 入队）。
 *
 * 调用方：仅由 pal_wasm_dispatch_pending_interrupts() 在 JS 队列 drain 完后级联
 * 调用——所有 dispatch 路径（Phase 0 / pal_irq_restore 最外层 unlock）都经过
 * pal_wasm_dispatch_pending_interrupts() 单一入口，保证派发顺序 JS→C 全局一致。
 *
 * 中断锁语义：如果当前持有 IRQ 锁（s_irq_lock_nest_count > 0），直接返回——
 * 派发推迟到 pal_irq_restore() 最外层 unlock 时执行（入口函数同样检查锁）。
 *
 * 分发时若发现 irq_num 对应的 handler 已被 pal_irq_disable 置 NULL（clear_pending
 * 的实际语义），该条目被静默丢弃——避免 disable 后还触发 stale ISR。
 */
void pal_wasm_dispatch_pending_irqs(void)
{
    if (s_irq_lock_nest_count > 0) {
        return;
    }

    uint32_t irq_num;
    while (sw_dequeue(&irq_num)) {
        if (irq_num < WASM_MAX_IRQ && s_wasm_irq_table[irq_num] != NULL) {
            pal_os_set_sim_isr_context(true);
            s_wasm_irq_table[irq_num](s_wasm_irq_arg[irq_num]);
            pal_os_set_sim_isr_context(false);
        }
        /* 如果 handler 被 disable 置 NULL：静默丢（pal_irq_clear_pending 语义）*/
    }
}

/**
 * @brief 分发 JS 侧 Poll 队列（GPIO 边沿等外部中断，方案 C）。
 *
 * 循环调用 js_pal_poll_interrupt 直到队列为空（FIFO 顺序），对每个 pending 条目
 * 将 callback_index 还原为函数指针并调用 ISR。
 *
 * Phase C P0-1 关键修复：开头检查 s_irq_lock_nest_count，持有 IRQ 锁期间直接
 * 返回（JS 队列里的条目保留在 JS 侧，下次 drain 自然拿到）。pal_irq_restore()
 * 最外层 unlock 会重新调用本函数补发，保证临界区内到达的外部中断在锁释放瞬间
 * 被立刻兑现，而不是被拖延到下一次调度器 tick——匹配 ESP32 "开中断瞬间 pending
 * IRQ 立刻派发" 的语义。
 *
 * 调用方：
 *   1. pal_sim_scheduler_run Phase 0（每个 tick 开头，main 调度器上下文，
 *      此时无任何 IRQ 锁持有，正常 drain）；
 *   2. pal_irq_restore() 最外层 unlock（补发）。
 */
void pal_wasm_dispatch_pending_interrupts(void) {
    if (s_irq_lock_nest_count > 0) {
        return;
    }

    uint32_t callback_index;
    uint32_t arg_ptr;
    /* drain 所有 pending 中断（FIFO）直到队列空 */
    while (js_pal_poll_interrupt(&callback_index, &arg_ptr)) {
        pal_gpio_isr_t isr = (pal_gpio_isr_t)(uintptr_t)callback_index;
        if (isr != NULL) {
            pal_os_set_sim_isr_context(true);
            isr((void *)(uintptr_t)arg_ptr);
            pal_os_set_sim_isr_context(false);
        }
    }

    /* JS 外部 IRQ drain 完成后，顺带 drain C 侧软中断 FIFO——保证
     * pal_irq_set_pending 在 ISR 内 self-retrigger 的路径能在同一个
     * dispatch 回合被派发，而不是拖到下一 tick。*/
    pal_wasm_dispatch_pending_irqs();
}

#endif /* __EMSCRIPTEN__ */
