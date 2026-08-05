// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_irq_esp32.c
 * @brief ESP32 target PAL IRQ subsystem implementation.
 */
#include "pal_hal.h"
#define WINK_ALLOW_ADVANCED_IRQ_APIS
#include "pal_irq_advanced.h"
#include "pal_osal.h"
#include "pal_atomic_esp32.h"
#include "pal_hal_internal_esp32.h"

#if defined(ESP_PLATFORM)
#include "esp_err.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "xtensa/hal.h"

static volatile uint32_t s_irq_in_flight[32] = {0};

#define SYNCHRONIZE_TIMEOUT_US  100000

static intr_handle_t s_irq_handles[32] = {NULL};

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

    pal_irq_clear_pending(irq_num);

    Atomic_Increment_u32(&s_irq_in_flight[irq_num]);

    isr_wrapper_ctx_t *ctx = &s_isr_ctx[irq_num];
    if (ctx->user_handler != NULL) {
        ctx->user_handler(ctx->user_arg);
    }

    Atomic_Decrement_u32(&s_irq_in_flight[irq_num]);
}

wink_status_t pal_irq_enable(uint32_t irq_num, pal_irq_prio_t prio,
                              pal_isr_t handler, void *arg)
{
    if (irq_num >= 32 || handler == NULL ||
        prio < PAL_IRQ_PRIO_LOW || prio > PAL_IRQ_PRIO_HIGH) {
        return WINK_ERR_INVALID_ARG;
    }

    if (irq_num != 7 && irq_num != 8) {
        return WINK_ERR_INVALID_ARG;
    }

    static const int s_prio_flag_map[PAL_IRQ_PRIO_COUNT] = {
        [PAL_IRQ_PRIO_LOW]      = ESP_INTR_FLAG_LEVEL1,
        [PAL_IRQ_PRIO_NORMAL]   = ESP_INTR_FLAG_LEVEL2,
        [PAL_IRQ_PRIO_HIGH]     = ESP_INTR_FLAG_LEVEL3,
    };

    int source = (irq_num == 7) ? ETS_INTERNAL_SW0_INTR_SOURCE
                                 : ETS_INTERNAL_SW1_INTR_SOURCE;
    int flags = ESP_INTR_FLAG_IRAM | s_prio_flag_map[prio];

    intr_handle_t new_handle = NULL;
    esp_err_t err = esp_intr_alloc(source, flags,
                                    (intr_handler_t)generic_isr_wrapper,
                                    (void *)(uintptr_t)irq_num,
                                    &new_handle);
    if (err != ESP_OK) {
        return WINK_ERR_HARDWARE;
    }

    if (s_irq_handles[irq_num] != NULL) {
        (void)esp_intr_free(s_irq_handles[irq_num]);
        s_irq_handles[irq_num] = NULL;
    }
    s_isr_ctx[irq_num].user_handler = handler;
    s_isr_ctx[irq_num].user_arg     = arg;
    s_irq_handles[irq_num]          = new_handle;

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
    if (irq_num == ~0U) {
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
        pal_esp32_gpio_synchronize_all(SYNCHRONIZE_TIMEOUT_US);
    } else {
        uint64_t start = pal_os_get_us();
        while (Atomic_Load_u32(&s_irq_in_flight[irq_num]) > 0) {
            if (pal_os_get_us() - start > SYNCHRONIZE_TIMEOUT_US) {
                ESP_LOGE("pal_irq", "synchronize timeout on irq=%lu",
                         (unsigned long)irq_num);
                break;
            }
        }
    }

    esp_memory_barrier();
}

uint32_t pal_irq_save(void)
{
    return XTOS_SET_INTLEVEL(XCHAL_NUM_INTLEVELS);
}

uint32_t pal_irq_save_rtos_safe(void)
{
    return XTOS_SET_INTLEVEL(XCHAL_EXCM_LEVEL);
}

void pal_irq_restore(uint32_t mask)
{
    XTOS_RESTORE_JUST_INTLEVEL(mask);
}

#endif
