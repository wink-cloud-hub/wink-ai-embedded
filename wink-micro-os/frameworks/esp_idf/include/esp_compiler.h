/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef ESP_COMPILER_H_
#define ESP_COMPILER_H_

#ifndef likely
#if defined(__GNUC__) || defined(__clang__)
#define likely(x)   (__builtin_expect(!!(x), 1))
#else
#define likely(x)   (x)
#endif
#endif

#ifndef unlikely
#if defined(__GNUC__) || defined(__clang__)
#define unlikely(x) (__builtin_expect(!!(x), 0))
#else
#define unlikely(x) (x)
#endif
#endif

#endif /* ESP_COMPILER_H_ */
