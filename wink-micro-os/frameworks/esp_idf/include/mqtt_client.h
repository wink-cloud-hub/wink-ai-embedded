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

ESP_EVENT_DECLARE_BASE(MQTT_EVENTS);

typedef struct esp_mqtt_client* esp_mqtt_client_handle_t;

typedef enum {
    MQTT_EVENT_ANY = -1,
    MQTT_EVENT_ERROR = 0,
    MQTT_EVENT_CONNECTED,
    MQTT_EVENT_DISCONNECTED,
    MQTT_EVENT_SUBSCRIBED,
    MQTT_EVENT_UNSUBSCRIBED,
    MQTT_EVENT_PUBLISHED,
    MQTT_EVENT_DATA,
    MQTT_EVENT_BEFORE_CONNECT,
    MQTT_EVENT_DELETED,
    MQTT_EVENT_MAX
} esp_mqtt_event_id_t;

typedef enum {
    MQTT_ERROR_TYPE_NONE = 0,
    MQTT_ERROR_TYPE_TCP_TRANSPORT,
    MQTT_ERROR_TYPE_CONNECTION_REFUSED,
} esp_mqtt_error_type_t;

typedef struct {
    esp_mqtt_error_type_t error_type;
    int esp_tls_last_esp_err;
    int esp_tls_stack_err;
    int esp_transport_sock_errno;
} esp_mqtt_error_codes_t;

typedef struct {
    esp_mqtt_event_id_t event_id;
    esp_mqtt_client_handle_t client;
    void *user_context;
    char *data;
    int data_len;
    int total_data_len;
    int current_data_offset;
    char *topic;
    int topic_len;
    int msg_id;
    bool retain;
    int qos;
    bool dup;
    esp_mqtt_error_codes_t *error_handle;
} esp_mqtt_event_t;

typedef esp_mqtt_event_t* esp_mqtt_event_handle_t;

/* ESP-IDF v5/v6 规范嵌套 broker 结构体配置 */
typedef struct {
    struct {
        struct {
            const char *uri;
            const char *hostname;
            const char *path;
            uint32_t port;
        } address;
    } broker;
    struct {
        struct {
            const char *username;
            const char *password;
            const char *client_id;
        } credentials;
    } credentials;
    struct {
        int buffer_size;
        int out_buffer_size;
    } network;
    /* v4 兼容顶层字段 */
    const char *uri;
    const char *client_id;
    esp_event_handler_t event_handle;
    void *user_context;
} esp_mqtt_client_config_t;

/* 标准客户端生命周期与操作 API */
esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t *config);
esp_err_t esp_mqtt_client_set_uri(esp_mqtt_client_handle_t client, const char *uri);
esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t client);
esp_err_t esp_mqtt_client_stop(esp_mqtt_client_handle_t client);
esp_err_t esp_mqtt_client_reconnect(esp_mqtt_client_handle_t client);
esp_err_t esp_mqtt_client_disconnect(esp_mqtt_client_handle_t client);
esp_err_t esp_mqtt_client_destroy(esp_mqtt_client_handle_t client);
int esp_mqtt_client_publish(esp_mqtt_client_handle_t client, const char *topic, const char *data, int len, int qos, int retain);
int esp_mqtt_client_subscribe(esp_mqtt_client_handle_t client, const char *topic, int qos);
int esp_mqtt_client_unsubscribe(esp_mqtt_client_handle_t client, const char *topic);
esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t client, esp_mqtt_event_id_t event, esp_event_handler_t event_handler, void *event_handler_arg);

/* Wink 仿真与 UniSim 专用接口（导出符号保护） */
typedef void (*esp_mqtt_sim_publish_hook_t)(const char *topic, const char *data, int len);

WINK_SIM_EXPORT void esp_mqtt_sim_reset(void);
WINK_SIM_EXPORT bool esp_mqtt_sim_is_connected(esp_mqtt_client_handle_t client);
WINK_SIM_EXPORT void esp_mqtt_sim_set_network_ready(bool ready);
WINK_SIM_EXPORT int esp_mqtt_sim_inject_message(const char *topic, const char *data, int data_len);
WINK_SIM_EXPORT int esp_mqtt_sim_get_last_published(char *out_topic, size_t topic_max, char *out_data, size_t data_max);
WINK_SIM_EXPORT void esp_mqtt_sim_set_publish_hook(esp_mqtt_sim_publish_hook_t hook);

#ifdef __cplusplus
}
#endif
