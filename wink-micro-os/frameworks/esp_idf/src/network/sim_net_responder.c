/* SPDX-License-Identifier: LGPL-3.0-only */
#include "sim_net_responder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static sim_http_route_t s_routes[SIM_HTTP_MAX_ROUTES];
static int s_last_status_code = 0;
static int s_total_rx_bytes = 0;
static int s_request_count = 0;

void sim_http_responder_reset(void) {
    memset(s_routes, 0, sizeof(s_routes));
    s_last_status_code = 0;
    s_total_rx_bytes = 0;
    s_request_count = 0;
}

void sim_http_record_request(int status_code, size_t rx_bytes) {
    s_last_status_code = status_code;
    s_total_rx_bytes += (int)rx_bytes;
    s_request_count++;
}

int sim_http_get_last_status_code(void) {
    return s_last_status_code;
}

int sim_http_get_total_rx_bytes(void) {
    return s_total_rx_bytes;
}

int sim_http_get_request_count(void) {
    return s_request_count;
}

esp_err_t sim_http_responder_register_route(const sim_http_route_t *route) {
    if (!route || route->url_prefix[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    /* 检查是否已存在相同前缀与方法的路由，存在则覆盖更新 */
    for (size_t i = 0; i < SIM_HTTP_MAX_ROUTES; i++) {
        if (s_routes[i].is_active &&
            strcmp(s_routes[i].url_prefix, route->url_prefix) == 0 &&
            strcasecmp(s_routes[i].method, route->method) == 0) {
            s_routes[i] = *route;
            if (s_routes[i].resp.body_buf[0] != '\0') {
                s_routes[i].resp.body_data = (const uint8_t*)s_routes[i].resp.body_buf;
                s_routes[i].resp.body_len = strlen(s_routes[i].resp.body_buf);
            }
            s_routes[i].is_active = true;
            return ESP_OK;
        }
    }

    /* 寻找空闲槽位 */
    for (size_t i = 0; i < SIM_HTTP_MAX_ROUTES; i++) {
        if (!s_routes[i].is_active) {
            s_routes[i] = *route;
            if (s_routes[i].resp.body_buf[0] != '\0') {
                s_routes[i].resp.body_data = (const uint8_t*)s_routes[i].resp.body_buf;
                s_routes[i].resp.body_len = strlen(s_routes[i].resp.body_buf);
            }
            s_routes[i].is_active = true;
            return ESP_OK;
        }
    }

    return ESP_ERR_NO_MEM;
}

static bool url_prefix_matches(const char *url, const char *prefix) {
    if (!url || !prefix || prefix[0] == '\0') {
        return false;
    }
    size_t prefix_len = strlen(prefix);

    /* 1. Direct prefix match */
    if (strncmp(url, prefix, prefix_len) == 0) {
        return true;
    }

    /* 2. Path-only prefix (e.g. "/get") matching against full URL path */
    if (prefix[0] == '/') {
        const char *scheme_end = strstr(url, "://");
        const char *path = scheme_end ? strchr(scheme_end + 3, '/') : strchr(url, '/');
        if (path && strncmp(path, prefix, prefix_len) == 0) {
            return true;
        }
    }

    /* 3. Handle http://host:80/path matching against http://host/path */
    const char *found_port = strstr(url, ":80");
    if (found_port) {
        char stripped[SIM_HTTP_URL_MAX];
        size_t before_len = (size_t)(found_port - url);
        const char *after_port = found_port + 3;
        if (before_len + strlen(after_port) < sizeof(stripped)) {
            memcpy(stripped, url, before_len);
            strcpy(stripped + before_len, after_port);
            if (strncmp(stripped, prefix, prefix_len) == 0) {
                return true;
            }
        }
    }

    return false;
}

const sim_http_response_t *sim_http_responder_match(const char *method, const char *url) {
    if (!url) {
        return NULL;
    }

    for (size_t i = 0; i < SIM_HTTP_MAX_ROUTES; i++) {
        if (!s_routes[i].is_active) {
            continue;
        }

        /* 校验 HTTP Method（若路由指定了特定方法且入参非空） */
        if (method && s_routes[i].method[0] != '\0') {
            if (strcasecmp(method, s_routes[i].method) != 0) {
                continue;
            }
        }

        /* 综合前缀匹配 URL */
        if (url_prefix_matches(url, s_routes[i].url_prefix)) {
            return &s_routes[i].resp;
        }
    }

    /* 严格遵循 Fail-Loud：无匹配路由直接返回 NULL，由上层触发 ESP_ERR_HTTP_CONNECT */
    return NULL;
}

size_t sim_http_responder_route_count(void) {
    size_t count = 0;
    for (size_t i = 0; i < SIM_HTTP_MAX_ROUTES; i++) {
        if (s_routes[i].is_active) {
            count++;
        }
    }
    return count;
}

/* ── JSON 反序列化与注入 ─────────────────────────────────────────────── */

static bool json_get_str(const char *json, const char *key, char *out, size_t maxlen) {
    if (!json || !key || !out || maxlen == 0) return false;
    char search_buf[64];
    snprintf(search_buf, sizeof(search_buf), "\"%s\"", key);
    const char *pos = strstr(json, search_buf);
    if (!pos) return false;
    pos += strlen(search_buf);
    while (*pos == ' ' || *pos == '\t' || *pos == '\r' || *pos == '\n' || *pos == ':') {
        pos++;
    }
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

static bool json_get_int(const char *json, const char *key, int *out) {
    if (!json || !key || !out) return false;
    char search_buf[64];
    snprintf(search_buf, sizeof(search_buf), "\"%s\"", key);
    const char *pos = strstr(json, search_buf);
    if (!pos) return false;
    pos += strlen(search_buf);
    while (*pos == ' ' || *pos == '\t' || *pos == '\r' || *pos == '\n' || *pos == ':') {
        pos++;
    }
    char *endptr = NULL;
    long val = strtol(pos, &endptr, 10);
    if (endptr == pos) return false;
    *out = (int)val;
    return true;
}

static int parse_single_route_obj(const char *start, const char *end, sim_http_route_t *route) {
    static char s_parse_buf[2048];
    size_t len = (size_t)(end - start);
    if (len >= sizeof(s_parse_buf)) len = sizeof(s_parse_buf) - 1;
    memcpy(s_parse_buf, start, len);
    s_parse_buf[len] = '\0';

    memset(route, 0, sizeof(*route));
    if (!json_get_str(s_parse_buf, "url_prefix", route->url_prefix, sizeof(route->url_prefix))) {
        if (!json_get_str(s_parse_buf, "url", route->url_prefix, sizeof(route->url_prefix))) {
            return -1;
        }
    }

    if (!json_get_str(s_parse_buf, "method", route->method, sizeof(route->method))) {
        strcpy(route->method, "GET");
    }

    int status = 200;
    if (!json_get_int(s_parse_buf, "status_code", &status)) {
        json_get_int(s_parse_buf, "status", &status);
    }
    route->resp.status_code = status > 0 ? status : 200;

    /* 提取响应体 */
    const char *body_pos = strstr(s_parse_buf, "\"body\"");
    if (body_pos) {
        body_pos += 6;
        while (*body_pos == ' ' || *body_pos == '\t' || *body_pos == '\r' || *body_pos == '\n' || *body_pos == ':') {
            body_pos++;
        }
        if (*body_pos == '\"') {
            json_get_str(s_parse_buf, "body", route->resp.body_buf, sizeof(route->resp.body_buf));
        } else if (*body_pos == '{' || *body_pos == '[') {
            const char *b_start = body_pos;
            int depth = 0;
            const char *p = b_start;
            while (*p) {
                if (*p == '{' || *p == '[') depth++;
                else if (*p == '}' || *p == ']') {
                    depth--;
                    if (depth == 0) { p++; break; }
                }
                p++;
            }
            size_t blen = (size_t)(p - b_start);
            if (blen >= sizeof(route->resp.body_buf)) blen = sizeof(route->resp.body_buf) - 1;
            memcpy(route->resp.body_buf, b_start, blen);
            route->resp.body_buf[blen] = '\0';
        }
    }

    route->resp.body_len = strlen(route->resp.body_buf);
    route->resp.body_data = (const uint8_t*)route->resp.body_buf;

    /* 默认头 */
    strcpy(route->resp.headers[0].key, "Content-Type");
    strcpy(route->resp.headers[0].value, "application/json");
    route->resp.header_count = 1;

    return 0;
}

int sim_http_responder_inject_json(const char *json_str) {
    if (!json_str) return -1;

    const char *routes_start = strstr(json_str, "\"routes\"");
    if (routes_start) {
        const char *p = strchr(routes_start, '[');
        if (!p) return -1;
        p++;

        size_t count = 0;
        while (*p && count < SIM_HTTP_MAX_ROUTES) {
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
                static sim_http_route_t s_inject_route;
                if (parse_single_route_obj(obj_start, obj_end, &s_inject_route) == 0) {
                    sim_http_responder_register_route(&s_inject_route);
                    count++;
                }
            } else {
                p++;
            }
        }
        return count > 0 ? 0 : -1;
    } else {
        static sim_http_route_t s_inject_route;
        if (parse_single_route_obj(json_str, json_str + strlen(json_str), &s_inject_route) == 0) {
            return sim_http_responder_register_route(&s_inject_route) == ESP_OK ? 0 : -1;
        }
        return -1;
    }
}
