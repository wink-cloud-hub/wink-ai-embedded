/* SPDX-License-Identifier: LGPL-3.0-only */
#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos_sync.h"
#include "sim_network_broker.h"
#include "esp_netif.h"
#include <string.h>

#if defined(__EMSCRIPTEN__)
#  include <emscripten.h>
#  define WINK_SIM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#  define WINK_SIM_EXPORT
#endif

#define TAG "ESP_MQTT"

#define MAX_MQTT_CLIENTS    2
#define MAX_SUBSCRIPTIONS   8
#define MAX_TOPIC_LEN       128
#define MAX_DATA_LEN        512

ESP_EVENT_DEFINE_BASE(MQTT_EVENTS);

typedef struct {
    bool used;
    char topic_pattern[MAX_TOPIC_LEN];
    int qos;
    esp_mqtt_client_handle_t client;
} mqtt_subscription_t;

struct esp_mqtt_client {
    bool initialized;
    bool started;
    bool connected;
    uint32_t client_slot;
    uint32_t token;
    uint32_t work_item_id;
    esp_mqtt_client_config_t config;
    esp_netif_t *bound_netif;
    esp_event_handler_t event_handler;
    void *event_handler_arg;
    esp_mqtt_event_id_t registered_event;
    char rx_topic[MAX_TOPIC_LEN];
    char rx_data[MAX_DATA_LEN];
    char tx_topic[MAX_TOPIC_LEN];
    char tx_data[MAX_DATA_LEN];
    esp_mqtt_error_codes_t error_codes;
};

static struct esp_mqtt_client s_clients[MAX_MQTT_CLIENTS];
static mqtt_subscription_t s_subscriptions[MAX_SUBSCRIPTIONS];

static char s_last_topic[MAX_TOPIC_LEN];
static char s_last_data[MAX_DATA_LEN];
static int s_last_data_len = 0;
static int s_last_msg_id = 0;
static int s_next_msg_id = 1;

extern esp_netif_t* esp_netif_get_handle_sta(void);

static bool client_matches_netif(const struct esp_mqtt_client *client, const esp_netif_t *netif) {
    if (!client || !client->initialized) {
        return false;
    }
    esp_netif_t *target = client->bound_netif ? client->bound_netif : esp_netif_get_handle_sta();
    return (target == netif);
}

static uint32_t s_global_mqtt_token = 1000;
static esp_mqtt_sim_publish_hook_t s_publish_hook = NULL;
static bool s_mqtt_core_handler_registered = false;

typedef struct {
    uint32_t client_token;
    uint8_t client_slot;
    esp_mqtt_event_id_t event_id;
    int msg_id;
    bool dup;
    bool retain;
    int qos;
    int topic_len;
    int data_len;
    int current_data_offset;
    int total_data_len;
    void *user_context;
    esp_event_handler_t event_handler;
    void *event_handler_arg;
    esp_mqtt_event_id_t registered_event;
    esp_event_handler_t config_event_handle;
    char topic[MAX_TOPIC_LEN];
    char data[MAX_DATA_LEN];
    esp_mqtt_error_codes_t error_codes;
} mqtt_event_envelope_t;

static void mqtt_core_event_pump_handler(void *arg, esp_event_base_t base, int32_t event_id, void *data) {
    (void)arg;
    (void)base;
    if (!data) {
        return;
    }
    mqtt_event_envelope_t *env = (mqtt_event_envelope_t *)data;
    if (env->client_slot >= MAX_MQTT_CLIENTS) {
        return;
    }
    struct esp_mqtt_client *client = &s_clients[env->client_slot];
    /* 核对代际 token 与初始化状态，防止已被 destroy 或复用换代（DELETED 善后事件除外） */
    if (env->event_id != MQTT_EVENT_DELETED) {
        if (!client->initialized || client->token != env->client_token) {
            return;
        }
    }

    esp_mqtt_event_t evt;
    memset(&evt, 0, sizeof(evt));
    evt.event_id = env->event_id;
    evt.client = client;
    evt.user_context = env->user_context;
    evt.msg_id = env->msg_id;
    evt.dup = env->dup;
    evt.retain = env->retain;
    evt.qos = env->qos;
    evt.topic = (env->topic_len > 0) ? env->topic : NULL;
    evt.topic_len = env->topic_len;
    evt.data = (env->data_len > 0) ? env->data : NULL;
    evt.data_len = env->data_len;
    evt.total_data_len = env->total_data_len;
    evt.current_data_offset = env->current_data_offset;
    if (env->event_id == MQTT_EVENT_ERROR) {
        evt.error_handle = &env->error_codes;
    }

    /* 严格按 D2 契约在事件泵上下文按顺序执行：
     * 1. 客户端通过 esp_mqtt_client_register_event 注册的回调
     */
    if (env->event_handler) {
        if (env->registered_event == MQTT_EVENT_ANY ||
            (int)env->registered_event == (int)ESP_EVENT_ANY_ID ||
            env->registered_event == (esp_mqtt_event_id_t)event_id) {
            env->event_handler(env->event_handler_arg, MQTT_EVENTS, event_id, &evt);
        }
    }

    /* 2. 兼容配置通过 config.event_handle 注册的回调 */
    if (env->config_event_handle) {
        env->config_event_handle(env->user_context, MQTT_EVENTS, event_id, &evt);
    }
}

static void ensure_mqtt_core_handler_registered(void) {
    if (!s_mqtt_core_handler_registered) {
        (void)esp_event_handler_register(MQTT_EVENTS, ESP_EVENT_ANY_ID, mqtt_core_event_pump_handler, NULL);
        s_mqtt_core_handler_registered = true;
    }
}

static void dispatch_event(esp_mqtt_client_handle_t client, esp_mqtt_event_id_t event_id, esp_mqtt_event_t *event) {
    if (!client || !event) {
        return;
    }
    ensure_mqtt_core_handler_registered();

    mqtt_event_envelope_t env;
    memset(&env, 0, sizeof(env));
    env.client_token = client->token;
    env.client_slot = (uint8_t)(client - s_clients);
    env.event_id = event_id;
    env.user_context = client->config.user_context;
    env.event_handler = client->event_handler;
    env.event_handler_arg = client->event_handler_arg;
    env.registered_event = client->registered_event;
    env.config_event_handle = client->config.event_handle;
    env.msg_id = event->msg_id;
    env.dup = event->dup;
    env.retain = event->retain;
    env.qos = event->qos;
    env.topic_len = event->topic_len;
    env.data_len = event->data_len;
    env.current_data_offset = event->current_data_offset;
    env.total_data_len = event->total_data_len;
    if (event->error_handle) {
        env.error_codes = *event->error_handle;
    }
    if (event->topic && event->topic_len > 0) {
        size_t cplen = (size_t)event->topic_len < (MAX_TOPIC_LEN - 1) ? (size_t)event->topic_len : (MAX_TOPIC_LEN - 1);
        memcpy(env.topic, event->topic, cplen);
        env.topic[cplen] = '\0';
        env.topic_len = (int)cplen;
    }
    if (event->data && event->data_len > 0) {
        size_t cplen = (size_t)event->data_len < MAX_DATA_LEN ? (size_t)event->data_len : MAX_DATA_LEN;
        memcpy(env.data, event->data, cplen);
        env.data_len = (int)cplen;
    }

    /* 异步投递进默认事件队列，绝不在调用者栈上同步触发用户回调 */
    (void)esp_event_post(MQTT_EVENTS, (int32_t)event_id, &env, sizeof(env), 0);
}

static void mqtt_connect_work_cb(void *arg, uint32_t work_token);

static void on_network_broker_state_changed(esp_netif_t *netif, sim_netif_event_t event, void *user_ctx) {
    struct esp_mqtt_client *c = (struct esp_mqtt_client *)user_ctx;
    if (!c || !c->initialized) {
        return;
    }
    if (!client_matches_netif(c, netif)) {
        return;
    }
    bool ready = (event == SIM_NETIF_EVT_UP);
    if (!ready) {
        if (c->started && c->connected) {
            c->connected = false;
            c->error_codes.error_type = MQTT_ERROR_TYPE_TCP_TRANSPORT;
            c->error_codes.esp_transport_sock_errno = 113; /* EHOSTUNREACH */

            esp_mqtt_event_t err_event;
            memset(&err_event, 0, sizeof(err_event));
            err_event.event_id = MQTT_EVENT_ERROR;
            err_event.client = c;
            err_event.user_context = c->config.user_context;
            err_event.error_handle = &c->error_codes;
            dispatch_event(c, MQTT_EVENT_ERROR, &err_event);

            esp_mqtt_event_t disc_event;
            memset(&disc_event, 0, sizeof(disc_event));
            disc_event.event_id = MQTT_EVENT_DISCONNECTED;
            disc_event.client = c;
            disc_event.user_context = c->config.user_context;
            dispatch_event(c, MQTT_EVENT_DISCONNECTED, &disc_event);
        }
    } else {
        if (c->started && !c->connected) {
            uint32_t token = ++s_global_mqtt_token;
            c->token = token;
            if (c->work_item_id != 0) {
                esp_freertos_timer_cancel_work_item(c->work_item_id);
                c->work_item_id = 0;
            }
            esp_freertos_timer_post_work_item(mqtt_connect_work_cb,
                                              (void *)(uintptr_t)token,
                                              &c->work_item_id,
                                              pdMS_TO_TICKS(10));
        }
    }
}

static bool mqtt_topic_match(const char *sub, const char *pub) {
    if (!sub || !pub) {
        return false;
    }

    while (*sub && *pub) {
        if (*sub == '#') {
            return (*(sub + 1) == '\0');
        }
        if (*sub == '+') {
            sub++;
            while (*pub && *pub != '/') {
                pub++;
            }
            if (*sub == '/') {
                if (*pub != '/') {
                    return false;
                }
                sub++;
                pub++;
            } else if (*sub == '\0') {
                return (*pub == '\0');
            } else {
                return false;
            }
        } else {
            if (*sub != *pub) {
                return false;
            }
            sub++;
            pub++;
        }
    }

    if (*pub == '\0') {
        if (strcmp(sub, "/#") == 0 || strcmp(sub, "#") == 0) {
            return true;
        }
    }

    return (*sub == '\0' && *pub == '\0');
}

static void mqtt_connect_work_cb(void *arg, uint32_t work_token) {
    (void)work_token;
    uint32_t my_token = (uint32_t)(uintptr_t)arg;
    struct esp_mqtt_client *client = NULL;
    for (size_t i = 0; i < MAX_MQTT_CLIENTS; i++) {
        if (s_clients[i].initialized && s_clients[i].token == my_token) {
            client = &s_clients[i];
            break;
        }
    }
    if (!client || !client->started) {
        return;
    }
    client->work_item_id = 0;

    esp_netif_t *target = client->bound_netif ? client->bound_netif : esp_netif_get_handle_sta();
    if (!sim_network_broker_is_netif_ready(target)) {
        client->connected = false;
        client->error_codes.error_type = MQTT_ERROR_TYPE_TCP_TRANSPORT;
        client->error_codes.esp_transport_sock_errno = 113; /* EHOSTUNREACH */

        esp_mqtt_event_t err_event;
        memset(&err_event, 0, sizeof(err_event));
        err_event.event_id = MQTT_EVENT_ERROR;
        err_event.client = client;
        err_event.user_context = client->config.user_context;
        err_event.error_handle = &client->error_codes;
        dispatch_event(client, MQTT_EVENT_ERROR, &err_event);

        esp_mqtt_event_t disc_event;
        memset(&disc_event, 0, sizeof(disc_event));
        disc_event.event_id = MQTT_EVENT_DISCONNECTED;
        disc_event.client = client;
        disc_event.user_context = client->config.user_context;
        dispatch_event(client, MQTT_EVENT_DISCONNECTED, &disc_event);
        return;
    }

    client->connected = true;
    esp_mqtt_event_t conn_event;
    memset(&conn_event, 0, sizeof(conn_event));
    conn_event.event_id = MQTT_EVENT_CONNECTED;
    conn_event.client = client;
    conn_event.user_context = client->config.user_context;
    dispatch_event(client, MQTT_EVENT_CONNECTED, &conn_event);
}

esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t *config) {
    if (!config) {
        return NULL;
    }

    for (int i = 0; i < MAX_MQTT_CLIENTS; i++) {
        if (!s_clients[i].initialized) {
            memset(&s_clients[i], 0, sizeof(s_clients[i]));
            s_clients[i].initialized = true;
            s_clients[i].client_slot = (uint32_t)i;
            s_clients[i].token = ++s_global_mqtt_token;
            s_clients[i].config = *config;
            s_clients[i].bound_netif = config->network.netif;
            s_clients[i].registered_event = MQTT_EVENT_ANY;
            sim_network_broker_register_cb(on_network_broker_state_changed, &s_clients[i]);
            return &s_clients[i];
        }
    }
    ESP_LOGE(TAG, "Max MQTT clients reached (%d)", MAX_MQTT_CLIENTS);
    return NULL;
}

esp_err_t esp_mqtt_client_set_uri(esp_mqtt_client_handle_t client, const char *uri) {
    if (!client || !uri) {
        return ESP_ERR_INVALID_ARG;
    }
    client->config.uri = uri;
    client->config.broker.address.uri = uri;
    return ESP_OK;
}

esp_err_t esp_mqtt_client_set_netif(esp_mqtt_client_handle_t client, esp_netif_t *netif) {
    if (!client || !client->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    client->bound_netif = netif;
    return ESP_OK;
}

esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t client) {
    if (!client) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!client->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (client->started) {
        return ESP_OK;
    }

    client->started = true;
    uint32_t token = ++s_global_mqtt_token;
    client->token = token;

    if (client->work_item_id != 0) {
        esp_freertos_timer_cancel_work_item(client->work_item_id);
        client->work_item_id = 0;
    }

    BaseType_t rc = esp_freertos_timer_post_work_item(mqtt_connect_work_cb,
                                                      (void *)(uintptr_t)token,
                                                      &client->work_item_id,
                                                      pdMS_TO_TICKS(50));
    if (rc != pdPASS) {
        client->started = false;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t esp_mqtt_client_stop(esp_mqtt_client_handle_t client) {
    if (!client) {
        return ESP_ERR_INVALID_ARG;
    }
    client->token = ++s_global_mqtt_token;
    if (client->work_item_id != 0) {
        esp_freertos_timer_cancel_work_item(client->work_item_id);
        client->work_item_id = 0;
    }
    client->started = false;

    bool was_connected = client->connected;
    client->connected = false;

    if (was_connected) {
        esp_mqtt_event_t event;
        memset(&event, 0, sizeof(event));
        event.event_id = MQTT_EVENT_DISCONNECTED;
        event.client = client;
        event.user_context = client->config.user_context;
        dispatch_event(client, MQTT_EVENT_DISCONNECTED, &event);
    }
    return ESP_OK;
}

esp_err_t esp_mqtt_client_reconnect(esp_mqtt_client_handle_t client) {
    if (!client) {
        return ESP_ERR_INVALID_ARG;
    }
    if (client->work_item_id != 0) {
        esp_freertos_timer_cancel_work_item(client->work_item_id);
        client->work_item_id = 0;
    }
    client->connected = false;
    esp_mqtt_event_t disc_event;
    memset(&disc_event, 0, sizeof(disc_event));
    disc_event.event_id = MQTT_EVENT_DISCONNECTED;
    disc_event.client = client;
    disc_event.user_context = client->config.user_context;
    dispatch_event(client, MQTT_EVENT_DISCONNECTED, &disc_event);

    uint32_t token = ++s_global_mqtt_token;
    client->token = token;
    client->started = true;
    BaseType_t rc = esp_freertos_timer_post_work_item(mqtt_connect_work_cb,
                                                      (void *)(uintptr_t)token,
                                                      &client->work_item_id,
                                                      pdMS_TO_TICKS(50));
    if (rc != pdPASS) {
        client->started = false;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t esp_mqtt_client_disconnect(esp_mqtt_client_handle_t client) {
    if (!client) {
        return ESP_ERR_INVALID_ARG;
    }
    if (client->connected) {
        client->connected = false;
        esp_mqtt_event_t event;
        memset(&event, 0, sizeof(event));
        event.event_id = MQTT_EVENT_DISCONNECTED;
        event.client = client;
        event.user_context = client->config.user_context;
        dispatch_event(client, MQTT_EVENT_DISCONNECTED, &event);
    }
    return ESP_OK;
}

esp_err_t esp_mqtt_client_destroy(esp_mqtt_client_handle_t client) {
    if (!client) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_mqtt_client_stop(client);
    sim_network_broker_unregister_cb(on_network_broker_state_changed, client);

    for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
        if (s_subscriptions[i].used && s_subscriptions[i].client == client) {
            memset(&s_subscriptions[i], 0, sizeof(s_subscriptions[i]));
        }
    }

    esp_mqtt_event_t event;
    memset(&event, 0, sizeof(event));
    event.event_id = MQTT_EVENT_DELETED;
    event.client = client;
    event.user_context = client->config.user_context;
    dispatch_event(client, MQTT_EVENT_DELETED, &event);

    memset(client, 0, sizeof(*client));
    return ESP_OK;
}

int esp_mqtt_client_publish(esp_mqtt_client_handle_t client, const char *topic, const char *data, int len, int qos, int retain) {
    if (!client || !topic) {
        return -1;
    }
    if (!client->started || !client->connected) {
        return -1;
    }

    if (data && len == 0) {
        len = (int)strlen(data);
    } else if (!data) {
        len = 0;
    }

    /* 记录到 last_published 遥测探测缓冲 */
    strncpy(s_last_topic, topic, sizeof(s_last_topic) - 1);
    s_last_topic[sizeof(s_last_topic) - 1] = '\0';
    if (data && len > 0) {
        size_t cplen = (size_t)len < sizeof(s_last_data) - 1 ? (size_t)len : sizeof(s_last_data) - 1;
        memcpy(s_last_data, data, cplen);
        s_last_data[cplen] = '\0';
        s_last_data_len = (int)cplen;
    } else {
        s_last_data[0] = '\0';
        s_last_data_len = 0;
    }

    int msg_id = s_next_msg_id++;
    s_last_msg_id = msg_id;

    /* 拷贝至发送端 tx 独立缓冲 */
    strncpy(client->tx_topic, topic, sizeof(client->tx_topic) - 1);
    client->tx_topic[sizeof(client->tx_topic) - 1] = '\0';
    if (data && len > 0) {
        size_t cplen = (size_t)len < sizeof(client->tx_data) - 1 ? (size_t)len : sizeof(client->tx_data) - 1;
        memcpy(client->tx_data, data, cplen);
        client->tx_data[cplen] = '\0';
    } else {
        client->tx_data[0] = '\0';
    }

    /* 触发 UniSim 前端推流钩子（若注册） */
    if (s_publish_hook) {
        s_publish_hook(topic, data, len);
    }

    /* 内存 Mock Broker 分发给匹配的订阅者 */
    for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
        if (s_subscriptions[i].used && mqtt_topic_match(s_subscriptions[i].topic_pattern, topic)) {
            esp_mqtt_client_handle_t sub_client = s_subscriptions[i].client;
            if (sub_client && sub_client->connected) {
                strncpy(sub_client->rx_topic, topic, sizeof(sub_client->rx_topic) - 1);
                sub_client->rx_topic[sizeof(sub_client->rx_topic) - 1] = '\0';
                size_t cplen = 0;
                if (data && len > 0) {
                    cplen = (size_t)len < sizeof(sub_client->rx_data) - 1 ? (size_t)len : sizeof(sub_client->rx_data) - 1;
                    memcpy(sub_client->rx_data, data, cplen);
                    sub_client->rx_data[cplen] = '\0';
                } else {
                    sub_client->rx_data[0] = '\0';
                }

                esp_mqtt_event_t event;
                memset(&event, 0, sizeof(event));
                event.event_id = MQTT_EVENT_DATA;
                event.client = sub_client;
                event.user_context = sub_client->config.user_context;
                event.topic = sub_client->rx_topic;
                event.topic_len = (int)strlen(sub_client->rx_topic);
                event.data = sub_client->rx_data;
                event.data_len = (int)cplen;
                event.total_data_len = len;
                event.current_data_offset = 0;
                event.msg_id = msg_id;
                event.qos = qos;
                event.retain = (bool)retain;

                dispatch_event(sub_client, MQTT_EVENT_DATA, &event);
            }
        }
    }

    /* 向发布者自身派发 MQTT_EVENT_PUBLISHED 事件（仅限 QoS > 0，与 ESP-IDF 官方规范一致） */
    if (qos > 0) {
        esp_mqtt_event_t pub_event;
        memset(&pub_event, 0, sizeof(pub_event));
        pub_event.event_id = MQTT_EVENT_PUBLISHED;
        pub_event.client = client;
        pub_event.user_context = client->config.user_context;
        pub_event.msg_id = msg_id;
        dispatch_event(client, MQTT_EVENT_PUBLISHED, &pub_event);
    }

    return msg_id;
}

int esp_mqtt_client_subscribe(esp_mqtt_client_handle_t client, const char *topic, int qos) {
    if (!client || !topic) {
        return -1;
    }

    /* 若已存在相同订阅，则幂等更新 QoS */
    for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
        if (s_subscriptions[i].used && s_subscriptions[i].client == client &&
            strcmp(s_subscriptions[i].topic_pattern, topic) == 0) {
            s_subscriptions[i].qos = qos;
            int msg_id = s_next_msg_id++;
            esp_mqtt_event_t event;
            memset(&event, 0, sizeof(event));
            event.event_id = MQTT_EVENT_SUBSCRIBED;
            event.client = client;
            event.user_context = client->config.user_context;
            event.msg_id = msg_id;
            dispatch_event(client, MQTT_EVENT_SUBSCRIBED, &event);
            return msg_id;
        }
    }

    for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
        if (!s_subscriptions[i].used) {
            s_subscriptions[i].used = true;
            strncpy(s_subscriptions[i].topic_pattern, topic, sizeof(s_subscriptions[i].topic_pattern) - 1);
            s_subscriptions[i].topic_pattern[sizeof(s_subscriptions[i].topic_pattern) - 1] = '\0';
            s_subscriptions[i].qos = qos;
            s_subscriptions[i].client = client;

            int msg_id = s_next_msg_id++;
            esp_mqtt_event_t event;
            memset(&event, 0, sizeof(event));
            event.event_id = MQTT_EVENT_SUBSCRIBED;
            event.client = client;
            event.user_context = client->config.user_context;
            event.msg_id = msg_id;
            dispatch_event(client, MQTT_EVENT_SUBSCRIBED, &event);
            return msg_id;
        }
    }

    ESP_LOGE(TAG, "Subscription table full (max %d)", MAX_SUBSCRIPTIONS);
    return -1;
}

int esp_mqtt_client_unsubscribe(esp_mqtt_client_handle_t client, const char *topic) {
    if (!client || !topic) {
        return -1;
    }

    for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
        if (s_subscriptions[i].used && s_subscriptions[i].client == client &&
            strcmp(s_subscriptions[i].topic_pattern, topic) == 0) {
            s_subscriptions[i].used = false;
            memset(&s_subscriptions[i], 0, sizeof(s_subscriptions[i]));

            int msg_id = s_next_msg_id++;
            esp_mqtt_event_t event;
            memset(&event, 0, sizeof(event));
            event.event_id = MQTT_EVENT_UNSUBSCRIBED;
            event.client = client;
            event.user_context = client->config.user_context;
            event.msg_id = msg_id;
            dispatch_event(client, MQTT_EVENT_UNSUBSCRIBED, &event);
            return msg_id;
        }
    }
    return -1;
}

esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t client, esp_mqtt_event_id_t event, esp_event_handler_t event_handler, void *event_handler_arg) {
    if (!client || !event_handler) {
        return ESP_ERR_INVALID_ARG;
    }
    client->registered_event = event;
    client->event_handler = event_handler;
    client->event_handler_arg = event_handler_arg;
    return ESP_OK;
}

/* ── Wink 仿真与 UniSim 扩展接口 ─────────────────────────────────────── */

void esp_mqtt_sim_reset(void) {
    for (size_t i = 0; i < MAX_MQTT_CLIENTS; i++) {
        if (s_clients[i].initialized) {
            if (s_clients[i].work_item_id != 0) {
                esp_freertos_timer_cancel_work_item(s_clients[i].work_item_id);
                s_clients[i].work_item_id = 0;
            }
            sim_network_broker_unregister_cb(on_network_broker_state_changed, &s_clients[i]);
        }
    }
    s_global_mqtt_token++;
    s_publish_hook = NULL;
    s_next_msg_id = 1;
    s_mqtt_core_handler_registered = false;
    memset(s_subscriptions, 0, sizeof(s_subscriptions));
    memset(s_last_topic, 0, sizeof(s_last_topic));
    memset(s_last_data, 0, sizeof(s_last_data));
    s_last_data_len = 0;
    s_last_msg_id = 0;
    memset(s_clients, 0, sizeof(s_clients));
}

bool esp_mqtt_sim_is_connected(esp_mqtt_client_handle_t client) {
    return (client && client->initialized && client->connected);
}

void esp_mqtt_sim_set_network_ready(bool ready) {
    sim_network_broker_set_ready(ready);
}

int esp_mqtt_sim_inject_message(const char *topic, const char *data, int data_len) {
    if (!topic) {
        return -1;
    }

    if (data && data_len == 0) {
        data_len = (int)strlen(data);
    } else if (!data) {
        data_len = 0;
    }

    int matched = 0;
    int msg_id = s_next_msg_id++;

    for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
        if (s_subscriptions[i].used && mqtt_topic_match(s_subscriptions[i].topic_pattern, topic)) {
            esp_mqtt_client_handle_t sub_client = s_subscriptions[i].client;
            if (sub_client && sub_client->connected) {
                strncpy(sub_client->rx_topic, topic, sizeof(sub_client->rx_topic) - 1);
                sub_client->rx_topic[sizeof(sub_client->rx_topic) - 1] = '\0';
                size_t cplen = 0;
                if (data && data_len > 0) {
                    cplen = (size_t)data_len < sizeof(sub_client->rx_data) - 1 ? (size_t)data_len : sizeof(sub_client->rx_data) - 1;
                    memcpy(sub_client->rx_data, data, cplen);
                    sub_client->rx_data[cplen] = '\0';
                } else {
                    sub_client->rx_data[0] = '\0';
                }

                esp_mqtt_event_t event;
                memset(&event, 0, sizeof(event));
                event.event_id = MQTT_EVENT_DATA;
                event.client = sub_client;
                event.user_context = sub_client->config.user_context;
                event.topic = sub_client->rx_topic;
                event.topic_len = (int)strlen(sub_client->rx_topic);
                event.data = sub_client->rx_data;
                event.data_len = (int)cplen;
                event.total_data_len = data_len;
                event.current_data_offset = 0;
                event.msg_id = msg_id;
                event.qos = s_subscriptions[i].qos;
                event.retain = false;

                dispatch_event(sub_client, MQTT_EVENT_DATA, &event);
                matched++;
            }
        }
    }
    return matched;
}

int esp_mqtt_sim_get_last_published(char *out_topic, size_t topic_max, char *out_data, size_t data_max) {
    if (out_topic && topic_max > 0) {
        strncpy(out_topic, s_last_topic, topic_max - 1);
        out_topic[topic_max - 1] = '\0';
    }
    if (out_data && data_max > 0) {
        strncpy(out_data, s_last_data, data_max - 1);
        out_data[data_max - 1] = '\0';
    }
    return s_last_data_len;
}

int esp_mqtt_sim_get_last_msg_id(void) {
    return s_last_msg_id;
}

void esp_mqtt_sim_set_publish_hook(esp_mqtt_sim_publish_hook_t hook) {
    s_publish_hook = hook;
}

WINK_SIM_EXPORT int sim_mqtt_get_state(void) {
    for (int i = 0; i < MAX_MQTT_CLIENTS; i++) {
        if (s_clients[i].initialized && s_clients[i].connected) {
            return 1;
        }
    }
    return 0;
}

WINK_SIM_EXPORT const char* sim_mqtt_get_last_topic(void) {
    return s_last_topic;
}

WINK_SIM_EXPORT const char* sim_mqtt_get_last_data(void) {
    return s_last_data;
}

