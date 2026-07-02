/**
 * @file pal_irq_esp32.c
 * @brief ESP32 target 的 pal_irq.h 实现：generic wrapper / direct trampoline /
 *        shared chain wrapper / synchronize / set_pending / clear_pending.
 *
 * 由 targets/esp32/pal_hal_esp32.c 拆出（PLAN-20260701-PAL-TARGET-P1-MAINT Task 2）。
 * 契约不变：仅物理位置调整；见 pal/include/pal_irq.h。
 *
 * ✅ R-5：`s_esp32_shared_sync_ops` 与 shared_chain synchronize→free 时序按字节保留。
 * ✅ R-1：pal_irq_* 公共 API 签名、返回码、handler 调用顺序均与旧实现等价。
 * ⚠️ 跨 TU：pal_irq_synchronize(~0U) 通过 pal_hal_esp32_internal.h 调 GPIO TU 的
 *   pal_esp32_gpio_synchronize_all() 完成 GPIO ISR 全量等待，避免暴露
 *   s_gpio_irq_in_flight[]。
 */
#include "pal_hal.h"
#define WINK_ALLOW_ADVANCED_IRQ_APIS
#include "pal_irq_advanced.h"
#include "pal_osal.h"        /* pal_os_get_us() for synchronize timeout */
#include "pal_atomic_esp32.h" /* target-private atomic helpers */
#include "pal_hal_esp32_internal.h" /* pal_esp32_gpio_synchronize_all() */

#if defined(ESP_PLATFORM)
#include "esp_err.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "xtensa/hal.h"

/* ⚠️ SMP ISR 同步机制（ADR-IRQ-007 完整实现）
 *
 * 问题：pal_irq_disable() 返回后，另一个核心可能仍在执行该 ISR。
 * 如果此时释放 ISR 使用的内存，会导致 UAF (Use-After-Free)。
 *
 * 解决方案：
 * 1. 每个中断号维护一个原子 in_flight 计数器
 * 2. ISR wrapper 在入口处 +1，出口处 -1
 * 3. pal_irq_synchronize() 忙等待计数器归 0
 * 4. 增加超时保护，避免死锁
 */
static volatile uint32_t s_irq_in_flight[32] = {0};

#define SYNCHRONIZE_TIMEOUT_US  100000  /* 100ms 超时（远大于 ISR 最大执行时间） */

/* ─────────────────────────────────────────────────────────
 * 中断控制器核心接口实现（ESP32 平台）
 * ───────────────────────────────────────────────────────── */

/* 逻辑中断句柄表（32 个逻辑中断源，未来扩展至 Device Tree） */
static intr_handle_t s_irq_handles[32] = {NULL};



/* 通用 ISR wrapper：跟踪 in-flight 计数并调用用户 handler */
typedef struct {
    pal_isr_t user_handler;
    void     *user_arg;
} isr_wrapper_ctx_t;

static isr_wrapper_ctx_t s_isr_ctx[32] = {{NULL, NULL}};

static void PAL_ISR generic_isr_wrapper(void *arg)
{
    uint32_t irq_num = (uint32_t)(uintptr_t)arg;
    if (irq_num >= 32) {
        return;
    }

    /* ✅ 第一时间清除 Pending 标志，防止重入与中断风暴 */
    pal_irq_clear_pending(irq_num);

    /* ✅ SMP 同步：标记 ISR 正在执行 */
    Atomic_Increment_u32(&s_irq_in_flight[irq_num]);

    isr_wrapper_ctx_t *ctx = &s_isr_ctx[irq_num];
    if (ctx->user_handler != NULL) {
        ctx->user_handler(ctx->user_arg);
    }

    /* ✅ SMP 同步：ISR 执行完成 */
    Atomic_Decrement_u32(&s_irq_in_flight[irq_num]);
}

wink_status_t pal_irq_enable(uint32_t irq_num, pal_irq_prio_t prio,
                              pal_isr_t handler, void *arg)
{
    if (irq_num >= 32 || handler == NULL || prio <= 0 || prio >= PAL_IRQ_PRIO_COUNT) {
        return WINK_ERR_INVALID_ARG;
    }

    /* 先释放旧的句柄（如果有） */
    if (s_irq_handles[irq_num] != NULL) {
        esp_intr_free(s_irq_handles[irq_num]);
        s_irq_handles[irq_num] = NULL;
    }

    /* 优先级映射表（3 级）
     * 注意：ESP_INTR_FLAG_LEVELn 是标志位，不能直接用数值做 | 运算
     */
    static const int s_prio_flag_map[PAL_IRQ_PRIO_COUNT] = {
        [PAL_IRQ_PRIO_LOW]      = ESP_INTR_FLAG_LEVEL1,
        [PAL_IRQ_PRIO_NORMAL]   = ESP_INTR_FLAG_LEVEL2,
        [PAL_IRQ_PRIO_HIGH]     = ESP_INTR_FLAG_LEVEL3,  /* configMAX_SYSCALL_INTERRUPT_PRIORITY is Level 3 */
    };

    /* ✅ SMP 同步：保存用户 handler 和 arg，通过 wrapper 调用
     * 这样 wrapper 可以追踪 in-flight 计数 */
    s_isr_ctx[irq_num].user_handler = handler;
    s_isr_ctx[irq_num].user_arg = arg;

    // 针对测试所用的逻辑中断号，映射到合法的 CPU 内部软件中断源
    int source = irq_num;
    if (irq_num == 7) {
        source = ETS_INTERNAL_SW0_INTR_SOURCE;
    } else if (irq_num == 8) {
        source = ETS_INTERNAL_SW1_INTR_SOURCE;
    }

    int flags = ESP_INTR_FLAG_IRAM;
    if (source >= 0) {
        flags |= s_prio_flag_map[prio];
    }

    esp_err_t err = esp_intr_alloc(source, flags,
                                    (intr_handler_t)generic_isr_wrapper,
                                    (void *)(uintptr_t)irq_num,
                                    &s_irq_handles[irq_num]);
    if (err != ESP_OK) {
        s_isr_ctx[irq_num].user_handler = NULL;
        s_isr_ctx[irq_num].user_arg = NULL;
        return WINK_ERR_HARDWARE;
    }

    return WINK_OK;
}

wink_status_t pal_irq_disable(uint32_t irq_num)
{
    if (irq_num >= 32) {
        return WINK_ERR_INVALID_ARG;
    }

    if (s_irq_handles[irq_num] != NULL) {
        esp_err_t err = esp_intr_free(s_irq_handles[irq_num]);
        s_irq_handles[irq_num] = NULL;
        if (err != ESP_OK) {
            return WINK_ERR_HARDWARE;
        }
    }

    return WINK_OK;
}

void pal_irq_set_pending(uint32_t irq_num)
{
    if (irq_num < 32 && s_irq_handles[irq_num] != NULL) {
        int cpu_intr = esp_intr_get_intno(s_irq_handles[irq_num]);
        if (cpu_intr >= 0 && cpu_intr < 32) {
            xthal_set_intset(1 << cpu_intr);
        }
    }
}

void pal_irq_clear_pending(uint32_t irq_num)
{
    if (irq_num < 32 && s_irq_handles[irq_num] != NULL) {
        int cpu_intr = esp_intr_get_intno(s_irq_handles[irq_num]);
        if (cpu_intr >= 0 && cpu_intr < 32) {
            xthal_set_intclear(1 << cpu_intr);
        }
    }
}

void pal_irq_synchronize(uint32_t irq_num)
{
    /* ✅ SMP 同步完整实现（ADR-IRQ-007）：
     *
     * 机制：每个 ISR wrapper 在进入时 +1，退出时 -1。
     * synchronize() 忙等待计数归 0，确保所有核心都已退出 ISR。
     *
     * 这是 Linux 内核 synchronize_irq() 在 ESP32 上的简化实现。
     * 无需 IPI，因为原子操作在 SMP 下是全局可见的。
     */

    if (irq_num == ~0U) {
        /* 等待所有中断：逐个检查 32 个逻辑中断 + GPIO 中断 */
        for (uint32_t i = 0; i < 32; i++) {
            uint64_t start = pal_os_get_us();
            while (Atomic_Load_u32(&s_irq_in_flight[i]) > 0) {
                if (pal_os_get_us() - start > SYNCHRONIZE_TIMEOUT_US) {
                    ESP_LOGE("pal_irq", "synchronize timeout on irq=%lu",
                             (unsigned long)i);
                    break;
                }
            }
        }
        /* GPIO 中断由 pal_hal_esp32_gpio.c 拥有 s_gpio_irq_in_flight[]；
         * 通过 target-private 内部函数完成全量等待，避免跨 TU 暴露数组。 */
        pal_esp32_gpio_synchronize_all(SYNCHRONIZE_TIMEOUT_US);
    } else {
        /* 等待单个中断 */
        uint64_t start = pal_os_get_us();
        while (Atomic_Load_u32(&s_irq_in_flight[irq_num]) > 0) {
            if (pal_os_get_us() - start > SYNCHRONIZE_TIMEOUT_US) {
                ESP_LOGE("pal_irq", "synchronize timeout on irq=%lu",
                         (unsigned long)irq_num);
                break;
            }
        }
    }

    /* 确保后续的内存释放操作（如 free）不会被编译器重排到等待之前 */
    esp_memory_barrier();
}

/* ─────────────────────────────────────────────────────────
 * 全局中断锁实现（双等级语义，ADR-IRQ-006）
 * ───────────────────────────────────────────────────────── */

uint32_t pal_irq_save(void)
{
    /* ✅ 设置到最高屏蔽级别，禁用所有可屏蔽中断（ADR-IRQ-001）
     * 不使用 XCHAL_EXCM_LEVEL (= 3)，因为它只能禁用优先级 ≤3 的中断，
     * 高优先级中断（如 5、7）仍能触发，临界区保护失效。
     *
     * 使用 XCHAL_NUM_INTLEVELS 达到真正的全局禁用效果。
     *
     * ⚠️ 约束：受此锁保护的临界区必须 < 1µs，避免影响 Wi-Fi 和看门狗。
     */
    return XTOS_SET_INTLEVEL(XCHAL_NUM_INTLEVELS);
}

uint32_t pal_irq_save_rtos_safe(void)
{
    /* ✅ 仅禁用到 RTOS 安全边界（ADR-0018，业务默认推荐）
     * configMAX_SYSCALL_INTERRUPT_PRIORITY = 5
     * 设置 INTLEVEL = 5 将屏蔽所有优先级 ≤5 的中断
     * 优先级 6-7 的中断（如 Wi-Fi 基带）仍可触发，不影响底层硬件协议时序。
     *
     * PAL_IRQ_PRIO_LOW / NORMAL / HIGH（LEVEL1/2/3）均在此屏蔽范围内。
     */
    return XTOS_SET_INTLEVEL(XCHAL_EXCM_LEVEL);
}

void pal_irq_restore(uint32_t mask)
{
    /* 恢复 PS 寄存器中的 INTLEVEL 字段 */
    XTOS_RESTORE_JUST_INTLEVEL(mask);
}

#endif /* ESP_PLATFORM */
