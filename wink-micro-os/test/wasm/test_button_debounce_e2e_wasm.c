/**
 * @file test_button_debounce_e2e_wasm.c
 * @brief ADR-0009 Wave 2 Task 4 — WASM 端到端按键去抖测试（host parity）。
 *
 * 镜像 host 版本：test/test_button_debounce_e2e.c
 *   验证 WASM 侧 GPIO 退化中间件 (pal_gpio_read) + dal_button 计数去抖
 *   组合后的端到端行为与 host 完全一致：同一 bounce_us / 同一 tick / 同一
 *   去抖阈值 → 同样的稳定收敛轨迹。
 *
 * 与 host 版本的差异（仅是注入/驱动方式不同，golden 与判定完全一致）：
 *   ┌─────────────────────────┬──────────────────────┬─────────────────────────────────┐
 *   │ 维度                    │ host                 │ wasm                            │
 *   ├─────────────────────────┼──────────────────────┼─────────────────────────────────┤
 *   │ 时钟推进                │ pal_os_sleep_ms() 步进  │ pal_wasm_advance_virtual_clock  │
 *   │ 理想电平注入            │ sim_set_gpio_ideal() │ EM_JS mock + test_set_ideal()   │
 *   │ 故障配置                │ sim_set_faults(&f)   │ pal_wasm_set_bounce_us(us)      │
 *   │ 测试重置                │ sim_reset_time()     │ pal_wasm_reset_physical()       │
 *   │ pal_os_sleep_ms 副作用     │ 推进时钟（host 仿真）│ 立即返回（SSOT，Task 1 已验）   │
 *   └─────────────────────────┴──────────────────────┴─────────────────────────────────┘
 *
 * 故 wasm 端 run_ticks 不依赖 pal_os_sleep_ms 副作用，直接 advance 虚拟时钟。
 * 这恰是 ADR-0003 决策 3 / ADR-0009 §4.1 SSOT 架构落地：JS Worker（此处为
 * 测试 harness）独占时钟控制。
 *
 * golden（与 host 同源）：
 *   TICK_MS=10, BOUNCE_US=30000, DAL 去抖阈值=3。
 *   电平 false→true 跃变 → bounce 窗 [t_jump, t_jump+BOUNCE_US)
 *   → 窗内每 tick 强制翻转 → counter 反复清零
 *   → 出窗 raw 稳定 → counter 累积到 3 → stable_pressed=true
 *
 * 构建依赖（Task 5/6 add_wink_wasm_test CMake helper 落地后注册）：
 *   - dal_button.c（含 dal_button_poll / dal_button_is_pressed）
 *   - pal_hal_wasm.c（pal_gpio_read 退化中间件，Task 3）
 *   - pal_osal_wasm.c（pal_os_get_us / pal_os_sleep_ms / 虚拟时钟，Task 1）
 *   - pal_wasm_physical.c（faults 配置 setter + ctx 池，Task 2）
 *   - JS harness：必须 import 由 EM_JS 注入的 js_pal_gpio_read（见下）。
 */
#include "unity.h"
#include "dal_button.h"
#include "pal_hal.h"           /* pal_gpio_read for raw reads */
#include "pal_osal.h"          /* pal_os_sleep_ms（SSOT 下 no-op） */
#include "pal_wasm_internal.h" /* pal_wasm_advance_virtual_clock + setters + ctx 访问 */
#include "wink_sim_physical.h" /* wink_phys_debounce_ctx_t 字段 */

#include <emscripten.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* 退化配置 setter（KEEPALIVE 导出，在 pal_wasm_physical.c 中定义）。 */
extern void pal_wasm_set_bounce_us(uint32_t us);
extern void pal_wasm_set_prng_seed(uint32_t seed);
extern void pal_wasm_reset_physical(void);
extern void pal_wasm_advance_virtual_clock(uint64_t us);

/* 场景参数（与 host test_button_debounce_e2e.c 严格一致 — 强制交替模型）：
 *   TICK_MS=10（对齐 WINK_RUNTIME_TICK_MS=10），bounce_us=30000（窗内 3 个 tick 采样）。
 *   active_low 按键：释放 raw=true，按下 raw=false。dal_button 计数去抖阈值=3。 */
#define TICK_MS 10
#define BOUNCE_US 30000u

/* ─────────────────────────────────────────────────────────
 * JS 侧 mock：用 EM_JS 注入 js_pal_gpio_read 的测试桩
 * ─────────────────────────────────────────────────────────
 * EM_JS 让我们直接在 .c 文件内嵌入 JS 实现：链接时 js_pal_gpio_read
 * 会被解析到本宏生成的桩，而非 harness 环境提供的真实 import。
 *
 * 该桩从 globalThis.__wink_gpio_ideal 表（Map<pin, level>）读取，写入由
 * test_set_gpio_ideal() 完成（同样 EM_JS 注入），未注册的 pin 返回 false
 * （raw=低，与 host 端 sim_set_gpio_ideal 未注册行为对称）。
 *
 * 这避免本测试依赖外部 harness 注入 js_pal_gpio_read，让 .c 单测自包含。
 */
EM_JS(bool, js_pal_gpio_read, (uint16_t pin), {
    if (typeof globalThis.__wink_gpio_ideal === 'undefined') {
        globalThis.__wink_gpio_ideal = {};
    }
    var v = globalThis.__wink_gpio_ideal[pin];
    return (v === true) ? 1 : 0;
});

EM_JS(void, test_set_gpio_ideal_js, (uint16_t pin, bool level), {
    if (typeof globalThis.__wink_gpio_ideal === 'undefined') {
        globalThis.__wink_gpio_ideal = {};
    }
    globalThis.__wink_gpio_ideal[pin] = (level ? true : false);
});

EM_JS(void, test_clear_gpio_ideal_js, (void), {
    globalThis.__wink_gpio_ideal = {};
});

/* host_test_ctrl.h::sim_set_gpio_ideal 双语义的 wasm 端等价物：
 *   首次注册 pin = 上电态（ctx.stable_level=level，不抖）；
 *   再次更新 = 跃变（仅改 JS 端理想电平，ctx 旧 stable 不动 → 下次采样
 *               target≠stable → 进入 bounce 窗）。
 * 这是为了让 golden 与 host test_button_debounce_e2e.c 严格一致。
 *
 * 实现：C 端维护"已注册"位图（pin → registered？），首次 set 时通过
 * pal_wasm_get_debounce_ctx() 取该 pin 的 ctx 把 stable_level 同步为
 * 当前 level（这正是 host pal_osal_host.c::sim_set_gpio_ideal 注册分支
 * 在做的事）。 */
static bool s_pin_registered[WASM_SIM_MAX_PINS];

static void wasm_set_gpio_ideal(uint16_t pin, bool level) {
    test_set_gpio_ideal_js(pin, level);
    if (pin >= WASM_SIM_MAX_PINS) { return; }
    if (!s_pin_registered[pin]) {
        s_pin_registered[pin] = true;
        wink_phys_debounce_ctx_t *ctx = pal_wasm_get_debounce_ctx(pin);
        if (ctx != NULL) {
            ctx->stable_level    = level;   /* 上电态：与 ideal 一致，不触发抖动 */
            ctx->in_bounce       = false;
            ctx->bounce_start_us = 0;
            ctx->bounce_flip     = false;
        }
    }
    /* 已注册路径：仅更新 JS 端理想电平，ctx 保持旧 stable → 下次采样触发跃变。 */
}

static void wasm_clear_gpio_ideal(void) {
    test_clear_gpio_ideal_js();
    memset(s_pin_registered, 0, sizeof(s_pin_registered));
}

void setUp(void) {
    /* 重置退化引擎：faults=0, ctx 清零, PRNG=1。
     * 虚拟时钟无 reset 接口（业务代码不应能回拨时钟），但 WASM 单测
     * 每个 main 重新实例化，所以同一可执行文件内多用例时钟会累加。
     * 本测试中每个用例换 pin 编号，per-pin ctx 独立，时钟单调不影响判定。 */
    pal_wasm_reset_physical();
    wasm_clear_gpio_ideal();
}

void tearDown(void) {}

/* 每个 tick：推进虚拟时钟 TICK_MS 毫秒，然后调用 dal_button_poll。
 * pal_os_sleep_ms 在 wasm SSOT 下不主动推时钟（test_virtual_clock.c 已验证），
 * 故必须显式 advance。 */
static void run_ticks(dal_button_t *btn, int n) {
    for (int i = 0; i < n; i++) {
        pal_wasm_advance_virtual_clock((uint64_t)TICK_MS * 1000u);
        TEST_ASSERT_EQUAL(WINK_OK, dal_button_poll(btn));
    }
}

/* 负对照 helper：无去抖的裸采样（与 dal_button.c::button_raw_pressed 同语义）。 */
static bool raw_pressed(uint16_t pin, bool active_low) {
    return pal_gpio_read(pin) != active_low;
}

/* 【主线·正】电平跃变 → dal_button 计数去抖吸收抖动 → 稳定 pressed。
 *
 * golden（强制交替模型，与 host 完全一致）：
 *   跃变后首次 poll 时 ctx->stable_level 与 ideal 不符 → 进入 bounce 窗，
 *   窗内每次采样强制翻转 → dal_button counter 反复清零；
 *   出窗后 raw 稳定 → counter 累积到 3 → stable_pressed=true。
 *   run_ticks(6) = 3 窗内采样 + 3 出窗去抖。
 */
void test_dal_button_absorbs_bounce_and_settles(void) {
    pal_wasm_set_bounce_us(BOUNCE_US);
    pal_wasm_set_prng_seed(1u);

    dal_button_t btn;
    const dal_button_config_t cfg = { .owner = "wasm_e2e_debounce_bounce", .pin = 7, .active_low = true };
    TEST_ASSERT_EQUAL(WINK_OK, dal_button_init(&btn, &cfg));   /* active_low */

    wasm_set_gpio_ideal(7, true);                              /* ① 上电=释放(raw=true)，不抖（ctx 初值与 ideal 一致） */
    run_ticks(&btn, 2);                                        /* 稳定到「未按下」 */
    bool released = true;
    TEST_ASSERT_EQUAL(WINK_OK, dal_button_is_pressed(&btn, &released));
    TEST_ASSERT_FALSE(released);

    wasm_set_gpio_ideal(7, false);                             /* ② 跃变=按下(raw=false) → 抖动窗 */
    run_ticks(&btn, 6);                                        /* 3 窗内 + 3 出窗去抖 */

    bool pressed = false;
    TEST_ASSERT_EQUAL(WINK_OK, dal_button_is_pressed(&btn, &pressed));
    TEST_ASSERT_TRUE(pressed);                                 /* 去抖吸收抖动，稳定按下 */
}

/* 【主线·负对照】同一抖动电平序列，无去抖裸采样 → 抖动窗内必跳变（误触发）。
 * 证明 ADR-0009 §3.1 核心论点「不写去抖则多次误触发」。
 * 强制交替模型保证窗内既采到 pressed 又采到 released。 */
void test_raw_read_without_debounce_bounces(void) {
    pal_wasm_set_bounce_us(BOUNCE_US);
    pal_wasm_set_prng_seed(1u);

    wasm_set_gpio_ideal(9, true);                              /* 上电=释放（pin9，避耦合），不触发抖动 */
    pal_wasm_advance_virtual_clock((uint64_t)TICK_MS * 1000u); /* +10ms */
    wasm_set_gpio_ideal(9, false);                             /* 跃变=按下 → bounce 窗 */

    bool saw_pressed = false, saw_released = false;
    for (int i = 0; i < 3; i++) {                              /* 窗内 3 次裸采样（强制交替必跳变） */
        if (raw_pressed(9, true)) { saw_pressed = true; }
        else { saw_released = true; }
        pal_wasm_advance_virtual_clock((uint64_t)TICK_MS * 1000u);  /* +10ms，仍在窗内 */
    }
    TEST_ASSERT_TRUE(saw_pressed && saw_released);             /* 无去抖 → 既「按下」又「释放」=误触发 */
}

/* 【基线】无退化（bounce_us=0）→ 快速稳定，无抖动 */
void test_no_bounce_config_settles_fast(void) {
    /* pal_wasm_reset_physical() 已把 bounce_us 清 0，显式不再设。 */
    dal_button_t btn;
    const dal_button_config_t cfg = { .owner = "wasm_e2e_debounce_baseline", .pin = 8, .active_low = false };
    TEST_ASSERT_EQUAL(WINK_OK, dal_button_init(&btn, &cfg));   /* active_high */
    wasm_set_gpio_ideal(8, true);                              /* 按下 raw=true */
    run_ticks(&btn, 5);
    bool pressed = false;
    TEST_ASSERT_EQUAL(WINK_OK, dal_button_is_pressed(&btn, &pressed));
    TEST_ASSERT_TRUE(pressed);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_dal_button_absorbs_bounce_and_settles);
    RUN_TEST(test_raw_read_without_debounce_bounces);
    RUN_TEST(test_no_bounce_config_settles_fast);
    return UNITY_END();
}
