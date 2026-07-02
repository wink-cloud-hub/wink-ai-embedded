/**
 * @file pal_resource.c
 * @brief Host与Wasm仿真端共用的资源占用治理实现（静态表，零动态分配）。
 *        遵循 ADR-0003 双端同源仿真与 ADR-0012 契约诚实原则。
 */
#include "pal_resource.h"
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    pal_resource_type_t type;
    uint32_t            id;
    const char         *owner;   /* 静态存储，见 pal_resource.h 生命周期契约 */
} pal_resource_claim_t;

static pal_resource_claim_t s_claims[PAL_RESOURCE_MAX_CLAIMS];
static uint32_t s_count = 0;

void pal_resource_reset(void) {
    s_count = 0;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_resource_claim(pal_resource_type_t type, uint32_t id, const char *owner) {
    if (owner == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    /* 幂等 / 冲突判定：同 (type,id) 同 owner → OK；不同 owner → BUSY */
    for (uint32_t i = 0; i < s_count; i++) {
        if (s_claims[i].type == type && s_claims[i].id == id) {
            if (strcmp(s_claims[i].owner, owner) == 0) {
                return WINK_OK;        /* 同 owner：幂等 */
            }
            return WINK_ERR_BUSY;      /* 不同 owner：冲突 */
        }
    }
    if (s_count >= PAL_RESOURCE_MAX_CLAIMS) {
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }
    s_claims[s_count].type  = type;
    s_claims[s_count].id    = id;
    s_claims[s_count].owner = owner;
    s_count++;
    return WINK_OK;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_resource_release(pal_resource_type_t type, uint32_t id, const char *owner) {
    if (owner == NULL) { return WINK_ERR_INVALID_ARG; }

    for (uint32_t i = 0; i < s_count; i++) {
        if (s_claims[i].type == type && s_claims[i].id == id) {
            if (strcmp(s_claims[i].owner, owner) != 0) {
                return WINK_ERR_INVALID_ARG;   /* 不同 owner：拒绝误释放 */
            }
            /* 命中：用末尾元素覆盖并缩表（保持紧凑无空洞） */
            s_count--;
            s_claims[i] = s_claims[s_count];
            return WINK_OK;
        }
    }
    return WINK_ERR_INVALID_ARG;   /* 未占用 */
}

bool pal_resource_is_claimed(pal_resource_type_t type, uint32_t id) {
    for (uint32_t i = 0; i < s_count; i++) {
        if (s_claims[i].type == type && s_claims[i].id == id) {
            return true;
        }
    }
    return false;
}
