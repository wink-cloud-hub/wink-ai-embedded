/**
 * @file selftest_gpio_isr.c
 * @brief S4-isr: GPIO 中断注册 + arg roundtrip 验证。
 *
 * 验证：
 *   1. pal_gpio_init + pal_gpio_enable_interrupt 在测试 pin 上注册成功，
 *      无错误返回；
 *   2. ISR 被触发时 arg 指针正确（通过 magic 标记）。
 *      触发方式：启用 pal_test_enable_hardware_loopback(pin, pin) 自环，
 *      配 INPUT_OUTPUT 模式，软件翻转电平；在支持 GPIO 矩阵回灌触发的
 *      平台（ESP32 真机）会真的触发 ISR；host/wasm 因 loopback 不 fire ISR，
 *      只验证注册路径不崩，注册成功即 PASS（与 smoke S4 原验收一致）。
 *   3. 清理：disable_interrupt + synchronize + deinit loopback + release pin。
 *
 * 测试 pin 选择 22（DevKitC 上未被 smoke 其他 S-test 占用；避开 boot=0/led=?/trig=18/echo=19/pwm=4,5）。
 */
#define LOG_TAG "selftest.isr"

#include "wink_selftest.h"
#include "wink_selftest_internal.h"
#include "wink_status.h"
#include "wink_log.h"
#include "pal_hal.h"
#include "pal_osal.h"
#include "pal_resource.h"
#include "pal_irq.h"
#include "internal/pal_test_loopback.h"

#define ISR_TEST_PIN  22u
#define ISR_MAGIC     0xA5A5A5A5u

typedef struct {
    volatile uint32_t fired;
    volatile uint32_t magic_seen;
    uint32_t magic;
} isr_test_ctx_t;

static PAL_ISR void roundtrip_isr(void *arg)
{
    isr_test_ctx_t *ctx = (isr_test_ctx_t *)arg;
    ctx->fired++;
    if (ctx->magic == ISR_MAGIC) {
        ctx->magic_seen = 1;
    }
}

wink_status_t wink_selftest_gpio_isr_roundtrip(wink_selftest_result_t *r)
{
    r->note = "ISR registration";
    r->metric = 0;

    /* 防御性：先 release（防御未清理场景，结果忽略）*/
    WINK_IGNORE_RESULT(pal_resource_release(PAL_RESOURCE_GPIO_PIN, ISR_TEST_PIN, "selftest_isr"));
    wink_status_t st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, ISR_TEST_PIN, "selftest_isr");
    if (wink_status_is_error(st) && st != WINK_ERR_BUSY) {
        r->note = "resource claim failed";
        return st;
    }

    /* 配置 pin 为 bidir（输出可写，输入可触发中断）*/
    st = pal_gpio_init(ISR_TEST_PIN, PAL_GPIO_INPUT_OUTPUT);
    if (wink_status_is_error(st)) {
        /* 回退到 INPUT_PULLUP（某些 target 不支持 INPUT_OUTPUT）*/
        st = pal_gpio_init(ISR_TEST_PIN, PAL_GPIO_INPUT_PULLUP);
        if (wink_status_is_error(st)) {
            WINK_IGNORE_RESULT(pal_resource_release(PAL_RESOURCE_GPIO_PIN, ISR_TEST_PIN, "selftest_isr"));
            r->note = "pal_gpio_init failed";
            return st;
        }
    }
    WINK_IGNORE_RESULT(pal_gpio_write(ISR_TEST_PIN, false));

    /* 准备 ctx 并注册 ISR（ANY_EDGE：双向翻转都触发）*/
    static isr_test_ctx_t ctx;
    ctx.fired = 0;
    ctx.magic_seen = 0;
    ctx.magic = ISR_MAGIC;

    st = pal_gpio_enable_interrupt(ISR_TEST_PIN, PAL_GPIO_INTR_ANY_EDGE,
                                   roundtrip_isr, &ctx);
    if (wink_status_is_error(st)) {
        if (st == WINK_ERR_UNSUPPORTED) {
            WINK_IGNORE_RESULT(pal_gpio_init(ISR_TEST_PIN, PAL_GPIO_INPUT_PULLUP));
            WINK_IGNORE_RESULT(pal_resource_release(PAL_RESOURCE_GPIO_PIN, ISR_TEST_PIN, "selftest_isr"));
            r->note = "gpio interrupt not supported";
            return WINK_ERR_UNSUPPORTED;
        }
        WINK_IGNORE_RESULT(pal_resource_release(PAL_RESOURCE_GPIO_PIN, ISR_TEST_PIN, "selftest_isr"));
        r->note = "pal_gpio_enable_interrupt failed";
        return st;
    }
    r->metric = 1;  /* registered successfully */

    /* 尝试自环 + 软件翻转，期望在真机上触发 ISR */
    if (pal_test_enable_hardware_loopback(ISR_TEST_PIN, ISR_TEST_PIN) == WINK_OK) {
        /* busy-wait 让硬件环回稳定 */
        pal_os_busy_wait_us(50);
        WINK_IGNORE_RESULT(pal_gpio_write(ISR_TEST_PIN, true));
        pal_os_busy_wait_us(20);
        WINK_IGNORE_RESULT(pal_gpio_write(ISR_TEST_PIN, false));
        pal_os_busy_wait_us(50);
        WINK_IGNORE_RESULT(pal_test_disable_hardware_loopback(ISR_TEST_PIN, ISR_TEST_PIN));
    }

    /* 清理：disable + synchronize 保证 ISR 已退出 */
    WINK_IGNORE_RESULT(pal_gpio_disable_interrupt(ISR_TEST_PIN));
    WINK_IGNORE_RESULT(pal_gpio_synchronize_interrupt(ISR_TEST_PIN));

    if (ctx.fired > 0) {
        r->metric = 2;
        if (ctx.magic_seen) {
            r->note = "ISR fired, arg roundtrip OK";
        } else {
            r->note = "ISR fired but magic mismatch (arg roundtrip broken?)";
            WINK_IGNORE_RESULT(pal_resource_release(PAL_RESOURCE_GPIO_PIN, ISR_TEST_PIN, "selftest_isr"));
            return WINK_ERR_HARDWARE;
        }
    } else {
        /* 平台未支持 loopback 触发（host/wasm）——注册成功本身就是 PASS */
        r->note = "ISR registered (firing requires physical/hardware signal)";
    }

    WINK_IGNORE_RESULT(pal_resource_release(PAL_RESOURCE_GPIO_PIN, ISR_TEST_PIN, "selftest_isr"));
    return WINK_OK;
}
