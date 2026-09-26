/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef ESP_HEAP_CAPS_H_
#define ESP_HEAP_CAPS_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MALLOC_CAP_EXEC             (1<<0)  ///< Memory must be able to run code
#define MALLOC_CAP_32BIT            (1<<1)  ///< Memory must allow for aligned 32-bit data accesses
#define MALLOC_CAP_8BIT             (1<<2)  ///< Memory must allow for 8/16/...-bit data accesses
#define MALLOC_CAP_DMA              (1<<3)  ///< Memory must be able to access by DMA
#define MALLOC_CAP_PID2             (1<<4)  ///< Memory must be mapped to PID2 memory space
#define MALLOC_CAP_PID3             (1<<5)  ///< Memory must be mapped to PID3 memory space
#define MALLOC_CAP_PID4             (1<<6)  ///< Memory must be mapped to PID4 memory space
#define MALLOC_CAP_PID5             (1<<7)  ///< Memory must be mapped to PID5 memory space
#define MALLOC_CAP_SPIRAM           (1<<10) ///< Memory must be in external SPI RAM
#define MALLOC_CAP_INTERNAL         (1<<11) ///< Memory must be internal; no external SPI RAM
#define MALLOC_CAP_DEFAULT          (1<<12) ///< Memory can be returned in a non-capability-specific memory allocation
#define MALLOC_CAP_IRAM_8BIT        (1<<13) ///< Memory must be in IRAM and allow unaligned access
#define MALLOC_CAP_RETENTION        (1<<14) ///< Memory must be in retention memory
#define MALLOC_CAP_RTCRAM           (1<<15) ///< Memory must be in RTC fast or slow memory
#define MALLOC_CAP_TCM              (1<<16) ///< Memory must be in tightly-coupled memory

#define MALLOC_CAP_INVALID          (1<<31)

void *heap_caps_malloc(size_t size, uint32_t caps);
void *heap_caps_calloc(size_t n, size_t size, uint32_t caps);
void *heap_caps_realloc(void *ptr, size_t size, uint32_t caps);
void *heap_caps_aligned_alloc(size_t alignment, size_t size, uint32_t caps);
void heap_caps_free(void *ptr);

size_t heap_caps_get_free_size(uint32_t caps);
size_t heap_caps_get_minimum_free_size(uint32_t caps);
size_t heap_caps_get_largest_free_block(uint32_t caps);
size_t heap_caps_get_total_size(uint32_t caps);

bool heap_caps_check_integrity_all(bool print_errors);

/* Facade reset hook */
void esp_heap_caps_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* ESP_HEAP_CAPS_H_ */
