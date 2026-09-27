/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__EMSCRIPTEN__)
#  include <emscripten.h>
#  define WINK_SIM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#  define WINK_SIM_EXPORT
#endif

typedef struct esp_http_client* esp_http_client_handle_t;

typedef enum {
    HTTP_TRANSPORT_UNKNOWN = 0x0,
    HTTP_TRANSPORT_OVER_TCP,
    HTTP_TRANSPORT_OVER_SSL,
} esp_http_client_transport_t;

typedef enum {
    HTTP_METHOD_GET = 0,
    HTTP_METHOD_POST,
    HTTP_METHOD_PUT,
    HTTP_METHOD_PATCH,
    HTTP_METHOD_DELETE,
    HTTP_METHOD_HEAD,
    HTTP_METHOD_NOTIFY,
    HTTP_METHOD_SUBSCRIBE,
    HTTP_METHOD_UNSUBSCRIBE,
    HTTP_METHOD_OPTIONS,
    HTTP_METHOD_MAX,
} esp_http_client_method_t;

typedef enum {
    HTTP_AUTH_TYPE_NONE = 0,
    HTTP_AUTH_TYPE_BASIC,
    HTTP_AUTH_TYPE_DIGEST,
} esp_http_client_auth_type_t;

typedef enum {
    HTTP_EVENT_ERROR = 0,
    HTTP_EVENT_ON_CONNECTED,
    HTTP_EVENT_HEADER_SENT,
    HTTP_EVENT_ON_HEADER,
    HTTP_EVENT_ON_DATA,
    HTTP_EVENT_ON_FINISH,
    HTTP_EVENT_DISCONNECTED,
    HTTP_EVENT_REDIRECT
} esp_http_client_event_id_t;

typedef struct {
    esp_http_client_event_id_t event_id;
    esp_http_client_handle_t client;
    void *data;
    int data_len;
    void *user_data;
    char *header_key;
    char *header_value;
} esp_http_client_event_t;

typedef esp_err_t (*esp_http_client_event_handle_cb)(esp_http_client_event_t *evt);

typedef struct {
    const char *url;
    const char *host;
    int port;
    const char *username;
    const char *password;
    const char *path;
    esp_http_client_method_t method;
    int timeout_ms;
    esp_http_client_event_handle_cb event_handler;
    esp_http_client_transport_t transport_type;
    int buffer_size;
    int buffer_size_tx;
    void *user_data;
    bool is_async;
    const char *cert_pem;
    const char *client_cert_pem;
    const char *client_key_pem;
    esp_err_t (*crt_bundle_attach)(void *conf);
    esp_http_client_auth_type_t auth_type;
    const char *query;
    bool disable_auto_redirect;
    int max_redirection_count;
    bool keep_alive_enable;
} esp_http_client_config_t;

/* 高阶通用执行 API */
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *config);
esp_err_t esp_http_client_perform(esp_http_client_handle_t client);
esp_err_t esp_http_client_set_url(esp_http_client_handle_t client, const char *url);
esp_err_t esp_http_client_set_method(esp_http_client_handle_t client, esp_http_client_method_t method);
esp_err_t esp_http_client_set_header(esp_http_client_handle_t client, const char *key, const char *value);
esp_err_t esp_http_client_get_header(esp_http_client_handle_t client, const char *key, char **value);
esp_err_t esp_http_client_delete_header(esp_http_client_handle_t client, const char *key);
esp_err_t esp_http_client_set_post_field(esp_http_client_handle_t client, const char *data, int len);
int esp_http_client_get_post_field(esp_http_client_handle_t client, char **data);
int esp_http_client_get_status_code(esp_http_client_handle_t client);
int64_t esp_http_client_get_content_length(esp_http_client_handle_t client);
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t client);
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t client);

/* 低级 Native 流式读取 API（官方语料核心依赖） */
esp_err_t esp_http_client_open(esp_http_client_handle_t client, int write_len);
int esp_http_client_fetch_headers(esp_http_client_handle_t client);
int esp_http_client_read(esp_http_client_handle_t client, char *buffer, int len);
int esp_http_client_read_response(esp_http_client_handle_t client, char *buffer, int len);
int esp_http_client_write(esp_http_client_handle_t client, const char *buffer, int len);
bool esp_http_client_is_chunked_response(esp_http_client_handle_t client);
esp_err_t esp_http_client_close(esp_http_client_handle_t client);

/* Wink 仿真与 Mock 专用 */
WINK_SIM_EXPORT void esp_http_client_sim_reset(void);
WINK_SIM_EXPORT void esp_http_client_sim_set_response(esp_http_client_handle_t client, int status_code, const char *content, size_t content_len);

#ifdef __cplusplus
}
#endif
