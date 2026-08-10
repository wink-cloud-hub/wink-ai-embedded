// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_osal_ringbuf.c
 * @brief PAL OSAL ring buffer — shared WASM / host implementation (single-threaded lock-free in-memory ring buffer).
 *
 * Compiled jointly by WASM and host simulation OSAL targets.
 * ESP32 uses FreeRTOS xRingbuffer and bare-metal uses interrupt disable atomic implementation.
 *
 * API Contract:
 *   - size must be a power of 2 (used as bitmask); returns NULL if invalid.
 *   - Single producer / single consumer, lock-free. Single-threaded simulation sandbox.
 *   - Returns NULL or negative status code on malloc failure according to status code conventions.
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

bool pal_os_ringbuf_coalesce_event(
    pal_os_ringbuf_handle_t rb,
    const void* event_ptr,
    uint32_t event_size,
    uint32_t match_offset,
    uint32_t match_size
) {
    if (rb == NULL || event_ptr == NULL || event_size == 0 || match_size == 0) {
        return false;
    }
    uint32_t used = pal_os_ringbuf_used(rb);
    if (used < event_size) {
        return false;
    }

    uint32_t count = used / event_size;
    const uint8_t *new_evt = (const uint8_t *)event_ptr;

    for (uint32_t idx = 0; idx < count; idx++) {
        uint32_t item_tail = rb->tail + idx * event_size;
        bool match = true;
        for (uint32_t k = 0; k < match_size; k++) {
            uint32_t buf_pos = (item_tail + match_offset + k) & (rb->size - 1);
            if (rb->buffer[buf_pos] != new_evt[match_offset + k]) {
                match = false;
                break;
            }
        }
        if (match) {
            for (uint32_t k = 0; k < event_size; k++) {
                uint32_t buf_pos = (item_tail + k) & (rb->size - 1);
                rb->buffer[buf_pos] = new_evt[k];
            }
            return true;
        }
    }
    return false;
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
