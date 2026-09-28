/* SPDX-License-Identifier: GPL-3.0-only
 *
 * test_esp_idf_phase3.c — Full test suite for ESP-IDF Simulation Interception Phase 3
 *
 * Per PLAN-20260927-ESP-IDF-SIM-PHASE3 §6 Task 5:
 *   Group 1: Virtual GPIO edge ISR, dual-context injection, and FreeRTOS waking
 *   Group 2: NVS persistence, CRUD lifecycle, and cold reboot from sandbox file
 *   Group 3: Heap capabilities (DMA alignment, honest ordinary-domain hints, SPIRAM rejection)
 *   Group 4: Ordinary libc free() and special allocation tracker isolation
 *   Group 5: Dynamic tree/node parsing and lifecycle validation (JSON-like interop)
 */

#include "unity.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

/* FreeRTOS and ESP-IDF headers */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "freertos/portmacro.h"
#include "freertos_sync.h"

#include "driver/gpio.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_idf_wink.h"

#include "wink_sim_scheduler.h"
#include "osal/pal_osal.h"

extern void sim_set_mono_time_us(uint64_t us);

/* ── Unity setUp / tearDown ─────────────────────────────────────────────── */

void setUp(void) {
    sim_set_mono_time_us(0);
    sim_scheduler_reset(42);
    esp_freertos_pools_reset();
    esp_peripherals_reset();
    nvs_flash_erase();
}

void tearDown(void) {
    sim_set_mono_time_us(0);
    sim_scheduler_reset(0);
    esp_freertos_pools_reset();
    esp_peripherals_reset();
    nvs_flash_erase();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Group 1: Virtual GPIO ISR, Dual Context Injection & FreeRTOS Waking
 * ═══════════════════════════════════════════════════════════════════════════ */

static uint32_t s_isr_call_count = 0;
static void *s_isr_last_arg = NULL;
static bool s_isr_observed_in_isr = false;

static void test_gpio_isr_handler(void *arg) {
    s_isr_call_count++;
    s_isr_last_arg = arg;
    s_isr_observed_in_isr = pal_os_in_isr();
}

void test_gpio_isr_edge_filtering_and_lifecycle(void) {
    s_isr_call_count = 0;
    s_isr_last_arg = NULL;
    s_isr_observed_in_isr = false;

    /* Install ISR service */
    TEST_ASSERT_EQUAL_INT(ESP_OK, gpio_install_isr_service(0));

    gpio_num_t pin = GPIO_NUM_4;
    TEST_ASSERT_EQUAL_INT(ESP_OK, gpio_set_intr_type(pin, GPIO_INTR_POSEDGE));
    TEST_ASSERT_EQUAL_INT(ESP_OK, gpio_isr_handler_add(pin, test_gpio_isr_handler, (void *)0x42));

    /* POSEDGE: Falling edge 1 -> 0 must NOT trigger handler */
    TEST_ASSERT_EQUAL_INT(ESP_OK, esp_sim_gpio_inject_edge(pin, 1, 0));
    TEST_ASSERT_EQUAL_UINT32(0, s_isr_call_count);

    /* POSEDGE: Rising edge 0 -> 1 MUST trigger handler */
    TEST_ASSERT_EQUAL_INT(ESP_OK, esp_sim_gpio_inject_edge(pin, 0, 1));
    TEST_ASSERT_EQUAL_UINT32(1, s_isr_call_count);
    TEST_ASSERT_EQUAL_PTR((void *)0x42, s_isr_last_arg);
    TEST_ASSERT_TRUE(s_isr_observed_in_isr);

    /* Change to NEGEDGE */
    TEST_ASSERT_EQUAL_INT(ESP_OK, gpio_set_intr_type(pin, GPIO_INTR_NEGEDGE));
    TEST_ASSERT_EQUAL_INT(ESP_OK, esp_sim_gpio_inject_edge(pin, 0, 1));
    TEST_ASSERT_EQUAL_UINT32(1, s_isr_call_count);

    TEST_ASSERT_EQUAL_INT(ESP_OK, esp_sim_gpio_inject_edge(pin, 1, 0));
    TEST_ASSERT_EQUAL_UINT32(2, s_isr_call_count);

    /* Change to ANYEDGE */
    TEST_ASSERT_EQUAL_INT(ESP_OK, gpio_set_intr_type(pin, GPIO_INTR_ANYEDGE));
    TEST_ASSERT_EQUAL_INT(ESP_OK, esp_sim_gpio_inject_edge(pin, 0, 1));
    TEST_ASSERT_EQUAL_UINT32(3, s_isr_call_count);
    TEST_ASSERT_EQUAL_INT(ESP_OK, esp_sim_gpio_inject_edge(pin, 1, 0));
    TEST_ASSERT_EQUAL_UINT32(4, s_isr_call_count);

    /* Disable interrupt */
    TEST_ASSERT_EQUAL_INT(ESP_OK, gpio_intr_disable(pin));
    TEST_ASSERT_EQUAL_INT(ESP_OK, esp_sim_gpio_inject_edge(pin, 0, 1));
    TEST_ASSERT_EQUAL_UINT32(4, s_isr_call_count);

    /* Re-enable interrupt */
    TEST_ASSERT_EQUAL_INT(ESP_OK, gpio_intr_enable(pin));
    TEST_ASSERT_EQUAL_INT(ESP_OK, esp_sim_gpio_inject_edge(pin, 0, 1));
    TEST_ASSERT_EQUAL_UINT32(5, s_isr_call_count);

    /* Remove handler */
    TEST_ASSERT_EQUAL_INT(ESP_OK, gpio_isr_handler_remove(pin));
    TEST_ASSERT_EQUAL_INT(ESP_OK, esp_sim_gpio_inject_edge(pin, 0, 1));
    TEST_ASSERT_EQUAL_UINT32(5, s_isr_call_count);

    /* Uninstall service */
    TEST_ASSERT_EQUAL_INT(ESP_OK, gpio_uninstall_isr_service());
}

/* Waking test: Higher priority task waiting on Queue */
static QueueHandle_t s_test_queue = NULL;
static SemaphoreHandle_t s_test_sem = NULL;
static EventGroupHandle_t s_test_event = NULL;
static bool s_high_prio_queue_task_done = false;
static bool s_high_prio_sem_task_done = false;
static bool s_high_prio_event_task_done = false;
static uint32_t s_received_queue_val = 0;

static void isr_waking_queue_handler(void *arg) {
    (void)arg;
    uint32_t val = 999;
    BaseType_t higher_prio_woken = pdFALSE;
    xQueueSendFromISR(s_test_queue, &val, &higher_prio_woken);
    /* Higher priority task is waiting, so higher_prio_woken MUST be pdTRUE */
    TEST_ASSERT_EQUAL(pdTRUE, higher_prio_woken);
    portYIELD_FROM_ISR(higher_prio_woken);
}

static void high_prio_queue_waiter_task(void *arg) {
    (void)arg;
    uint32_t val = 0;
    if (xQueueReceive(s_test_queue, &val, 50) == pdPASS) {
        s_received_queue_val = val;
        s_high_prio_queue_task_done = true;
    }
    vTaskDelete(NULL);
}

static void low_prio_queue_injector_task(void *arg) {
    (void)arg;
    vTaskDelay(2); /* Let high prio task run first and block on queue */
    /* Inject edge from within fiber context */
    esp_sim_gpio_inject_edge(GPIO_NUM_5, 0, 1);
    vTaskDelete(NULL);
}

void test_gpio_isr_queue_priority_waking_in_fiber(void) {
    s_test_queue = xQueueCreate(2, sizeof(uint32_t));
    TEST_ASSERT_NOT_NULL(s_test_queue);
    s_high_prio_queue_task_done = false;
    s_received_queue_val = 0;

    TEST_ASSERT_EQUAL_INT(ESP_OK, gpio_install_isr_service(0));
    TEST_ASSERT_EQUAL_INT(ESP_OK, gpio_set_intr_type(GPIO_NUM_5, GPIO_INTR_POSEDGE));
    TEST_ASSERT_EQUAL_INT(ESP_OK, gpio_isr_handler_add(GPIO_NUM_5, isr_waking_queue_handler, NULL));

    TaskHandle_t h_high, h_low;
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(high_prio_queue_waiter_task, "high", 32 * 1024, NULL, 6, &h_high));
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(low_prio_queue_injector_task, "low", 32 * 1024, NULL, 2, &h_low));

    wink_status_t st = pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 50);
    TEST_ASSERT_EQUAL(WINK_OK, st);

    TEST_ASSERT_TRUE(s_high_prio_queue_task_done);
    TEST_ASSERT_EQUAL_UINT32(999, s_received_queue_val);

    vQueueDelete(s_test_queue);
    gpio_uninstall_isr_service();
}

static void isr_waking_sem_handler(void *arg) {
    (void)arg;
    BaseType_t higher_prio_woken = pdFALSE;
    xSemaphoreGiveFromISR(s_test_sem, &higher_prio_woken);
    /* In fiber context with waiting high prio task, higher_prio_woken must be pdTRUE */
    if (s_test_sem && higher_prio_woken == pdTRUE) {
        portYIELD_FROM_ISR(higher_prio_woken);
    }
}

static void high_prio_sem_waiter_task(void *arg) {
    (void)arg;
    if (xSemaphoreTake(s_test_sem, 50) == pdPASS) {
        s_high_prio_sem_task_done = true;
    }
    vTaskDelete(NULL);
}

static void low_prio_sem_injector_task(void *arg) {
    (void)arg;
    vTaskDelay(2); /* Let waiter task block on semaphore */
    esp_sim_gpio_inject_edge(GPIO_NUM_18, 0, 1);
    vTaskDelete(NULL);
}

static void isr_waking_event_handler(void *arg) {
    (void)arg;
    BaseType_t higher_prio_woken = pdFALSE;
    xEventGroupSetBitsFromISR(s_test_event, 0x08, &higher_prio_woken);
    if (s_test_event && higher_prio_woken == pdTRUE) {
        portYIELD_FROM_ISR(higher_prio_woken);
    }
}

static void high_prio_event_waiter_task(void *arg) {
    (void)arg;
    EventBits_t bits = xEventGroupWaitBits(s_test_event, 0x08, pdTRUE, pdFALSE, 50);
    if ((bits & 0x08) != 0) {
        s_high_prio_event_task_done = true;
    }
    vTaskDelete(NULL);
}

static void low_prio_event_injector_task(void *arg) {
    (void)arg;
    vTaskDelay(2); /* Let waiter task block on event bits */
    esp_sim_gpio_inject_edge(GPIO_NUM_19, 0, 1);
    vTaskDelete(NULL);
}

void test_gpio_isr_semaphore_and_eventgroup_waking_in_fiber(void) {
    /* 1. Test Semaphore waking in fiber */
    s_test_sem = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(s_test_sem);
    s_high_prio_sem_task_done = false;

    TEST_ASSERT_EQUAL_INT(ESP_OK, gpio_install_isr_service(0));
    TEST_ASSERT_EQUAL_INT(ESP_OK, gpio_set_intr_type(GPIO_NUM_18, GPIO_INTR_POSEDGE));
    TEST_ASSERT_EQUAL_INT(ESP_OK, gpio_isr_handler_add(GPIO_NUM_18, isr_waking_sem_handler, NULL));

    TaskHandle_t h_wait, h_inj;
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(high_prio_sem_waiter_task, "sem_wait", 32 * 1024, NULL, 6, &h_wait));
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(low_prio_sem_injector_task, "sem_inj", 32 * 1024, NULL, 2, &h_inj));

    wink_status_t st = pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 50);
    TEST_ASSERT_EQUAL(WINK_OK, st);
    TEST_ASSERT_TRUE(s_high_prio_sem_task_done);

    vSemaphoreDelete(s_test_sem);
    gpio_uninstall_isr_service();

    /* 2. Test EventGroup waking in fiber */
    s_test_event = xEventGroupCreate();
    TEST_ASSERT_NOT_NULL(s_test_event);
    s_high_prio_event_task_done = false;

    TEST_ASSERT_EQUAL_INT(ESP_OK, gpio_install_isr_service(0));
    TEST_ASSERT_EQUAL_INT(ESP_OK, gpio_set_intr_type(GPIO_NUM_19, GPIO_INTR_POSEDGE));
    TEST_ASSERT_EQUAL_INT(ESP_OK, gpio_isr_handler_add(GPIO_NUM_19, isr_waking_event_handler, NULL));

    TaskHandle_t h_ev_wait, h_ev_inj;
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(high_prio_event_waiter_task, "ev_wait", 32 * 1024, NULL, 6, &h_ev_wait));
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(low_prio_event_injector_task, "ev_inj", 32 * 1024, NULL, 2, &h_ev_inj));

    st = pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 50);
    TEST_ASSERT_EQUAL(WINK_OK, st);
    TEST_ASSERT_TRUE(s_high_prio_event_task_done);

    vEventGroupDelete(s_test_event);
    gpio_uninstall_isr_service();
}

void test_gpio_isr_host_thread_injection_safety(void) {
    /* Test host-thread edge injection when no fiber is active (sim_scheduler_current_ctx() == NULL).
     * Must not attempt to switch context and must not crash (R-302). */
    s_test_sem = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(s_test_sem);
    s_high_prio_sem_task_done = false;

    TEST_ASSERT_EQUAL_INT(ESP_OK, gpio_install_isr_service(0));
    TEST_ASSERT_EQUAL_INT(ESP_OK, gpio_set_intr_type(GPIO_NUM_18, GPIO_INTR_POSEDGE));
    TEST_ASSERT_EQUAL_INT(ESP_OK, gpio_isr_handler_add(GPIO_NUM_18, isr_waking_sem_handler, NULL));

    /* Inject from host thread */
    TEST_ASSERT_EQUAL_INT(ESP_OK, esp_sim_gpio_inject_edge(GPIO_NUM_18, 0, 1));

    /* Create task and run scheduler: task immediately consumes the given semaphore */
    TaskHandle_t h_task;
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(high_prio_sem_waiter_task, "host_inj_wait", 32 * 1024, NULL, 5, &h_task));

    wink_status_t st = pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 20);
    TEST_ASSERT_EQUAL(WINK_OK, st);
    TEST_ASSERT_TRUE(s_high_prio_sem_task_done);

    vSemaphoreDelete(s_test_sem);
    gpio_uninstall_isr_service();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Group 2: NVS Persistence, CRUD Lifecycle & Cold Reboot
 * ═══════════════════════════════════════════════════════════════════════════ */

void test_nvs_full_crud_lifecycle(void) {
    TEST_ASSERT_EQUAL_INT(ESP_OK, nvs_flash_init());

    nvs_handle_t handle = 0;
    TEST_ASSERT_EQUAL_INT(ESP_OK, nvs_open("sim_crud", NVS_READWRITE, &handle));
    TEST_ASSERT_NOT_EQUAL(0, handle);

    /* Write U8, U32, STR, BLOB */
    TEST_ASSERT_EQUAL_INT(ESP_OK, nvs_set_u8(handle, "key_u8", 0x42));
    TEST_ASSERT_EQUAL_INT(ESP_OK, nvs_set_u32(handle, "key_u32", 0x12345678));
    TEST_ASSERT_EQUAL_INT(ESP_OK, nvs_set_str(handle, "key_str", "wink_embedded"));

    uint8_t blob_in[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    TEST_ASSERT_EQUAL_INT(ESP_OK, nvs_set_blob(handle, "key_blob", blob_in, sizeof(blob_in)));

    /* Verify reads */
    uint8_t u8_val = 0;
    TEST_ASSERT_EQUAL_INT(ESP_OK, nvs_get_u8(handle, "key_u8", &u8_val));
    TEST_ASSERT_EQUAL_UINT8(0x42, u8_val);

    uint32_t u32_val = 0;
    TEST_ASSERT_EQUAL_INT(ESP_OK, nvs_get_u32(handle, "key_u32", &u32_val));
    TEST_ASSERT_EQUAL_UINT32(0x12345678, u32_val);

    char str_buf[32] = {0};
    size_t str_len = sizeof(str_buf);
    TEST_ASSERT_EQUAL_INT(ESP_OK, nvs_get_str(handle, "key_str", str_buf, &str_len));
    TEST_ASSERT_EQUAL_STRING("wink_embedded", str_buf);

    uint8_t blob_buf[16] = {0};
    size_t blob_len = sizeof(blob_buf);
    TEST_ASSERT_EQUAL_INT(ESP_OK, nvs_get_blob(handle, "key_blob", blob_buf, &blob_len));
    TEST_ASSERT_EQUAL_UINT32(sizeof(blob_in), blob_len);
    TEST_ASSERT_EQUAL_MEMORY(blob_in, blob_buf, sizeof(blob_in));

    /* Erase single key */
    TEST_ASSERT_EQUAL_INT(ESP_OK, nvs_erase_key(handle, "key_u8"));
    TEST_ASSERT_EQUAL_INT(ESP_ERR_NVS_NOT_FOUND, nvs_get_u8(handle, "key_u8", &u8_val));

    /* Other keys remain intact */
    TEST_ASSERT_EQUAL_INT(ESP_OK, nvs_get_u32(handle, "key_u32", &u32_val));
    TEST_ASSERT_EQUAL_UINT32(0x12345678, u32_val);

    /* Erase all keys in namespace */
    TEST_ASSERT_EQUAL_INT(ESP_OK, nvs_erase_all(handle));
    TEST_ASSERT_EQUAL_INT(ESP_ERR_NVS_NOT_FOUND, nvs_get_u32(handle, "key_u32", &u32_val));
    TEST_ASSERT_EQUAL_INT(ESP_ERR_NVS_NOT_FOUND, nvs_get_str(handle, "key_str", str_buf, &str_len));

    nvs_close(handle);
}

void test_nvs_disk_persistence_and_cold_reboot(void) {
    /* 1. Initialize and write records */
    TEST_ASSERT_EQUAL_INT(ESP_OK, nvs_flash_init());

    nvs_handle_t handle = 0;
    TEST_ASSERT_EQUAL_INT(ESP_OK, nvs_open("persist_test", NVS_READWRITE, &handle));
    TEST_ASSERT_EQUAL_INT(ESP_OK, nvs_set_u32(handle, "boot_count", 101));
    TEST_ASSERT_EQUAL_INT(ESP_OK, nvs_set_str(handle, "node_name", "wink_core_device"));
    TEST_ASSERT_EQUAL_INT(ESP_OK, nvs_commit(handle));
    nvs_close(handle);

    /* 2. Cold reboot: Reset in-memory cache completely (RAM wiped, disk remains) */
    esp_sim_nvs_reset_memory();

    /* 3. Re-initialize NVS: reads from .sim_sandbox/nvs_storage.bin */
    TEST_ASSERT_EQUAL_INT(ESP_OK, nvs_flash_init());

    /* 4. Verify persisted values after cold reload */
    nvs_handle_t read_handle = 0;
    TEST_ASSERT_EQUAL_INT(ESP_OK, nvs_open("persist_test", NVS_READONLY, &read_handle));

    uint32_t boot_cnt = 0;
    TEST_ASSERT_EQUAL_INT(ESP_OK, nvs_get_u32(read_handle, "boot_count", &boot_cnt));
    TEST_ASSERT_EQUAL_UINT32(101, boot_cnt);

    char node_name[32] = {0};
    size_t len = sizeof(node_name);
    TEST_ASSERT_EQUAL_INT(ESP_OK, nvs_get_str(read_handle, "node_name", node_name, &len));
    TEST_ASSERT_EQUAL_STRING("wink_core_device", node_name);

    nvs_close(read_handle);

    /* 5. Clean up sandbox */
    TEST_ASSERT_EQUAL_INT(ESP_OK, nvs_flash_erase());
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Group 3: Heap Capabilities, Alignment & SSOT Watermark
 * ═══════════════════════════════════════════════════════════════════════════ */

void test_heap_caps_allocations_and_alignment(void) {
    /* MALLOC_CAP_DEFAULT */
    void *p_def = heap_caps_malloc(128, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(p_def);
    memset(p_def, 0x55, 128);
    heap_caps_free(p_def);

    /* MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT */
    void *p_int = heap_caps_malloc(256, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    TEST_ASSERT_NOT_NULL(p_int);
    heap_caps_free(p_int);

    /* DMA 32-byte alignment test */
    void *p_dma = heap_caps_malloc(64, MALLOC_CAP_DMA);
    TEST_ASSERT_NOT_NULL(p_dma);
    TEST_ASSERT_EQUAL_UINT32(0, ((uintptr_t)p_dma) % 32);
    heap_caps_free(p_dma);

    /* SPIRAM honest rejection when not supported */
    void *p_psram = heap_caps_malloc(64, MALLOC_CAP_SPIRAM);
#if defined(SOC_SPIRAM_SUPPORTED) && (SOC_SPIRAM_SUPPORTED != 0) && defined(CONFIG_SPIRAM)
    TEST_ASSERT_NOT_NULL(p_psram);
    heap_caps_free(p_psram);
#else
    TEST_ASSERT_NULL(p_psram);
#endif

    /* Ordinary heap metrics are capacity hints, not live free-byte counters. */
    size_t free_before = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    TEST_ASSERT_TRUE(free_before > 0);
    TEST_ASSERT_EQUAL_UINT32(free_before, esp_get_free_heap_size());

    void *temp_buf = heap_caps_malloc(2048, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(temp_buf);
    size_t free_after = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    TEST_ASSERT_EQUAL_UINT32(free_before, free_after);

    heap_caps_free(temp_buf);
    size_t free_restored = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    TEST_ASSERT_EQUAL_UINT32(free_before, free_restored);
    TEST_ASSERT_EQUAL_UINT32(free_restored, esp_get_free_heap_size());

    /* Minimum free size query */
    size_t min_free = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
    TEST_ASSERT_TRUE(min_free > 0);
    TEST_ASSERT_EQUAL_UINT32(free_restored, min_free);
    TEST_ASSERT_EQUAL_UINT32(free_restored,
        heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Group 4: Standard libc free() Interop (Zero Pointer Offset)
 * ═══════════════════════════════════════════════════════════════════════════ */

void test_heap_caps_libc_free_interop(void) {
    /* Allocate with heap_caps_malloc, free with standard libc free() */
    uint8_t *ptr = (uint8_t *)heap_caps_malloc(512, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(ptr);

    for (int i = 0; i < 512; i++) {
        ptr[i] = (uint8_t)(i & 0xFF);
    }
    for (int i = 0; i < 512; i++) {
        TEST_ASSERT_EQUAL_UINT8((uint8_t)(i & 0xFF), ptr[i]);
    }

    /* Free with libc free() — MUST NOT crash or corrupt heap */
    free(ptr);

    /* Allocate with heap_caps_calloc, verify zero-init, free with free() */
    uint32_t *cptr = (uint32_t *)heap_caps_calloc(8, sizeof(uint32_t), MALLOC_CAP_INTERNAL);
    TEST_ASSERT_NOT_NULL(cptr);
    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_EQUAL_UINT32(0, cptr[i]);
    }
    free(cptr);

    /* Realloc test */
    void *rptr = heap_caps_malloc(64, MALLOC_CAP_INTERNAL);
    TEST_ASSERT_NOT_NULL(rptr);
    memset(rptr, 0xEE, 64);

    void *rptr_new = heap_caps_realloc(rptr, 256, MALLOC_CAP_INTERNAL);
    TEST_ASSERT_NOT_NULL(rptr_new);
    TEST_ASSERT_EQUAL_UINT8(0xEE, ((uint8_t *)rptr_new)[0]);
    heap_caps_free(rptr_new);
}

void test_heap_caps_ordinary_free_does_not_consume_special_tracker(void) {
    size_t dma_free_before = heap_caps_get_free_size(MALLOC_CAP_DMA);
    for (size_t i = 0; i < 160; ++i) {
        void *ordinary = heap_caps_malloc(8, MALLOC_CAP_DEFAULT);
        TEST_ASSERT_NOT_NULL(ordinary);
        free(ordinary);
    }

    void *dma = heap_caps_malloc(64, MALLOC_CAP_DMA);
    TEST_ASSERT_NOT_NULL(dma);
    TEST_ASSERT_EQUAL_UINT32(0, ((uintptr_t)dma) % 32);
    TEST_ASSERT_EQUAL_UINT32(dma_free_before - 64,
        heap_caps_get_free_size(MALLOC_CAP_DMA));
    heap_caps_free(dma);
    TEST_ASSERT_EQUAL_UINT32(dma_free_before,
        heap_caps_get_free_size(MALLOC_CAP_DMA));
}

void test_heap_caps_explicit_alignment_uses_special_release_path(void) {
    void *aligned = heap_caps_aligned_alloc(64, 127, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(aligned);
    TEST_ASSERT_EQUAL_UINT32(0, ((uintptr_t)aligned) % 64);
    heap_caps_free(aligned);

    void *dma_aligned = heap_caps_aligned_alloc(8, 127, MALLOC_CAP_DMA);
    TEST_ASSERT_NOT_NULL(dma_aligned);
    TEST_ASSERT_EQUAL_UINT32(0, ((uintptr_t)dma_aligned) % 32);
    heap_caps_free(dma_aligned);
}

void test_heap_caps_special_tracker_full_fails_and_reuses_released_slot(void) {
    const size_t slot_count = 512;
    void **special = (void **)calloc(slot_count, sizeof(*special));
    TEST_ASSERT_NOT_NULL(special);
    size_t count = 0;
    while (count < slot_count) {
        special[count] = heap_caps_malloc(1, MALLOC_CAP_DMA);
        if (special[count] == NULL) break;
        count++;
    }
    TEST_ASSERT_TRUE(count > 0);
    TEST_ASSERT_TRUE(count < slot_count);
    TEST_ASSERT_NULL(heap_caps_malloc(1, MALLOC_CAP_DMA));

    heap_caps_free(special[count - 1]);
    special[count - 1] = NULL;
    void *replacement = heap_caps_malloc(1, MALLOC_CAP_DMA);
    TEST_ASSERT_NOT_NULL(replacement);
    heap_caps_free(replacement);
    for (size_t i = 0; i < count; ++i) heap_caps_free(special[i]);
    free(special);
}

void test_heap_caps_realloc_category_transitions_preserve_old_data(void) {
    uint8_t *ordinary = (uint8_t *)heap_caps_malloc(16, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(ordinary);
    memset(ordinary, 0xA5, 16);
    void *unsupported = heap_caps_realloc(ordinary, 64, MALLOC_CAP_DMA);
    TEST_ASSERT_NULL(unsupported);
    TEST_ASSERT_EQUAL_UINT8(0xA5, ordinary[0]);
    TEST_ASSERT_EQUAL_UINT8(0xA5, ordinary[15]);
    free(ordinary);

    uint8_t *special = (uint8_t *)heap_caps_malloc(16, MALLOC_CAP_DMA);
    TEST_ASSERT_NOT_NULL(special);
    memset(special, 0x5A, 16);
    uint8_t *ordinary_result = (uint8_t *)heap_caps_realloc(
        special, 32, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(ordinary_result);
    TEST_ASSERT_EQUAL_UINT8(0x5A, ordinary_result[0]);
    TEST_ASSERT_EQUAL_UINT8(0x5A, ordinary_result[15]);
    heap_caps_free(ordinary_result);
}

void test_heap_caps_special_realloc_reuses_its_quota_transactionally(void) {
    const size_t quota = 320u * 1024u;
    esp_heap_caps_reset();
    uint8_t *special = (uint8_t *)heap_caps_malloc(quota, MALLOC_CAP_DMA);
    TEST_ASSERT_NOT_NULL(special);
    special[0] = 0x31;
    special[quota - 1] = 0x73;

    /* Replacing an allocation of the same size must not double-charge quota. */
    uint8_t *replacement = (uint8_t *)heap_caps_realloc(
        special, quota, MALLOC_CAP_DMA);
    TEST_ASSERT_NOT_NULL(replacement);
    TEST_ASSERT_EQUAL_UINT8(0x31, replacement[0]);
    TEST_ASSERT_EQUAL_UINT8(0x73, replacement[quota - 1]);
    TEST_ASSERT_EQUAL_UINT32(0, heap_caps_get_free_size(MALLOC_CAP_DMA));

    heap_caps_free(replacement);
    TEST_ASSERT_EQUAL_UINT32(quota, heap_caps_get_free_size(MALLOC_CAP_DMA));
}

void test_heap_caps_calloc_rejects_multiplication_overflow(void) {
    TEST_ASSERT_NULL(heap_caps_calloc(SIZE_MAX / 2 + 1, 2, MALLOC_CAP_DEFAULT));
}

void test_heap_caps_reset_releases_special_but_preserves_ordinary(void) {
    uint8_t *ordinary = (uint8_t *)heap_caps_malloc(8, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(ordinary);
    memset(ordinary, 0xC3, 8);
    void *special = heap_caps_malloc(64, MALLOC_CAP_DMA);
    TEST_ASSERT_NOT_NULL(special);

    esp_heap_caps_reset();
    TEST_ASSERT_EQUAL_UINT8(0xC3, ordinary[0]);
    TEST_ASSERT_EQUAL_UINT8(0xC3, ordinary[7]);
    TEST_ASSERT_EQUAL_UINT32(320u * 1024u,
        heap_caps_get_free_size(MALLOC_CAP_DMA));
    free(ordinary);

    /* A repeated reset is idempotent and tracker slots are reusable. */
    esp_heap_caps_reset();
    void *after_reset = heap_caps_malloc(64, MALLOC_CAP_DMA);
    TEST_ASSERT_NOT_NULL(after_reset);
    heap_caps_free(after_reset);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Group 5: Dynamic Node Tree Parsing & Teardown (JSON-like Interop)
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    TEST_NODE_INT,
    TEST_NODE_STR,
    TEST_NODE_OBJECT
} test_node_type_t;

typedef struct test_node {
    test_node_type_t type;
    char *key;
    union {
        int int_val;
        char *str_val;
        struct {
            struct test_node **children;
            size_t count;
            size_t cap;
        } obj;
    } val;
} test_node_t;

static test_node_t *test_node_create(test_node_type_t type, const char *key) {
    test_node_t *n = (test_node_t *)heap_caps_calloc(1, sizeof(test_node_t), MALLOC_CAP_DEFAULT);
    if (!n) return NULL;
    n->type = type;
    if (key) {
        size_t klen = strlen(key) + 1;
        n->key = (char *)heap_caps_malloc(klen, MALLOC_CAP_DEFAULT);
        memcpy(n->key, key, klen);
    }
    return n;
}

static void test_node_add_child(test_node_t *parent, test_node_t *child) {
    if (!parent || parent->type != TEST_NODE_OBJECT || !child) return;
    if (parent->val.obj.count >= parent->val.obj.cap) {
        size_t new_cap = parent->val.obj.cap == 0 ? 4 : parent->val.obj.cap * 2;
        test_node_t **new_arr = (test_node_t **)heap_caps_realloc(
            parent->val.obj.children, new_cap * sizeof(test_node_t *), MALLOC_CAP_DEFAULT);
        parent->val.obj.children = new_arr;
        parent->val.obj.cap = new_cap;
    }
    parent->val.obj.children[parent->val.obj.count++] = child;
}

static void test_node_free(test_node_t *node) {
    if (!node) return;
    if (node->key) {
        heap_caps_free(node->key);
    }
    if (node->type == TEST_NODE_STR && node->val.str_val) {
        heap_caps_free(node->val.str_val);
    } else if (node->type == TEST_NODE_OBJECT) {
        for (size_t i = 0; i < node->val.obj.count; i++) {
            test_node_free(node->val.obj.children[i]);
        }
        if (node->val.obj.children) {
            heap_caps_free(node->val.obj.children);
        }
    }
    heap_caps_free(node);
}

void test_dynamic_node_parsing_and_teardown(void) {
    size_t free_initial = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);

    /* Construct dynamic tree representing JSON { "device": "esp32", "id": 42, "config": { "baud": 115200 } } */
    test_node_t *root = test_node_create(TEST_NODE_OBJECT, "root");
    TEST_ASSERT_NOT_NULL(root);

    test_node_t *n_dev = test_node_create(TEST_NODE_STR, "device");
    n_dev->val.str_val = (char *)heap_caps_malloc(16, MALLOC_CAP_DEFAULT);
    strcpy(n_dev->val.str_val, "esp32");
    test_node_add_child(root, n_dev);

    test_node_t *n_id = test_node_create(TEST_NODE_INT, "id");
    n_id->val.int_val = 42;
    test_node_add_child(root, n_id);

    test_node_t *n_cfg = test_node_create(TEST_NODE_OBJECT, "config");
    test_node_t *n_baud = test_node_create(TEST_NODE_INT, "baud");
    n_baud->val.int_val = 115200;
    test_node_add_child(n_cfg, n_baud);
    test_node_add_child(root, n_cfg);

    /* Verify tree structure */
    TEST_ASSERT_EQUAL_UINT32(3, root->val.obj.count);
    TEST_ASSERT_EQUAL_STRING("esp32", root->val.obj.children[0]->val.str_val);
    TEST_ASSERT_EQUAL_INT(42, root->val.obj.children[1]->val.int_val);
    TEST_ASSERT_EQUAL_INT(115200, root->val.obj.children[2]->val.obj.children[0]->val.int_val);

    /* Teardown entire tree using heap_caps_free() */
    test_node_free(root);

    /* Verify heap watermark fully restored */
    size_t free_final = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    TEST_ASSERT_EQUAL_UINT32(free_initial, free_final);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Unity Main Runner
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    UNITY_BEGIN();

    /* Group 1: GPIO ISR and FreeRTOS */
    RUN_TEST(test_gpio_isr_edge_filtering_and_lifecycle);
    RUN_TEST(test_gpio_isr_queue_priority_waking_in_fiber);
    RUN_TEST(test_gpio_isr_semaphore_and_eventgroup_waking_in_fiber);
    RUN_TEST(test_gpio_isr_host_thread_injection_safety);

    /* Group 2: NVS Persistence and CRUD */
    RUN_TEST(test_nvs_full_crud_lifecycle);
    RUN_TEST(test_nvs_disk_persistence_and_cold_reboot);

    /* Group 3: Heap Capabilities */
    RUN_TEST(test_heap_caps_allocations_and_alignment);

    /* Group 4: Standard libc free() interop */
    RUN_TEST(test_heap_caps_libc_free_interop);
    RUN_TEST(test_heap_caps_ordinary_free_does_not_consume_special_tracker);
    RUN_TEST(test_heap_caps_explicit_alignment_uses_special_release_path);
    RUN_TEST(test_heap_caps_special_tracker_full_fails_and_reuses_released_slot);
    RUN_TEST(test_heap_caps_realloc_category_transitions_preserve_old_data);
    RUN_TEST(test_heap_caps_special_realloc_reuses_its_quota_transactionally);
    RUN_TEST(test_heap_caps_calloc_rejects_multiplication_overflow);
    RUN_TEST(test_heap_caps_reset_releases_special_but_preserves_ordinary);

    /* Group 5: Dynamic Node Tree */
    RUN_TEST(test_dynamic_node_parsing_and_teardown);

    return UNITY_END();
}
