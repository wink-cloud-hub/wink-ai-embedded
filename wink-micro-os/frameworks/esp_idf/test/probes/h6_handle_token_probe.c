/* SPDX-License-Identifier: GPL-3.0-only */
/* H6 design spike: 32-bit opaque tokens carried through pointer-shaped handles. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#define H6_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#include <stdio.h>
#define H6_EXPORT
#endif

/* PRO queues/semaphores have 64 slots. Seven nonzero kinds fit in three bits. */
#define H6_SLOTS 64u
#define H6_KINDS 8u
#define H6_SEQUENCE_MAX ((1u << 22) - 1u)
#define H6_SLOT_SHIFT 1u
#define H6_KIND_SHIFT 7u
#define H6_SEQUENCE_SHIFT 10u

typedef struct {
    uint32_t sequence;
    bool used;
} h6_entry_t;

static h6_entry_t s_entries[H6_KINDS][H6_SLOTS];
static uint32_t s_sequence;
static bool s_initialized;

/* The caller owns the counter across a complete Wasm module replacement. */
H6_EXPORT int h6_init(uint32_t previous_sequence) {
    if (s_initialized || previous_sequence > H6_SEQUENCE_MAX) return 0;
    memset(s_entries, 0, sizeof(s_entries));
    s_sequence = previous_sequence;
    s_initialized = true;
    return 1;
}

H6_EXPORT uint32_t h6_sequence(void) {
    return s_sequence;
}

H6_EXPORT uint32_t h6_create(uint32_t kind, uint32_t slot) {
    if (!s_initialized || kind == 0 || kind >= H6_KINDS || slot >= H6_SLOTS ||
        s_entries[kind][slot].used || s_sequence == H6_SEQUENCE_MAX) {
        return 0;
    }
    uint32_t next = s_sequence + 1u;
    uint32_t token = (next << H6_SEQUENCE_SHIFT) |
                     (kind << H6_KIND_SHIFT) | (slot << H6_SLOT_SHIFT) | 1u;
    void *opaque = (void *)(uintptr_t)token;
    if ((uintptr_t)opaque != (uintptr_t)token) return 0;
    s_sequence = next;
    s_entries[kind][slot].sequence = next;
    s_entries[kind][slot].used = true;
    return (uint32_t)(uintptr_t)opaque;
}

H6_EXPORT int h6_valid(uint32_t expected_kind, uint32_t token) {
    void *opaque = (void *)(uintptr_t)token;
    uintptr_t raw = (uintptr_t)opaque;
    if (!s_initialized || raw > UINT32_MAX || (raw & 1u) == 0) return 0;
    uint32_t slot = (uint32_t)((raw >> H6_SLOT_SHIFT) & 0x3fu);
    uint32_t kind = (uint32_t)((raw >> H6_KIND_SHIFT) & 0x7u);
    uint32_t sequence = (uint32_t)(raw >> H6_SEQUENCE_SHIFT);
    if (kind == 0 || kind != expected_kind || sequence == 0 ||
        sequence > H6_SEQUENCE_MAX) return 0;
    const h6_entry_t *entry = &s_entries[kind][slot];
    return entry->used && entry->sequence == sequence;
}

H6_EXPORT int h6_delete(uint32_t expected_kind, uint32_t token) {
    if (!h6_valid(expected_kind, token)) return 0;
    uint32_t slot = (token >> H6_SLOT_SHIFT) & 0x3fu;
    uint32_t kind = (token >> H6_KIND_SHIFT) & 0x7u;
    s_entries[kind][slot].used = false;
    return 1;
}

H6_EXPORT void h6_reset(void) {
    memset(s_entries, 0, sizeof(s_entries));
}

#if !defined(__EMSCRIPTEN__)
int main(void) {
    assert(sizeof(uintptr_t) >= 4u);
    assert(h6_create(1, 0) == 0);
    assert(h6_init(0));
    assert(!h6_init(0)); /* A live instance may not roll its counter back. */
    uint32_t old = h6_create(1, 63);
    assert(old != 0 && h6_valid(1, old));
    assert(h6_create(1, 63) == 0);
    assert(h6_create(1, 64) == 0);
    assert(h6_create(0, 0) == 0 && h6_create(8, 0) == 0);
    assert(h6_delete(1, old) && !h6_valid(1, old));
    uint32_t reused = h6_create(1, 63);
    assert(reused != old && h6_valid(1, reused) && !h6_valid(1, old));
    assert(!h6_valid(2, reused));
    assert(!h6_valid(1, reused ^ (1u << H6_KIND_SHIFT)));
    h6_reset();
    assert(!h6_valid(1, reused));
    uint32_t after_reset = h6_create(1, 63);
    assert(after_reset != reused && !h6_valid(1, reused));

    assert(h6_sequence() > 0);
    /* The Wasm harness tests real replacement with a caller-owned counter. */
    h6_reset();
    s_sequence = H6_SEQUENCE_MAX - 1u;
    uint32_t last = h6_create(7, 63);
    assert(last != 0 && h6_valid(7, last) && h6_sequence() == H6_SEQUENCE_MAX);
    assert(h6_create(7, 62) == 0); /* Exhaustion fails closed; no wrap. */
    printf("H6 token probe passed: Host%zu, max_slots=%u, max_sequence=%u\n",
           sizeof(uintptr_t) * 8u, H6_SLOTS, H6_SEQUENCE_MAX);
    return 0;
}
#endif
