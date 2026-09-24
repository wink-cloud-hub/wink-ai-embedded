/* SPDX-License-Identifier: LGPL-3.0-only */
#include "wink_app.h"
#include "pal_log.h"
#include <stddef.h>

extern void app_main(void);

static void esp_idf_framework_init(void) {
    pal_log_i("ESP_IDF", "Framework initialized in simulation mode");
}

static void esp_idf_app_loop(void) {
    /* app_main 由 runtime 作为独立 fiber 启动，主循环配合调度 */
}

static const wink_app_callbacks_t s_esp_idf_callbacks = {
    esp_idf_framework_init,
    esp_idf_app_loop,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

const wink_app_callbacks_t* wink_app_get_callbacks(void) {
    return &s_esp_idf_callbacks;
}
