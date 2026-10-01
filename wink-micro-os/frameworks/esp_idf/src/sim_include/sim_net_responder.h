/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef SIM_NET_RESPONDER_H_
#define SIM_NET_RESPONDER_H_

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__EMSCRIPTEN__)
#  include <emscripten.h>
#  define WINK_SIM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#  define WINK_SIM_EXPORT
#endif

#define SIM_HTTP_MAX_ROUTES     16
#define SIM_HTTP_MAX_HEADERS    8
#define SIM_HTTP_KEY_MAX        32
#define SIM_HTTP_VAL_MAX        128
#define SIM_HTTP_URL_MAX        128
#define SIM_HTTP_BODY_MAX       1024

typedef struct {
    char key[SIM_HTTP_KEY_MAX];
    char value[SIM_HTTP_VAL_MAX];
} sim_http_header_kv_t;

typedef struct {
    int status_code;
    sim_http_header_kv_t headers[SIM_HTTP_MAX_HEADERS];
    size_t header_count;
    char body_buf[SIM_HTTP_BODY_MAX];
    const uint8_t *body_data;
    size_t body_len;
    esp_err_t fault_inject_err; /* 若非 ESP_OK 则触发模拟底层网络异常 */
    uint32_t delay_us;          /* 模拟时序推进延迟 */
    bool is_chunked;
} sim_http_response_t;

typedef struct {
    char method[16];            /* GET, POST, PUT, DELETE 等 (空串或 NULL 表示匹配所有) */
    char url_prefix[SIM_HTTP_URL_MAX]; /* 匹配前缀 (如 "http://httpbin.org/get") */
    sim_http_response_t resp;
    bool is_active;
} sim_http_route_t;

/* 路由注册与重置 API（供测试用例或场景加载器初始化） */
WINK_SIM_EXPORT void sim_http_responder_reset(void);
WINK_SIM_EXPORT esp_err_t sim_http_responder_register_route(const sim_http_route_t *route);
const sim_http_response_t *sim_http_responder_match(const char *method, const char *url);
size_t sim_http_responder_route_count(void);

/* Wasm 仿真注入与指标观测 API */
WINK_SIM_EXPORT int sim_http_responder_inject_json(const char *json_str);
WINK_SIM_EXPORT int sim_http_get_last_status_code(void);
WINK_SIM_EXPORT int sim_http_get_total_rx_bytes(void);
WINK_SIM_EXPORT int sim_http_get_request_count(void);
void sim_http_record_request(int status_code, size_t rx_bytes);

#ifdef __cplusplus
}
#endif

#endif /* SIM_NET_RESPONDER_H_ */
