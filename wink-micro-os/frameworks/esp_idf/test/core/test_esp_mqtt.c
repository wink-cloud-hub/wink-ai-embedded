/* SPDX-License-Identifier: GPL-3.0-only */
#include "unity.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "esp_err.h"
#include "mqtt_client.h"
#include "esp_event.h"
#include "esp_idf_wink.h"
#include "wink_sim_scheduler.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern void sim_set_mono_time_us(uint64_t us);

static int s_connected_count = 0;
static int s_disconnected_count = 0;
static int s_error_count = 0;
static int s_subscribed_count = 0;
static int s_unsubscribed_count = 0;
static int s_published_count = 0;
static int s_data_count = 0;
static int s_deleted_count = 0;

static int s_last_msg_id = 0;
static int s_last_published_msg_id = 0;
static esp_mqtt_error_type_t s_last_error_type = MQTT_ERROR_TYPE_NONE;
static char s_last_event_topic[128] = {0};
static char s_last_event_data[512] = {0};

/* 重入测试上下文 */
static bool s_reentrant_active = false;
static bool s_reentrant_published_ok = false;
static char s_reentrant_saved_data[128] = {0};

static void mqtt_test_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    (void)handler_args;
    (void)base;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    if (!event) {
        return;
    }

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        s_connected_count++;
        break;
    case MQTT_EVENT_DISCONNECTED:
        s_disconnected_count++;
        break;
    case MQTT_EVENT_ERROR:
        s_error_count++;
        if (event->error_handle) {
            s_last_error_type = event->error_handle->error_type;
        }
        break;
    case MQTT_EVENT_SUBSCRIBED:
        s_subscribed_count++;
        s_last_msg_id = event->msg_id;
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        s_unsubscribed_count++;
        s_last_msg_id = event->msg_id;
        break;
    case MQTT_EVENT_PUBLISHED:
        s_published_count++;
        s_last_published_msg_id = event->msg_id;
        break;
    case MQTT_EVENT_DATA:
        s_data_count++;
        s_last_msg_id = event->msg_id;
        if (event->topic && event->topic_len > 0) {
            size_t tlen = (size_t)event->topic_len < sizeof(s_last_event_topic) - 1 ? (size_t)event->topic_len : sizeof(s_last_event_topic) - 1;
            memcpy(s_last_event_topic, event->topic, tlen);
            s_last_event_topic[tlen] = '\0';
        } else {
            s_last_event_topic[0] = '\0';
        }
        if (event->data && event->data_len > 0) {
            size_t dlen = (size_t)event->data_len < sizeof(s_last_event_data) - 1 ? (size_t)event->data_len : sizeof(s_last_event_data) - 1;
            memcpy(s_last_event_data, event->data, dlen);
            s_last_event_data[dlen] = '\0';
        } else {
            s_last_event_data[0] = '\0';
        }

        /* 重入测试触发点 */
        if (s_reentrant_active && strcmp(s_last_event_topic, "req/ping") == 0) {
            int pub_rc = esp_mqtt_client_publish(event->client, "resp/pong", "ack_payload", 11, 0, 0);
            if (pub_rc > 0) {
                s_reentrant_published_ok = true;
            }
            /* 保存当前回调接收到的数据，以验证 nested publish 后 event->data 未被污染 */
            strncpy(s_reentrant_saved_data, event->data, sizeof(s_reentrant_saved_data) - 1);
        }
        break;
    case MQTT_EVENT_DELETED:
        s_deleted_count++;
        break;
    default:
        break;
    }
}

void setUp(void) {
    sim_set_mono_time_us(0);
    sim_scheduler_reset(42);
    esp_freertos_pools_reset();
    esp_event_loop_sim_reset();
    esp_mqtt_sim_reset();

    s_connected_count = 0;
    s_disconnected_count = 0;
    s_error_count = 0;
    s_subscribed_count = 0;
    s_unsubscribed_count = 0;
    s_published_count = 0;
    s_data_count = 0;
    s_deleted_count = 0;
    s_last_msg_id = 0;
    s_last_published_msg_id = 0;
    s_last_error_type = MQTT_ERROR_TYPE_NONE;
    memset(s_last_event_topic, 0, sizeof(s_last_event_topic));
    memset(s_last_event_data, 0, sizeof(s_last_event_data));
    s_reentrant_active = false;
    s_reentrant_published_ok = false;
    memset(s_reentrant_saved_data, 0, sizeof(s_reentrant_saved_data));
}

void tearDown(void) {
    sim_set_mono_time_us(0);
    sim_scheduler_reset(0);
    esp_freertos_pools_reset();
    esp_event_loop_sim_reset();
    esp_mqtt_sim_reset();
}

static esp_mqtt_client_handle_t helper_start_connected_client(void) {
    TEST_ASSERT_EQUAL(ESP_OK, esp_event_loop_create_default());
    esp_mqtt_sim_set_network_ready(true);

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = "mqtt://127.0.0.1:1883",
        .broker.address.port = 1883,
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&cfg);
    TEST_ASSERT_NOT_NULL(client);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_test_event_handler, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mqtt_client_start(client));

    wink_status_t st = pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 50);
    TEST_ASSERT_EQUAL(WINK_OK, st);
    TEST_ASSERT_TRUE(esp_mqtt_sim_is_connected(client));
    TEST_ASSERT_EQUAL(1, s_connected_count);
    return client;
}

/* TC-MQTT-01: 未联网状态启动 MQTT */
void test_mqtt_unconnected_network_error(void) {
    TEST_ASSERT_EQUAL(ESP_OK, esp_event_loop_create_default());
    esp_mqtt_sim_set_network_ready(false);

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = "mqtt://127.0.0.1:1883",
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&cfg);
    TEST_ASSERT_NOT_NULL(client);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_test_event_handler, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mqtt_client_start(client));

    wink_status_t st = pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 50);
    TEST_ASSERT_EQUAL(WINK_OK, st);

    TEST_ASSERT_FALSE(esp_mqtt_sim_is_connected(client));
    TEST_ASSERT_EQUAL(1, s_error_count);
    TEST_ASSERT_EQUAL(MQTT_ERROR_TYPE_TCP_TRANSPORT, s_last_error_type);
    TEST_ASSERT_EQUAL(1, s_disconnected_count);
}

/* TC-MQTT-02: 正常连接流程 */
void test_mqtt_normal_connect_flow(void) {
    esp_mqtt_client_handle_t client = helper_start_connected_client();
    TEST_ASSERT_NOT_NULL(client);
    TEST_ASSERT_TRUE(esp_mqtt_sim_is_connected(client));
    TEST_ASSERT_EQUAL(1, s_connected_count);
}

/* TC-MQTT-03: 主题精确订阅与事件回调 */
void test_mqtt_exact_subscription_and_callback(void) {
    esp_mqtt_client_handle_t client = helper_start_connected_client();
    int msg_id = esp_mqtt_client_subscribe(client, "/topic/a", 0);
    TEST_ASSERT_TRUE(msg_id > 0);
    esp_event_loop_run_all_pending();
    TEST_ASSERT_EQUAL(1, s_subscribed_count);
    TEST_ASSERT_EQUAL(msg_id, s_last_msg_id);
}

/* TC-MQTT-04: 本地发布回环消费 */
void test_mqtt_publish_subscribe_loopback(void) {
    esp_mqtt_client_handle_t client = helper_start_connected_client();
    TEST_ASSERT_TRUE(esp_mqtt_client_subscribe(client, "device/telemetry", 0) > 0);
    esp_event_loop_run_all_pending();

    const char *payload = "{\"val\": 123}";
    int pub_id = esp_mqtt_client_publish(client, "device/telemetry", payload, 0, 0, 0);
    TEST_ASSERT_TRUE(pub_id > 0);
    esp_event_loop_run_all_pending();

    TEST_ASSERT_EQUAL(1, s_data_count);
    TEST_ASSERT_EQUAL_STRING("device/telemetry", s_last_event_topic);
    TEST_ASSERT_EQUAL_STRING(payload, s_last_event_data);
}

/* TC-MQTT-05: 未订阅主题发布 */
void test_mqtt_publish_unsubscribed_topic(void) {
    esp_mqtt_client_handle_t client = helper_start_connected_client();
    TEST_ASSERT_TRUE(esp_mqtt_client_subscribe(client, "device/telemetry", 0) > 0);

    int pub_id = esp_mqtt_client_publish(client, "device/unrelated", "hello", 5, 0, 0);
    TEST_ASSERT_TRUE(pub_id > 0);
    TEST_ASSERT_EQUAL(0, s_data_count);
}

/* TC-MQTT-06: 取消订阅 */
void test_mqtt_unsubscribe(void) {
    esp_mqtt_client_handle_t client = helper_start_connected_client();
    TEST_ASSERT_TRUE(esp_mqtt_client_subscribe(client, "/topic/a", 0) > 0);
    esp_event_loop_run_all_pending();

    int unsub_id = esp_mqtt_client_unsubscribe(client, "/topic/a");
    TEST_ASSERT_TRUE(unsub_id > 0);
    esp_event_loop_run_all_pending();
    TEST_ASSERT_EQUAL(1, s_unsubscribed_count);

    esp_mqtt_client_publish(client, "/topic/a", "no_listen", 0, 0, 0);
    esp_event_loop_run_all_pending();
    TEST_ASSERT_EQUAL(0, s_data_count);
}

/* TC-MQTT-07: 外部/UniSim 模拟下发 */
void test_mqtt_sim_inject_message(void) {
    esp_mqtt_client_handle_t client = helper_start_connected_client();
    TEST_ASSERT_TRUE(esp_mqtt_client_subscribe(client, "cloud/command", 0) > 0);
    esp_event_loop_run_all_pending();

    int count = esp_mqtt_sim_inject_message("cloud/command", "restart", 7);
    TEST_ASSERT_EQUAL(1, count);
    esp_event_loop_run_all_pending();
    TEST_ASSERT_EQUAL(1, s_data_count);
    TEST_ASSERT_EQUAL_STRING("cloud/command", s_last_event_topic);
    TEST_ASSERT_EQUAL_STRING("restart", s_last_event_data);
}

/* TC-MQTT-08: 遥测探测验证 */
void test_mqtt_sim_get_last_published(void) {
    esp_mqtt_client_handle_t client = helper_start_connected_client();
    int pub_id = esp_mqtt_client_publish(client, "sensor/temp", "25.5", 4, 0, 0);
    TEST_ASSERT_TRUE(pub_id > 0);

    char out_topic[64] = {0};
    char out_data[64] = {0};
    int len = esp_mqtt_sim_get_last_published(out_topic, sizeof(out_topic), out_data, sizeof(out_data));
    TEST_ASSERT_EQUAL(4, len);
    TEST_ASSERT_EQUAL_STRING("sensor/temp", out_topic);
    TEST_ASSERT_EQUAL_STRING("25.5", out_data);
}

/* TC-MQTT-09: 连接中途 stop/取消 */
void test_mqtt_stop_cancels_pending_connection(void) {
    TEST_ASSERT_EQUAL(ESP_OK, esp_event_loop_create_default());
    esp_mqtt_sim_set_network_ready(true);

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = "mqtt://127.0.0.1:1883",
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&cfg);
    TEST_ASSERT_NOT_NULL(client);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_test_event_handler, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mqtt_client_start(client));

    /* 立即在 50ms 超时前调用 stop */
    TEST_ASSERT_EQUAL(ESP_OK, esp_mqtt_client_stop(client));

    wink_status_t st = pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 50);
    TEST_ASSERT_EQUAL(WINK_OK, st);

    /* 必须不产生幽灵 CONNECTED 事件 */
    TEST_ASSERT_EQUAL(0, s_connected_count);
    TEST_ASSERT_FALSE(esp_mqtt_sim_is_connected(client));
}

/* TC-MQTT-10: 订阅表满池防御 */
void test_mqtt_subscription_table_full(void) {
    esp_mqtt_client_handle_t client = helper_start_connected_client();
    const char *topics[8] = {
        "t/1", "t/2", "t/3", "t/4", "t/5", "t/6", "t/7", "t/8"
    };
    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_TRUE(esp_mqtt_client_subscribe(client, topics[i], 0) > 0);
    }
    esp_event_loop_run_all_pending();
    TEST_ASSERT_EQUAL(8, s_subscribed_count);

    /* 第 9 个订阅超出 MAX_SUBSCRIPTIONS (8)，返回 -1 */
    int overflow = esp_mqtt_client_subscribe(client, "t/9", 0);
    TEST_ASSERT_EQUAL(-1, overflow);
    esp_event_loop_run_all_pending();
    TEST_ASSERT_EQUAL(8, s_subscribed_count);
}

/* TC-MQTT-11: 空参数与越界参数防守 */
void test_mqtt_null_and_invalid_arguments(void) {
    TEST_ASSERT_NULL(esp_mqtt_client_init(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mqtt_client_start(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mqtt_client_stop(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mqtt_client_destroy(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mqtt_client_set_uri(NULL, "mqtt://x"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mqtt_client_register_event(NULL, MQTT_EVENT_ANY, mqtt_test_event_handler, NULL));

    TEST_ASSERT_EQUAL(-1, esp_mqtt_client_publish(NULL, "top", "dat", 3, 0, 0));
    TEST_ASSERT_EQUAL(-1, esp_mqtt_client_subscribe(NULL, "top", 0));
    TEST_ASSERT_EQUAL(-1, esp_mqtt_client_unsubscribe(NULL, "top"));

    esp_mqtt_client_handle_t client = helper_start_connected_client();
    TEST_ASSERT_EQUAL(-1, esp_mqtt_client_publish(client, NULL, "dat", 3, 0, 0));
    TEST_ASSERT_EQUAL(-1, esp_mqtt_client_subscribe(client, NULL, 0));
    TEST_ASSERT_EQUAL(-1, esp_mqtt_client_unsubscribe(client, NULL));
}

/* TC-MQTT-12: destroy 生命周期清理 */
void test_mqtt_destroy_lifecycle(void) {
    esp_mqtt_client_handle_t client = helper_start_connected_client();
    TEST_ASSERT_TRUE(esp_mqtt_client_subscribe(client, "sub/1", 0) > 0);
    esp_event_loop_run_all_pending();

    TEST_ASSERT_EQUAL(ESP_OK, esp_mqtt_client_destroy(client));
    esp_event_loop_run_all_pending();
    TEST_ASSERT_EQUAL(1, s_deleted_count);

    /* 销毁后该实例无法再次 start */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_mqtt_client_start(client));
}

/* TC-MQTT-13: 单层通配符 + 订阅分发 */
void test_mqtt_single_level_wildcard_plus(void) {
    esp_mqtt_client_handle_t client = helper_start_connected_client();
    TEST_ASSERT_TRUE(esp_mqtt_client_subscribe(client, "sensor/+/temp", 0) > 0);
    esp_event_loop_run_all_pending();

    /* 匹配单个层级 */
    esp_mqtt_client_publish(client, "sensor/1/temp", "24", 2, 0, 0);
    esp_event_loop_run_all_pending();
    TEST_ASSERT_EQUAL(1, s_data_count);
    TEST_ASSERT_EQUAL_STRING("sensor/1/temp", s_last_event_topic);

    /* 两个层级不匹配 + */
    esp_mqtt_client_publish(client, "sensor/1/2/temp", "25", 2, 0, 0);
    esp_event_loop_run_all_pending();
    TEST_ASSERT_EQUAL(1, s_data_count);

    /* 另一命名单层匹配 */
    esp_mqtt_client_publish(client, "sensor/kitchen/temp", "26", 2, 0, 0);
    esp_event_loop_run_all_pending();
    TEST_ASSERT_EQUAL(2, s_data_count);
    TEST_ASSERT_EQUAL_STRING("sensor/kitchen/temp", s_last_event_topic);
}

/* TC-MQTT-14: 多层通配符 # 订阅分发 */
void test_mqtt_multi_level_wildcard_hash(void) {
    esp_mqtt_client_handle_t client = helper_start_connected_client();
    TEST_ASSERT_TRUE(esp_mqtt_client_subscribe(client, "device/#", 0) > 0);
    esp_event_loop_run_all_pending();

    esp_mqtt_client_publish(client, "device/status", "online", 6, 0, 0);
    esp_event_loop_run_all_pending();
    TEST_ASSERT_EQUAL(1, s_data_count);

    esp_mqtt_client_publish(client, "device/room/sensor/temp", "22", 2, 0, 0);
    esp_event_loop_run_all_pending();
    TEST_ASSERT_EQUAL(2, s_data_count);

    /* 不同前缀不匹配 */
    esp_mqtt_client_publish(client, "other/device/status", "test", 4, 0, 0);
    esp_event_loop_run_all_pending();
    TEST_ASSERT_EQUAL(2, s_data_count);
}

/* TC-MQTT-15: 发布端 MQTT_EVENT_PUBLISHED (仅限 QoS > 0) */
void test_mqtt_published_event_dispatched_to_publisher(void) {
    esp_mqtt_client_handle_t client = helper_start_connected_client();
    s_published_count = 0;
    s_last_published_msg_id = 0;

    int pub_id = esp_mqtt_client_publish(client, "out/topic", "msg", 3, 1, 0);
    TEST_ASSERT_TRUE(pub_id > 0);
    esp_event_loop_run_all_pending();
    TEST_ASSERT_EQUAL(1, s_published_count);
    TEST_ASSERT_EQUAL(pub_id, s_last_published_msg_id);
}

/* TC-MQTT-16: 回调内重入发布防守 */
void test_mqtt_reentrant_publish_in_callback(void) {
    esp_mqtt_client_handle_t client = helper_start_connected_client();
    TEST_ASSERT_TRUE(esp_mqtt_client_subscribe(client, "req/ping", 0) > 0);
    esp_event_loop_run_all_pending();

    s_reentrant_active = true;
    s_reentrant_published_ok = false;

    const char *orig_payload = "ping_payload_content";
    int pub_id = esp_mqtt_client_publish(client, "req/ping", orig_payload, 0, 0, 0);
    TEST_ASSERT_TRUE(pub_id > 0);
    esp_event_loop_run_all_pending();

    TEST_ASSERT_TRUE(s_reentrant_published_ok);
    /* 验证外层回调内读取到的 payload 没有被 nested publish 的 ack_payload 覆写 */
    TEST_ASSERT_EQUAL_STRING(orig_payload, s_reentrant_saved_data);
    s_reentrant_active = false;
}

/* TC-MQTT-17: QoS 0 严禁派发 MQTT_EVENT_PUBLISHED (ADR-0012) */
void test_mqtt_qos0_does_not_dispatch_published_event(void) {
    esp_mqtt_client_handle_t client = helper_start_connected_client();
    s_published_count = 0;
    s_last_published_msg_id = 0;

    int pub_id = esp_mqtt_client_publish(client, "out/qos0", "hello", 5, 0, 0);
    TEST_ASSERT_TRUE(pub_id > 0);
    esp_event_loop_run_all_pending();
    TEST_ASSERT_EQUAL(0, s_published_count);
}

/* --------------------------------------------------------------------------
 * Unity Main Runner
 * -------------------------------------------------------------------------- */
int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_mqtt_unconnected_network_error);
    RUN_TEST(test_mqtt_normal_connect_flow);
    RUN_TEST(test_mqtt_exact_subscription_and_callback);
    RUN_TEST(test_mqtt_publish_subscribe_loopback);
    RUN_TEST(test_mqtt_publish_unsubscribed_topic);
    RUN_TEST(test_mqtt_unsubscribe);
    RUN_TEST(test_mqtt_sim_inject_message);
    RUN_TEST(test_mqtt_sim_get_last_published);
    RUN_TEST(test_mqtt_stop_cancels_pending_connection);
    RUN_TEST(test_mqtt_subscription_table_full);
    RUN_TEST(test_mqtt_null_and_invalid_arguments);
    RUN_TEST(test_mqtt_destroy_lifecycle);
    RUN_TEST(test_mqtt_single_level_wildcard_plus);
    RUN_TEST(test_mqtt_multi_level_wildcard_hash);
    RUN_TEST(test_mqtt_published_event_dispatched_to_publisher);
    RUN_TEST(test_mqtt_reentrant_publish_in_callback);
    RUN_TEST(test_mqtt_qos0_does_not_dispatch_published_event);

    return UNITY_END();
}
