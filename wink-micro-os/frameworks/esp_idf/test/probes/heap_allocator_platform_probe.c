/* SPDX-License-Identifier: GPL-3.0-only */
/* Small ABI probe for malloc/aligned allocation and matching release pairs. */
#define _ISOC11_SOURCE 1
#define _POSIX_C_SOURCE 200112L
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdalign.h>

#if defined(_WIN32)
#include <malloc.h>
#endif

static int check_alignment(const char *name, void *ptr, size_t alignment) {
    if (!ptr || ((uintptr_t)ptr % alignment) != 0) {
        fprintf(stderr, "%s failed alignment %zu\n", name, alignment);
        return 1;
    }
    return 0;
}

int main(void) {
    int failures = 0;
    const size_t base_alignment = alignof(max_align_t);
    for (size_t i = 0; i < 256; ++i) {
        void *p = malloc(i + 1);
        failures += check_alignment("malloc", p, base_alignment);
        free(p);
    }

    void *zero = malloc(0);
    free(zero);
#if defined(_WIN32)
    void *ms_aligned = _aligned_malloc(127, 64);
    failures += check_alignment("_aligned_malloc", ms_aligned, 64);
    _aligned_free(ms_aligned);
#else
    void *aligned = aligned_alloc(64, 128);
    failures += check_alignment("aligned_alloc/free", aligned, 64);
    free(aligned);
    void *posix_aligned = NULL;
    int posix_rc = posix_memalign(&posix_aligned, 64, 127);
    if (posix_rc != 0) {
        fprintf(stderr, "posix_memalign failed: %d\n", posix_rc);
        failures++;
    } else {
        failures += check_alignment("posix_memalign/free", posix_aligned, 64);
        free(posix_aligned);
    }
#endif

    if (failures != 0) return 1;
    printf("heap allocator probe passed; max_align_t=%zu; platform=%s\n",
           base_alignment,
#if defined(__EMSCRIPTEN__)
           "wasm32"
#elif defined(_WIN32)
           "windows"
#else
           "posix"
#endif
    );
    return 0;
}
