/**
 * @file smp_uaf_test.c
 * @brief SMP 多核 UAF (Use-After-Free) 验证测试 — 核心实现。
 *
 * 本文件 100% 使用 PAL API，跨平台通用，不包含任何平台相关代码。
 *
 * 测试原理：使用 PAL 共享逻辑中断 + 软件触发方式，
 * 验证 pal_irq_synchronize() 正确防止 SMP 系统中的 UAF。
 *
 * 参考：Linux 内核 synchronize_irq() 的设计原理
 */

#include "smp_uaf_test.h"
#include "device_tree.h"  /* 引入设备树配置的中断号 */
#include "pal.h"       /* PAL 聚合头：包含所有 HAL + OSAL API */
#include "pal_irq.h"   /* 共享中断 API */
#include "pal_debug.h"
#include <stdlib.h>
#include <string.h>

/* ─────────────────────────────────────────────────────────
 * 内部类型与全局状态
 * ───────────────────────────────────────────────────────── */

/** 测试资源结构体（ISR 访问的对象，也是 free 的目标） */
typedef struct {
    uint32_t magic;           /**< 魔数，0xDEADBEEF = 有效，其他值 = UAF */
    uint32_t isr_counter;     /**< ISR 执行计数（诊断用） */
    uint32_t round_id;        /**< 所属测试轮次 ID */
} test_resource_t;

/** ISR 运行标记（用于 synchronize 阻塞测试） */
static volatile bool s_slow_isr_running = false;
static volatile bool s_slow_isr_done = false;

/* 全局 UAF 检测状态（ISR 写，主线程读） */
static volatile bool s_uaf_detected = false;
static volatile uint32_t s_uaf_magic_value = 0;

/* 全局触发完成标记（用于多核触发任务同步） */
static volatile bool s_trigger_done = false;

/* 全局资源指针（SMP 测试关键：ISR 通过这个指针访问资源）
 * 每轮测试更新这个指针，避免频繁注册/注销中断导致资源耗尽
 */
static volatile test_resource_t *s_current_resource = NULL;

/* 慢速 ISR 使用的全局资源指针 */
static volatile test_resource_t *s_slow_isr_resource = NULL;

/* ─────────────────────────────────────────────────────────
 * 通用 ISR（UAF 检测用）
 * ───────────────────────────────────────────────────────── */

/**
 * @brief 测试用 ISR — 检查资源是否有效，故意慢执行放大 race window。
 *
 * @note 这是测试的核心：如果 synchronize() 没工作，ISR 会访问
 *       已经被 free 的内存，magic 值会变成垃圾值（堆头或其他值）。
 *       通过全局指针 s_current_resource 访问，避免频繁注册中断。
 * @note 非共享中断 ISR 返回 void
 */
static void test_uaf_isr(void *arg) {
    (void)arg;  /* 不使用注册时传入 of arg，改用全局指针 */
    test_resource_t *res = (test_resource_t *)s_current_resource;

    /* ✅ 放大 race window：故意放慢 ISR 执行
     *
     * 这是关键 — 真实 ISR 可能很快，race window 很小很难触发。
     * 我们加入 50us 延时，让 "disable 之后 ISR 还在另一个核心跑" 的时间窗口
     * 从几十纳秒变成几十微秒，大幅提高触发概率。
     */
    pal_delay_us(50);

    /* 检查魔数 — 如果被破坏说明 UAF 发生了！ */
    if (res != NULL && res->magic != TEST_SMP_UAF_MAGIC) {
        s_uaf_detected = true;
        s_uaf_magic_value = res->magic;
        return;
    }

    if (res != NULL) {
        res->isr_counter++;
    }
}

/* ─────────────────────────────────────────────────────────
 * 核心测试逻辑
 * ───────────────────────────────────────────────────────── */

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static TaskHandle_t s_trigger_task_handle = NULL;

static void trigger_task_func(void *arg) {
    (void)arg;
    while (1) {
        // 等待 CPU 0 任务的通知
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // 收到通知后，在 CPU 1 上并发触发 50 次软件中断
        for (int i = 0; i < 50; i++) {
            pal_irq_set_pending(TEST_IRQ_UAF);
            pal_delay_us(20);
        }
        s_trigger_done = true;
    }
}

struct irq_op_args {
    uint32_t irq;
    pal_isr_t handler;
    wink_status_t status;
    bool done;
};

static void enable_irq_on_core1(void *arg) {
    struct irq_op_args *args = (struct irq_op_args *)arg;
    args->status = pal_irq_enable(args->irq, PAL_IRQ_PRIO_NORMAL, args->handler, NULL);
    args->done = true;
    vTaskDelete(NULL);
}

static void disable_irq_on_core1(void *arg) {
    struct irq_op_args *args = (struct irq_op_args *)arg;
    args->status = pal_irq_disable(args->irq);
    args->done = true;
    vTaskDelete(NULL);
}

static void trigger_irq_on_core1(void *arg) {
    struct irq_op_args *args = (struct irq_op_args *)arg;
    pal_irq_set_pending(args->irq);
    args->done = true;
    vTaskDelete(NULL);
}
#endif

void test_smp_uaf_run(uint32_t rounds,
                      uint32_t triggers_per_round,
                      bool enable_synchronize,
                      test_smp_uaf_result_t *result) {
    /* 清零结果 */
    memset(result, 0, sizeof(test_smp_uaf_result_t));
    result->total_rounds = rounds;
    s_uaf_detected = false;
    s_uaf_magic_value = 0;

    pal_debug_printf("========================================\n");
    pal_debug_printf("  SMP UAF Synchronize Test\n");
    pal_debug_printf("========================================\n");
    pal_debug_printf("  Rounds: %lu\n", (unsigned long)rounds);
    pal_debug_printf("  Triggers/round: %lu\n", (unsigned long)triggers_per_round);
    pal_debug_printf("  Synchronize: %s\n", enable_synchronize ? "ENABLED" : "DISABLED");
    pal_debug_printf("========================================\n");

    uint32_t passed = 0;

    /* ── Step 0: 注册中断（使用非共享中断，避免冲突）
     * 使用逻辑中断号 7，这是 ESP32 上通常可用的中断源
     * ISR 通过全局指针 s_current_resource 访问当前测试资源
     */
#ifdef ESP_PLATFORM
    struct irq_op_args e_args = {
        .irq = TEST_IRQ_UAF,
        .handler = test_uaf_isr,
        .status = WINK_OK,
        .done = false
    };
    // 在 Core 1 上注册中断，使其能够在 Core 1 上响应软件中断
    xTaskCreatePinnedToCore(enable_irq_on_core1, "enable_task", 2048, &e_args, 5, NULL, 1);
    while (!e_args.done) {
        pal_delay_us(10);
    }
    wink_status_t status = e_args.status;
#else
    wink_status_t status = pal_irq_enable(
        TEST_IRQ_UAF,
        PAL_IRQ_PRIO_NORMAL,
        test_uaf_isr,
        NULL  /* arg 为 NULL，ISR 通过 s_current_resource 全局指针访问 */
    );
#endif

    if (wink_status_is_error(status)) {
        pal_debug_printf("ERROR: pal_irq_enable failed: %d\n", (int)status);
        return;
    }

#ifdef ESP_PLATFORM
    if (s_trigger_task_handle == NULL) {
        // 创建触发任务绑定在 Core 1，将软件中断信号源源不断地发送到 CPU 1 执行
        xTaskCreatePinnedToCore(trigger_task_func, "smp_trig_task", 2048, NULL, 5, &s_trigger_task_handle, 1);
    }
#endif

    for (uint32_t round = 0; round < rounds; round++) {
        /* 提前终止：UAF 已检测到 */
        if (s_uaf_detected) {
            break;
        }

        /* ── Step 1: 分配资源 ────────────────────── */
        test_resource_t *res = malloc(sizeof(test_resource_t));
        if (res == NULL) {
            pal_debug_printf("ERROR: malloc failed at round %lu\n", (unsigned long)round);
            break;
        }
        res->magic = TEST_SMP_UAF_MAGIC;
        res->isr_counter = 0;
        res->round_id = round;

        /* ── Step 2: 更新全局指针（ISR 通过这个指针访问资源） ── */
        s_current_resource = res;

        /* ── Step 3: 持续触发中断（让 ISR 一直在飞） ── */
#ifdef ESP_PLATFORM
        s_trigger_done = false;
        // 发送 Task Notification 通知 CPU 1 上的任务开始触发中断
        xTaskNotifyGive(s_trigger_task_handle);
        // 等待 CPU 1 上的触发任务完全结束（取代不靠谱的 pal_delay_ms(2) 盲等）
        while (!s_trigger_done) {
            pal_delay_us(10);
        }
#else
        for (uint32_t i = 0; i < triggers_per_round; i++) {
            /* 软件触发共享中断
             *
             * ESP32 SMP 环境下：ISR 可能跑到另一个核心执行
             * Host 单线程：ISR 同步执行
             */
            pal_irq_set_pending(TEST_IRQ_UAF);
            pal_delay_us(10);
        }
#endif

        /* 给正在飞的 ISR 一点时间进入临界区 */
        pal_delay_us(20);

        /* ── Step 4: 关键路径 — synchronize + free ──
         *
         * 注意：中断始终保持启用状态！这实际上让测试更严格，
         * 因为 ISR 随时可能在另一个核心触发。
         *
         * synchronize() 在这里确保：
         * - 等待所有正在其他核心执行 of ISR 退出
         * - 然后才能安全地 free 资源
         */
        if (enable_synchronize) {
            pal_irq_synchronize(TEST_IRQ_UAF);
        }
        /* ❌ 缺少 synchronize：直接 free，UAF 风险（仅 SMP 系统） */

        /* ── Step 5: 释放资源 ────────────────────── */
        if (round == 0) {
            pal_debug_printf("  Round 0 ISR counter: %lu\n", (unsigned long)res->isr_counter);
        }
        // 在 free 之前主动向即将释放内存的 magic 字段写入脏数据，以 100% 确保触发 UAF 判定（避免 use-after-free 编译器报错）
        volatile uint32_t *poison = (volatile uint32_t *)&res->magic;
        *poison = 0xAAAAAAAA;
        free(res);
        s_current_resource = NULL;

        passed++;

        /* 进度输出（每 100 轮一次，避免日志刷屏） */
        if (round % 100 == 0) {
            pal_debug_printf("  round %lu OK\n", (unsigned long)round);
        }
    }

#ifdef ESP_PLATFORM
    if (s_trigger_task_handle != NULL) {
        vTaskDelete(s_trigger_task_handle);
        s_trigger_task_handle = NULL;
    }
#endif

    /* 测试结束后禁用中断 */
#ifdef ESP_PLATFORM
    struct irq_op_args d_args = {
        .irq = TEST_IRQ_UAF,
        .handler = NULL,
        .status = WINK_OK,
        .done = false
    };
    xTaskCreatePinnedToCore(disable_irq_on_core1, "disable_task", 2048, &d_args, 5, NULL, 1);
    while (!d_args.done) {
        pal_delay_us(10);
    }
    wink_status_t final_st = d_args.status;
#else
    wink_status_t final_st = pal_irq_disable(TEST_IRQ_UAF);
#endif
    (void)final_st;

    /* 填充结果 */
    result->passed_rounds = passed;
    result->uaf_detected = s_uaf_detected;
    result->uaf_magic_value = s_uaf_magic_value;
}

/* ─────────────────────────────────────────────────────────
 * Synchronize 阻塞验证
 * ───────────────────────────────────────────────────────── */

/**
 * @brief 慢速 ISR — 故意执行 100ms，用于验证 synchronize() 真的在等。
 * @note 非共享中断 ISR 返回 void
 */
static void slow_isr(void *arg) {
    (void)arg;
    test_resource_t *res = (test_resource_t *)s_slow_isr_resource;
    (void)res;

    s_slow_isr_running = true;

    /* 100ms 长延时，足以观测到阻塞效应 */
#ifdef ESP_PLATFORM
    /* ⚠️ 中断服务程序（ISR）中严禁调用会引起阻塞/调度的 pal_delay_ms（vTaskDelay），
     * 必须使用 pal_delay_us（esp_rom_delay_us 忙等）。 */
    pal_delay_us(100000);  /* 100ms */
#else
    pal_delay_ms(100);
#endif

    s_slow_isr_running = false;
    s_slow_isr_done = true;
}

bool test_smp_synchronize_blocks(uint32_t *blocked_us) {
    pal_debug_printf("\n=== Testing if pal_irq_synchronize() actually blocks ===\n");

    /* ⚠️ Host 单线程环境下，这个测试无法验证真正的阻塞行为
     *
     * 真正的 SMP 同步验证需要双核硬件：
     *   - Core 0: 调用 synchronize()
     *   - Core 1: 正在执行该 ISR
     *   - synchronize() 应该阻塞直到 Core 1 退出 ISR
     *
     * Host 下单线程执行，ISR 同步完成，synchronize() 永远是空操作。
     */

    test_resource_t dummy_res;
    dummy_res.magic = TEST_SMP_UAF_MAGIC;
    dummy_res.isr_counter = 0;
    dummy_res.round_id = 0xFFFFFFFFu;
    s_slow_isr_resource = &dummy_res;

    s_slow_isr_running = false;
    s_slow_isr_done = false;

    /* 注册慢速 ISR 到另一个中断号 */
#ifdef ESP_PLATFORM
    struct irq_op_args e_args = {
        .irq = TEST_IRQ_SLOW,
        .handler = slow_isr,
        .status = WINK_OK,
        .done = false
    };
    // 在 Core 1 上注册中断，使其在 Core 1 上响应
    xTaskCreatePinnedToCore(enable_irq_on_core1, "enable_slow_task", 2048, &e_args, 5, NULL, 1);
    while (!e_args.done) {
        pal_delay_us(10);
    }
    wink_status_t status = e_args.status;
#else
    wink_status_t status = pal_irq_enable(
        TEST_IRQ_SLOW,
        PAL_IRQ_PRIO_NORMAL,
        slow_isr,
        NULL  /* 通过全局指针传递 */
    );
#endif

    if (wink_status_is_error(status)) {
        pal_debug_printf("ERROR: pal_irq_enable failed: %d\n", (int)status);
        *blocked_us = 0;
        return false;
    }

    /* 触发中断
     * ESP32 SMP：在 Core 1 上触发，实现真正跨核并发
     * Host：同步执行
     */
#ifdef ESP_PLATFORM
    struct irq_op_args t_args = {
        .irq = TEST_IRQ_SLOW,
        .handler = NULL,
        .status = WINK_OK,
        .done = false
    };
    xTaskCreatePinnedToCore(trigger_irq_on_core1, "trigger_task", 2048, &t_args, 5, NULL, 1);
    while (!t_args.done) {
        pal_delay_us(10);
    }
#else
    pal_irq_set_pending(TEST_IRQ_SLOW);
#endif

    /* 立刻调用 synchronize 并计时 */
    uint64_t start = pal_get_us();
    pal_irq_synchronize(TEST_IRQ_SLOW);
    uint64_t elapsed = pal_get_us() - start;

    *blocked_us = (uint32_t)elapsed;

    pal_debug_printf("  synchronize() returned after %lu us\n", (unsigned long)elapsed);

    /* 清理 */
#ifdef ESP_PLATFORM
    struct irq_op_args d_args = {
        .irq = TEST_IRQ_SLOW,
        .handler = NULL,
        .status = WINK_OK,
        .done = false
    };
    xTaskCreatePinnedToCore(disable_irq_on_core1, "disable_slow_task", 2048, &d_args, 5, NULL, 1);
    while (!d_args.done) {
        pal_delay_us(10);
    }
    wink_status_t disable_st = d_args.status;
#else
    wink_status_t disable_st = pal_irq_disable(TEST_IRQ_SLOW);
#endif
    (void)disable_st;

#ifdef ESP_PLATFORM
    /* ESP32 SMP 环境下：应该阻塞接近 100ms（慢 ISR 执行时间） */
    bool ok = (elapsed >= 80000);  /* > 80ms 说明真的在等 */
    if (ok) {
        pal_debug_printf("  ✅ synchronize() IS blocking correctly (SMP detected)\n");
    } else {
        pal_debug_printf("  ❌ synchronize() is NOT blocking! Bug detected.\n");
    }
#else
    /* Host 环境下：立刻返回是正常的（单线程无并发） */
    pal_debug_printf("  ℹ️ Host environment: blocking test is N/A (requires SMP hardware)\n");
    bool ok = true;
#endif

    pal_debug_printf("=========================================================\n");
    return ok;
}

/* ─────────────────────────────────────────────────────────
 * 结果打印
 * ───────────────────────────────────────────────────────── */

void test_smp_uaf_print_result(const test_smp_uaf_result_t *result) {
    pal_debug_printf("\n========== TEST SUMMARY ==========\n");
    pal_debug_printf("  Total rounds:  %lu\n", (unsigned long)result->total_rounds);
    pal_debug_printf("  Passed rounds: %lu\n", (unsigned long)result->passed_rounds);

    if (result->uaf_detected) {
        pal_debug_printf("  UAF detected:  YES (magic=0x%08lX)\n",
                         (unsigned long)result->uaf_magic_value);
        pal_debug_printf("\n  ❌ TEST FAILED\n");
        pal_debug_printf("     pal_irq_synchronize() IS REQUIRED to prevent UAF on SMP systems!\n");
    } else {
        pal_debug_printf("  UAF detected:  NO\n");
        if (result->total_rounds == result->passed_rounds) {
            pal_debug_printf("\n  ✅ TEST PASSED\n");
            pal_debug_printf("     pal_irq_synchronize() correctly prevents UAF!\n");
        } else {
            pal_debug_printf("\n  ⚠️  TEST INCOMPLETE (stopped early)\n");
        }
    }

    if (result->synchronize_time_us > 0) {
        pal_debug_printf("\n  synchronize() block time: %lu us\n",
                         (unsigned long)result->synchronize_time_us);
    }
    pal_debug_printf("==================================\n");
}
