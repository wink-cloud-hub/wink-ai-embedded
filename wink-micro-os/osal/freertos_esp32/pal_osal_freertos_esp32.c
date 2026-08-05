// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_osal_freertos_esp32.c
 * @brief ESP32 physical target PAL OSAL implementation (FreeRTOS + ESP-IDF).
 *
 * @verified HARDWARE-SMOKE-PASSED (DevKitC, 2026-06-27)
 *    - pal_os_get_ms(): monotonic timestamp verified
 *    - pal_os_wdt_init(): timeout + reset + reason detection works
 *    - Reset reason: WATCHDOG/PANIC detected by runtime boot check
 * @verified HARDWARE-SMOKE-PASSED (DevKitC, 2026-06-28) — ADR-0007 closed-loop
 *    - pal_os_task_create(): Core 0/1/ANY affinity pinning verified
 *    - pal_os_ringbuf_create/push/pop(): Cross-core ring buffer verified
 */

#include "pal_osal.h"
#include <stdlib.h>     /* malloc/free */
#include <string.h>     /* memcpy */

#if defined(ESP_PLATFORM)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/ringbuf.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "esp_system.h"       /* esp_reset_reason() + esp_reset_reason_t */
#include "esp_idf_version.h"
#include "esp_attr.h"         /* RTC_NOINIT_ATTR (boot-count persistence, ADR-0010) */
#else
/* Non-ESP32 build stub definitions for static analysis */
typedef void* SemaphoreHandle_t;
#define pdMS_TO_TICKS(ms) (ms)
#define portMAX_DELAY 0xffffffff
#define portMUX_INITIALIZER_UNLOCKED {0}
typedef struct { int reserved; } portMUX_TYPE;
#endif

/* ---------------------------------------------------------
 * System Time and High-Precision Delays
 * --------------------------------------------------------- */

void pal_os_sleep_ms(uint32_t ms) {
    extern int esp_rom_printf(const char *fmt, ...);
    if (xPortInIsrContext() == pdTRUE) {
        esp_rom_printf("WARNING: pal_os_sleep_ms(%lu) called in ISR context! Falling back to busy-wait.\n", (unsigned long)ms);
        esp_rom_delay_us(ms * 1000ULL);
        return;
    }
    if (ms == 0) {
        taskYIELD();
        return;
    }
    uint32_t ticks = pdMS_TO_TICKS(ms);
    if (ticks == 0) {
        ticks = 1;
    }
    vTaskDelay(ticks);
}

void pal_os_busy_wait_us(uint32_t us) {
    esp_rom_delay_us(us);
}

uint64_t pal_os_get_ms(void) {
    return esp_timer_get_time() / 1000ULL;
}

uint64_t pal_os_get_us(void) {
    return esp_timer_get_time();
}

/* ---------------------------------------------------------
 * Thread Synchronization Mutex
 * --------------------------------------------------------- */

pal_os_mutex_t pal_os_mutex_create(void) {
    SemaphoreHandle_t mux = xSemaphoreCreateMutex();
    return (pal_os_mutex_t)mux;
}

wink_status_t pal_os_mutex_lock(pal_os_mutex_t mutex, uint32_t timeout_ms) {
    if (mutex == NULL) { return WINK_ERR_INVALID_ARG; }
    BaseType_t ok = xSemaphoreTake((SemaphoreHandle_t)mutex,
        timeout_ms == WINK_MUTEX_WAIT_FOREVER ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms));
    return ok == pdPASS ? WINK_OK : WINK_ERR_TIMEOUT;
}

wink_status_t pal_os_mutex_unlock(pal_os_mutex_t mutex) {
    if (mutex == NULL) { return WINK_ERR_INVALID_ARG; }
    BaseType_t ok = xSemaphoreGive((SemaphoreHandle_t)mutex);
    return ok == pdPASS ? WINK_OK : WINK_ERR_HARDWARE;
}

void pal_os_mutex_destroy(pal_os_mutex_t mutex) {
    if (mutex != NULL) {
        vSemaphoreDelete((SemaphoreHandle_t)mutex);
    }
}

/* ---------------------------------------------------------
 * Thread Synchronization Binary Semaphore
 * --------------------------------------------------------- */

pal_os_sem_t pal_os_sem_create(void) {
    SemaphoreHandle_t sem = xSemaphoreCreateBinary();
    return (pal_os_sem_t)sem;
}

wink_status_t pal_os_sem_take(pal_os_sem_t sem, uint32_t timeout_ms) {
    if (sem == NULL) { return WINK_ERR_INVALID_ARG; }
    BaseType_t ok = xSemaphoreTake((SemaphoreHandle_t)sem,
        timeout_ms == WINK_MUTEX_WAIT_FOREVER ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms));
    return ok == pdPASS ? WINK_OK : WINK_ERR_TIMEOUT;
}

wink_status_t pal_os_sem_give(pal_os_sem_t sem) {
    if (sem == NULL) { return WINK_ERR_INVALID_ARG; }
    BaseType_t ok = xSemaphoreGive((SemaphoreHandle_t)sem);
    return ok == pdPASS ? WINK_OK : WINK_ERR_HARDWARE;
}

wink_status_t pal_os_sem_give_isr(pal_os_sem_t sem) {
    if (sem == NULL) { return WINK_ERR_INVALID_ARG; }
    BaseType_t higher_priority_task_woken = pdFALSE;
    BaseType_t ok = xSemaphoreGiveFromISR((SemaphoreHandle_t)sem, &higher_priority_task_woken);
    if (higher_priority_task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
    return ok == pdPASS ? WINK_OK : WINK_ERR_HARDWARE;
}

void pal_os_sem_destroy(pal_os_sem_t sem) {
    if (sem != NULL) {
        vSemaphoreDelete((SemaphoreHandle_t)sem);
    }
}

/* ---------------------------------------------------------
 * Reset Reason and Watchdog (Phase 5 Fail-Safe)
 * --------------------------------------------------------- */

pal_os_reset_reason_t pal_os_get_reset_reason(void) {
    esp_reset_reason_t rr = esp_reset_reason();
    switch (rr) {
        case ESP_RST_POWERON:     return PAL_OS_RESET_REASON_POWER_ON;
        case ESP_RST_SW:          return PAL_OS_RESET_REASON_SOFTWARE;
        case ESP_RST_INT_WDT:     return PAL_OS_RESET_REASON_WATCHDOG;
        case ESP_RST_TASK_WDT:    return PAL_OS_RESET_REASON_WATCHDOG;
        case ESP_RST_WDT:         return PAL_OS_RESET_REASON_WATCHDOG;
        case ESP_RST_BROWNOUT:    return PAL_OS_RESET_REASON_BROWNOUT;
        case ESP_RST_PANIC:       return PAL_OS_RESET_REASON_PANIC;   /* Triggers boot safe-lock */
        default:                  return PAL_OS_RESET_REASON_UNKNOWN;
    }
}

/* ADR-0010 boot safe-lock recovery strategy */
#define WINK_BOOT_COUNT_MAGIC 0xB007C0DEu
static RTC_NOINIT_ATTR uint32_t s_abnormal_count;
static RTC_NOINIT_ATTR uint32_t s_abnormal_count_magic;

uint32_t pal_os_get_abnormal_boot_count(void) {
    return (s_abnormal_count_magic == WINK_BOOT_COUNT_MAGIC) ? s_abnormal_count : 0u;
}

void pal_os_set_abnormal_boot_count(uint32_t count) {
    s_abnormal_count = count;
    s_abnormal_count_magic = WINK_BOOT_COUNT_MAGIC;
}

WINK_WARN_UNUSED_RESULT wink_status_t pal_os_wdt_init(uint32_t timeout_ms) {
    /* ESP-IDF v5.x Task Watchdog API */
    esp_task_wdt_config_t cfg = {
        .timeout_ms = timeout_ms,
        .idle_core_mask = 0,  /* Do not monitor idle task */
        .trigger_panic = true,
    };
    esp_err_t err = esp_task_wdt_init(&cfg);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
    /* If already initialized, attempt reconfiguration (supported in v5.1+) */
    if (err == ESP_ERR_INVALID_STATE) {
        err = esp_task_wdt_reconfigure(&cfg);
    }
#endif
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return WINK_ERR_HARDWARE;
    }
    err = esp_task_wdt_add(NULL);  /* Subscribe current task */
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return WINK_ERR_HARDWARE;
    }
    return WINK_OK;
}

WINK_WARN_UNUSED_RESULT wink_status_t pal_os_wdt_feed(void) {
    esp_err_t err = esp_task_wdt_reset();
    if (err != ESP_OK) { return WINK_ERR_HARDWARE; }
    return WINK_OK;
}

/* ---------------------------------------------------------
 * Critical Section (Explicit task/ISR dual-entry dispatch, ADR-0016)
 * --------------------------------------------------------- */

static portMUX_TYPE s_global_mux = portMUX_INITIALIZER_UNLOCKED;

uint32_t pal_os_critical_enter(void) {
    portENTER_CRITICAL(&s_global_mux);
    return 0;
}

void pal_os_critical_exit(uint32_t key) {
    (void)key;
    portEXIT_CRITICAL(&s_global_mux);
}

uint32_t pal_os_critical_enter_isr(void) {
    portENTER_CRITICAL_ISR(&s_global_mux);
    return 0;
}

void pal_os_critical_exit_isr(uint32_t key) {
    (void)key;
    portEXIT_CRITICAL_ISR(&s_global_mux);
}

/* Physical context provided directly by FreeRTOS; simulation context flags are no-ops */
void pal_os_set_sim_isr_context(bool in_isr) { (void)in_isr; }
bool pal_os_in_sim_isr_context(void) { return false; }
bool pal_os_in_isr(void) { return xPortInIsrContext() == pdTRUE; }

void pal_os_set_sim_pt_context(bool in_pt) { (void)in_pt; }
bool pal_os_in_sim_pt_context(void) { return false; }
bool wink_pt_in_context(void) { return false; }

/* ---------------------------------------------------------
 * Task Creation and Multi-Core Affinity
 * --------------------------------------------------------- */

wink_status_t pal_os_task_create(
    void (*func)(void* arg),
    const char* name,
    uint32_t stack_depth,
    void* arg,
    int32_t priority,
    pal_os_core_id_t core_id,
    pal_os_task_handle_t* task_handle
) {
    BaseType_t core;
    TaskHandle_t xHandle;
    BaseType_t ret;

    /* Map PAL core ID to FreeRTOS xCoreID */
    switch (core_id) {
        case PAL_OS_CORE_0:
            core = 0;                /* Pin to Core 0 */
            break;
        case PAL_OS_CORE_1:
            core = 1;                /* Pin to Core 1 for control loop isolation */
            break;
        case PAL_OS_CORE_ANY:
        default:
            core = tskNO_AFFINITY;   /* Explicit ANY uses no affinity */
            break;
    }

    ret = xTaskCreatePinnedToCore(
        (TaskFunction_t)func,
        name,
        stack_depth / sizeof(StackType_t),  /* FreeRTOS uses words, not bytes */
        arg,
        priority,
        &xHandle,
        core
    );

    if (ret != pdPASS) {
        return WINK_ERR_NO_MEM;
    }

    if (task_handle != NULL) {
        *task_handle = (pal_os_task_handle_t)xHandle;
    }

    return WINK_OK;
}

void pal_os_task_delete(pal_os_task_handle_t task_handle) {
    vTaskDelete((TaskHandle_t)task_handle);
}

uint32_t pal_os_get_free_heap_size(void) {
    return (uint32_t)xPortGetFreeHeapSize();
}

uint32_t pal_os_get_min_free_heap_size(void) {
    return (uint32_t)xPortGetMinimumEverFreeHeapSize();
}

uint32_t pal_os_get_current_task_stack_free(void) {
    return (uint32_t)uxTaskGetStackHighWaterMark(NULL) * (uint32_t)sizeof(StackType_t);
}

/* ---------------------------------------------------------
 * Cross-Core Communication Ring Buffer (Ringbuf)
 * --------------------------------------------------------- */

struct pal_os_ringbuf {
    RingbufHandle_t handle;
    uint32_t size;
};

pal_os_ringbuf_handle_t pal_os_ringbuf_create(uint32_t size) {
    struct pal_os_ringbuf* rb;

    /* Size must be power of 2 (API contract) */
    if ((size & (size - 1)) != 0) {
        return NULL;
    }

    rb = malloc(sizeof(struct pal_os_ringbuf));
    if (rb == NULL) {
        return NULL;
    }

    rb->size = size;
    rb->handle = xRingbufferCreate(size, RINGBUF_TYPE_BYTEBUF);
    if (rb->handle == NULL) {
        free(rb);
        return NULL;
    }

    return rb;
}

wink_status_t pal_os_ringbuf_push(
    pal_os_ringbuf_handle_t rb,
    const void* data,
    uint32_t size
) {
    BaseType_t ret;

    if (rb == NULL || data == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    /* Non-blocking push (tick = 0), task context only */
    ret = xRingbufferSend(rb->handle, (void*)data, size, 0);
    if (ret != pdTRUE) {
        return WINK_ERR_FULL;
    }

    return WINK_OK;
}

wink_status_t pal_os_ringbuf_pop(
    pal_os_ringbuf_handle_t rb,
    void* data,
    uint32_t size
) {
    uint8_t* item;
    size_t item_size;

    if (rb == NULL || data == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    /* Non-blocking pop (tick = 0) */
    item = xRingbufferReceive(rb->handle, &item_size, 0);
    if (item == NULL) {
        return WINK_ERR_EMPTY;
    }

    if (item_size != size) {
        /* Size mismatch - return item and indicate state error */
        vRingbufferReturnItem(rb->handle, item);
        return WINK_ERR_INVALID_STATE;
    }

    memcpy(data, item, size);
    vRingbufferReturnItem(rb->handle, item);

    return WINK_OK;
}

uint32_t pal_os_ringbuf_used(pal_os_ringbuf_handle_t rb) {
    if (rb == NULL) {
        return 0;
    }
    size_t free_size = xRingbufferGetCurFreeSize(rb->handle);
    if (free_size >= rb->size) {
        return 0;
    }
    return (uint32_t)(rb->size - free_size);
}

void pal_os_ringbuf_destroy(pal_os_ringbuf_handle_t rb) {
    if (rb == NULL) {
        return;
    }

    vRingbufferDelete(rb->handle);
    free(rb);
}
