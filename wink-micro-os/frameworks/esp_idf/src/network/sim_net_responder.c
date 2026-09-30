/* SPDX-License-Identifier: LGPL-3.0-only */
#include "sim_net_responder.h"
#include <string.h>
#include <strings.h>

static sim_http_route_t s_routes[SIM_HTTP_MAX_ROUTES];

void sim_http_responder_reset(void) {
    memset(s_routes, 0, sizeof(s_routes));
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
            s_routes[i].is_active = true;
            return ESP_OK;
        }
    }

    /* 寻找空闲槽位 */
    for (size_t i = 0; i < SIM_HTTP_MAX_ROUTES; i++) {
        if (!s_routes[i].is_active) {
            s_routes[i] = *route;
            s_routes[i].is_active = true;
            return ESP_OK;
        }
    }

    return ESP_ERR_NO_MEM;
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

        /* 前缀匹配 URL */
        size_t prefix_len = strlen(s_routes[i].url_prefix);
        if (prefix_len > 0 && strncmp(url, s_routes[i].url_prefix, prefix_len) == 0) {
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
