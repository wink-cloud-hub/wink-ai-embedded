// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_pal_dma.c
 * @brief Unit tests for PAL DMA cache operations and buffer alignment contracts.
 */
#include "unity.h"
#include "hal/pal_dma.h"
#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

static PAL_DMA_BUF_ATTR PAL_DMA_BUF_ALIGN uint8_t s_test_dma_buf[128];

void test_dma_buffer_alignment(void) {
    uintptr_t addr = (uintptr_t)s_test_dma_buf;
    /* Verify 32-byte cache line alignment */
    TEST_ASSERT_EQUAL_UINT32(0, addr % PAL_DMA_CACHE_LINE_SIZE);
}

void test_dma_cache_operations(void) {
    /* Verify NULL and zero-length boundaries do not crash */
    pal_dma_cache_clean(NULL, 0);
    pal_dma_cache_clean(s_test_dma_buf, 0);
    pal_dma_cache_clean(NULL, 64);

    pal_dma_cache_invalidate(NULL, 0);
    pal_dma_cache_invalidate(s_test_dma_buf, 0);
    pal_dma_cache_invalidate(NULL, 64);

    /* Valid clean & invalidate calls */
    pal_dma_cache_clean(s_test_dma_buf, sizeof(s_test_dma_buf));
    pal_dma_cache_invalidate(s_test_dma_buf, sizeof(s_test_dma_buf));

    TEST_ASSERT_TRUE(true);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_dma_buffer_alignment);
    RUN_TEST(test_dma_cache_operations);
    return UNITY_END();
}
