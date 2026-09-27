/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OS_MBUF_DATA(om, type) ((type)((om)->om_data))
#define OS_MBUF_PKTLEN(om)     ((om)->om_len)

struct os_mbuf {
    uint8_t *om_data;
    uint16_t om_len;
    uint8_t om_databuf[256];
};

static inline int os_mbuf_append(struct os_mbuf *om, const void *data, uint16_t len) {
    if (!om || !data) return -1;
    if ((uint32_t)om->om_len + len > sizeof(om->om_databuf)) return -1;
    memcpy(om->om_databuf + om->om_len, data, len);
    om->om_len += len;
    om->om_data = om->om_databuf;
    return 0;
}

static inline int os_mbuf_copydata(const struct os_mbuf *om, int off, int len, void *dst) {
    if (!om || !dst || off < 0 || len < 0) return -1;
    if ((uint32_t)(off + len) > (uint32_t)om->om_len) return -1;
    memcpy(dst, om->om_databuf + off, len);
    return 0;
}

static inline void os_mbuf_free_chain(struct os_mbuf *om) {
    (void)om;
}

#ifdef __cplusplus
}
#endif
