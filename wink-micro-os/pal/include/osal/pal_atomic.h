// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_atomic.h
 * @brief Portable atomic memory operations and fences across Host, Wasm, and ESP32.
 */

#ifndef PAL_ATOMIC_H
#define PAL_ATOMIC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PAL_ACQ      __ATOMIC_ACQUIRE
#define PAL_REL      __ATOMIC_RELEASE
#define PAL_ACQ_REL  __ATOMIC_ACQ_REL
#define PAL_RLX      __ATOMIC_RELAXED
#define PAL_SEQ_CST  __ATOMIC_SEQ_CST

#if defined(__GNUC__) || defined(__clang__)

#define PAL_ATOMIC_LOAD(ptr, ord)      __atomic_load_n((ptr), (ord))
#define PAL_ATOMIC_STORE(ptr, v, ord)  __atomic_store_n((ptr), (v), (ord))
#define PAL_ATOMIC_ADD(ptr, v, ord)    __atomic_fetch_add((ptr), (v), (ord))
#define PAL_ATOMIC_SUB(ptr, v, ord)    __atomic_fetch_sub((ptr), (v), (ord))
#define PAL_ATOMIC_XCHG(ptr, v, ord)   __atomic_exchange_n((ptr), (v), (ord))
#define PAL_ATOMIC_CAS(ptr, exp, des, succ_ord, fail_ord) \
    __atomic_compare_exchange_n((ptr), (exp), (des), false, (succ_ord), (fail_ord))
#define PAL_ATOMIC_THREAD_FENCE(ord)   __atomic_thread_fence(ord)
#define PAL_ATOMIC_SIGNAL_FENCE(ord)   __atomic_signal_fence(ord)

#elif defined(_MSC_VER)

#include <windows.h>
#include <intrin.h>

static inline uint64_t _pal_atomic_add_impl(volatile void *p, uint64_t v, size_t sz) {
    if (sz == 8) {
        return (uint64_t)(InterlockedAdd64((volatile LONG64 *)p, (LONG64)v) - (LONG64)v);
    } else {
        return (uint64_t)(InterlockedAdd((volatile LONG *)p, (LONG)v) - (LONG)v);
    }
}

static inline uint64_t _pal_atomic_sub_impl(volatile void *p, uint64_t v, size_t sz) {
    if (sz == 8) {
        return (uint64_t)(InterlockedAdd64((volatile LONG64 *)p, -(LONG64)v) + (LONG64)v);
    } else {
        return (uint64_t)(InterlockedAdd((volatile LONG *)p, -(LONG)v) + (LONG)v);
    }
}

static inline uint64_t _pal_atomic_xchg_impl(volatile void *p, uint64_t v, size_t sz) {
    if (sz == 8) {
        return (uint64_t)InterlockedExchange64((volatile LONG64 *)p, (LONG64)v);
    } else {
        return (uint64_t)InterlockedExchange((volatile LONG *)p, (LONG)v);
    }
}

#define PAL_ATOMIC_LOAD(ptr, ord)      (*(ptr))
#define PAL_ATOMIC_STORE(ptr, v, ord)  do { *(ptr) = (v); MemoryBarrier(); } while(0)
#define PAL_ATOMIC_ADD(ptr, v, ord)    _pal_atomic_add_impl((volatile void *)(ptr), (uint64_t)(v), sizeof(*(ptr)))
#define PAL_ATOMIC_SUB(ptr, v, ord)    _pal_atomic_sub_impl((volatile void *)(ptr), (uint64_t)(v), sizeof(*(ptr)))
#define PAL_ATOMIC_XCHG(ptr, v, ord)   _pal_atomic_xchg_impl((volatile void *)(ptr), (uint64_t)(v), sizeof(*(ptr)))
#define PAL_ATOMIC_THREAD_FENCE(ord)   MemoryBarrier()
#define PAL_ATOMIC_SIGNAL_FENCE(ord)   _ReadWriteBarrier()

#else

#include <stdatomic.h>

#define PAL_ATOMIC_LOAD(ptr, ord)      atomic_load_explicit((ptr), (ord))
#define PAL_ATOMIC_STORE(ptr, v, ord)  atomic_store_explicit((ptr), (v), (ord))
#define PAL_ATOMIC_ADD(ptr, v, ord)    atomic_fetch_add_explicit((ptr), (v), (ord))
#define PAL_ATOMIC_SUB(ptr, v, ord)    atomic_fetch_sub_explicit((ptr), (v), (ord))
#define PAL_ATOMIC_XCHG(ptr, v, ord)   atomic_exchange_explicit((ptr), (v), (ord))
#define PAL_ATOMIC_THREAD_FENCE(ord)   atomic_thread_fence(ord)
#define PAL_ATOMIC_SIGNAL_FENCE(ord)   atomic_signal_fence(ord)

#endif

#endif /* PAL_ATOMIC_H */
