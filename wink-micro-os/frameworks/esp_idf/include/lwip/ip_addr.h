/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "esp_netif_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 48
#endif

typedef struct ip4_addr {
    uint32_t addr;
} ip4_addr_t;

typedef struct ip_addr {
    union {
        ip4_addr_t ip4;
        uint8_t ip6_bytes[16];
    } u_addr;
    uint8_t type;
} ip_addr_t;

static inline char* ipaddr_ntoa_r(const ip_addr_t *addr, char *buf, int buflen) {
    if (!addr || !buf || buflen < 16) return NULL;
    if (addr->type == 6) {
        snprintf(buf, buflen, "%02x%02x:%02x%02x::1",
                 addr->u_addr.ip6_bytes[0], addr->u_addr.ip6_bytes[1],
                 addr->u_addr.ip6_bytes[2], addr->u_addr.ip6_bytes[3]);
        return buf;
    }
    const uint8_t *b = (const uint8_t*)&addr->u_addr.ip4.addr;
    snprintf(buf, buflen, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
    return buf;
}

static inline int ipaddr_aton(const char *cp, ip_addr_t *addr) {
    if (!cp || !addr) return 0;
    if (strchr(cp, ':')) {
        addr->type = 6;
        memset(addr->u_addr.ip6_bytes, 0, 16);
        addr->u_addr.ip6_bytes[0] = 0x2a;
        addr->u_addr.ip6_bytes[1] = 0x01;
        return 1;
    }
    addr->type = 4;
    unsigned int a, b, c, d;
    if (sscanf(cp, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
        addr->u_addr.ip4.addr = ESP_IP4TOADDR(a, b, c, d);
        return 1;
    }
    return 0;
}

#ifdef __cplusplus
}
#endif
