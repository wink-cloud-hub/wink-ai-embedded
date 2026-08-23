// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_pal_atomic.c
 * @brief PAL portable atomic operations unit tests (32-bit and 64-bit).
 */
#include "unity.h"
#include "osal/pal_atomic.h"

void setUp(void) {}
void tearDown(void) {}

void test_atomic_u32_ops(void) {
    volatile uint32_t val = 0;

    PAL_ATOMIC_STORE(&val, 100u, PAL_REL);
    TEST_ASSERT_EQUAL_UINT32(100u, PAL_ATOMIC_LOAD(&val, PAL_ACQ));

    uint32_t prev = (uint32_t)PAL_ATOMIC_ADD(&val, 25u, PAL_ACQ_REL);
    TEST_ASSERT_EQUAL_UINT32(100u, prev);
    TEST_ASSERT_EQUAL_UINT32(125u, PAL_ATOMIC_LOAD(&val, PAL_ACQ));

    prev = (uint32_t)PAL_ATOMIC_SUB(&val, 15u, PAL_ACQ_REL);
    TEST_ASSERT_EQUAL_UINT32(125u, prev);
    TEST_ASSERT_EQUAL_UINT32(110u, PAL_ATOMIC_LOAD(&val, PAL_ACQ));

    prev = (uint32_t)PAL_ATOMIC_XCHG(&val, 500u, PAL_ACQ_REL);
    TEST_ASSERT_EQUAL_UINT32(110u, prev);
    TEST_ASSERT_EQUAL_UINT32(500u, PAL_ATOMIC_LOAD(&val, PAL_ACQ));
}

void test_atomic_u64_ops(void) {
    volatile uint64_t val = 0;

    PAL_ATOMIC_STORE(&val, 0x100000000ULL, PAL_REL);
    TEST_ASSERT_EQUAL_UINT64(0x100000000ULL, PAL_ATOMIC_LOAD(&val, PAL_ACQ));

    uint64_t prev = (uint64_t)PAL_ATOMIC_ADD(&val, 0x200ULL, PAL_ACQ_REL);
    TEST_ASSERT_EQUAL_UINT64(0x100000000ULL, prev);
    TEST_ASSERT_EQUAL_UINT64(0x100000200ULL, PAL_ATOMIC_LOAD(&val, PAL_ACQ));

    prev = (uint64_t)PAL_ATOMIC_SUB(&val, 0x100ULL, PAL_ACQ_REL);
    TEST_ASSERT_EQUAL_UINT64(0x100000200ULL, prev);
    TEST_ASSERT_EQUAL_UINT64(0x100000100ULL, PAL_ATOMIC_LOAD(&val, PAL_ACQ));

    prev = (uint64_t)PAL_ATOMIC_XCHG(&val, 0x8888888888888888ULL, PAL_ACQ_REL);
    TEST_ASSERT_EQUAL_UINT64(0x100000100ULL, prev);
    TEST_ASSERT_EQUAL_UINT64(0x8888888888888888ULL, PAL_ATOMIC_LOAD(&val, PAL_ACQ));
}

void test_atomic_fences(void) {
    PAL_ATOMIC_THREAD_FENCE(PAL_ACQ_REL);
    PAL_ATOMIC_SIGNAL_FENCE(PAL_ACQ_REL);
    TEST_ASSERT_TRUE(true);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_atomic_u32_ops);
    RUN_TEST(test_atomic_u64_ops);
    RUN_TEST(test_atomic_fences);
    return UNITY_END();
}
