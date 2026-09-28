/* SPDX-License-Identifier: GPL-3.0-only */
#include "unity.h"
#include "esp_event.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

ESP_EVENT_DEFINE_BASE(TEST_EVENT_BASE_A);
ESP_EVENT_DEFINE_BASE(TEST_EVENT_BASE_B);

extern esp_err_t esp_event_loop_run_step(void);

typedef struct {
    uint32_t val;
    char text[16];
} test_payload_t;

static int s_handler_call_count = 0;
static test_payload_t s_last_payload;
static int32_t s_last_event_id = -1;
static esp_event_base_t s_last_base = NULL;
static int s_nested_call_depth = 0;
static int s_max_call_depth = 0;

static void simple_test_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg;
    s_nested_call_depth++;
    if (s_nested_call_depth > s_max_call_depth) {
        s_max_call_depth = s_nested_call_depth;
    }
    s_handler_call_count++;
    s_last_base = base;
    s_last_event_id = id;
    if (data) {
        memcpy(&s_last_payload, data, sizeof(test_payload_t));
    }
    s_nested_call_depth--;
}

static void recursive_post_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg;
    (void)base;
    (void)data;
    s_nested_call_depth++;
    if (s_nested_call_depth > s_max_call_depth) {
        s_max_call_depth = s_nested_call_depth;
    }
    s_handler_call_count++;
    if (id == 1) {
        /* Post next event from inside callback */
        test_payload_t p2 = { .val = 200, .text = "second" };
        esp_event_post(TEST_EVENT_BASE_A, 2, &p2, sizeof(p2), 0);
    }
    s_nested_call_depth--;
}

void setUp(void) {
    esp_event_loop_sim_reset();
    s_handler_call_count = 0;
    memset(&s_last_payload, 0, sizeof(s_last_payload));
    s_last_event_id = -1;
    s_last_base = NULL;
    s_nested_call_depth = 0;
    s_max_call_depth = 0;
}

void tearDown(void) {
    esp_event_loop_sim_reset();
}

void test_esp_event_post_async_decoupling(void) {
    TEST_ASSERT_EQUAL_INT32(ESP_OK, esp_event_loop_create_default());

    TEST_ASSERT_EQUAL_INT32(ESP_OK, esp_event_handler_register(
        TEST_EVENT_BASE_A, 1, simple_test_handler, NULL));

    test_payload_t p = { .val = 123, .text = "async_test" };
    TEST_ASSERT_EQUAL_INT32(ESP_OK, esp_event_post(
        TEST_EVENT_BASE_A, 1, &p, sizeof(p), 0));

    /* D2-T1 Assertion: post() returns immediately; handler has NOT been called on the caller's stack */
    TEST_ASSERT_EQUAL_INT(0, s_handler_call_count);

    /* Pump one event */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, esp_event_loop_run_step());
    TEST_ASSERT_EQUAL_INT(1, s_handler_call_count);
    TEST_ASSERT_EQUAL_PTR(TEST_EVENT_BASE_A, s_last_base);
    TEST_ASSERT_EQUAL_INT32(1, s_last_event_id);
    TEST_ASSERT_EQUAL_UINT32(123, s_last_payload.val);
    TEST_ASSERT_EQUAL_STRING("async_test", s_last_payload.text);

    TEST_ASSERT_EQUAL_INT32(ESP_OK, esp_event_loop_delete_default());
}

void test_esp_event_recursive_post_stack_depth(void) {
    TEST_ASSERT_EQUAL_INT32(ESP_OK, esp_event_loop_create_default());

    TEST_ASSERT_EQUAL_INT32(ESP_OK, esp_event_handler_register(
        TEST_EVENT_BASE_A, ESP_EVENT_ANY_ID, recursive_post_handler, NULL));

    test_payload_t p1 = { .val = 100, .text = "first" };
    TEST_ASSERT_EQUAL_INT32(ESP_OK, esp_event_post(
        TEST_EVENT_BASE_A, 1, &p1, sizeof(p1), 0));

    TEST_ASSERT_EQUAL_INT(0, s_handler_call_count);

    /* Pump first event: handler posts event 2 */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, esp_event_loop_run_step());
    TEST_ASSERT_EQUAL_INT(1, s_handler_call_count);
    /* Calling depth must never exceed 1 (no recursive in-stack dispatch) */
    TEST_ASSERT_EQUAL_INT(1, s_max_call_depth);

    /* Pump second event */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, esp_event_loop_run_step());
    TEST_ASSERT_EQUAL_INT(2, s_handler_call_count);
    TEST_ASSERT_EQUAL_INT(1, s_max_call_depth);

    TEST_ASSERT_EQUAL_INT32(ESP_OK, esp_event_loop_delete_default());
}

void test_esp_event_payload_deep_copy(void) {
    TEST_ASSERT_EQUAL_INT32(ESP_OK, esp_event_loop_create_default());

    TEST_ASSERT_EQUAL_INT32(ESP_OK, esp_event_handler_register(
        TEST_EVENT_BASE_A, 1, simple_test_handler, NULL));

    {
        /* Temporary stack payload */
        test_payload_t stack_payload = { .val = 9999, .text = "immutable" };
        TEST_ASSERT_EQUAL_INT32(ESP_OK, esp_event_post(
            TEST_EVENT_BASE_A, 1, &stack_payload, sizeof(stack_payload), 0));

        /* Immediately clobber stack memory */
        memset(&stack_payload, 0xCC, sizeof(stack_payload));
    }

    /* Pump event */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, esp_event_loop_run_step());
    TEST_ASSERT_EQUAL_INT(1, s_handler_call_count);

    /* D2-T2 Assertion: Event handler sees intact snapshot, not clobbered stack */
    TEST_ASSERT_EQUAL_UINT32(9999, s_last_payload.val);
    TEST_ASSERT_EQUAL_STRING("immutable", s_last_payload.text);

    TEST_ASSERT_EQUAL_INT32(ESP_OK, esp_event_loop_delete_default());
}

void test_esp_event_validation_and_edges(void) {
    /* Before loop created */
    test_payload_t p = { .val = 1, .text = "a" };
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_STATE, esp_event_post(
        TEST_EVENT_BASE_A, 1, &p, sizeof(p), 0));

    TEST_ASSERT_EQUAL_INT32(ESP_OK, esp_event_loop_create_default());

    /* Duplicate creation rejected */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_STATE, esp_event_loop_create_default());

    /* ANY_BASE or ANY_ID not allowed for post */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, esp_event_post(
        ESP_EVENT_ANY_BASE, 1, &p, sizeof(p), 0));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, esp_event_post(
        TEST_EVENT_BASE_A, ESP_EVENT_ANY_ID, &p, sizeof(p), 0));

    /* NULL data with positive size rejected */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, esp_event_post(
        TEST_EVENT_BASE_A, 1, NULL, 16, 0));

    /* NULL handler rejected */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, esp_event_handler_register(
        TEST_EVENT_BASE_A, 1, NULL, NULL));

    /* Unregister unknown */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NOT_FOUND, esp_event_handler_unregister(
        TEST_EVENT_BASE_A, 1, simple_test_handler));

    TEST_ASSERT_EQUAL_INT32(ESP_OK, esp_event_loop_delete_default());
}

void test_esp_event_queue_capacity_and_timeout(void) {
    TEST_ASSERT_EQUAL_INT32(ESP_OK, esp_event_loop_create_default());

    test_payload_t p = { .val = 1, .text = "queue_fill" };

    /* Fill default queue of 32 items */
    for (int i = 0; i < 32; i++) {
        p.val = (uint32_t)i;
        TEST_ASSERT_EQUAL_INT32(ESP_OK, esp_event_post(
            TEST_EVENT_BASE_A, 1, &p, sizeof(p), 0));
    }

    /* 33rd post with timeout 0 must return ESP_ERR_TIMEOUT */
    p.val = 32;
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_TIMEOUT, esp_event_post(
        TEST_EVENT_BASE_A, 1, &p, sizeof(p), 0));

    /* Drain 1 item */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, esp_event_loop_run_step());

    /* Now post succeeds */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, esp_event_post(
        TEST_EVENT_BASE_A, 1, &p, sizeof(p), 0));

    TEST_ASSERT_EQUAL_INT32(ESP_OK, esp_event_loop_delete_default());
}

void test_esp_event_handler_instance_anti_aba(void) {
    TEST_ASSERT_EQUAL_INT32(ESP_OK, esp_event_loop_create_default());

    esp_event_handler_instance_t inst1 = NULL;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, esp_event_handler_instance_register(
        TEST_EVENT_BASE_A, 1, simple_test_handler, NULL, &inst1));
    TEST_ASSERT_NOT_NULL(inst1);

    /* Unregister inst1 */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, esp_event_handler_instance_unregister(
        TEST_EVENT_BASE_A, 1, inst1));

    /* Register inst2, which reuses the same slot */
    esp_event_handler_instance_t inst2 = NULL;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, esp_event_handler_instance_register(
        TEST_EVENT_BASE_A, 1, simple_test_handler, NULL, &inst2));
    TEST_ASSERT_NOT_NULL(inst2);
    TEST_ASSERT_NOT_EQUAL(inst1, inst2); /* Token must differ */

    /* Attempt to unregister with stale inst1 must fail (Anti-ABA) */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NOT_FOUND, esp_event_handler_instance_unregister(
        TEST_EVENT_BASE_A, 1, inst1));

    /* inst2 still valid */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, esp_event_handler_instance_unregister(
        TEST_EVENT_BASE_A, 1, inst2));

    TEST_ASSERT_EQUAL_INT32(ESP_OK, esp_event_loop_delete_default());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_esp_event_post_async_decoupling);
    RUN_TEST(test_esp_event_recursive_post_stack_depth);
    RUN_TEST(test_esp_event_payload_deep_copy);
    RUN_TEST(test_esp_event_validation_and_edges);
    RUN_TEST(test_esp_event_queue_capacity_and_timeout);
    RUN_TEST(test_esp_event_handler_instance_anti_aba);
    return UNITY_END();
}
