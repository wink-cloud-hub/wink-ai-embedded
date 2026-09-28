/* SPDX-License-Identifier: LGPL-3.0-only */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200112L
#endif
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "soc/soc_caps.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#if defined(_WIN32)
#include <malloc.h>
#endif

typedef union {
    long double long_double_member;
    long long long_long_member;
    void *pointer_member;
} esp_sim_max_align_t;
typedef struct {
    char pad;
    esp_sim_max_align_t value;
} esp_sim_alignment_probe_t;
#define ESP_SIM_MALLOC_ALIGNMENT \
    offsetof(esp_sim_alignment_probe_t, value)

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
    bool     aligned_allocator;
    bool     used;
} esp_sim_heap_record_t;

static esp_sim_heap_record_t s_heap_records[ESP_SIM_HEAP_TRACKER_MAX];
static size_t s_allocated_internal = 0;
static size_t s_allocated_spiram = 0;
static size_t s_minimum_internal_free = INTERNAL_HEAP_TOTAL_BYTES;
static size_t s_minimum_spiram_free = SPIRAM_HEAP_TOTAL_BYTES;

static inline bool soc_supports_spiram(void) {
#if defined(SOC_SPIRAM_SUPPORTED) && (SOC_SPIRAM_SUPPORTED != 0) && defined(CONFIG_SPIRAM)
    return true;
#else
    return false;
#endif
}

static bool caps_require_special_allocation(uint32_t caps) {
    return (caps & (MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM)) != 0;
}

static bool is_spiram_caps(uint32_t caps) {
    return (caps & MALLOC_CAP_SPIRAM) != 0;
}

static size_t special_allocated_bytes(uint32_t caps) {
    return is_spiram_caps(caps) ? s_allocated_spiram : s_allocated_internal;
}

static size_t special_capacity_bytes(uint32_t caps) {
    return is_spiram_caps(caps) ? SPIRAM_HEAP_TOTAL_BYTES : INTERNAL_HEAP_TOTAL_BYTES;
}

static size_t *special_allocated_counter(uint32_t caps) {
    return is_spiram_caps(caps) ? &s_allocated_spiram : &s_allocated_internal;
}

static size_t *special_minimum_free_counter(uint32_t caps) {
    return is_spiram_caps(caps) ? &s_minimum_spiram_free : &s_minimum_internal_free;
}

static int tracker_reserve_slot(void) {
    for (size_t i = 0; i < ESP_SIM_HEAP_TRACKER_MAX; ++i) {
        if (!s_heap_records[i].used) return (int)i;
    }
    return -1;
}

static void *allocate_aligned(size_t size, size_t alignment, bool *aligned_allocator) {
    *aligned_allocator = false;
    if (alignment <= ESP_SIM_MALLOC_ALIGNMENT) return malloc(size);
#if defined(_WIN32)
    void *ptr = _aligned_malloc(size, alignment);
    if (ptr) *aligned_allocator = true;
    return ptr;
#else
    void *ptr = NULL;
    if (posix_memalign(&ptr, alignment, size) != 0) return NULL;
    *aligned_allocator = true;
    return ptr;
#endif
}

static void free_aligned(void *ptr, bool aligned_allocator) {
    if (!ptr) return;
#if defined(_WIN32)
    if (aligned_allocator) {
        _aligned_free(ptr);
        return;
    }
#else
    (void)aligned_allocator;
#endif
    free(ptr);
}

static bool quota_allows(size_t size, uint32_t caps) {
    size_t used = special_allocated_bytes(caps);
    size_t capacity = special_capacity_bytes(caps);
    return used <= capacity && size <= capacity - used;
}

static void tracker_commit(int slot, void *ptr, size_t size, uint32_t caps,
                           bool aligned_allocator) {
    esp_sim_heap_record_t *record = &s_heap_records[slot];
    record->ptr = ptr;
    record->size = size;
    record->caps = caps;
    record->aligned_allocator = aligned_allocator;
    record->used = true;
    size_t *used = special_allocated_counter(caps);
    *used += size;
    size_t free_bytes = special_capacity_bytes(caps) - *used;
    size_t *minimum = special_minimum_free_counter(caps);
    if (free_bytes < *minimum) *minimum = free_bytes;
}

static void special_free_record(size_t slot) {
    esp_sim_heap_record_t record = s_heap_records[slot];
    memset(&s_heap_records[slot], 0, sizeof(s_heap_records[slot]));
    size_t *used = special_allocated_counter(record.caps);
    if (*used >= record.size) *used -= record.size;
    free_aligned(record.ptr, record.aligned_allocator);
}

void *heap_caps_malloc(size_t size, uint32_t caps) {
    if (size == 0) return NULL;

    /* Conflicting capability rejection */
    if ((caps & MALLOC_CAP_SPIRAM) && (caps & MALLOC_CAP_INTERNAL)) {
        ESP_LOGE("HEAP_CAPS", "Cannot request both MALLOC_CAP_SPIRAM and MALLOC_CAP_INTERNAL");
        return NULL;
    }

    if (caps & MALLOC_CAP_SPIRAM) {
        if (!soc_supports_spiram()) {
            ESP_LOGE("HEAP_CAPS", "MALLOC_CAP_SPIRAM requested but SoC has no PSRAM support (ADR-0012)");
            return NULL;
        }
    }

    if (!caps_require_special_allocation(caps)) return malloc(size);

    if (!quota_allows(size, caps)) {
        ESP_LOGW("HEAP_CAPS", "Special heap allocation of %zu bytes exceeds quota", size);
        return NULL;
    }
    int slot = tracker_reserve_slot();
    if (slot < 0) {
        ESP_LOGW("HEAP_CAPS", "Special heap tracker is full");
        return NULL;
    }

    size_t alignment = ESP_SIM_MALLOC_ALIGNMENT;
    if ((caps & MALLOC_CAP_DMA) && alignment < 32) alignment = 32;
    bool aligned_allocator = false;
    void *p = allocate_aligned(size, alignment, &aligned_allocator);
    if (!p) {
        return NULL;
    }
    tracker_commit(slot, p, size, caps, aligned_allocator);
    return p;
}

void *heap_caps_calloc(size_t n, size_t size, uint32_t caps) {
    if (size != 0 && n > SIZE_MAX / size) return NULL;
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
    if (size == 0) return NULL;
    if ((caps & MALLOC_CAP_SPIRAM) && (caps & MALLOC_CAP_INTERNAL)) {
        ESP_LOGE("HEAP_CAPS", "Cannot request both MALLOC_CAP_SPIRAM and MALLOC_CAP_INTERNAL");
        return NULL;
    }
    if ((caps & MALLOC_CAP_DMA) && alignment < 32) alignment = 32;
    if ((caps & MALLOC_CAP_SPIRAM) && !soc_supports_spiram()) {
        ESP_LOGE("HEAP_CAPS", "MALLOC_CAP_SPIRAM requested but SoC has no PSRAM support");
        return NULL;
    }
    if (alignment <= ESP_SIM_MALLOC_ALIGNMENT && !caps_require_special_allocation(caps)) {
        return malloc(size);
    }
    if (!quota_allows(size, caps)) return NULL;
    int slot = tracker_reserve_slot();
    if (slot < 0) return NULL;
    bool aligned_allocator = false;
    void *ptr = allocate_aligned(size, alignment, &aligned_allocator);
    if (!ptr) return NULL;
    tracker_commit(slot, ptr, size, caps, aligned_allocator);
    return ptr;
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
        esp_sim_heap_record_t old_record = s_heap_records[found_idx];
        bool new_is_special = caps_require_special_allocation(caps);
        if ((caps & MALLOC_CAP_SPIRAM) && (caps & MALLOC_CAP_INTERNAL)) {
            ESP_LOGE("HEAP_CAPS", "Cannot request both MALLOC_CAP_SPIRAM and MALLOC_CAP_INTERNAL");
            return NULL;
        }
        if ((caps & MALLOC_CAP_SPIRAM) && !soc_supports_spiram()) {
            ESP_LOGE("HEAP_CAPS", "MALLOC_CAP_SPIRAM requested but SoC has no PSRAM support");
            return NULL;
        }

        size_t alignment = ESP_SIM_MALLOC_ALIGNMENT;
        if ((caps & MALLOC_CAP_DMA) && alignment < 32) alignment = 32;
        if (new_is_special) {
            size_t used_after_old_release = special_allocated_bytes(caps);
            if (is_spiram_caps(old_record.caps) == is_spiram_caps(caps) &&
                used_after_old_release >= old_record.size) {
                used_after_old_release -= old_record.size;
            }
            size_t capacity = special_capacity_bytes(caps);
            if (used_after_old_release > capacity ||
                size > capacity - used_after_old_release) {
                ESP_LOGW("HEAP_CAPS", "Special heap realloc of %zu bytes exceeds quota", size);
                return NULL;
            }
        }

        bool aligned_allocator = false;
        void *new_p = new_is_special
            ? allocate_aligned(size, alignment, &aligned_allocator)
            : malloc(size);
        if (!new_p) return NULL;
        memcpy(new_p, ptr, old_record.size < size ? old_record.size : size);
        special_free_record((size_t)found_idx);
        if (new_is_special) {
            tracker_commit(found_idx, new_p, size, caps, aligned_allocator);
        }
        return new_p;
    }

    if (caps_require_special_allocation(caps)) return NULL;
    return realloc(ptr, size);
}

void heap_caps_free(void *ptr) {
    if (!ptr) return;

    for (size_t i = 0; i < ESP_SIM_HEAP_TRACKER_MAX; i++) {
        if (s_heap_records[i].used && s_heap_records[i].ptr == ptr) {
            special_free_record(i);
            return;
        }
    }
    free(ptr);
}

size_t heap_caps_get_free_size(uint32_t caps) {
    if (caps & MALLOC_CAP_SPIRAM) {
        if (!soc_supports_spiram()) return 0;
        return s_allocated_spiram <= SPIRAM_HEAP_TOTAL_BYTES
                   ? SPIRAM_HEAP_TOTAL_BYTES - s_allocated_spiram : 0;
    }
    if (caps & MALLOC_CAP_DMA) {
        return s_allocated_internal <= INTERNAL_HEAP_TOTAL_BYTES
                   ? INTERNAL_HEAP_TOTAL_BYTES - s_allocated_internal : 0;
    }
    return INTERNAL_HEAP_TOTAL_BYTES;
}

size_t heap_caps_get_minimum_free_size(uint32_t caps) {
    if (caps & MALLOC_CAP_SPIRAM) {
        return soc_supports_spiram() ? s_minimum_spiram_free : 0;
    }
    if (caps & MALLOC_CAP_DMA) return s_minimum_internal_free;
    return INTERNAL_HEAP_TOTAL_BYTES;
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
    for (size_t i = 0; i < ESP_SIM_HEAP_TRACKER_MAX; ++i) {
        if (s_heap_records[i].used) special_free_record(i);
    }
    s_allocated_internal = 0;
    s_allocated_spiram = 0;
    s_minimum_internal_free = INTERNAL_HEAP_TOTAL_BYTES;
    s_minimum_spiram_free = SPIRAM_HEAP_TOTAL_BYTES;
}
