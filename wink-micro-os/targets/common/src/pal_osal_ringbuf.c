/**
 * @file pal_osal_ringbuf.c
 * @brief PAL OSAL 环形缓冲区 —— WASM / host 共享实现（单线程无并发纯内存环形缓冲）。
 *
 * 由 wasm 与 host 两个仿真 target 共同编译（各自 CMakeLists.txt 通过相对路径
 * `../common/src/pal_osal_ringbuf.c` 引入）。ESP32 target 走 FreeRTOS xRingbuffer、
 * baremetal target 走关中断原子实现，均**不**编译本文件。
 *
 * 合约与 pal_osal.h 中的声明一致：
 *   - size 必须是 2 的幂（用作 & mask 掩码）；否则 create 返回 NULL。
 *   - 单生产者/单消费者，无锁；仿真沙箱天然单线程，volatile head/tail 足矣。
 *   - malloc 失败返回 NULL 或降级错误码，符合 wink_status_t 负数错误码约定。
 */
#include <stdlib.h>
#include <stdint.h>
#include "pal_osal.h"
#include "wink_status.h"

struct pal_os_ringbuf {
    uint8_t* buffer;
    uint32_t size;
    volatile uint32_t head;
    volatile uint32_t tail;
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

    rb->buffer = malloc(size);
    if (rb->buffer == NULL) {
        free(rb);
        return NULL;
    }

    rb->size = size;
    rb->head = 0;
    rb->tail = 0;

    return rb;
}

wink_status_t pal_os_ringbuf_push(
    pal_os_ringbuf_handle_t rb,
    const void* data,
    uint32_t size
) {
    uint32_t i;
    const uint8_t* src = (const uint8_t*)data;

    if (rb == NULL || data == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    if (pal_os_ringbuf_used(rb) + size > rb->size) {
        return WINK_ERR_FULL;
    }

    for (i = 0; i < size; i++) {
        rb->buffer[rb->head & (rb->size - 1)] = src[i];
        rb->head++;
    }

    return WINK_OK;
}

wink_status_t pal_os_ringbuf_pop(
    pal_os_ringbuf_handle_t rb,
    void* data,
    uint32_t size
) {
    uint32_t i;
    uint8_t* dst = (uint8_t*)data;

    if (rb == NULL || data == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    if (pal_os_ringbuf_used(rb) < size) {
        return WINK_ERR_EMPTY;
    }

    for (i = 0; i < size; i++) {
        dst[i] = rb->buffer[rb->tail & (rb->size - 1)];
        rb->tail++;
    }

    return WINK_OK;
}

uint32_t pal_os_ringbuf_used(pal_os_ringbuf_handle_t rb) {
    if (rb == NULL) {
        return 0;
    }
    return rb->head - rb->tail;
}

void pal_os_ringbuf_destroy(pal_os_ringbuf_handle_t rb) {
    if (rb == NULL) {
        return;
    }

    free(rb->buffer);
    free(rb);
}
