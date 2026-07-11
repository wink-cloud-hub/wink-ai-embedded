#include "unity.h"
#include "wink_event.h"
#include "pal_osal.h"
#include <string.h>

void setUp(void) {
    /* Initialize with capacity 16 */
    wink_status_t st = wink_event_queue_init(16);
    (void)st;
}

void tearDown(void) {
    wink_event_queue_deinit();
}

void test_init_twice_is_idempotent(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_event_queue_init(16));
}

void test_post_and_pend_success(void) {
    wink_event_t ev_send = {
        .type = WINK_EVENT_BUTTON_PRESSED,
        .device = (void*)0x12345678,
        .param = 42,
        .timestamp = 999
    };
    wink_event_t ev_recv;
    memset(&ev_recv, 0, sizeof(ev_recv));

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_event_post(&ev_send));
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_event_pend(&ev_recv, 0));

    TEST_ASSERT_EQUAL_UINT32(ev_send.type, ev_recv.type);
    TEST_ASSERT_EQUAL_PTR(ev_send.device, ev_recv.device);
    TEST_ASSERT_EQUAL_UINT32(ev_send.param, ev_recv.param);
    TEST_ASSERT_EQUAL_UINT64(ev_send.timestamp, ev_recv.timestamp);
}

void test_pend_empty_returns_err_empty(void) {
    wink_event_t ev;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_EMPTY, wink_event_pend(&ev, 0));
}

void test_queue_overflow_behavior(void) {
    wink_event_t ev = { .type = 1 };
    
    /* Fill queue (capacity = 16) */
    for (int i = 0; i < 16; i++) {
        ev.param = i;
        TEST_ASSERT_EQUAL_INT(WINK_OK, wink_event_post(&ev));
    }

    /* 17th post should fail */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_RESOURCE_EXHAUSTED, wink_event_post(&ev));

    /* Drain and verify */
    wink_event_t ev_out;
    for (int i = 0; i < 16; i++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, wink_event_pend(&ev_out, 0));
        TEST_ASSERT_EQUAL_UINT32(i, ev_out.param);
    }

    /* Queue should be empty now */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_EMPTY, wink_event_pend(&ev_out, 0));
}

void test_pend_timeout_expiry(void) {
    wink_event_t ev;
    wink_status_t st = wink_event_pend(&ev, 50);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_TIMEOUT, st);
}

void test_post_from_isr_context(void) {
    wink_event_t ev_send = {
        .type = WINK_EVENT_BUTTON_RELEASED,
        .device = (void*)0xABCD,
        .param = 1,
        .timestamp = 100
    };
    wink_event_t ev_recv;

    /* Simulate ISR execution context */
    pal_os_set_sim_isr_context(true);
    wink_status_t st = wink_event_post(&ev_send);
    pal_os_set_sim_isr_context(false);

    TEST_ASSERT_EQUAL_INT(WINK_OK, st);

    /* Pend should successfully pop the event posted from ISR */
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_event_pend(&ev_recv, 10));
    TEST_ASSERT_EQUAL_UINT32(ev_send.type, ev_recv.type);
    TEST_ASSERT_EQUAL_PTR(ev_send.device, ev_recv.device);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_init_twice_is_idempotent);
    RUN_TEST(test_post_and_pend_success);
    RUN_TEST(test_pend_empty_returns_err_empty);
    RUN_TEST(test_queue_overflow_behavior);
    RUN_TEST(test_pend_timeout_expiry);
    RUN_TEST(test_post_from_isr_context);
    return UNITY_END();
}
