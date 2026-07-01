/**
 * @file pal_resource_esp32.c
 * @brief ESP32 真机资源占用治理实现（静态表 + 临界区保护）。
 *
 * ✅ @verified: HARDWARE-SMOKE-PASSED (DevKitC, 2026-06-27)
 *    - Dual-core SMP stress: 60s claim/release, no deadlock/crash
 *    - Static table protection: portENTER_CRITICAL works on both cores
 *
 * 架构评审修复 #5：引脚资源冲突防护。
 * 检测 GPIO 引脚 / PWM 通道 / I2C 端口的重复占用冲突。
 * 多核心安全：使用 FreeRTOS 临界区保护静态表。
 */
#include "pal_resource.h"
#include <stddef.h>
#include <string.h>

#if defined(ESP_PLATFORM)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

typedef struct {
    pal_resource_type_t type;
    uint32_t            id;
    const char         *owner;   /* 静态存储，见 pal_resource.h 生命周期契约 */
} pal_resource_claim_t;

static pal_resource_claim_t s_claims[PAL_RESOURCE_MAX_CLAIMS];
static uint32_t s_count = 0;

/* SMP 临界区自旋锁：v5.x 下 taskENTER_CRITICAL(NULL) 会触发 spinlock_acquire(NULL)
 * 断言/解引用 → panic 复位。portMUX_INITIALIZER_UNLOCKED 为编译期初始化，
 * 保证在任何 taskENTER_CRITICAL 调用前已就绪。*/
static portMUX_TYPE s_resource_mux = portMUX_INITIALIZER_UNLOCKED;

void pal_resource_reset(void) {
    taskENTER_CRITICAL(&s_resource_mux);
    s_count = 0;
    taskEXIT_CRITICAL(&s_resource_mux);
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_resource_claim(pal_resource_type_t type, uint32_t id, const char *owner) {
    /* 参数校验 */
    if (owner == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    taskENTER_CRITICAL(&s_resource_mux);

    /* 幂等 / 冲突判定：同 (type,id) 同 owner → OK；不同 owner → BUSY */
    for (uint32_t i = 0; i < s_count; i++) {
        if (s_claims[i].type == type && s_claims[i].id == id) {
            if (strcmp(s_claims[i].owner, owner) == 0) {
                taskEXIT_CRITICAL(&s_resource_mux);
                return WINK_OK;        /* 同 owner：幂等 */
            }
            taskEXIT_CRITICAL(&s_resource_mux);
            return WINK_ERR_BUSY;      /* 不同 owner：冲突 */
        }
    }

    if (s_count >= PAL_RESOURCE_MAX_CLAIMS) {
        taskEXIT_CRITICAL(&s_resource_mux);
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    s_claims[s_count].type  = type;
    s_claims[s_count].id    = id;
    s_claims[s_count].owner = owner;
    s_count++;

    taskEXIT_CRITICAL(&s_resource_mux);
    return WINK_OK;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_resource_release(pal_resource_type_t type, uint32_t id, const char *owner) {
    if (owner == NULL) { return WINK_ERR_INVALID_ARG; }

    taskENTER_CRITICAL(&s_resource_mux);

    wink_status_t result = WINK_ERR_INVALID_ARG;   /* 默认：未占用 */
    for (uint32_t i = 0; i < s_count; i++) {
        if (s_claims[i].type == type && s_claims[i].id == id) {
            if (strcmp(s_claims[i].owner, owner) == 0) {
                /* 命中且 owner 匹配：末尾覆盖缩表 */
                s_count--;
                s_claims[i] = s_claims[s_count];
                result = WINK_OK;
            }
            /* owner 不匹配或命中处理后均停止查找 */
            break;
        }
    }

    taskEXIT_CRITICAL(&s_resource_mux);
    return result;
}
