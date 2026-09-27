/* SPDX-License-Identifier: CC0-1.0 */
#ifndef H_ESP_PERIPHERAL_
#define H_ESP_PERIPHERAL_
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

static inline int scli_init(void) { return 0; }
static inline int scli_receive_key(int *key) { (void)key; return 0; }
static inline void print_bytes(const uint8_t *bytes, int len) { (void)bytes; (void)len; }
static inline char *addr_str(const void *addr) { (void)addr; return ""; }
#endif
