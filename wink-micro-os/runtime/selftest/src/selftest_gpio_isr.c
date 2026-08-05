// SPDX-License-Identifier: Apache-2.0
/**
 * @file selftest_gpio_isr.c
 * @brief GPIO ISR registration and argument roundtrip selftest.
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

    WINK_IGNORE_RESULT(pal_resource_release(PAL_RESOURCE_GPIO_PIN, ISR_TEST_PIN, "selftest_isr"));
    wink_status_t st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, ISR_TEST_PIN, "selftest_isr");
    if (wink_status_is_error(st) && st != WINK_ERR_BUSY) {
        r->note = "resource claim failed";
        return st;
    }

    st = pal_gpio_init(ISR_TEST_PIN, PAL_GPIO_INPUT_OUTPUT);
    if (wink_status_is_error(st)) {
        st = pal_gpio_init(ISR_TEST_PIN, PAL_GPIO_INPUT_PULLUP);
        if (wink_status_is_error(st)) {
            WINK_IGNORE_RESULT(pal_resource_release(PAL_RESOURCE_GPIO_PIN, ISR_TEST_PIN, "selftest_isr"));
            r->note = "pal_gpio_init failed";
            return st;
        }
    }
    WINK_IGNORE_RESULT(pal_gpio_write(ISR_TEST_PIN, false));

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
    r->metric = 1;

    if (pal_test_enable_hardware_loopback(ISR_TEST_PIN, ISR_TEST_PIN) == WINK_OK) {
        pal_os_busy_wait_us(50);
        WINK_IGNORE_RESULT(pal_gpio_write(ISR_TEST_PIN, true));
        pal_os_busy_wait_us(20);
        WINK_IGNORE_RESULT(pal_gpio_write(ISR_TEST_PIN, false));
        pal_os_busy_wait_us(50);
        WINK_IGNORE_RESULT(pal_test_disable_hardware_loopback(ISR_TEST_PIN, ISR_TEST_PIN));
    }

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
        r->note = "ISR registered (firing requires physical/hardware signal)";
    }

    WINK_IGNORE_RESULT(pal_resource_release(PAL_RESOURCE_GPIO_PIN, ISR_TEST_PIN, "selftest_isr"));
    return WINK_OK;
}
