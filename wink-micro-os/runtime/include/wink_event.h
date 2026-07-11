/**
 * @file wink_event.h
 * @brief OS Event Queue / mbox asynchronous primitives.
 *
 * Implements the asynchronous event dispatching system (ADR-0022).
 * Enables thread-safe and ISR-safe post, and thread-safe blocking pend.
 *
 * Copyright (c) 2026 Wink-AI.
 */
#ifndef WINK_EVENT_H
#define WINK_EVENT_H

#include "wink_status.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Default capacity of the global event queue (must be power of 2) */
#define WINK_EVENT_QUEUE_DEFAULT_CAPACITY 32u

/**
 * @brief Standard Event Types
 */
typedef enum {
    WINK_EVENT_NONE = 0,
    WINK_EVENT_BUTTON_PRESSED = 1,
    WINK_EVENT_BUTTON_RELEASED = 2,
    WINK_EVENT_BUTTON_LONG_PRESS = 3,
    
    WINK_EVENT_USER_START = 1000u
} wink_event_type_t;

/**
 * @brief Event structure
 */
typedef struct {
    uint32_t type;         /**< Event type (e.g. WINK_EVENT_BUTTON_PRESSED) */
    void *device;          /**< Pointer to the source device instance */
    uint32_t param;        /**< Optional event parameter (e.g. click count or duration) */
    uint64_t timestamp;    /**< System uptime in milliseconds when event occurred */
} wink_event_t;

/**
 * @brief Initialize the global event queue.
 * @param capacity Maximum number of events in the queue (must be a power of 2).
 * @return WINK_OK on success; WINK_ERR_* on failure.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_event_queue_init(uint32_t capacity);

/**
 * @brief Deinitialize the global event queue.
 */
void wink_event_queue_deinit(void);

/**
 * @brief Post an event to the global queue.
 *
 * This function is both thread-safe and ISR-safe. It can be called from
 * tasks, soft-timer callbacks, and interrupt handlers.
 *
 * @param event Pointer to the event to copy into the queue.
 * @return WINK_OK on success;
 *         WINK_ERR_RESOURCE_EXHAUSTED if the queue is full;
 *         WINK_ERR_INVALID_ARG if event is NULL.
 */
wink_status_t wink_event_post(const wink_event_t *event);

/**
 * @brief Wait for an event from the global queue.
 *
 * Blocks the calling task until an event is available or the timeout expires.
 *
 * @param[out] out_event Pointer to structure to copy the received event into.
 * @param timeout_ms Timeout in milliseconds, or WINK_MUTEX_WAIT_FOREVER to block indefinitely.
 * @return WINK_OK on success;
 *         WINK_ERR_TIMEOUT if timeout expires;
 *         WINK_ERR_EMPTY if timeout is 0 and queue is empty;
 *         WINK_ERR_INVALID_ARG if out_event is NULL.
 * @note WINK_BLOCKING. Not available from ISR context.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_event_pend(wink_event_t *out_event, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* WINK_EVENT_H */
