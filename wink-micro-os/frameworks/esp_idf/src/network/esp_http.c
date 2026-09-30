/* SPDX-License-Identifier: LGPL-3.0-only */
#include "esp_http_client.h"
#include "sim_net_responder.h"
#include "sim_bounded_stream.h"
#include "esp_log.h"
#include "esp_err.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <strings.h>

#define TAG "ESP_HTTP_CLIENT"

#define MAX_HTTP_CLIENTS 8
#define MAX_HTTP_HEADERS 8
#define HTTP_URL_MAX     256
#define HTTP_HOST_MAX    128
#define HTTP_PATH_MAX    128

typedef struct {
    char key[32];
    char value[64];
    bool used;
} http_header_slot_t;

struct esp_http_client {
    bool initialized;
    esp_http_client_config_t config;
    char url[HTTP_URL_MAX];
    char host[HTTP_HOST_MAX];
    char path[HTTP_PATH_MAX];
    int port;
    esp_http_client_method_t method;
    http_header_slot_t headers[MAX_HTTP_HEADERS];

    sim_bounded_stream_t req_stream;
    sim_bounded_stream_t stream;

    int status_code;
    int64_t response_len;
    bool is_open;
    bool is_complete;
    const sim_http_response_t *matched_resp;
};

static struct esp_http_client s_http_clients[MAX_HTTP_CLIENTS];

static const char *method_to_str(esp_http_client_method_t m) {
    switch (m) {
        case HTTP_METHOD_GET: return "GET";
        case HTTP_METHOD_POST: return "POST";
        case HTTP_METHOD_PUT: return "PUT";
        case HTTP_METHOD_PATCH: return "PATCH";
        case HTTP_METHOD_DELETE: return "DELETE";
        case HTTP_METHOD_HEAD: return "HEAD";
        case HTTP_METHOD_NOTIFY: return "NOTIFY";
        case HTTP_METHOD_SUBSCRIBE: return "SUBSCRIBE";
        case HTTP_METHOD_UNSUBSCRIBE: return "UNSUBSCRIBE";
        case HTTP_METHOD_OPTIONS: return "OPTIONS";
        default: return "GET";
    }
}

static void parse_url(struct esp_http_client *client, const char *url) {
    if (!client || !url) {
        return;
    }
    strncpy(client->url, url, sizeof(client->url) - 1);
    client->url[sizeof(client->url) - 1] = '\0';

    const char *p = url;
    if (strncmp(p, "http://", 7) == 0) {
        p += 7;
        client->port = 80;
    } else if (strncmp(p, "https://", 8) == 0) {
        p += 8;
        client->port = 443;
    }

    const char *path_start = strchr(p, '/');
    const char *colon = strchr(p, ':');

    if (colon && (!path_start || colon < path_start)) {
        size_t host_len = (size_t)(colon - p);
        if (host_len >= sizeof(client->host)) {
            host_len = sizeof(client->host) - 1;
        }
        memcpy(client->host, p, host_len);
        client->host[host_len] = '\0';

        client->port = atoi(colon + 1);
    } else if (path_start) {
        size_t host_len = (size_t)(path_start - p);
        if (host_len >= sizeof(client->host)) {
            host_len = sizeof(client->host) - 1;
        }
        memcpy(client->host, p, host_len);
        client->host[host_len] = '\0';
    } else {
        strncpy(client->host, p, sizeof(client->host) - 1);
        client->host[sizeof(client->host) - 1] = '\0';
    }

    if (path_start) {
        strncpy(client->path, path_start, sizeof(client->path) - 1);
        client->path[sizeof(client->path) - 1] = '\0';
    } else {
        strcpy(client->path, "/");
    }
}

static void dispatch_http_event(struct esp_http_client *client, esp_http_client_event_id_t event_id,
                                void *data, int data_len, char *header_key, char *header_value) {
    if (!client || !client->config.event_handler) {
        return;
    }
    esp_http_client_event_t evt;
    memset(&evt, 0, sizeof(evt));
    evt.event_id = event_id;
    evt.client = client;
    evt.user_data = client->config.user_data;
    evt.data = data;
    evt.data_len = data_len;
    evt.header_key = header_key;
    evt.header_value = header_value;

    client->config.event_handler(&evt);
}

esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *config) {
    if (!config) {
        return NULL;
    }

    for (int i = 0; i < MAX_HTTP_CLIENTS; i++) {
        if (!s_http_clients[i].initialized) {
            memset(&s_http_clients[i], 0, sizeof(s_http_clients[i]));
            s_http_clients[i].initialized = true;
            s_http_clients[i].config = *config;
            s_http_clients[i].method = config->method;
            sim_bounded_stream_init(&s_http_clients[i].req_stream);
            sim_bounded_stream_init(&s_http_clients[i].stream);

            if (config->url) {
                parse_url(&s_http_clients[i], config->url);
            } else if (config->host) {
                int port = config->port ? config->port : 80;
                const char *path = config->path ? config->path : "/";
                strncpy(s_http_clients[i].host, config->host, sizeof(s_http_clients[i].host) - 1);
                s_http_clients[i].port = port;
                strncpy(s_http_clients[i].path, path, sizeof(s_http_clients[i].path) - 1);
                snprintf(s_http_clients[i].url, sizeof(s_http_clients[i].url), "http://%.96s:%d%.96s",
                         config->host, port, path);
            }
            return &s_http_clients[i];
        }
    }
    ESP_LOGE(TAG, "Max HTTP clients reached (%d)", MAX_HTTP_CLIENTS);
    return NULL;
}

esp_err_t esp_http_client_set_url(esp_http_client_handle_t client, const char *url) {
    if (!client || !url) {
        return ESP_ERR_INVALID_ARG;
    }
    parse_url(client, url);
    return ESP_OK;
}

esp_err_t esp_http_client_set_method(esp_http_client_handle_t client, esp_http_client_method_t method) {
    if (!client || method >= HTTP_METHOD_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    client->method = method;
    return ESP_OK;
}

esp_err_t esp_http_client_set_header(esp_http_client_handle_t client, const char *key, const char *value) {
    if (!client || !key || !value) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < MAX_HTTP_HEADERS; i++) {
        if (client->headers[i].used && strcmp(client->headers[i].key, key) == 0) {
            strncpy(client->headers[i].value, value, sizeof(client->headers[i].value) - 1);
            client->headers[i].value[sizeof(client->headers[i].value) - 1] = '\0';
            return ESP_OK;
        }
    }

    for (int i = 0; i < MAX_HTTP_HEADERS; i++) {
        if (!client->headers[i].used) {
            client->headers[i].used = true;
            strncpy(client->headers[i].key, key, sizeof(client->headers[i].key) - 1);
            client->headers[i].key[sizeof(client->headers[i].key) - 1] = '\0';
            strncpy(client->headers[i].value, value, sizeof(client->headers[i].value) - 1);
            client->headers[i].value[sizeof(client->headers[i].value) - 1] = '\0';
            return ESP_OK;
        }
    }
    return ESP_ERR_NO_MEM;
}

esp_err_t esp_http_client_get_header(esp_http_client_handle_t client, const char *key, char **value) {
    if (!client || !key || !value) {
        return ESP_ERR_INVALID_ARG;
    }
    for (int i = 0; i < MAX_HTTP_HEADERS; i++) {
        if (client->headers[i].used && strcmp(client->headers[i].key, key) == 0) {
            *value = client->headers[i].value;
            return ESP_OK;
        }
    }
    *value = NULL;
    return ESP_ERR_NOT_FOUND;
}

esp_err_t esp_http_client_delete_header(esp_http_client_handle_t client, const char *key) {
    if (!client || !key) {
        return ESP_ERR_INVALID_ARG;
    }
    for (int i = 0; i < MAX_HTTP_HEADERS; i++) {
        if (client->headers[i].used && strcmp(client->headers[i].key, key) == 0) {
            client->headers[i].used = false;
            memset(&client->headers[i], 0, sizeof(client->headers[i]));
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t esp_http_client_set_post_field(esp_http_client_handle_t client, const char *data, int len) {
    if (!client) {
        return ESP_ERR_INVALID_ARG;
    }
    if (data && len <= 0) {
        len = (int)strlen(data);
    }
    sim_bounded_stream_reset(&client->req_stream);
    if (!data || len == 0) {
        return ESP_OK;
    }

    return sim_bounded_stream_write(&client->req_stream, (const uint8_t*)data, (size_t)len);
}

int esp_http_client_get_post_field(esp_http_client_handle_t client, char **data) {
    if (!client || !data) {
        return -1;
    }
    if (client->req_stream.head) {
        *data = (char*)(client->req_stream.head->data + client->req_stream.head->read_offset);
        return (int)(client->req_stream.head->write_len - client->req_stream.head->read_offset);
    }
    *data = NULL;
    return 0;
}

int esp_http_client_get_status_code(esp_http_client_handle_t client) {
    if (!client) {
        return -1;
    }
    return client->status_code;
}

int64_t esp_http_client_get_content_length(esp_http_client_handle_t client) {
    if (!client) {
        return -1;
    }
    return client->response_len;
}

bool esp_http_client_is_complete_data_received(esp_http_client_handle_t client) {
    if (!client) {
        return false;
    }
    return client->is_complete || (client->is_open && sim_bounded_stream_available(&client->stream) == 0);
}

esp_err_t esp_http_client_cleanup(esp_http_client_handle_t client) {
    if (!client) {
        return ESP_ERR_INVALID_ARG;
    }
    sim_bounded_stream_reset(&client->req_stream);
    sim_bounded_stream_reset(&client->stream);
    memset(client, 0, sizeof(*client));
    return ESP_OK;
}

esp_err_t esp_http_client_perform(esp_http_client_handle_t client) {
    if (!client) {
        return ESP_ERR_INVALID_ARG;
    }

    const char *method_str = method_to_str(client->method);
    const sim_http_response_t *resp = sim_http_responder_match(method_str, client->url);

    if (!resp) {
        ESP_LOGE(TAG, "No matching mock route for %s %s (Fail-Loud)", method_str, client->url);
        dispatch_http_event(client, HTTP_EVENT_ERROR, NULL, 0, NULL, NULL);
        return ESP_ERR_HTTP_CONNECT;
    }

    if (resp->fault_inject_err != ESP_OK) {
        ESP_LOGE(TAG, "Fault injection triggered: %s", esp_err_to_name(resp->fault_inject_err));
        dispatch_http_event(client, HTTP_EVENT_ERROR, NULL, 0, NULL, NULL);
        return resp->fault_inject_err;
    }

    client->status_code = resp->status_code;
    client->response_len = (int64_t)resp->body_len;

    /* 1. ON_CONNECTED */
    dispatch_http_event(client, HTTP_EVENT_ON_CONNECTED, NULL, 0, NULL, NULL);

    /* 2. HEADER_SENT */
    dispatch_http_event(client, HTTP_EVENT_HEADER_SENT, NULL, 0, NULL, NULL);

    /* 3. ON_HEADER: 派发路由定义中的响应头 */
    for (size_t i = 0; i < resp->header_count; i++) {
        char key_buf[32];
        char val_buf[128];
        strncpy(key_buf, resp->headers[i].key, sizeof(key_buf) - 1);
        key_buf[sizeof(key_buf) - 1] = '\0';
        strncpy(val_buf, resp->headers[i].value, sizeof(val_buf) - 1);
        val_buf[sizeof(val_buf) - 1] = '\0';
        dispatch_http_event(client, HTTP_EVENT_ON_HEADER, NULL, 0, key_buf, val_buf);
    }

    /* 4. ON_DATA: 载荷写入有界流缓冲区并派发事件 */
    sim_bounded_stream_reset(&client->stream);
    if (resp->body_len > 0 && resp->body_data) {
        esp_err_t werr = sim_bounded_stream_write(&client->stream, resp->body_data, resp->body_len);
        if (werr != ESP_OK) {
            ESP_LOGW(TAG, "Bounded stream write backpressured (%zu bytes)", resp->body_len);
        }
        dispatch_http_event(client, HTTP_EVENT_ON_DATA, (void*)resp->body_data, (int)resp->body_len, NULL, NULL);
    }

    /* 5. ON_FINISH */
    dispatch_http_event(client, HTTP_EVENT_ON_FINISH, NULL, 0, NULL, NULL);

    /* 6. DISCONNECTED */
    dispatch_http_event(client, HTTP_EVENT_DISCONNECTED, NULL, 0, NULL, NULL);

    client->is_complete = true;
    return ESP_OK;
}

/* ── 低级 Native 流式读取 API ─────────────────────────────────────────── */

esp_err_t esp_http_client_open(esp_http_client_handle_t client, int write_len) {
    if (!client) {
        return ESP_ERR_INVALID_ARG;
    }

    const char *method_str = method_to_str(client->method);
    const sim_http_response_t *resp = sim_http_responder_match(method_str, client->url);
    if (!resp) {
        ESP_LOGE(TAG, "open: No route for %s %s", method_str, client->url);
        dispatch_http_event(client, HTTP_EVENT_ERROR, NULL, 0, NULL, NULL);
        return ESP_ERR_HTTP_CONNECT;
    }
    if (resp->fault_inject_err != ESP_OK) {
        dispatch_http_event(client, HTTP_EVENT_ERROR, NULL, 0, NULL, NULL);
        return resp->fault_inject_err;
    }

    client->is_open = true;
    client->is_complete = false;
    client->matched_resp = resp;
    client->status_code = resp->status_code;
    client->response_len = (int64_t)resp->body_len;

    sim_bounded_stream_reset(&client->req_stream);
    sim_bounded_stream_reset(&client->stream);
    if (resp->body_len > 0 && resp->body_data) {
        sim_bounded_stream_write(&client->stream, resp->body_data, resp->body_len);
    }

    (void)write_len;
    dispatch_http_event(client, HTTP_EVENT_ON_CONNECTED, NULL, 0, NULL, NULL);
    return ESP_OK;
}

int esp_http_client_write(esp_http_client_handle_t client, const char *buffer, int len) {
    if (!client || !buffer || len < 0) {
        return -1;
    }
    if (len == 0) {
        return 0;
    }

    esp_err_t err = sim_bounded_stream_write(&client->req_stream, (const uint8_t*)buffer, (size_t)len);
    if (err != ESP_OK) {
        return -1;
    }
    return len;
}

int64_t esp_http_client_fetch_headers(esp_http_client_handle_t client) {
    if (!client) {
        return -1;
    }
    const sim_http_response_t *resp = client->matched_resp;
    if (!resp) {
        const char *method_str = method_to_str(client->method);
        resp = sim_http_responder_match(method_str, client->url);
        client->matched_resp = resp;
        if (!resp) {
            return -1;
        }
        client->status_code = resp->status_code;
        client->response_len = (int64_t)resp->body_len;
    }

    for (size_t i = 0; i < resp->header_count; i++) {
        char key_buf[32];
        char val_buf[128];
        strncpy(key_buf, resp->headers[i].key, sizeof(key_buf) - 1);
        key_buf[sizeof(key_buf) - 1] = '\0';
        strncpy(val_buf, resp->headers[i].value, sizeof(val_buf) - 1);
        val_buf[sizeof(val_buf) - 1] = '\0';
        dispatch_http_event(client, HTTP_EVENT_ON_HEADER, NULL, 0, key_buf, val_buf);
    }

    if (resp->is_chunked) {
        return -1;
    }
    return (int64_t)resp->body_len;
}

int esp_http_client_read(esp_http_client_handle_t client, char *buffer, int len) {
    if (!client || !buffer || len <= 0) {
        return -1;
    }
    int rc = sim_bounded_stream_read(&client->stream, (uint8_t*)buffer, (size_t)len);
    if (sim_bounded_stream_available(&client->stream) == 0) {
        client->is_complete = true;
    }
    return rc;
}

int esp_http_client_read_response(esp_http_client_handle_t client, char *buffer, int len) {
    return esp_http_client_read(client, buffer, len);
}

bool esp_http_client_is_chunked_response(esp_http_client_handle_t client) {
    if (!client || !client->matched_resp) {
        return false;
    }
    return client->matched_resp->is_chunked;
}

esp_err_t esp_http_client_close(esp_http_client_handle_t client) {
    if (!client) {
        return ESP_ERR_INVALID_ARG;
    }
    client->is_open = false;
    client->matched_resp = NULL;
    sim_bounded_stream_reset(&client->stream);
    sim_bounded_stream_reset(&client->req_stream);
    dispatch_http_event(client, HTTP_EVENT_DISCONNECTED, NULL, 0, NULL, NULL);
    return ESP_OK;
}

/* ── Wink 仿真与 Mock 专用 ───────────────────────────────────────────── */

void esp_http_client_sim_reset(void) {
    memset(s_http_clients, 0, sizeof(s_http_clients));
    sim_http_responder_reset();
}

void esp_http_client_sim_set_response(esp_http_client_handle_t client, int status_code, const char *content, size_t content_len) {
    sim_http_route_t route;
    memset(&route, 0, sizeof(route));
    if (client && client->url[0] != '\0') {
        strncpy(route.url_prefix, client->url, sizeof(route.url_prefix) - 1);
    } else {
        strcpy(route.url_prefix, "http");
    }
    route.resp.status_code = status_code;
    route.resp.body_data = (const uint8_t*)content;
    route.resp.body_len = content_len;
    strcpy(route.resp.headers[0].key, "Content-Type");
    strcpy(route.resp.headers[0].value, "text/plain");
    route.resp.header_count = 1;
    sim_http_responder_register_route(&route);
}

esp_err_t esp_http_client_set_redirection(esp_http_client_handle_t client) {
    (void)client;
    return ESP_OK;
}

int esp_http_client_chunk_write_begin(esp_http_client_handle_t client, int len) {
    (void)client;
    (void)len;
    return 0;
}

int esp_http_client_chunk_write_end(esp_http_client_handle_t client, bool is_last) {
    (void)client;
    (void)is_last;
    return 0;
}
