/* SPDX-License-Identifier: LGPL-3.0-only */
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_err.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <strings.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#define WINK_SIM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define WINK_SIM_EXPORT
#endif

#define TAG "esp_http_server"

#define MAX_URI_HANDLERS 16
#define MAX_REQ_HEADERS 16
#define MAX_RESP_HEADERS 16
#define MAX_HEADER_LEN 128
#define MAX_BODY_LEN 2048

typedef struct {
    char key[MAX_HEADER_LEN];
    char val[MAX_HEADER_LEN];
    bool used;
} http_hdr_t;

typedef struct {
    httpd_uri_t uri_handler;
    bool used;
} server_uri_slot_t;

typedef struct {
    bool active;
    httpd_config_t config;
    server_uri_slot_t handlers[MAX_URI_HANDLERS];
    httpd_err_handler_func_t err_handlers[HTTPD_ERR_CODE_MAX];
} http_server_inst_t;

static http_server_inst_t s_server;

typedef struct {
    http_hdr_t headers[MAX_REQ_HEADERS];
    size_t header_count;
    char query_str[CONFIG_HTTPD_MAX_URI_LEN];
    char body_buf[MAX_BODY_LEN];
    size_t body_len;
    size_t body_read_pos;

    /* Response state */
    char resp_status[32];
    char resp_type[64];
    http_hdr_t resp_headers[MAX_RESP_HEADERS];
    size_t resp_header_count;
    char resp_body[MAX_BODY_LEN];
    size_t resp_body_len;
    int resp_status_code;
} req_aux_t;

static req_aux_t s_curr_aux;
static httpd_req_t s_curr_req;

/* Simulation metrics */
static int s_server_state = 0; /* 0: stopped, 1: running */
static int s_server_port = 80;
static int s_server_req_count = 0;
static int s_server_last_status = 0;
static char s_server_last_uri[128] = {0};
static char s_server_last_resp[512] = {0};
static size_t s_server_total_tx_bytes = 0;

WINK_SIM_EXPORT void sim_http_server_reset(void) {
    memset(&s_server, 0, sizeof(s_server));
    memset(&s_curr_aux, 0, sizeof(s_curr_aux));
    memset(&s_curr_req, 0, sizeof(s_curr_req));
    s_server_state = 0;
    s_server_port = 80;
    s_server_req_count = 0;
    s_server_last_status = 0;
    s_server_last_uri[0] = '\0';
    s_server_last_resp[0] = '\0';
    s_server_total_tx_bytes = 0;
}

void esp_http_server_sim_reset(void) {
    sim_http_server_reset();
}

WINK_SIM_EXPORT int sim_http_server_get_state(void) {
    return s_server_state;
}

WINK_SIM_EXPORT int sim_http_server_get_port(void) {
    return s_server_port;
}

WINK_SIM_EXPORT int sim_http_server_get_request_count(void) {
    return s_server_req_count;
}

WINK_SIM_EXPORT int sim_http_server_get_last_status(void) {
    return s_server_last_status;
}

WINK_SIM_EXPORT const char* sim_http_server_get_last_uri(void) {
    return s_server_last_uri;
}

WINK_SIM_EXPORT const char* sim_http_server_get_last_resp(void) {
    return s_server_last_resp;
}

WINK_SIM_EXPORT int sim_http_server_get_total_tx_bytes(void) {
    return (int)s_server_total_tx_bytes;
}

static bool s_server_stopping = false;

esp_err_t httpd_start(httpd_handle_t *handle, const httpd_config_t *config) {
    if (!handle || !config) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_server.active || s_server_stopping) {
        return ESP_ERR_HTTPD_ALLOC_MEM;
    }
    s_server.active = true;
    s_server.config = *config;
    s_server_state = 1;
    s_server_port = config->server_port ? config->server_port : 80;
    *handle = &s_server;
    ESP_LOGI(TAG, "HTTP Server started on port %d", s_server_port);
    return ESP_OK;
}

esp_err_t httpd_stop(httpd_handle_t handle) {
    if (!handle || handle != &s_server) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_server.active || s_server_stopping) {
        return ESP_ERR_INVALID_STATE;
    }
    s_server_stopping = true;
    s_server.active = false;
    s_server_state = 0;

    for (int i = 0; i < MAX_URI_HANDLERS; i++) {
        s_server.handlers[i].used = false;
        memset(&s_server.handlers[i].uri_handler, 0, sizeof(httpd_uri_t));
    }
    for (int i = 0; i < HTTPD_ERR_CODE_MAX; i++) {
        s_server.err_handlers[i] = NULL;
    }

    memset(&s_curr_aux, 0, sizeof(s_curr_aux));
    memset(&s_curr_req, 0, sizeof(s_curr_req));

    void *user_ctx = s_server.config.global_user_ctx;
    httpd_free_ctx_fn_t user_free = s_server.config.global_user_ctx_free_fn;
    s_server.config.global_user_ctx = NULL;
    s_server.config.global_user_ctx_free_fn = NULL;
    if (user_ctx) {
        if (user_free) {
            user_free(user_ctx);
        } else {
            free(user_ctx);
        }
    }

    void *transport_ctx = s_server.config.global_transport_ctx;
    httpd_free_ctx_fn_t transport_free = s_server.config.global_transport_ctx_free_fn;
    s_server.config.global_transport_ctx = NULL;
    s_server.config.global_transport_ctx_free_fn = NULL;
    if (transport_ctx) {
        if (transport_free) {
            transport_free(transport_ctx);
        } else {
            free(transport_ctx);
        }
    }

    s_server_stopping = false;
    ESP_LOGI(TAG, "HTTP Server stopped");
    return ESP_OK;
}

esp_err_t httpd_register_uri_handler(httpd_handle_t handle, const httpd_uri_t *uri_handler) {
    if (!handle || handle != &s_server || !s_server.active || !uri_handler || !uri_handler->uri || !uri_handler->handler) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Check if already registered */
    for (int i = 0; i < MAX_URI_HANDLERS; i++) {
        if (s_server.handlers[i].used &&
            strcmp(s_server.handlers[i].uri_handler.uri, uri_handler->uri) == 0 &&
            s_server.handlers[i].uri_handler.method == uri_handler->method) {
            return ESP_ERR_HTTPD_HANDLER_EXISTS;
        }
    }

    /* Find empty slot */
    for (int i = 0; i < MAX_URI_HANDLERS; i++) {
        if (!s_server.handlers[i].used) {
            s_server.handlers[i].uri_handler = *uri_handler;
            s_server.handlers[i].used = true;
            return ESP_OK;
        }
    }

    return ESP_ERR_HTTPD_HANDLERS_FULL;
}

esp_err_t httpd_unregister_uri_handler(httpd_handle_t handle, const char *uri, httpd_method_t method) {
    if (!handle || handle != &s_server || !s_server.active || !uri) {
        return ESP_ERR_INVALID_ARG;
    }
    for (int i = 0; i < MAX_URI_HANDLERS; i++) {
        if (s_server.handlers[i].used &&
            strcmp(s_server.handlers[i].uri_handler.uri, uri) == 0 &&
            s_server.handlers[i].uri_handler.method == method) {
            s_server.handlers[i].used = false;
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t httpd_unregister_uri(httpd_handle_t handle, const char *uri) {
    if (!handle || handle != &s_server || !s_server.active || !uri) {
        return ESP_ERR_INVALID_ARG;
    }
    bool found = false;
    for (int i = 0; i < MAX_URI_HANDLERS; i++) {
        if (s_server.handlers[i].used &&
            strcmp(s_server.handlers[i].uri_handler.uri, uri) == 0) {
            s_server.handlers[i].used = false;
            found = true;
        }
    }
    return found ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t httpd_register_err_handler(httpd_handle_t handle,
                                     httpd_err_code_t error,
                                     httpd_err_handler_func_t handler_fn) {
    if (!handle || handle != &s_server || !s_server.active || error >= HTTPD_ERR_CODE_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    s_server.err_handlers[error] = handler_fn;
    return ESP_OK;
}

int httpd_req_recv(httpd_req_t *r, char *buf, size_t buf_len) {
    if (!r || !buf || buf_len == 0) {
        return HTTPD_SOCK_ERR_INVALID;
    }
    if (s_curr_aux.body_read_pos >= s_curr_aux.body_len) {
        return 0;
    }
    size_t remaining = s_curr_aux.body_len - s_curr_aux.body_read_pos;
    size_t to_copy = buf_len < remaining ? buf_len : remaining;
    memcpy(buf, s_curr_aux.body_buf + s_curr_aux.body_read_pos, to_copy);
    s_curr_aux.body_read_pos += to_copy;
    return (int)to_copy;
}

size_t httpd_req_get_hdr_value_len(httpd_req_t *r, const char *field) {
    if (!r || !field) {
        return 0;
    }
    for (size_t i = 0; i < s_curr_aux.header_count; i++) {
        if (s_curr_aux.headers[i].used &&
            strcasecmp(s_curr_aux.headers[i].key, field) == 0) {
            return strlen(s_curr_aux.headers[i].val);
        }
    }
    return 0;
}

esp_err_t httpd_req_get_hdr_value_str(httpd_req_t *r, const char *field, char *val, size_t val_size) {
    if (!r || !field || !val || val_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    for (size_t i = 0; i < s_curr_aux.header_count; i++) {
        if (s_curr_aux.headers[i].used &&
            strcasecmp(s_curr_aux.headers[i].key, field) == 0) {
            size_t len = strlen(s_curr_aux.headers[i].val);
            if (len >= val_size) {
                strncpy(val, s_curr_aux.headers[i].val, val_size - 1);
                val[val_size - 1] = '\0';
                return ESP_ERR_HTTPD_RESULT_TRUNC;
            }
            strcpy(val, s_curr_aux.headers[i].val);
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

size_t httpd_req_get_url_query_len(httpd_req_t *r) {
    if (!r) return 0;
    return strlen(s_curr_aux.query_str);
}

esp_err_t httpd_req_get_url_query_str(httpd_req_t *r, char *buf, size_t buf_len) {
    if (!r || !buf || buf_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t qlen = strlen(s_curr_aux.query_str);
    if (qlen == 0) {
        return ESP_ERR_NOT_FOUND;
    }
    if (qlen >= buf_len) {
        strncpy(buf, s_curr_aux.query_str, buf_len - 1);
        buf[buf_len - 1] = '\0';
        return ESP_ERR_HTTPD_RESULT_TRUNC;
    }
    strcpy(buf, s_curr_aux.query_str);
    return ESP_OK;
}

esp_err_t httpd_query_key_value(const char *qry, const char *key, char *val, size_t val_size) {
    if (!qry || !key || !val || val_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t key_len = strlen(key);
    const char *p = qry;
    while (*p) {
        const char *eq = strchr(p, '=');
        const char *amp = strchr(p, '&');
        if (!eq || (amp && eq > amp)) {
            if (!amp) break;
            p = amp + 1;
            continue;
        }
        size_t cur_klen = (size_t)(eq - p);
        if (cur_klen == key_len && strncmp(p, key, key_len) == 0) {
            const char *val_start = eq + 1;
            size_t val_len = amp ? (size_t)(amp - val_start) : strlen(val_start);
            if (val_len >= val_size) {
                strncpy(val, val_start, val_size - 1);
                val[val_size - 1] = '\0';
                return ESP_ERR_HTTPD_RESULT_TRUNC;
            }
            memcpy(val, val_start, val_len);
            val[val_len] = '\0';
            return ESP_OK;
        }
        if (!amp) break;
        p = amp + 1;
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t httpd_resp_set_status(httpd_req_t *r, const char *status) {
    if (!r || !status) return ESP_ERR_INVALID_ARG;
    strncpy(s_curr_aux.resp_status, status, sizeof(s_curr_aux.resp_status) - 1);
    s_curr_aux.resp_status_code = atoi(status);
    return ESP_OK;
}

esp_err_t httpd_resp_set_type(httpd_req_t *r, const char *type) {
    if (!r || !type) return ESP_ERR_INVALID_ARG;
    strncpy(s_curr_aux.resp_type, type, sizeof(s_curr_aux.resp_type) - 1);
    return ESP_OK;
}

esp_err_t httpd_resp_set_hdr(httpd_req_t *r, const char *field, const char *value) {
    if (!r || !field || !value) return ESP_ERR_INVALID_ARG;
    if (s_curr_aux.resp_header_count >= MAX_RESP_HEADERS) {
        return ESP_ERR_HTTPD_RESP_HDR;
    }
    http_hdr_t *h = &s_curr_aux.resp_headers[s_curr_aux.resp_header_count++];
    strncpy(h->key, field, sizeof(h->key) - 1);
    strncpy(h->val, value, sizeof(h->val) - 1);
    h->used = true;
    return ESP_OK;
}

esp_err_t httpd_resp_send(httpd_req_t *r, const char *buf, ssize_t buf_len) {
    if (!r) return ESP_ERR_INVALID_ARG;
    size_t len = 0;
    if (buf) {
        len = (buf_len == HTTPD_RESP_USE_STRLEN) ? strlen(buf) : (size_t)buf_len;
    }
    if (len >= sizeof(s_curr_aux.resp_body)) {
        len = sizeof(s_curr_aux.resp_body) - 1;
    }
    if (buf && len > 0) {
        memcpy(s_curr_aux.resp_body, buf, len);
    }
    s_curr_aux.resp_body[len] = '\0';
    s_curr_aux.resp_body_len = len;

    int code = s_curr_aux.resp_status_code ? s_curr_aux.resp_status_code : 200;
    s_server_last_status = code;
    strncpy(s_server_last_resp, s_curr_aux.resp_body, sizeof(s_server_last_resp) - 1);
    s_server_total_tx_bytes += len;

    /* Purge request headers per ESP-IDF contract */
    for (size_t i = 0; i < s_curr_aux.header_count; i++) {
        s_curr_aux.headers[i].used = false;
    }
    s_curr_aux.header_count = 0;

    return ESP_OK;
}

esp_err_t httpd_resp_send_chunk(httpd_req_t *r, const char *buf, ssize_t buf_len) {
    if (!r) return ESP_ERR_INVALID_ARG;
    if (!buf || buf_len == 0) {
        /* End of chunked response */
        int code = s_curr_aux.resp_status_code ? s_curr_aux.resp_status_code : 200;
        s_server_last_status = code;
        strncpy(s_server_last_resp, s_curr_aux.resp_body, sizeof(s_server_last_resp) - 1);
        return ESP_OK;
    }
    size_t len = (buf_len == HTTPD_RESP_USE_STRLEN) ? strlen(buf) : (size_t)buf_len;
    size_t available = sizeof(s_curr_aux.resp_body) - 1 - s_curr_aux.resp_body_len;
    if (len > available) len = available;
    memcpy(s_curr_aux.resp_body + s_curr_aux.resp_body_len, buf, len);
    s_curr_aux.resp_body_len += len;
    s_curr_aux.resp_body[s_curr_aux.resp_body_len] = '\0';
    s_server_total_tx_bytes += len;
    return ESP_OK;
}

esp_err_t httpd_resp_send_err(httpd_req_t *req, httpd_err_code_t error, const char *msg) {
    if (!req) return ESP_ERR_INVALID_ARG;
    const char *status_str = HTTPD_500;
    if (error == HTTPD_404_NOT_FOUND) {
        status_str = HTTPD_404;
    } else if (error == HTTPD_408_REQ_TIMEOUT) {
        status_str = HTTPD_408;
    } else if (error == HTTPD_400_BAD_REQUEST) {
        status_str = HTTPD_400;
    } else if (error == HTTPD_401_UNAUTHORIZED) {
        status_str = HTTPD_401;
    }
    httpd_resp_set_status(req, status_str);
    httpd_resp_set_type(req, HTTPD_TYPE_TEXT);
    return httpd_resp_send(req, msg ? msg : status_str, HTTPD_RESP_USE_STRLEN);
}

esp_err_t httpd_resp_send_custom_err(httpd_req_t *req, const char *status, const char *msg) {
    if (!req || !status) return ESP_ERR_INVALID_ARG;
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, HTTPD_TYPE_TEXT);
    return httpd_resp_send(req, msg ? msg : status, HTTPD_RESP_USE_STRLEN);
}

bool httpd_uri_match_wildcard(const char *uri_pattern, const char *uri_to_match, size_t match_upto) {
    if (!uri_pattern || !uri_to_match) return false;
    const size_t pat_len = strlen(uri_pattern);
    if (pat_len > 0 && uri_pattern[pat_len - 1] == '*') {
        size_t prefix_len = pat_len - 1;
        if (match_upto < prefix_len) return false;
        return strncmp(uri_pattern, uri_to_match, prefix_len) == 0;
    }
    if (pat_len != match_upto) return false;
    return strncmp(uri_pattern, uri_to_match, match_upto) == 0;
}

int httpd_req_to_sockfd(httpd_req_t *r) {
    return r ? 1 : -1;
}

esp_err_t httpd_queue_work(httpd_handle_t handle, httpd_work_fn_t work, void *arg) {
    if (!work) return ESP_ERR_INVALID_ARG;
    (void)handle;
    work(arg);
    return ESP_OK;
}

static void ws_set_frame_metadata(httpd_ws_frame_t *pkt, size_t total_len, size_t left_len) {
    pkt->type = HTTPD_WS_TYPE_TEXT;
    pkt->final = true;
    pkt->fragmented = false;
    pkt->len = total_len;
    pkt->left_len = left_len;
}

esp_err_t httpd_ws_recv_frame(httpd_req_t *req, httpd_ws_frame_t *pkt, size_t max_len) {
    if (!req || !pkt) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t total_len = s_curr_aux.body_len;
    size_t pos = s_curr_aux.body_read_pos;
    size_t rem = (total_len > pos) ? (total_len - pos) : 0;

    ws_set_frame_metadata(pkt, total_len, rem);

    if (max_len == 0) {
        return ESP_OK;
    }
    if (max_len < total_len) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (rem == 0) {
        return ESP_OK;
    }
    if (!pkt->payload) {
        return ESP_FAIL;
    }
    memcpy(pkt->payload, s_curr_aux.body_buf + pos, rem);
    s_curr_aux.body_read_pos = pos + rem;
    pkt->left_len = 0;
    return ESP_OK;
}

esp_err_t httpd_ws_recv_frame_part(httpd_req_t *req, httpd_ws_frame_t *pkt, size_t max_len) {
    if (!req || !pkt) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t total_len = s_curr_aux.body_len;
    size_t pos = s_curr_aux.body_read_pos;
    size_t rem = (total_len > pos) ? (total_len - pos) : 0;

    ws_set_frame_metadata(pkt, total_len, rem);

    if (max_len == 0) {
        return ESP_OK;
    }
    if (rem == 0) {
        return ESP_OK;
    }
    if (!pkt->payload) {
        return ESP_FAIL;
    }
    size_t to_copy = (rem < max_len) ? rem : max_len;
    memcpy(pkt->payload, s_curr_aux.body_buf + pos, to_copy);
    s_curr_aux.body_read_pos = pos + to_copy;
    pkt->left_len = rem - to_copy;
    return ESP_OK;
}

esp_err_t httpd_ws_send_frame(httpd_req_t *req, httpd_ws_frame_t *pkt) {
    if (!req || !pkt) return ESP_ERR_INVALID_ARG;
    if (pkt->payload && pkt->len > 0) {
        if (pkt->fragmented) {
            if (pkt->type != HTTPD_WS_TYPE_CONTINUE && s_curr_aux.resp_body_len == 0) {
                s_server_last_resp[0] = '\0';
            }
            size_t curr_len = strlen(s_server_last_resp);
            size_t avail = sizeof(s_server_last_resp) - 1 - curr_len;
            size_t to_copy = pkt->len < avail ? pkt->len : avail;
            memcpy(s_server_last_resp + curr_len, pkt->payload, to_copy);
            s_server_last_resp[curr_len + to_copy] = '\0';
            s_curr_aux.resp_body_len += to_copy;
        } else {
            size_t to_copy = pkt->len < sizeof(s_server_last_resp) - 1 ? pkt->len : sizeof(s_server_last_resp) - 1;
            memcpy(s_server_last_resp, pkt->payload, to_copy);
            s_server_last_resp[to_copy] = '\0';
            s_curr_aux.resp_body_len = to_copy;
        }
        s_server_total_tx_bytes += pkt->len;
    }
    s_server_last_status = 200;
    return ESP_OK;
}

esp_err_t httpd_ws_send_frame_async(httpd_handle_t hd, int fd, httpd_ws_frame_t *frame) {
    if (!frame) return ESP_ERR_INVALID_ARG;
    (void)hd;
    (void)fd;
    if (frame->payload && frame->len > 0) {
        size_t to_copy = frame->len < sizeof(s_server_last_resp) - 1 ? frame->len : sizeof(s_server_last_resp) - 1;
        memcpy(s_server_last_resp, frame->payload, to_copy);
        s_server_last_resp[to_copy] = '\0';
        s_server_total_tx_bytes += frame->len;
    }
    s_server_last_status = 200;
    return ESP_OK;
}

/* Dispatch a simulated incoming request to the HTTP server */
static int dispatch_request_internal(
    const char *method_str,
    const char *raw_uri,
    const char *body,
    size_t body_len,
    const char *host_header
) {
    if (!s_server.active) {
        ESP_LOGE(TAG, "Cannot dispatch request: server not running");
        return -1;
    }
    if (!method_str || !raw_uri) {
        return -1;
    }

    memset(&s_curr_aux, 0, sizeof(s_curr_aux));
    memset(&s_curr_req, 0, sizeof(s_curr_req));
    s_curr_req.aux = &s_curr_aux;

    /* Full URI preserved in s_curr_req.uri for query access */
    strncpy(s_curr_req.uri, raw_uri, sizeof(s_curr_req.uri) - 1);
    s_curr_req.uri[sizeof(s_curr_req.uri) - 1] = '\0';

    char path_buf[CONFIG_HTTPD_MAX_URI_LEN + 1];
    const char *qmark = strchr(raw_uri, '?');
    if (qmark) {
        size_t path_len = (size_t)(qmark - raw_uri);
        if (path_len >= sizeof(path_buf)) path_len = sizeof(path_buf) - 1;
        memcpy(path_buf, raw_uri, path_len);
        path_buf[path_len] = '\0';
        strncpy(s_curr_aux.query_str, qmark + 1, sizeof(s_curr_aux.query_str) - 1);
    } else {
        strncpy(path_buf, raw_uri, sizeof(path_buf) - 1);
        path_buf[sizeof(path_buf) - 1] = '\0';
        s_curr_aux.query_str[0] = '\0';
    }

    /* Populate Host header if provided */
    if (host_header && host_header[0] != '\0') {
        strncpy(s_curr_aux.headers[0].key, "Host", MAX_HEADER_LEN - 1);
        strncpy(s_curr_aux.headers[0].val, host_header, MAX_HEADER_LEN - 1);
        s_curr_aux.headers[0].used = true;
        s_curr_aux.header_count = 1;
    }

    /* Map method: GET, POST, PUT, DELETE, PATCH, HEAD supported */
    int method = -1;
    if (strcasecmp(method_str, "GET") == 0) method = HTTP_GET;
    else if (strcasecmp(method_str, "POST") == 0) method = HTTP_POST;
    else if (strcasecmp(method_str, "PUT") == 0) method = HTTP_PUT;
    else if (strcasecmp(method_str, "DELETE") == 0) method = HTTP_DELETE;
    else if (strcasecmp(method_str, "PATCH") == 0) method = HTTP_PATCH;
    else if (strcasecmp(method_str, "HEAD") == 0) method = HTTP_HEAD;
    else {
        /* Unrecognized or unimplemented method -> 501 */
        s_server_last_status = 501;
        if (s_server.err_handlers[HTTPD_501_METHOD_NOT_IMPLEMENTED]) {
            s_server.err_handlers[HTTPD_501_METHOD_NOT_IMPLEMENTED](&s_curr_req, HTTPD_501_METHOD_NOT_IMPLEMENTED);
        } else {
            httpd_resp_send_custom_err(&s_curr_req, HTTPD_501, "Not Implemented");
        }
        return 501;
    }
    s_curr_req.method = method;

    /* Body setup */
    if (body && body_len > 0) {
        size_t blen = body_len < sizeof(s_curr_aux.body_buf) ? body_len : sizeof(s_curr_aux.body_buf);
        memcpy(s_curr_aux.body_buf, body, blen);
        s_curr_aux.body_len = blen;
        s_curr_req.content_len = blen;
    }

    s_curr_req.handle = &s_server;
    s_server_req_count++;
    strncpy(s_server_last_uri, path_buf, sizeof(s_server_last_uri) - 1);

    /* Match registered handler */
    server_uri_slot_t *matched = NULL;
    bool uri_exists = false;
    for (int i = 0; i < MAX_URI_HANDLERS; i++) {
        if (s_server.handlers[i].used) {
            httpd_uri_t *u = &s_server.handlers[i].uri_handler;
            bool is_match = false;
            if (s_server.config.uri_match_fn) {
                typedef bool (*httpd_match_fn_t)(const char *, const char *, size_t);
                is_match = ((httpd_match_fn_t)s_server.config.uri_match_fn)(
                    u->uri, path_buf, strlen(path_buf)
                );
            } else {
                is_match = (strcmp(u->uri, path_buf) == 0);
            }
            if (is_match) {
                uri_exists = true;
                if (u->method == HTTP_ANY || (int)u->method == s_curr_req.method) {
                    matched = &s_server.handlers[i];
                    break;
                }
            }
        }
    }

    if (matched) {
        s_curr_req.user_ctx = matched->uri_handler.user_ctx;
        if (matched->uri_handler.is_websocket) {
            if (matched->uri_handler.ws_pre_handshake_cb) {
                esp_err_t cb_ret = matched->uri_handler.ws_pre_handshake_cb(&s_curr_req);
                if (cb_ret != ESP_OK) {
                    ESP_LOGW(TAG, "WebSocket pre-handshake callback rejected connection");
                    s_server_last_status = 403;
                    httpd_resp_send_custom_err(&s_curr_req, HTTPD_403, "Handshake rejected");
                    return 403;
                }
            }
            if (matched->uri_handler.ws_post_handshake_cb) {
                matched->uri_handler.ws_post_handshake_cb(&s_curr_req);
            }
        }
        esp_err_t rc = matched->uri_handler.handler(&s_curr_req);
        if (rc != ESP_OK) {
            ESP_LOGW(TAG, "URI handler returned %d", rc);
        }
        int status = s_server_last_status > 0 ? s_server_last_status : 200;
        return rc == ESP_OK ? status : -1;
    }

    if (uri_exists) {
        /* Method not allowed -> 405 */
        s_server_last_status = 405;
        if (s_server.err_handlers[HTTPD_405_METHOD_NOT_ALLOWED]) {
            s_server.err_handlers[HTTPD_405_METHOD_NOT_ALLOWED](&s_curr_req, HTTPD_405_METHOD_NOT_ALLOWED);
        } else {
            httpd_resp_send_custom_err(&s_curr_req, HTTPD_405, "Method Not Allowed");
        }
        return 405;
    }

    /* No handler matched -> custom 404 handler or default 404 */
    s_server_last_status = 404;
    if (s_server.err_handlers[HTTPD_404_NOT_FOUND]) {
        s_server.err_handlers[HTTPD_404_NOT_FOUND](&s_curr_req, HTTPD_404_NOT_FOUND);
    } else {
        httpd_resp_send_404(&s_curr_req);
    }
    return 404;
}

WINK_SIM_EXPORT int sim_http_server_dispatch_request(
    const char *method_str,
    const char *raw_uri,
    const char *body,
    size_t body_len
) {
    return dispatch_request_internal(method_str, raw_uri, body, body_len, NULL);
}

WINK_SIM_EXPORT int sim_http_server_dispatch_request_with_host(
    const char *method_str,
    const char *raw_uri,
    const char *body,
    size_t body_len,
    const char *host_header
) {
    return dispatch_request_internal(method_str, raw_uri, body, body_len, host_header);
}

static bool json_extract_str(const char *json, const char *key, char *out, size_t maxlen) {
    if (!json || !key || !out || maxlen == 0) return false;
    char search_buf[64];
    snprintf(search_buf, sizeof(search_buf), "\"%s\"", key);
    const char *pos = strstr(json, search_buf);
    if (!pos) return false;
    pos += strlen(search_buf);
    while (*pos == ' ' || *pos == '\t' || *pos == '\r' || *pos == '\n' || *pos == ':') pos++;
    if (*pos != '\"') return false;
    pos++;
    size_t i = 0;
    while (*pos && *pos != '\"' && i < maxlen - 1) {
        if (*pos == '\\' && *(pos + 1)) pos++;
        out[i++] = *pos++;
    }
    out[i] = '\0';
    return true;
}

/* Static parsing buffers to keep stack frame small (< 128 bytes) */
static char s_inject_buf[1024];
static char s_inject_method[16];
static char s_inject_uri[256];
static char s_inject_body[512];
static char s_inject_host[64];

/* Parse JSON array of requests from INJECT_NET_FIXTURE and dispatch */
WINK_SIM_EXPORT int sim_http_server_inject_json(const char *json_str) {
    if (!json_str) return 400;
    while (*json_str == ' ' || *json_str == '\t' || *json_str == '\r' || *json_str == '\n') json_str++;
    if (*json_str != '{' && *json_str != '[') return 400;

    const char *req_start = strstr(json_str, "\"requests\"");
    if (!req_start) {
        req_start = strstr(json_str, "\"client_requests\"");
    }
    if (!req_start) {
        /* Single request or route object */
        strncpy(s_inject_method, "GET", sizeof(s_inject_method));
        strncpy(s_inject_uri, "/", sizeof(s_inject_uri));
        s_inject_body[0] = '\0';
        s_inject_host[0] = '\0';
        json_extract_str(json_str, "method", s_inject_method, sizeof(s_inject_method));
        json_extract_str(json_str, "host", s_inject_host, sizeof(s_inject_host));
        if (json_extract_str(json_str, "uri", s_inject_uri, sizeof(s_inject_uri)) ||
            json_extract_str(json_str, "url", s_inject_uri, sizeof(s_inject_uri))) {
            json_extract_str(json_str, "body", s_inject_body, sizeof(s_inject_body));
            return dispatch_request_internal(s_inject_method, s_inject_uri, s_inject_body, strlen(s_inject_body), s_inject_host[0] ? s_inject_host : NULL);
        }
        return 400;
    }

    const char *p = strchr(req_start, '[');
    if (!p) return 400;
    p++;

    int dispatched = 0;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ',') p++;
        if (*p == ']' || *p == '\0') break;
        if (*p == '{') {
            const char *obj_start = p;
            int depth = 1;
            p++;
            while (*p && depth > 0) {
                if (*p == '{') depth++;
                else if (*p == '}') depth--;
                p++;
            }
            const char *obj_end = p;
            size_t obj_len = (size_t)(obj_end - obj_start);
            if (obj_len < sizeof(s_inject_buf)) {
                memcpy(s_inject_buf, obj_start, obj_len);
                s_inject_buf[obj_len] = '\0';
                strncpy(s_inject_method, "GET", sizeof(s_inject_method));
                strncpy(s_inject_uri, "/", sizeof(s_inject_uri));
                s_inject_body[0] = '\0';
                s_inject_host[0] = '\0';
                json_extract_str(s_inject_buf, "method", s_inject_method, sizeof(s_inject_method));
                json_extract_str(s_inject_buf, "host", s_inject_host, sizeof(s_inject_host));
                if (json_extract_str(s_inject_buf, "uri", s_inject_uri, sizeof(s_inject_uri)) ||
                    json_extract_str(s_inject_buf, "url", s_inject_uri, sizeof(s_inject_uri))) {
                    json_extract_str(s_inject_buf, "body", s_inject_body, sizeof(s_inject_body));
                    dispatch_request_internal(s_inject_method, s_inject_uri, s_inject_body, strlen(s_inject_body), s_inject_host[0] ? s_inject_host : NULL);
                    dispatched++;
                }
            }
        } else {
            p++;
        }
    }
    return dispatched;
}

WINK_SIM_EXPORT int sim_http_server_inject_raw_request(const char *json_str) {
    return sim_http_server_inject_json(json_str);
}
