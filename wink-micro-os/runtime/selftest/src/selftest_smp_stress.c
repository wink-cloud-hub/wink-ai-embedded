/**
 * @file selftest_smp_stress.c
 * @brief S7: PAL resource 表并发压测（SMP 双核 / 单线程降级）。
 *
 * 设计：
 *   - 同步部分（所有平台，包括 app_init 上下文）：在当前线程连续做
 *     N 次 claim/release 循环，验证 resource 表 spinlock/临界区本身无崩。
 *   - 可选异步部分（仅当 pal_os_task_create 可用且调度器已启动时）：
 *     在 core0/core1 各 spawn 一个 pinned 后台 task 做 STRESS_DURATION_MS
 *     压测；不阻塞 selftest 返回，若真有 SMP 争用/死锁，会通过 WDT 或后续
 *     fault 路径暴露（符合 smoke S7 "spawn 不崩 + 系统不 panic"的验收标准）。
 *
 *   - ESP32:  STRESS_DURATION_MS = 60s（真 SMP 争用窗口）
 *   - host/wasm/simulation: 10ms（单线程循环覆盖代码路径）
 *
 * 不依赖 app 业务状态，不改动真实 GPIO（pin 100/101 是虚拟 resource id）。
 */
#define LOG_TAG "selftest.smp"

#include "wink_selftest.h"
#include "wink_selftest_internal.h"
#include "wink_status.h"
#include "wink_log.h"
#include "pal_osal.h"
#include "pal_resource.h"

#if defined(PLATFORM_host) || defined(PLATFORM_wasm) || defined(SIMULATION)
#define STRESS_DURATION_MS   10u   /* host/wasm: 虚拟时间短窗口 */
#define SINGLE_THREAD_ITERS  2000u /* 单线程迭代次数 */
#else
#define STRESS_DURATION_MS   60000u
#define SINGLE_THREAD_ITERS  10000u
#endif

/* ── 后台异步压测 task（fire-and-forget，仅当 task_create 成功时） ── */
static void stress_task_fn(void *arg)
{
    uint32_t core_id = (uint32_t)(uintptr_t)arg;
    uint64_t end_time = pal_os_get_ms() + STRESS_DURATION_MS;
    uint64_t last_yield = pal_os_get_ms();
    uint32_t pin = 100u + (core_id & 1u);

    while (pal_os_get_ms() < end_time) {
        WINK_IGNORE_RESULT(pal_resource_claim(PAL_RESOURCE_GPIO_PIN, pin, "selftest_smp"));
        WINK_IGNORE_RESULT(pal_resource_release(PAL_RESOURCE_GPIO_PIN, pin, "selftest_smp"));

        /* 每 ~1s 让出 1ms：
         *   - ESP32: 让 IDLE 喂 Task WDT；
         *   - host/wasm: 协程环境下切回主循环推进虚拟时钟（否则 end_time 不前进）。*/
        uint64_t now = pal_os_get_ms();
        if ((uint32_t)(now - last_yield) >= 1000u) {
            pal_os_sleep_ms(1);
            last_yield = pal_os_get_ms();
        }
    }

    pal_os_task_delete(NULL);
}

wink_status_t wink_selftest_smp_resource_stress(wink_selftest_result_t *r)
{
    r->note = "resource claim/release single-thread";

    /* ── 1. 同步单线程压测：所有平台都能跑 ── */
    uint32_t iters = 0;
    for (uint32_t i = 0; i < SINGLE_THREAD_ITERS; i++) {
        /* 交替对 pin 100 / 101 claim+release，模拟两核争用的序列 */
        WINK_IGNORE_RESULT(pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 100u, "selftest_smp"));
        WINK_IGNORE_RESULT(pal_resource_release(PAL_RESOURCE_GPIO_PIN, 100u, "selftest_smp"));
        WINK_IGNORE_RESULT(pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 101u, "selftest_smp"));
        WINK_IGNORE_RESULT(pal_resource_release(PAL_RESOURCE_GPIO_PIN, 101u, "selftest_smp"));
        iters++;
    }
    r->metric = iters;

    /* ── 2. 尝试后台双核压测（fire-and-forget，不阻塞） ── */
    bool spawned_smp = false;
    /* 注意：在 app_init 上下文（调度器未启动时），pal_os_task_create 仍可能返回 WINK_OK
     * 并把 task 注册到调度表——这些 task 会在 pal_sim_scheduler_run 启动后才被调度。
     * 对 ESP32（FreeRTOS 调度器永远在跑）：task 立刻开始执行。*/
    /* 32KB stack 满足 host sim min 要求；ESP32 上 2048 即可，但用 4096 保稳 */
    uint32_t stack_size = 4096u;
#if defined(PLATFORM_host) || defined(PLATFORM_wasm) || defined(SIMULATION)
    stack_size = 32768u;
#endif
    wink_status_t st0 = pal_os_task_create(stress_task_fn, "selftest_smp_0",
                                           stack_size, (void*)(uintptr_t)0,
                                           2, PAL_OS_CORE_0, NULL);
    wink_status_t st1 = pal_os_task_create(stress_task_fn, "selftest_smp_1",
                                           stack_size, (void*)(uintptr_t)1,
                                           2, PAL_OS_CORE_1, NULL);
    if (st0 == WINK_OK && st1 == WINK_OK) {
        spawned_smp = true;
        r->note = "single-thread ok; SMP tasks spawned (background)";
    } else if (st0 == WINK_ERR_UNSUPPORTED || st1 == WINK_ERR_UNSUPPORTED) {
        r->note = "single-thread ok; SMP not supported (SKIP background)";
    } else {
        /* 单线程已经验证了 resource 表本身不崩，spawn 失败不构成 FAIL；
         * 仅标注，PASS 处理（与 smoke S7 "spawn 失败=degraded"语义一致）。*/
        r->note = "single-thread ok; SMP task spawn returned error";
        LOG_W("smp_stress: background task spawn returned %d/%d", (int)st0, (int)st1);
    }

    (void)spawned_smp;
    return WINK_OK;
}
