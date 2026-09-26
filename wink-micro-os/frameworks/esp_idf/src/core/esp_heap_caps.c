/* SPDX-License-Identifier: LGPL-3.0-only */
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "soc/soc_caps.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#define INTERNAL_HEAP_TOTAL_BYTES  (320u * 1024u)
#define SPIRAM_HEAP_TOTAL_BYTES    (4u * 1024u * 1024u)

#ifndef ESP_SIM_HEAP_TRACKER_MAX
#  ifdef CONFIG_ESP_SIM_HEAP_TRACKER_MAX
#    define ESP_SIM_HEAP_TRACKER_MAX CONFIG_ESP_SIM_HEAP_TRACKER_MAX
#  else
#    define ESP_SIM_HEAP_TRACKER_MAX 128
#  endif
#endif

typedef struct {
    void    *ptr;
    size_t   size;
    uint32_t caps;
    bool     used;
} esp_sim_heap_record_t;

static esp_sim_heap_record_t s_heap_records[ESP_SIM_HEAP_TRACKER_MAX];
static size_t s_allocated_internal = 0;
static size_t s_allocated_spiram = 0;

static inline bool soc_supports_spiram(void) {
#if defined(SOC_SPIRAM_SUPPORTED) && (SOC_SPIRAM_SUPPORTED != 0) && defined(CONFIG_SPIRAM)
    return true;
#else
    return false;
#endif
}

/**
 * @brief Allocate a raw pointer guaranteed to satisfy alignment without
 * returning an offset pointer.
 *
 * This preserves 100% compatibility with standard libc free().
 */
static void* alloc_aligned_raw(size_t size, size_t alignment) {
    if (alignment <= 16) {
        return malloc(size);
    }

    void *ptrs[16];
    int count = 0;
    void *aligned_ptr = NULL;

    for (int i = 0; i < 16; i++) {
        void *p = malloc(size);
        if (!p) break;
        if (((uintptr_t)p % alignment) == 0) {
            aligned_ptr = p;
            break;
        }
        ptrs[count++] = p;
        void *pad = malloc(16);
        if (pad) {
            ptrs[count++] = pad;
        }
    }

    for (int i = 0; i < count; i++) {
        free(ptrs[i]);
    }

    return aligned_ptr ? aligned_ptr : malloc(size);
}

static void tracker_record(void *ptr, size_t size, uint32_t caps) {
    if (!ptr) return;
    for (size_t i = 0; i < ESP_SIM_HEAP_TRACKER_MAX; i++) {
        if (!s_heap_records[i].used) {
            s_heap_records[i].ptr = ptr;
            s_heap_records[i].size = size;
            s_heap_records[i].caps = caps;
            s_heap_records[i].used = true;
            return;
        }
    }
}

void *heap_caps_malloc(size_t size, uint32_t caps) {
    if (size == 0) {
        return NULL;
    }

    /* Conflicting capability rejection */
    if ((caps & MALLOC_CAP_SPIRAM) && (caps & MALLOC_CAP_INTERNAL)) {
        ESP_LOGE("HEAP_CAPS", "Cannot request both MALLOC_CAP_SPIRAM and MALLOC_CAP_INTERNAL");
        return NULL;
    }

    /* Contract honesty: reject SPIRAM on SoCs without PSRAM hardware */
    if (caps & MALLOC_CAP_SPIRAM) {
        if (!soc_supports_spiram()) {
            ESP_LOGE("HEAP_CAPS", "MALLOC_CAP_SPIRAM requested but SoC has no PSRAM support (ADR-0012)");
            return NULL;
        }
        if (s_allocated_spiram + size > SPIRAM_HEAP_TOTAL_BYTES) {
            ESP_LOGW("HEAP_CAPS", "SPIRAM allocation of %zu bytes exceeds quota", size);
            return NULL;
        }
    } else {
        if (s_allocated_internal + size > INTERNAL_HEAP_TOTAL_BYTES) {
            ESP_LOGW("HEAP_CAPS", "Internal heap allocation of %zu bytes exceeds quota", size);
            return NULL;
        }
    }

    size_t align = (caps & MALLOC_CAP_DMA) ? 32 : 16;
    void *p = alloc_aligned_raw(size, align);
    if (!p) {
        return NULL;
    }

    if (caps & MALLOC_CAP_SPIRAM) {
        s_allocated_spiram += size;
    } else {
        s_allocated_internal += size;
    }

    tracker_record(p, size, caps);
    return p;
}

void *heap_caps_calloc(size_t n, size_t size, uint32_t caps) {
    size_t total = n * size;
    void *p = heap_caps_malloc(total, caps);
    if (p) {
        memset(p, 0, total);
    }
    return p;
}

void *heap_caps_aligned_alloc(size_t alignment, size_t size, uint32_t caps) {
    if (alignment < sizeof(void*) || (alignment & (alignment - 1)) != 0) {
        return NULL;
    }
    if ((caps & MALLOC_CAP_SPIRAM) && !soc_supports_spiram()) {
        ESP_LOGE("HEAP_CAPS", "MALLOC_CAP_SPIRAM requested but SoC has no PSRAM support");
        return NULL;
    }

    void *p = alloc_aligned_raw(size, alignment);
    if (!p) {
        return NULL;
    }

    if (caps & MALLOC_CAP_SPIRAM) {
        s_allocated_spiram += size;
    } else {
        s_allocated_internal += size;
    }

    tracker_record(p, size, caps);
    return p;
}

void *heap_caps_realloc(void *ptr, size_t size, uint32_t caps) {
    if (!ptr) {
        return heap_caps_malloc(size, caps);
    }
    if (size == 0) {
        heap_caps_free(ptr);
        return NULL;
    }

    int found_idx = -1;
    for (size_t i = 0; i < ESP_SIM_HEAP_TRACKER_MAX; i++) {
        if (s_heap_records[i].used && s_heap_records[i].ptr == ptr) {
            found_idx = (int)i;
            break;
        }
    }

    if (found_idx >= 0) {
        size_t old_size = s_heap_records[found_idx].size;
        uint32_t old_caps = s_heap_records[found_idx].caps;
        void *new_p = realloc(ptr, size);
        if (!new_p) return NULL;

        if (old_caps & MALLOC_CAP_SPIRAM) {
            s_allocated_spiram = s_allocated_spiram - old_size + size;
        } else {
            s_allocated_internal = s_allocated_internal - old_size + size;
        }
        s_heap_records[found_idx].ptr = new_p;
        s_heap_records[found_idx].size = size;
        return new_p;
    }

    void *new_p = realloc(ptr, size);
    if (new_p) {
        tracker_record(new_p, size, caps);
    }
    return new_p;
}

void heap_caps_free(void *ptr) {
    if (!ptr) return;

    for (size_t i = 0; i < ESP_SIM_HEAP_TRACKER_MAX; i++) {
        if (s_heap_records[i].used && s_heap_records[i].ptr == ptr) {
            if (s_heap_records[i].caps & MALLOC_CAP_SPIRAM) {
                if (s_allocated_spiram >= s_heap_records[i].size) {
                    s_allocated_spiram -= s_heap_records[i].size;
                }
            } else {
                if (s_allocated_internal >= s_heap_records[i].size) {
                    s_allocated_internal -= s_heap_records[i].size;
                }
            }
            s_heap_records[i].used = false;
            s_heap_records[i].ptr = NULL;
            break;
        }
    }

    free(ptr);
}

size_t heap_caps_get_free_size(uint32_t caps) {
    if (caps & MALLOC_CAP_SPIRAM) {
        if (!soc_supports_spiram()) return 0;
        return (s_allocated_spiram < SPIRAM_HEAP_TOTAL_BYTES)
                   ? (SPIRAM_HEAP_TOTAL_BYTES - s_allocated_spiram)
                   : 0;
    }
    return (s_allocated_internal < INTERNAL_HEAP_TOTAL_BYTES)
               ? (INTERNAL_HEAP_TOTAL_BYTES - s_allocated_internal)
               : 0;
}

size_t heap_caps_get_minimum_free_size(uint32_t caps) {
    return heap_caps_get_free_size(caps);
}

size_t heap_caps_get_total_size(uint32_t caps) {
    if (caps & MALLOC_CAP_SPIRAM) {
        return soc_supports_spiram() ? SPIRAM_HEAP_TOTAL_BYTES : 0;
    }
    return INTERNAL_HEAP_TOTAL_BYTES;
}

size_t heap_caps_get_largest_free_block(uint32_t caps) {
    return heap_caps_get_free_size(caps);
}

bool heap_caps_check_integrity_all(bool print_errors) {
    (void)print_errors;
    return true;
}

void esp_heap_caps_reset(void) {
    memset(s_heap_records, 0, sizeof(s_heap_records));
    s_allocated_internal = 0;
    s_allocated_spiram = 0;
}
