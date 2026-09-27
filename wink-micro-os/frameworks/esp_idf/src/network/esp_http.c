/* SPDX-License-Identifier: LGPL-3.0-only */
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_err.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define TAG "ESP_HTTP_CLIENT"

#define MAX_HTTP_CLIENTS 2
#define MAX_HTTP_HEADERS 8
#define HTTP_BODY_MAX    512
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
    char post_data[HTTP_BODY_MAX];
    int post_len;

    int status_code;
    char response_body[HTTP_BODY_MAX];
    int response_len;
    int read_offset;
    bool is_open;
};

static struct esp_http_client s_http_clients[MAX_HTTP_CLIENTS];
static int s_default_status_code = 200;
static char s_default_response_body[HTTP_BODY_MAX] = "OK";
static int s_default_response_len = 2;

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
            s_http_clients[i].status_code = s_default_status_code;
            memcpy(s_http_clients[i].response_body, s_default_response_body, (size_t)s_default_response_len);
            s_http_clients[i].response_body[s_default_response_len] = '\0';
            s_http_clients[i].response_len = s_default_response_len;

            if (config->url) {
                parse_url(&s_http_clients[i], config->url);
            } else if (config->host) {
                strncpy(s_http_clients[i].host, config->host, sizeof(s_http_clients[i].host) - 1);
                s_http_clients[i].port = config->port ? config->port : 80;
                if (config->path) {
                    strncpy(s_http_clients[i].path, config->path, sizeof(s_http_clients[i].path) - 1);
                } else {
                    strcpy(s_http_clients[i].path, "/");
                }
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
    if (!data) {
        client->post_data[0] = '\0';
        client->post_len = 0;
        return ESP_OK;
    }

    size_t cplen = (size_t)len < sizeof(client->post_data) - 1 ? (size_t)len : sizeof(client->post_data) - 1;
    memcpy(client->post_data, data, cplen);
    client->post_data[cplen] = '\0';
    client->post_len = (int)cplen;
    return ESP_OK;
}

int esp_http_client_get_post_field(esp_http_client_handle_t client, char **data) {
    if (!client || !data) {
        return -1;
    }
    *data = client->post_data;
    return client->post_len;
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
    return (int64_t)client->response_len;
}

bool esp_http_client_is_complete_data_received(esp_http_client_handle_t client) {
    if (!client) {
        return false;
    }
    return (client->read_offset >= client->response_len);
}

esp_err_t esp_http_client_cleanup(esp_http_client_handle_t client) {
    if (!client) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(client, 0, sizeof(*client));
    return ESP_OK;
}

esp_err_t esp_http_client_perform(esp_http_client_handle_t client) {
    if (!client) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 1. ON_CONNECTED */
    dispatch_http_event(client, HTTP_EVENT_ON_CONNECTED, NULL, 0, NULL, NULL);

    /* 2. HEADER_SENT */
    dispatch_http_event(client, HTTP_EVENT_HEADER_SENT, NULL, 0, NULL, NULL);

    /* 3. ON_HEADER: 保证非空合法默认头派发（防应用层 printf 空指针解引用崩溃） */
    char content_length_str[16];
    snprintf(content_length_str, sizeof(content_length_str), "%d", client->response_len);

    char ct_key[] = "Content-Type";
    char ct_val[] = "text/plain";
    dispatch_http_event(client, HTTP_EVENT_ON_HEADER, NULL, 0, ct_key, ct_val);

    char cl_key[] = "Content-Length";
    dispatch_http_event(client, HTTP_EVENT_ON_HEADER, NULL, 0, cl_key, content_length_str);

    /* 4. ON_DATA: 响应正文 */
    if (client->response_len > 0) {
        dispatch_http_event(client, HTTP_EVENT_ON_DATA, client->response_body, client->response_len, NULL, NULL);
    }

    /* 5. ON_FINISH */
    dispatch_http_event(client, HTTP_EVENT_ON_FINISH, NULL, 0, NULL, NULL);

    /* 6. DISCONNECTED */
    dispatch_http_event(client, HTTP_EVENT_DISCONNECTED, NULL, 0, NULL, NULL);

    client->read_offset = client->response_len;
    return ESP_OK;
}

/* ── 低级 Native 流式读取 API ─────────────────────────────────────────── */

esp_err_t esp_http_client_open(esp_http_client_handle_t client, int write_len) {
    (void)write_len;
    if (!client) {
        return ESP_ERR_INVALID_ARG;
    }
    client->is_open = true;
    client->read_offset = 0;

    dispatch_http_event(client, HTTP_EVENT_ON_CONNECTED, NULL, 0, NULL, NULL);
    return ESP_OK;
}

int esp_http_client_write(esp_http_client_handle_t client, const char *buffer, int len) {
    if (!client || !buffer) {
        return -1;
    }
    if (len < 0) {
        return -1;
    }
    return len;
}

int esp_http_client_fetch_headers(esp_http_client_handle_t client) {
    if (!client) {
        return -1;
    }
    char content_length_str[16];
    snprintf(content_length_str, sizeof(content_length_str), "%d", client->response_len);

    char ct_key[] = "Content-Type";
    char ct_val[] = "text/plain";
    dispatch_http_event(client, HTTP_EVENT_ON_HEADER, NULL, 0, ct_key, ct_val);

    char cl_key[] = "Content-Length";
    dispatch_http_event(client, HTTP_EVENT_ON_HEADER, NULL, 0, cl_key, content_length_str);

    return client->response_len;
}

int esp_http_client_read(esp_http_client_handle_t client, char *buffer, int len) {
    if (!client || !buffer || len <= 0) {
        return -1;
    }
    if (client->read_offset >= client->response_len) {
        return 0; /* EOF */
    }

    int remaining = client->response_len - client->read_offset;
    int to_read = (len < remaining) ? len : remaining;
    memcpy(buffer, client->response_body + client->read_offset, (size_t)to_read);
    client->read_offset += to_read;
    return to_read;
}

int esp_http_client_read_response(esp_http_client_handle_t client, char *buffer, int len) {
    return esp_http_client_read(client, buffer, len);
}

bool esp_http_client_is_chunked_response(esp_http_client_handle_t client) {
    (void)client;
    return false;
}

esp_err_t esp_http_client_close(esp_http_client_handle_t client) {
    if (!client) {
        return ESP_ERR_INVALID_ARG;
    }
    client->is_open = false;
    client->read_offset = 0;
    dispatch_http_event(client, HTTP_EVENT_DISCONNECTED, NULL, 0, NULL, NULL);
    return ESP_OK;
}

/* ── Wink 仿真与 Mock 专用 ───────────────────────────────────────────── */

void esp_http_client_sim_reset(void) {
    memset(s_http_clients, 0, sizeof(s_http_clients));
    s_default_status_code = 200;
    strcpy(s_default_response_body, "OK");
    s_default_response_len = 2;
}

void esp_http_client_sim_set_response(esp_http_client_handle_t client, int status_code, const char *content, size_t content_len) {
    if (client) {
        client->status_code = status_code;
        if (content && content_len > 0) {
            size_t cplen = content_len < sizeof(client->response_body) - 1 ? content_len : sizeof(client->response_body) - 1;
            memcpy(client->response_body, content, cplen);
            client->response_body[cplen] = '\0';
            client->response_len = (int)cplen;
        } else {
            client->response_body[0] = '\0';
            client->response_len = 0;
        }
    } else {
        s_default_status_code = status_code;
        if (content && content_len > 0) {
            size_t cplen = content_len < sizeof(s_default_response_body) - 1 ? content_len : sizeof(s_default_response_body) - 1;
            memcpy(s_default_response_body, content, cplen);
            s_default_response_body[cplen] = '\0';
            s_default_response_len = (int)cplen;
        } else {
            s_default_response_body[0] = '\0';
            s_default_response_len = 0;
        }
    }
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
