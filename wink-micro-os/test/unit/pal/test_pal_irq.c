// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_pal_irq.c
 * @brief PAL unified interrupt abstraction unit tests.
 */
#include "unity.h"
#include "wink_status.h"
#include "pal_irq.h"
#include "pal_hal.h"
#include "pal_osal.h"
#define WINK_ALLOW_ADVANCED_IRQ_APIS
#include "pal_irq_advanced.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern void pal_host_trigger_gpio_interrupt(wink_pin_t pin);
extern uint32_t pal_host_get_isr_call_count(wink_pin_t pin);
extern void pal_host_reset_isr_stats(void);
extern uint32_t pal_host_get_pending_count(void);
extern int pal_host_get_irq_lock_depth(void);
extern void pal_host_trigger_logical_interrupt(uint32_t irq_num);
extern uint32_t pal_host_get_logical_isr_call_count(uint32_t irq_num);

static volatile uint32_t s_test_isr_count = 0;
static volatile uint32_t s_test_isr_arg_val = 0;

void setUp(void)
{
    pal_host_reset_isr_stats();
    s_test_isr_count = 0;
    s_test_isr_arg_val = 0;
}

void tearDown(void) {}

static void test_gpio_isr(void *arg)
{
    (void)arg;
    s_test_isr_count++;
}

void test_gpio_interrupt_registration(void)
{
    const wink_pin_t TEST_PIN = 10;

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_gpio_enable_interrupt(TEST_PIN, PAL_GPIO_INTR_FALLING_EDGE,
                                   test_gpio_isr, NULL));

    TEST_ASSERT_EQUAL_UINT32(0, pal_host_get_isr_call_count(TEST_PIN));
    TEST_ASSERT_EQUAL_UINT32(0, s_test_isr_count);

    pal_host_trigger_gpio_interrupt(TEST_PIN);

    TEST_ASSERT_EQUAL_UINT32(1, pal_host_get_isr_call_count(TEST_PIN));
    TEST_ASSERT_EQUAL_UINT32(1, s_test_isr_count);

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_disable_interrupt(TEST_PIN));
    pal_host_trigger_gpio_interrupt(TEST_PIN);
    TEST_ASSERT_EQUAL_UINT32(1, pal_host_get_isr_call_count(TEST_PIN));
}

void test_gpio_interrupt_invalid_pin(void)
{
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
        pal_gpio_enable_interrupt(-1, PAL_GPIO_INTR_FALLING_EDGE, test_gpio_isr, NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
        pal_gpio_enable_interrupt(999, PAL_GPIO_INTR_FALLING_EDGE, test_gpio_isr, NULL));
}

void test_gpio_interrupt_null_callback(void)
{
    const wink_pin_t TEST_PIN = 11;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
        pal_gpio_enable_interrupt(TEST_PIN, PAL_GPIO_INTR_FALLING_EDGE, NULL, NULL));
}

void test_gpio_prio_locked_on_first_register(void)
{
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_gpio_enable_interrupt_ex(12, PAL_GPIO_INTR_FALLING_EDGE,
                                      PAL_IRQ_PRIO_HIGH, test_gpio_isr, NULL));
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_gpio_enable_interrupt_ex(13, PAL_GPIO_INTR_FALLING_EDGE,
                                      PAL_IRQ_PRIO_HIGH, test_gpio_isr, NULL));
}

void test_gpio_prio_mismatch_returns_invalid_arg(void)
{
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_gpio_enable_interrupt_ex(14, PAL_GPIO_INTR_FALLING_EDGE,
                                      PAL_IRQ_PRIO_NORMAL, test_gpio_isr, NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
        pal_gpio_enable_interrupt_ex(15, PAL_GPIO_INTR_FALLING_EDGE,
                                      PAL_IRQ_PRIO_HIGH, test_gpio_isr, NULL));
}

void test_gpio_non_ex_locks_to_normal(void)
{
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_gpio_enable_interrupt(16, PAL_GPIO_INTR_FALLING_EDGE,
                                   test_gpio_isr, NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
        pal_gpio_enable_interrupt_ex(17, PAL_GPIO_INTR_FALLING_EDGE,
                                      PAL_IRQ_PRIO_HIGH, test_gpio_isr, NULL));
}

void test_gpio_disable_does_not_unlock(void)
{
    const wink_pin_t PIN_A = 18;
    const wink_pin_t PIN_B = 19;

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_gpio_enable_interrupt_ex(PIN_A, PAL_GPIO_INTR_FALLING_EDGE,
                                      PAL_IRQ_PRIO_NORMAL, test_gpio_isr, NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
        pal_gpio_enable_interrupt_ex(PIN_B, PAL_GPIO_INTR_FALLING_EDGE,
                                      PAL_IRQ_PRIO_HIGH, test_gpio_isr, NULL));

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_disable_interrupt(PIN_A));

    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
        pal_gpio_enable_interrupt_ex(PIN_B, PAL_GPIO_INTR_FALLING_EDGE,
                                      PAL_IRQ_PRIO_HIGH, test_gpio_isr, NULL));
}

void test_irq_lock_pending_semantics(void)
{
    const wink_pin_t TEST_PIN = 12;

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_gpio_enable_interrupt(TEST_PIN, PAL_GPIO_INTR_FALLING_EDGE,
                                   test_gpio_isr, NULL));

    uint32_t mask = pal_irq_save();
    TEST_ASSERT_TRUE(pal_host_get_irq_lock_depth() > 0);

    pal_host_trigger_gpio_interrupt(TEST_PIN);

    TEST_ASSERT_EQUAL_UINT32(0, s_test_isr_count);
    TEST_ASSERT_EQUAL_UINT32(1, pal_host_get_pending_count());

    pal_irq_restore(mask);

    TEST_ASSERT_EQUAL_INT(0, pal_host_get_irq_lock_depth());
    TEST_ASSERT_EQUAL_UINT32(1, s_test_isr_count);
    TEST_ASSERT_EQUAL_UINT32(0, pal_host_get_pending_count());
}

void test_irq_lock_nesting(void)
{
    const wink_pin_t TEST_PIN = 13;

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_gpio_enable_interrupt(TEST_PIN, PAL_GPIO_INTR_FALLING_EDGE,
                                   test_gpio_isr, NULL));

    uint32_t mask1 = pal_irq_save();
    uint32_t mask2 = pal_irq_save();

    pal_host_trigger_gpio_interrupt(TEST_PIN);
    TEST_ASSERT_EQUAL_UINT32(0, s_test_isr_count);
    TEST_ASSERT_EQUAL_UINT32(1, pal_host_get_pending_count());

    pal_irq_restore(mask2);
    TEST_ASSERT_TRUE(pal_host_get_irq_lock_depth() > 0);
    TEST_ASSERT_EQUAL_UINT32(0, s_test_isr_count);
    TEST_ASSERT_EQUAL_UINT32(1, pal_host_get_pending_count());

    pal_irq_restore(mask1);
    TEST_ASSERT_EQUAL_INT(0, pal_host_get_irq_lock_depth());
    TEST_ASSERT_EQUAL_UINT32(1, s_test_isr_count);
}

void test_irq_lock_rtos_safe_same_semantics(void)
{
    const wink_pin_t TEST_PIN = 14;

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_gpio_enable_interrupt(TEST_PIN, PAL_GPIO_INTR_FALLING_EDGE,
                                   test_gpio_isr, NULL));

    uint32_t mask = pal_irq_save_rtos_safe();
    pal_host_trigger_gpio_interrupt(TEST_PIN);

    TEST_ASSERT_EQUAL_UINT32(0, s_test_isr_count);
    TEST_ASSERT_EQUAL_UINT32(1, pal_host_get_pending_count());

    pal_irq_restore(mask);
    TEST_ASSERT_EQUAL_UINT32(1, s_test_isr_count);
}

typedef struct {
    uint32_t counter;
    uint8_t  id;
    bool     flag;
} test_isr_context_t;

static test_isr_context_t s_test_ctx;

PAL_DEFINE_ISR(test_typed_isr, test_isr_context_t, ctx)
{
    ctx->counter++;
    ctx->flag = true;
    s_test_isr_arg_val = ctx->id;
}

void test_type_safe_isr_macro(void)
{
    const wink_pin_t TEST_PIN = 15;

    memset(&s_test_ctx, 0, sizeof(s_test_ctx));
    s_test_ctx.id = 0xAB;

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_gpio_enable_interrupt(TEST_PIN, PAL_GPIO_INTR_FALLING_EDGE,
                                   test_typed_isr, &s_test_ctx));

    pal_host_trigger_gpio_interrupt(TEST_PIN);

    TEST_ASSERT_EQUAL_UINT32(1, s_test_ctx.counter);
    TEST_ASSERT_TRUE(s_test_ctx.flag);
    TEST_ASSERT_EQUAL_UINT32(0xAB, s_test_isr_arg_val);
}

static void test_logical_isr(void *arg)
{
    s_test_isr_count++;
    s_test_isr_arg_val = (uint32_t)(uintptr_t)arg;
}

void test_logical_irq_enable_disable(void)
{
    const uint32_t TEST_IRQ = 5;

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_irq_enable(TEST_IRQ, PAL_IRQ_PRIO_NORMAL, test_logical_isr, (void *)0x1234));

    pal_host_trigger_logical_interrupt(TEST_IRQ);
    TEST_ASSERT_EQUAL_UINT32(1, s_test_isr_count);
    TEST_ASSERT_EQUAL_UINT32(0x1234, s_test_isr_arg_val);

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_irq_disable(TEST_IRQ));
    s_test_isr_count = 0;
    pal_host_trigger_logical_interrupt(TEST_IRQ);
    TEST_ASSERT_EQUAL_UINT32(0, s_test_isr_count);
}

void test_logical_irq_invalid_number(void)
{
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
        pal_irq_enable(999, PAL_IRQ_PRIO_NORMAL, test_logical_isr, NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
        pal_irq_disable(999));
}

void test_irq_priority_enum_bounds(void)
{
    const uint32_t TEST_IRQ = 8;

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_irq_enable(TEST_IRQ, PAL_IRQ_PRIO_LOW, test_logical_isr, NULL));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_irq_disable(TEST_IRQ));

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_irq_enable(TEST_IRQ, PAL_IRQ_PRIO_HIGH, test_logical_isr, NULL));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_irq_disable(TEST_IRQ));

    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
        pal_irq_enable(TEST_IRQ, (pal_irq_prio_t)999, test_logical_isr, NULL));
}

#if !defined(ESP_PLATFORM) && !defined(__EMSCRIPTEN__) && !defined(_WIN32)
#include <pthread.h>

typedef struct {
    pal_irq_prio_t prio;
    wink_pin_t     pin;
    wink_status_t  result;
} race_ctx_t;

static void race_isr(void *arg) { (void)arg; }

static void *race_worker(void *arg)
{
    race_ctx_t *ctx = (race_ctx_t *)arg;
    ctx->result = pal_gpio_enable_interrupt_ex(
        ctx->pin, PAL_GPIO_INTR_FALLING_EDGE, ctx->prio, race_isr, NULL);
    return NULL;
}

void test_gpio_concurrent_first_register_race(void)
{
    for (int iter = 0; iter < 100; ++iter) {
        pal_host_reset_isr_stats();

        race_ctx_t a = { .prio = PAL_IRQ_PRIO_NORMAL, .pin = 22, .result = WINK_OK };
        race_ctx_t b = { .prio = PAL_IRQ_PRIO_HIGH,   .pin = 23, .result = WINK_OK };

        pthread_t ta, tb;
        pthread_create(&ta, NULL, race_worker, &a);
        pthread_create(&tb, NULL, race_worker, &b);
        pthread_join(ta, NULL);
        pthread_join(tb, NULL);

        int ok_count = 0, invalid_count = 0;
        if (a.result == WINK_OK)              ok_count++;
        if (b.result == WINK_OK)              ok_count++;
        if (a.result == WINK_ERR_INVALID_ARG) invalid_count++;
        if (b.result == WINK_ERR_INVALID_ARG) invalid_count++;

        TEST_ASSERT_EQUAL_INT_MESSAGE(1, ok_count,
            "expected exactly one thread to win the first-register race");
        TEST_ASSERT_EQUAL_INT_MESSAGE(1, invalid_count,
            "the losing thread must return WINK_ERR_INVALID_ARG (not BUSY/UNSUPPORTED)");
    }
}
#else
void test_gpio_concurrent_first_register_race(void)
{
    TEST_IGNORE_MESSAGE("requires pthread (host build); skipped on ESP32/WASM");
}
#endif

void test_critical_section_macro(void)
{
    const wink_pin_t TEST_PIN = 20;

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_gpio_enable_interrupt(TEST_PIN, PAL_GPIO_INTR_FALLING_EDGE,
                                   test_gpio_isr, NULL));

    PAL_CRITICAL_SECTION({
        pal_host_trigger_gpio_interrupt(TEST_PIN);
        TEST_ASSERT_EQUAL_UINT32(0, s_test_isr_count);
        TEST_ASSERT_TRUE(pal_host_get_pending_count() > 0);
    });

    TEST_ASSERT_EQUAL_UINT32(1, s_test_isr_count);
    TEST_ASSERT_EQUAL_INT(0, pal_host_get_irq_lock_depth());
}

void test_critical_section_strict_macro(void)
{
    const wink_pin_t TEST_PIN = 21;

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_gpio_enable_interrupt(TEST_PIN, PAL_GPIO_INTR_FALLING_EDGE,
                                   test_gpio_isr, NULL));

    PAL_CRITICAL_SECTION_STRICT({
        pal_host_trigger_gpio_interrupt(TEST_PIN);
        TEST_ASSERT_EQUAL_UINT32(0, s_test_isr_count);
    });

    TEST_ASSERT_EQUAL_UINT32(1, s_test_isr_count);
}

void test_irq_synchronize_no_crash(void)
{
    const uint32_t TEST_IRQ = 15;

    pal_irq_synchronize(TEST_IRQ);
    pal_irq_synchronize(~0U);

    TEST_PASS();
}

static volatile uint32_t s_isr_critical_ok = 0;

static void test_isr_with_critical_section(void *arg)
{
    (void)arg;
    uint32_t key = pal_os_critical_enter_isr();
    s_isr_critical_ok = 1;
    pal_os_critical_exit_isr(key);
}

void test_isr_context_set_during_dispatch(void)
{
    const wink_pin_t TEST_PIN = 24;
    s_isr_critical_ok = 0;

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_gpio_enable_interrupt(TEST_PIN, PAL_GPIO_INTR_FALLING_EDGE,
                                   test_isr_with_critical_section, NULL));

    pal_host_trigger_gpio_interrupt(TEST_PIN);

    TEST_ASSERT_EQUAL_UINT32(1, s_isr_critical_ok);

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_disable_interrupt(TEST_PIN));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_gpio_interrupt_registration);
    RUN_TEST(test_gpio_interrupt_invalid_pin);
    RUN_TEST(test_gpio_interrupt_null_callback);

    RUN_TEST(test_gpio_prio_locked_on_first_register);
    RUN_TEST(test_gpio_prio_mismatch_returns_invalid_arg);
    RUN_TEST(test_gpio_non_ex_locks_to_normal);
    RUN_TEST(test_gpio_disable_does_not_unlock);

    RUN_TEST(test_irq_lock_pending_semantics);
    RUN_TEST(test_irq_lock_nesting);
    RUN_TEST(test_irq_lock_rtos_safe_same_semantics);

    RUN_TEST(test_type_safe_isr_macro);

    RUN_TEST(test_logical_irq_enable_disable);
    RUN_TEST(test_logical_irq_invalid_number);

    RUN_TEST(test_irq_priority_enum_bounds);

    RUN_TEST(test_gpio_concurrent_first_register_race);

    RUN_TEST(test_critical_section_macro);
    RUN_TEST(test_critical_section_strict_macro);

    RUN_TEST(test_irq_synchronize_no_crash);

    RUN_TEST(test_isr_context_set_during_dispatch);

    return UNITY_END();
}
