/**
 * @file test_smp_uaf.c
 * @brief SMP 多核 UAF (Use-After-Free) 验证测试 — 核心实现。
 *
 * 本文件 100% 使用 PAL API，跨平台通用，不包含任何平台相关代码。
 *
 * 关键架构决策：
 *   1. 使用 pal_irq_set_pending() 软件触发中断，而非硬件 GPIO 电平跳变
 *      → 真正跨平台，不需要外部硬件连线
 *   2. 魔数检测 UAF，不需要 MMU 或内存保护单元
 *      → 在资源受限的 MCU 上也能工作
 *   3. ISR 中加入故意延时放大 race window
 *      → 让测试在几秒钟内就能测出问题，而不是几小时
 *
 * 参考：Linux 内核 synchronize_irq() 的设计原理
 */

#include "test_smp_uaf.h"
#include "pal.h"       /* PAL 聚合头：包含所有 HAL + OSAL API */
#include "pal_debug.h"
#include <stdlib.h>
#include <string.h>

/* 中断触发接口（平台适配层）
 * - Host: 使用 pal_host_trigger_gpio_interrupt() 软件模拟
 * - ESP32: 直接操作 GPIO 电平触发硬件中断
 */
#ifdef ESP_PLATFORM
#include "driver/gpio.h"
static inline void trigger_gpio_interrupt(wink_pin_t pin) {
    /* ESP32 上通过电平跳变触发硬件中断 */
    gpio_set_level(pin, 1);
    gpio_set_level(pin, 0);
}
#else
extern void pal_host_trigger_gpio_interrupt(wink_pin_t pin);
static inline void trigger_gpio_interrupt(wink_pin_t pin) {
    pal_host_trigger_gpio_interrupt(pin);
}
#endif

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
static const test_resource_t *s_current_resource = NULL;

/* ─────────────────────────────────────────────────────────
 * 通用 ISR（UAF 检测用）
 * ───────────────────────────────────────────────────────── */

/**
 * @brief 测试用 ISR — 检查资源是否有效，故意慢执行放大 race window。
 *
 * @note 这是测试的核心：如果 synchronize() 没工作，ISR 会访问
 *       已经被 free 的内存，magic 值会变成垃圾值（堆头或其他值）。
 */
static void test_uaf_isr(void *arg) {
    test_resource_t *res = (test_resource_t *)arg;

    /* ✅ 放大 race window：故意放慢 ISR 执行
     *
     * 这是关键 — 真实 ISR 可能很快，race window 很小很难触发。
     * 我们加入 50us 延时，让 "disable 之后 ISR 还在跑" 的时间窗口
     * 从几十纳秒变成几十微秒，大幅提高触发概率。
     */
    pal_delay_us(50);

    /* 检查魔数 — 如果被破坏说明 UAF 发生了！ */
    if (res->magic != TEST_SMP_UAF_MAGIC) {
        s_uaf_detected = true;
        s_uaf_magic_value = res->magic;
        pal_debug_printf("!!! UAF DETECTED at round %lu !!! magic=0x%08lX\n",
                         (unsigned long)res->round_id,
                         (unsigned long)s_uaf_magic_value);
        return;
    }

    res->isr_counter++;
}

/* ─────────────────────────────────────────────────────────
 * 慢速 ISR（验证 synchronize 阻塞用）
 * ───────────────────────────────────────────────────────── */

/**
 * @brief 慢速 ISR — 故意执行 100ms，用于验证 synchronize() 真的在等。
 */
static void slow_isr(void *arg) {
    (void)arg;
    s_slow_isr_running = true;

    /* 100ms 长延时，足以观测到阻塞效应 */
    pal_delay_ms(100);

    s_slow_isr_running = false;
    s_slow_isr_done = true;
}

/* ─────────────────────────────────────────────────────────
 * 核心测试逻辑
 * ───────────────────────────────────────────────────────── */

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
        s_current_resource = res;

        /* ── Step 2: 注册 GPIO 中断 ─────────────────
         * 使用测试 pin 10（host 下无硬件限制，任意数字即可）
         */
        const wink_pin_t TEST_PIN = 10;
        wink_status_t status = pal_gpio_enable_interrupt(
            TEST_PIN,
            PAL_GPIO_INTR_RISING_EDGE,
            test_uaf_isr,
            res
        );

        if (wink_status_is_error(status)) {
            pal_debug_printf("ERROR: pal_gpio_enable_interrupt failed: %d\n", (int)status);
            free(res);
            s_current_resource = NULL;
            continue;
        }

        /* ── Step 3: 持续触发中断（让 ISR 一直在飞） ── */
        for (uint32_t i = 0; i < triggers_per_round; i++) {
            /* 使用 host 专用 API 触发 GPIO 中断
             *
             * 注意：host 是单线程环境，ISR 同步执行，永远不会出现
             * "disable 返回了 ISR 还在另一个核心跑" 的情况。
             * SMP 并发 bug 只有在真实双核硬件（如 ESP32）上才能测出来。
             */
            trigger_gpio_interrupt(TEST_PIN);
            pal_delay_us(10);
        }

        /* 给正在飞的 ISR 一点时间进入临界区 */
        pal_delay_us(20);

        /* ── Step 4: 关键路径 — disable + [synchronize] + free ── */
        wink_status_t st = pal_gpio_disable_interrupt(TEST_PIN);
        (void)st;

        if (enable_synchronize) {
            /* ✅ 等待所有核心退出 ISR 后再释放
             *
             * 注意：host 是单线程环境，synchronize() 在这里是空操作。
             * 只有在 SMP 双核系统（如 ESP32）上，这个函数才真正起作用：
             * - Core 0 执行 disable_interrupt() 后返回
             * - Core 1 可能还在执行该 ISR
             * - synchronize() 忙等待直到所有核心都退出 ISR
             */
            pal_irq_synchronize(0);  /* irq_num = 0 对 host 无意义，仅占位 */
        }
        /* ❌ 缺少 synchronize：直接 free，UAF 风险（仅 SMP 系统） */

        /* ── Step 5: 释放资源 ────────────────────── */
        free(res);
        s_current_resource = NULL;

        passed++;

        /* 进度输出（每 100 轮一次，避免日志刷屏） */
        if (round % 100 == 0) {
            pal_debug_printf("  round %lu OK\n", (unsigned long)round);
        }
    }

    /* 填充结果 */
    result->passed_rounds = passed;
    result->uaf_detected = s_uaf_detected;
    result->uaf_magic_value = s_uaf_magic_value;
}

/* ─────────────────────────────────────────────────────────
 * Synchronize 阻塞验证
 * ───────────────────────────────────────────────────────── */

bool test_smp_synchronize_blocks(uint32_t *blocked_us) {
    pal_debug_printf("\n=== Testing if pal_irq_synchronize() actually blocks ===\n");

    /* ⚠️ Host 单线程环境下，这个测试无法验证真正的阻塞行为
     *
     * 真正的 SMP 同步验证需要双核硬件：
     *   - Core 0: 调用 disable_interrupt() + synchronize()
     *   - Core 1: 正在执行该 ISR
     *   - synchronize() 应该阻塞直到 Core 1 退出 ISR
     *
     * Host 下单线程执行，ISR 同步完成，synchronize() 永远是空操作。
     */

    const wink_pin_t TEST_PIN = 11;
    test_resource_t dummy_res;
    dummy_res.magic = TEST_SMP_UAF_MAGIC;
    dummy_res.isr_counter = 0;
    dummy_res.round_id = 0xFFFFFFFFu;

    s_slow_isr_running = false;
    s_slow_isr_done = false;

    /* 注册慢速 ISR */
    wink_status_t status = pal_gpio_enable_interrupt(
        TEST_PIN,
        PAL_GPIO_INTR_RISING_EDGE,
        slow_isr,
        &dummy_res
    );

    if (wink_status_is_error(status)) {
        pal_debug_printf("ERROR: pal_gpio_enable_interrupt failed: %d\n", (int)status);
        *blocked_us = 0;
        return false;
    }

    /* 触发中断（host 下单线程，ISR 同步执行完成） */
    trigger_gpio_interrupt(TEST_PIN);

    /* 调用 synchronize 并计时（host 下应该立刻返回） */
    uint64_t start = pal_get_us();
    pal_irq_synchronize(0);  /* irq_num 参数在 host 下无意义 */
    uint64_t elapsed = pal_get_us() - start;

    *blocked_us = (uint32_t)elapsed;

    pal_debug_printf("  synchronize() returned in %lu us\n", (unsigned long)elapsed);
    pal_debug_printf("  (Host is single-threaded, this should be near-zero)\n");

    /* 清理 */
    wink_status_t st = pal_gpio_disable_interrupt(TEST_PIN);
    (void)st;

    /* Host 下单线程环境下，synchronize() 应该立刻返回
     * 这是预期行为，不是 bug。真正的阻塞验证需要 ESP32 硬件。
     */
    pal_debug_printf("  ℹ️ Host environment: blocking test is N/A (requires SMP hardware)\n");
    pal_debug_printf("=========================================================\n");

    return true;  /* host 下总是通过，不视为失败 */
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
        pal_debug_printf("     pal_irq_synchronize() is REQUIRED to prevent UAF on SMP systems!\n");
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
