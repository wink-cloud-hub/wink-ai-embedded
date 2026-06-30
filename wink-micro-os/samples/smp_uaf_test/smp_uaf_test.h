/**
 * @file test_smp_uaf.h
 * @brief SMP 多核 UAF (Use-After-Free) 验证测试 — 头文件。
 *
 * 测试目的：
 *   验证 pal_irq_synchronize() 是否真正解决了 SMP 系统中的经典并发问题：
 *   "Core 0 调用 pal_irq_disable() 返回后，Core 1 可能仍在执行该 ISR"。
 *
 * 测试原理：
 *   1. 分配带魔数的资源结构体，传给 ISR 使用
 *   2. Core 0 持续触发中断，让 ISR 一直在飞
 *   3. Core 0 调用 pal_gpio_disable_interrupt()，然后：
 *      - ✅ 有 pal_irq_synchronize() → 等待所有 ISR 退出后再 free，安全
 *      - ❌ 无 pal_irq_synchronize() → 立刻 free，触发 UAF
 *   4. ISR 中检查魔数是否被破坏，检测 UAF
 *
 * 平台兼容性：
 *   本测试 100% 使用 PAL API，理论上可在所有支持 PAL 的 SMP 多核平台运行：
 *   - ESP32 / ESP32-S3 (Xtensa SMP)
 *   - RP2350 (Pico 2, ARM Cortex-M33 SMP)
 *   - 其他有 PAL 端口的多核 MCU
 *
 * @note 这是一个真机验证测试，host 单线程环境永远测不出这个问题。
 */

#ifndef TEST_SMP_UAF_H
#define TEST_SMP_UAF_H

#include <stdint.h>
#include <stdbool.h>

/* ─────────────────────────────────────────────────────────
 * 测试配置
 * ───────────────────────────────────────────────────────── */

/** 测试资源魔数（用于 UAF 检测） */
#define TEST_SMP_UAF_MAGIC  0xDEADBEEFu

/** synchronize() 超时阈值（验证真的在阻塞） */
#define TEST_SMP_BLOCK_THRESHOLD_US  80000u  /* 80ms */

/* ─────────────────────────────────────────────────────────
 * 测试结果
 * ───────────────────────────────────────────────────────── */

/** 单次测试运行结果 */
typedef struct {
    uint32_t total_rounds;          /**< 总轮数 */
    uint32_t passed_rounds;         /**< 通过轮数 */
    bool     uaf_detected;          /**< 是否检测到 UAF */
    uint32_t uaf_magic_value;       /**< UAF 时的魔数值（诊断用） */
    bool     synchronize_works;     /**< synchronize() 是否真的在阻塞 */
    uint32_t synchronize_time_us;   /**< synchronize() 阻塞时间（微秒） */
} test_smp_uaf_result_t;

/* ─────────────────────────────────────────────────────────
 * 测试 API
 * ───────────────────────────────────────────────────────── */

/**
 * @brief 运行完整的 SMP UAF 测试套件。
 *
 * @param rounds              测试轮数（建议 1000 以上）
 * @param triggers_per_round  每轮的中断触发次数（建议 100-500）
 * @param enable_synchronize  true=启用 synchronize(), false=禁用（用于对比验证）
 * @param[out] result         测试结果输出
 *
 * @note 在双核 SMP 系统上：
 *   - enable_synchronize=true  → 应该全部通过
 *   - enable_synchronize=false → 几十轮内就会触发 UAF
 */
void test_smp_uaf_run(uint32_t rounds,
                      uint32_t triggers_per_round,
                      bool enable_synchronize,
                      test_smp_uaf_result_t *result);

/**
 * @brief 单独测试 pal_irq_synchronize() 是否真的在阻塞。
 *
 * 验证原理：
 *   1. 注册一个故意慢执行的 ISR（100ms）
 *   2. 触发中断后立刻调用 synchronize()
 *   3. 测量 synchronize() 阻塞时间，应该接近 ISR 执行时间
 *
 * @param[out] blocked_us  实际阻塞时间（微秒）
 * @return true=synchronize() 正确阻塞，false=没有正确工作
 */
bool test_smp_synchronize_blocks(uint32_t *blocked_us);

/**
 * @brief 打印测试结果（使用 pal_debug_printf）。
 */
void test_smp_uaf_print_result(const test_smp_uaf_result_t *result);

#endif /* TEST_SMP_UAF_H */
