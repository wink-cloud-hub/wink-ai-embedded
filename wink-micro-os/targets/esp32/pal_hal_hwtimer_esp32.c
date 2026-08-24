// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_hal_hwtimer_esp32.c
 * @brief ESP32 target PAL HWTIMER driver using ESP-IDF gptimer (ESP-IDF 5.4+).
 */
#include "pal_hal.h"
#include "hal/pal_hwtimer.h"
#include "pal_resource.h"
#include "pal_spinlock.h"
#include "pal_log.h"

#include <string.h>

#if defined(ESP_PLATFORM)
#include "driver/gptimer.h"
#include "esp_err.h"
#include "esp_intr_alloc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define LOG_TAG "pal_hwtimer"

typedef struct {
    bool              in_use;
    bool              is_running;
    pal_hwtimer_cfg_t cfg;
    gptimer_handle_t  gptimer;
} esp32_hwtimer_slot_t;

static PAL_IRAM_DATA esp32_hwtimer_slot_t s_timers[PAL_HWTIMERS_MAX];
static pal_spinlock_t s_hwtimer_lock = PAL_SPINLOCK_INITIALIZER;

/* Static init task structures for Core 1 / Core 0 interrupt pinning */
typedef struct {
    esp32_hwtimer_slot_t *slot;
    esp_err_t             result;
} hwtimer_init_params_t;

static StaticTask_t           s_init_task_tcb;
static StackType_t            s_init_task_stack[2048];
static StaticSemaphore_t      s_init_sem_buffer;
static SemaphoreHandle_t      s_init_done_sem = NULL;

static PAL_ISR bool esp32_gptimer_on_alarm(gptimer_handle_t timer,
                                            const gptimer_alarm_event_data_t *edata,
                                            void *user_ctx) {
    (void)timer;
    (void)edata;
    esp32_hwtimer_slot_t *slot = (esp32_hwtimer_slot_t *)user_ctx;
    if (slot != NULL && slot->cfg.callback != NULL) {
        if (slot->cfg.oneshot) {
            slot->is_running = false;
        }
        slot->cfg.callback(slot->cfg.callback_arg);
    }
    return false;
}

static void hwtimer_pinned_init_task(void *pvParameters) {
    hwtimer_init_params_t *params = (hwtimer_init_params_t *)pvParameters;
    esp32_hwtimer_slot_t *slot = params->slot;

    esp_err_t err = gptimer_enable(slot->gptimer);
    if (err == ESP_OK && slot->cfg.auto_start) {
        err = gptimer_start(slot->gptimer);
        if (err == ESP_OK) {
            slot->is_running = true;
        }
    }
    params->result = err;

    if (s_init_done_sem != NULL) {
        xSemaphoreGive(s_init_done_sem);
    }
    vTaskDelete(NULL);
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_hwtimer_init(const pal_hwtimer_cfg_t *cfg) {
    if (cfg == NULL || cfg->timer_id >= PAL_HWTIMERS_MAX || cfg->period_us == 0) {
        return WINK_ERR_INVALID_ARG;
    }

    /* FPU Safety Rule: ISR is forbidden from using hardware FPU */
    if (cfg->uses_fpu) {
        LOG_W(LOG_TAG, "uses_fpu=true is rejected: HW timer ISR must be fixed-point Q15/Q31");
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_hwtimer_lock);
    esp32_hwtimer_slot_t *slot = &s_timers[cfg->timer_id];
    if (slot->in_use) {
        pal_spinlock_unlock(&s_hwtimer_lock);
        return WINK_ERR_BUSY;
    }

    wink_status_t st = pal_resource_claim(PAL_RESOURCE_HWTIMER, cfg->timer_id, "pal_hwtimer_esp32");
    if (st != WINK_OK) {
        pal_spinlock_unlock(&s_hwtimer_lock);
        return st;
    }

    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, /* 1 MHz resolution -> 1 tick = 1 us */
        .intr_priority = (cfg->isr_priority > 0 && cfg->isr_priority <= 3) ? cfg->isr_priority : 0,
        .flags.intr_shared = false,
    };

    esp_err_t err = gptimer_new_timer(&timer_config, &slot->gptimer);
    if (err != ESP_OK) {
        pal_resource_release(PAL_RESOURCE_HWTIMER, cfg->timer_id, "pal_hwtimer_esp32");
        pal_spinlock_unlock(&s_hwtimer_lock);
        return WINK_ERR_HARDWARE;
    }

    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        .alarm_count = cfg->period_us,
        .flags.auto_reload_on_alarm = !cfg->oneshot,
    };

    err = gptimer_set_alarm_action(slot->gptimer, &alarm_config);
    if (err != ESP_OK) {
        gptimer_del_timer(slot->gptimer);
        pal_resource_release(PAL_RESOURCE_HWTIMER, cfg->timer_id, "pal_hwtimer_esp32");
        pal_spinlock_unlock(&s_hwtimer_lock);
        return WINK_ERR_HARDWARE;
    }

    gptimer_event_callbacks_t cbs = {
        .on_alarm = esp32_gptimer_on_alarm,
    };
    gptimer_register_event_callbacks(slot->gptimer, &cbs, slot);

    slot->in_use = true;
    slot->cfg = *cfg;
    slot->is_running = false;

    /* Core Affinity: pin interrupt allocation to target core (default Core 1 for fast loop) */
    BaseType_t target_core = (cfg->core_affinity == PAL_OS_CORE_0) ? 0 : 1;
    if (s_init_done_sem == NULL) {
        s_init_done_sem = xSemaphoreCreateBinaryStatic(&s_init_sem_buffer);
    }

    hwtimer_init_params_t init_params = {
        .slot = slot,
        .result = ESP_OK,
    };

    TaskHandle_t task_h = xTaskCreatePinnedToCoreStatic(
        hwtimer_pinned_init_task,
        "hwtimer_init",
        sizeof(s_init_task_stack) / sizeof(StackType_t),
        &init_params,
        configMAX_PRIORITIES - 1,
        s_init_task_stack,
        &s_init_task_tcb,
        target_core
    );

    if (task_h != NULL) {
        xSemaphoreTake(s_init_done_sem, portMAX_DELAY);
    } else {
        /* Fallback: enable on current core */
        gptimer_enable(slot->gptimer);
        if (cfg->auto_start) {
            gptimer_start(slot->gptimer);
            slot->is_running = true;
        }
    }

    pal_spinlock_unlock(&s_hwtimer_lock);
    return (init_params.result == ESP_OK) ? WINK_OK : WINK_ERR_HARDWARE;
}

wink_status_t pal_hwtimer_start(uint8_t timer_id) {
    if (timer_id >= PAL_HWTIMERS_MAX) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_hwtimer_lock);
    esp32_hwtimer_slot_t *slot = &s_timers[timer_id];
    if (!slot->in_use) {
        pal_spinlock_unlock(&s_hwtimer_lock);
        return WINK_ERR_INVALID_STATE;
    }

    esp_err_t err = gptimer_start(slot->gptimer);
    if (err == ESP_OK) {
        slot->is_running = true;
    }
    pal_spinlock_unlock(&s_hwtimer_lock);
    return (err == ESP_OK) ? WINK_OK : WINK_ERR_HARDWARE;
}

wink_status_t pal_hwtimer_stop(uint8_t timer_id) {
    if (timer_id >= PAL_HWTIMERS_MAX) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_hwtimer_lock);
    esp32_hwtimer_slot_t *slot = &s_timers[timer_id];
    if (!slot->in_use) {
        pal_spinlock_unlock(&s_hwtimer_lock);
        return WINK_ERR_INVALID_STATE;
    }

    esp_err_t err = gptimer_stop(slot->gptimer);
    if (err == ESP_OK) {
        slot->is_running = false;
    }
    pal_spinlock_unlock(&s_hwtimer_lock);
    return (err == ESP_OK) ? WINK_OK : WINK_ERR_HARDWARE;
}

wink_status_t pal_hwtimer_change_period(uint8_t timer_id, uint32_t period_us) {
    if (timer_id >= PAL_HWTIMERS_MAX || period_us == 0) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_hwtimer_lock);
    esp32_hwtimer_slot_t *slot = &s_timers[timer_id];
    if (!slot->in_use) {
        pal_spinlock_unlock(&s_hwtimer_lock);
        return WINK_ERR_INVALID_STATE;
    }

    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        .alarm_count = period_us,
        .flags.auto_reload_on_alarm = !slot->cfg.oneshot,
    };

    esp_err_t err = gptimer_set_alarm_action(slot->gptimer, &alarm_config);
    if (err == ESP_OK) {
        slot->cfg.period_us = period_us;
    }
    pal_spinlock_unlock(&s_hwtimer_lock);
    return (err == ESP_OK) ? WINK_OK : WINK_ERR_HARDWARE;
}

wink_status_t pal_hwtimer_deinit(uint8_t timer_id) {
    if (timer_id >= PAL_HWTIMERS_MAX) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_hwtimer_lock);
    esp32_hwtimer_slot_t *slot = &s_timers[timer_id];
    if (!slot->in_use) {
        pal_spinlock_unlock(&s_hwtimer_lock);
        return WINK_ERR_INVALID_STATE;
    }

    if (slot->is_running) {
        gptimer_stop(slot->gptimer);
        slot->is_running = false;
    }

    gptimer_disable(slot->gptimer);
    gptimer_del_timer(slot->gptimer);
    slot->gptimer = NULL;

    pal_resource_release(PAL_RESOURCE_HWTIMER, timer_id, "pal_hwtimer_esp32");
    slot->in_use = false;

    pal_spinlock_unlock(&s_hwtimer_lock);
    return WINK_OK;
}

wink_status_t pal_hwtimer_fire_soft(uint8_t timer_id) {
    if (timer_id >= PAL_HWTIMERS_MAX) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_hwtimer_lock);
    esp32_hwtimer_slot_t *slot = &s_timers[timer_id];
    if (!slot->in_use || slot->cfg.callback == NULL) {
        pal_spinlock_unlock(&s_hwtimer_lock);
        return WINK_ERR_INVALID_STATE;
    }

    pal_hwtimer_isr_t cb = slot->cfg.callback;
    void *cb_arg = slot->cfg.callback_arg;
    pal_spinlock_unlock(&s_hwtimer_lock);

    cb(cb_arg);
    return WINK_OK;
}

#else

/* Non-ESP32 fallback stubs for cross-compilation static analysis */
WINK_WARN_UNUSED_RESULT wink_status_t pal_hwtimer_init(const pal_hwtimer_cfg_t *cfg) { (void)cfg; return WINK_ERR_UNSUPPORTED; }
wink_status_t pal_hwtimer_start(uint8_t timer_id) { (void)timer_id; return WINK_ERR_UNSUPPORTED; }
wink_status_t pal_hwtimer_stop(uint8_t timer_id) { (void)timer_id; return WINK_ERR_UNSUPPORTED; }
wink_status_t pal_hwtimer_change_period(uint8_t timer_id, uint32_t new_period_us) { (void)timer_id; (void)new_period_us; return WINK_ERR_UNSUPPORTED; }
void pal_hwtimer_deinit(uint8_t timer_id) { (void)timer_id; }
wink_status_t pal_hwtimer_fire_soft(uint8_t timer_id) { (void)timer_id; return WINK_ERR_UNSUPPORTED; }

#endif
