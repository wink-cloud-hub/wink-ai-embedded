// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_wasm_irq.c
 * @brief Wasm simulation target PAL IRQ subsystem implementation.
 */
#include "pal_hal.h"
#include "pal_osal.h"
#define WINK_ALLOW_ADVANCED_IRQ_APIS
#include "pal_irq_advanced.h"
#include "wasm_bridge.h"
#include "pal_wasm_common.h"

#if defined(__EMSCRIPTEN__)

_Static_assert(sizeof(void*) == 4,
    "wasm64 migration required: see wasm_bridge.h ABI contract #5 "
    "and review every (uint32_t)(uintptr_t) cast in pal_wasm_irq.c / createUnisimImports.ts");

#define WASM_MAX_GPIO_PIN  50
static pal_gpio_isr_t s_gpio_isr[WASM_MAX_GPIO_PIN] = {NULL};
static void           *s_gpio_isr_arg[WASM_MAX_GPIO_PIN] = {NULL};
static pal_gpio_intr_t s_gpio_intr_type[WASM_MAX_GPIO_PIN] = {PAL_GPIO_INTR_DISABLE};

static bool             s_gpio_service_initialized = false;
static pal_irq_prio_t   s_gpio_service_prio        = PAL_IRQ_PRIO_NORMAL;

static uint32_t s_irq_lock_nest_count = 0;

#define WASM_MAX_PENDING  64

typedef struct {
    uint32_t irq_num;
} wasm_pending_irq_t;

static wasm_pending_irq_t s_pending_queue[WASM_MAX_PENDING];
static uint32_t s_pending_head = 0;
static uint32_t s_pending_count = 0;
static uint32_t s_pending_overflow_count = 0;

static inline void sw_enqueue(uint32_t irq_num) {
    if (s_pending_count >= WASM_MAX_PENDING) {
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

    if (s_gpio_service_initialized) {
        if (prio != s_gpio_service_prio) {
            return WINK_ERR_INVALID_ARG;
        }
    } else {
        s_gpio_service_prio        = prio;
        s_gpio_service_initialized = true;
    }

    s_gpio_isr[pin] = callback;
    s_gpio_isr_arg[pin] = arg;
    s_gpio_intr_type[pin] = intr_type;

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

wink_status_t pal_gpio_synchronize_interrupt(wink_pin_t pin)
{
    if (pin < 0 || pin >= WASM_MAX_GPIO_PIN) {
        return WINK_ERR_INVALID_ARG;
    }
    return WINK_OK;
}

#define WASM_MAX_IRQ  32
static pal_isr_t s_wasm_irq_table[WASM_MAX_IRQ] = {NULL};
static void *s_wasm_irq_arg[WASM_MAX_IRQ] = {NULL};

wink_status_t pal_irq_enable(uint32_t irq_num, pal_irq_prio_t prio,
                              pal_isr_t handler, void *arg)
{
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
}

void pal_irq_synchronize(uint32_t irq_num)
{
    (void)irq_num;
}

uint32_t pal_irq_save(void)
{
    uint32_t was_enabled = (s_irq_lock_nest_count == 0) ? 1 : 0;
    s_irq_lock_nest_count++;
    return was_enabled;
}

uint32_t pal_irq_save_rtos_safe(void)
{
    return pal_irq_save();
}

void pal_irq_restore(uint32_t mask)
{
    if (s_irq_lock_nest_count > 0) {
        s_irq_lock_nest_count--;
        if (s_irq_lock_nest_count == 0 && mask) {
            pal_wasm_dispatch_pending_interrupts();
        }
    }
}

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
    }
}

void pal_wasm_dispatch_pending_interrupts(void) {
    if (s_irq_lock_nest_count > 0) {
        return;
    }

    uint32_t callback_index;
    uint32_t arg_ptr;
    while (js_pal_poll_interrupt(&callback_index, &arg_ptr)) {
        pal_gpio_isr_t isr = (pal_gpio_isr_t)(uintptr_t)callback_index;
        if (isr != NULL) {
            pal_os_set_sim_isr_context(true);
            isr((void *)(uintptr_t)arg_ptr);
            pal_os_set_sim_isr_context(false);
        }
    }

    pal_wasm_dispatch_pending_irqs();
}

#endif
