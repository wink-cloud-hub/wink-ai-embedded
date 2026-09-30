/* SPDX-License-Identifier: GPL-3.0-only */
#include "unity.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "esp_err.h"
#include "esp_http_client.h"
#include "sim_net_responder.h"

static int s_event_connected_count = 0;
static int s_event_header_sent_count = 0;
static int s_event_on_header_count = 0;
static int s_event_on_data_count = 0;
static int s_event_on_finish_count = 0;
static int s_event_disconnected_count = 0;
static int s_event_error_count = 0;

static bool s_header_key_null_detected = false;
static bool s_header_val_null_detected = false;
static char s_last_header_key[64] = {0};
static char s_last_header_value[64] = {0};
static char s_last_data_received[256] = {0};
static int s_last_data_len = 0;

static esp_err_t http_test_event_handler(esp_http_client_event_t *evt) {
    if (!evt) {
        return ESP_ERR_INVALID_ARG;
    }
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        s_event_error_count++;
        break;
    case HTTP_EVENT_ON_CONNECTED:
        s_event_connected_count++;
        break;
    case HTTP_EVENT_HEADER_SENT:
        s_event_header_sent_count++;
        break;
    case HTTP_EVENT_ON_HEADER:
        s_event_on_header_count++;
        if (!evt->header_key) {
            s_header_key_null_detected = true;
        } else {
            strncpy(s_last_header_key, evt->header_key, sizeof(s_last_header_key) - 1);
        }
        if (!evt->header_value) {
            s_header_val_null_detected = true;
        } else {
            strncpy(s_last_header_value, evt->header_value, sizeof(s_last_header_value) - 1);
        }
        break;
    case HTTP_EVENT_ON_DATA:
        s_event_on_data_count++;
        if (evt->data && evt->data_len > 0) {
            size_t cplen = (size_t)evt->data_len < sizeof(s_last_data_received) - 1 ? (size_t)evt->data_len : sizeof(s_last_data_received) - 1;
            memcpy(s_last_data_received, evt->data, cplen);
            s_last_data_received[cplen] = '\0';
            s_last_data_len = (int)cplen;
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        s_event_on_finish_count++;
        break;
    case HTTP_EVENT_DISCONNECTED:
        s_event_disconnected_count++;
        break;
    default:
        break;
    }
    return ESP_OK;
}

void setUp(void) {
    esp_http_client_sim_reset();

    s_event_connected_count = 0;
    s_event_header_sent_count = 0;
    s_event_on_header_count = 0;
    s_event_on_data_count = 0;
    s_event_on_finish_count = 0;
    s_event_disconnected_count = 0;
    s_event_error_count = 0;

    s_header_key_null_detected = false;
    s_header_val_null_detected = false;
    memset(s_last_header_key, 0, sizeof(s_last_header_key));
    memset(s_last_header_value, 0, sizeof(s_last_header_value));
    memset(s_last_data_received, 0, sizeof(s_last_data_received));
    s_last_data_len = 0;
}

void tearDown(void) {
    esp_http_client_sim_reset();
}

/* TC-HTTP-01: 基本 GET 请求与状态码 */
void test_http_basic_get_and_status_code(void) {
    static const sim_http_route_t route = {
        .url_prefix = "http://httpbin.org/get",
        .resp = {
            .status_code = 200,
            .body_data = (const uint8_t *)"{\"origin\": \"127.0.0.1\"}",
            .body_len = 23,
        },
    };
    TEST_ASSERT_EQUAL(ESP_OK, sim_http_responder_register_route(&route));

    esp_http_client_config_t cfg = {
        .url = "http://httpbin.org/get",
        .event_handler = http_test_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    TEST_ASSERT_NOT_NULL(client);

    TEST_ASSERT_EQUAL(ESP_OK, esp_http_client_perform(client));
    TEST_ASSERT_EQUAL(200, esp_http_client_get_status_code(client));
    TEST_ASSERT_EQUAL(1, s_event_connected_count);
    TEST_ASSERT_EQUAL(1, s_event_header_sent_count);
    TEST_ASSERT_TRUE(s_event_on_data_count > 0);
    TEST_ASSERT_EQUAL(1, s_event_on_finish_count);
    TEST_ASSERT_EQUAL(1, s_event_disconnected_count);

    TEST_ASSERT_EQUAL(ESP_OK, esp_http_client_cleanup(client));
}

/* TC-HTTP-02: 自定义 URL 与路径设置 */
void test_http_custom_url_and_path(void) {
    static const sim_http_route_t route = {
        .url_prefix = "http://api.example.com/v2/sensor",
        .resp = {
            .status_code = 200,
            .body_data = (const uint8_t *)"{\"temp\": 25.0}",
            .body_len = 14,
        },
    };
    TEST_ASSERT_EQUAL(ESP_OK, sim_http_responder_register_route(&route));

    esp_http_client_config_t cfg = {
        .url = "http://api.example.com/v1/status",
        .event_handler = http_test_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    TEST_ASSERT_NOT_NULL(client);

    TEST_ASSERT_EQUAL(ESP_OK, esp_http_client_set_url(client, "http://api.example.com/v2/sensor"));
    TEST_ASSERT_EQUAL(ESP_OK, esp_http_client_perform(client));
    TEST_ASSERT_EQUAL(200, esp_http_client_get_status_code(client));

    TEST_ASSERT_EQUAL(ESP_OK, esp_http_client_cleanup(client));
}

/* TC-HTTP-03: 自定义 Mock 响应内容 */
void test_http_custom_mock_response(void) {
    esp_http_client_config_t cfg = {
        .url = "http://example.com/notfound",
        .event_handler = http_test_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    TEST_ASSERT_NOT_NULL(client);

    const char *custom_body = "Resource Not Found Error";
    esp_http_client_sim_set_response(client, 404, custom_body, strlen(custom_body));

    TEST_ASSERT_EQUAL(ESP_OK, esp_http_client_perform(client));
    TEST_ASSERT_EQUAL(404, esp_http_client_get_status_code(client));
    TEST_ASSERT_EQUAL((int64_t)strlen(custom_body), esp_http_client_get_content_length(client));
    TEST_ASSERT_EQUAL_STRING(custom_body, s_last_data_received);
    TEST_ASSERT_EQUAL((int)strlen(custom_body), s_last_data_len);

    TEST_ASSERT_EQUAL(ESP_OK, esp_http_client_cleanup(client));
}

/* TC-HTTP-04: POST 字段设置与获取 */
void test_http_post_field_set_and_get(void) {
    esp_http_client_config_t cfg = {
        .url = "http://httpbin.org/post",
        .method = HTTP_METHOD_POST,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    TEST_ASSERT_NOT_NULL(client);

    const char *post_data = "{\"temp\": 23.5, \"humidity\": 60}";
    TEST_ASSERT_EQUAL(ESP_OK, esp_http_client_set_post_field(client, post_data, strlen(post_data)));

    char *retrieved_data = NULL;
    int len = esp_http_client_get_post_field(client, &retrieved_data);
    TEST_ASSERT_EQUAL((int)strlen(post_data), len);
    TEST_ASSERT_NOT_NULL(retrieved_data);
    TEST_ASSERT_EQUAL_STRING(post_data, retrieved_data);

    TEST_ASSERT_EQUAL(ESP_OK, esp_http_client_cleanup(client));
}

/* TC-HTTP-05: 请求头设置、读取与删除 */
void test_http_headers_set_get_delete(void) {
    esp_http_client_config_t cfg = {
        .url = "http://example.com/headers",
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    TEST_ASSERT_NOT_NULL(client);

    TEST_ASSERT_EQUAL(ESP_OK, esp_http_client_set_header(client, "Authorization", "Bearer token123"));
    TEST_ASSERT_EQUAL(ESP_OK, esp_http_client_set_header(client, "User-Agent", "WinkOS/1.0"));

    char *val = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_http_client_get_header(client, "Authorization", &val));
    TEST_ASSERT_NOT_NULL(val);
    TEST_ASSERT_EQUAL_STRING("Bearer token123", val);

    TEST_ASSERT_EQUAL(ESP_OK, esp_http_client_delete_header(client, "Authorization"));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, esp_http_client_get_header(client, "Authorization", &val));

    /* User-Agent 依然存在 */
    TEST_ASSERT_EQUAL(ESP_OK, esp_http_client_get_header(client, "User-Agent", &val));
    TEST_ASSERT_EQUAL_STRING("WinkOS/1.0", val);

    TEST_ASSERT_EQUAL(ESP_OK, esp_http_client_cleanup(client));
}

/* TC-HTTP-06: 连续多次 perform */
void test_http_multiple_consecutive_performs(void) {
    static const sim_http_route_t route = {
        .url_prefix = "http://example.com/repeat",
        .resp = {
            .status_code = 200,
            .body_data = (const uint8_t *)"repeat_ok",
            .body_len = 9,
        },
    };
    TEST_ASSERT_EQUAL(ESP_OK, sim_http_responder_register_route(&route));

    esp_http_client_config_t cfg = {
        .url = "http://example.com/repeat",
        .event_handler = http_test_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    TEST_ASSERT_NOT_NULL(client);

    TEST_ASSERT_EQUAL(ESP_OK, esp_http_client_perform(client));
    TEST_ASSERT_EQUAL(200, esp_http_client_get_status_code(client));
    TEST_ASSERT_TRUE(esp_http_client_is_complete_data_received(client));

    TEST_ASSERT_EQUAL(ESP_OK, esp_http_client_perform(client));
    TEST_ASSERT_EQUAL(200, esp_http_client_get_status_code(client));
    TEST_ASSERT_EQUAL(2, s_event_connected_count);
    TEST_ASSERT_EQUAL(2, s_event_on_finish_count);

    TEST_ASSERT_EQUAL(ESP_OK, esp_http_client_cleanup(client));
}

/* TC-HTTP-07: 空参数与异常保护 */
void test_http_null_and_invalid_arguments(void) {
    TEST_ASSERT_NULL(esp_http_client_init(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_http_client_perform(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_http_client_set_url(NULL, "http://a"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_http_client_cleanup(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_http_client_open(NULL, 0));
    TEST_ASSERT_EQUAL(-1, esp_http_client_fetch_headers(NULL));
    TEST_ASSERT_EQUAL(-1, esp_http_client_read(NULL, NULL, 10));
    TEST_ASSERT_EQUAL(-1, esp_http_client_write(NULL, NULL, 10));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_http_client_close(NULL));
    TEST_ASSERT_EQUAL(-1, esp_http_client_get_status_code(NULL));
    TEST_ASSERT_EQUAL(-1, esp_http_client_get_content_length(NULL));
    TEST_ASSERT_FALSE(esp_http_client_is_complete_data_received(NULL));
}

/* TC-HTTP-08: cleanup 生命周期复位 */
void test_http_cleanup_lifecycle(void) {
    esp_http_client_config_t cfg = {
        .url = "http://example.com/cleanup",
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    TEST_ASSERT_NOT_NULL(client);

    TEST_ASSERT_EQUAL(ESP_OK, esp_http_client_cleanup(client));

    /* 静态池槽位已释放，重新分配仍可成功 */
    esp_http_client_handle_t client2 = esp_http_client_init(&cfg);
    TEST_ASSERT_NOT_NULL(client2);
    TEST_ASSERT_EQUAL(ESP_OK, esp_http_client_cleanup(client2));
}

/* TC-HTTP-09: Native 流式底层操作流水线 */
void test_http_native_streaming_pipeline(void) {
    esp_http_client_config_t cfg = {
        .url = "http://example.com/stream",
        .event_handler = http_test_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    TEST_ASSERT_NOT_NULL(client);

    const char *payload = "Streaming Response 123456789";
    esp_http_client_sim_set_response(client, 200, payload, strlen(payload));

    TEST_ASSERT_EQUAL(ESP_OK, esp_http_client_open(client, 10));
    TEST_ASSERT_EQUAL(4, esp_http_client_write(client, "ping", 4));

    int content_len = esp_http_client_fetch_headers(client);
    TEST_ASSERT_EQUAL((int)strlen(payload), content_len);

    char buf[16] = {0};
    int r1 = esp_http_client_read(client, buf, 10);
    TEST_ASSERT_EQUAL(10, r1);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, buf, 10);

    char buf2[32] = {0};
    int r2 = esp_http_client_read_response(client, buf2, sizeof(buf2));
    TEST_ASSERT_EQUAL((int)strlen(payload) - 10, r2);
    TEST_ASSERT_EQUAL_STRING(payload + 10, buf2);

    /* 读完后返回 0 (EOF) */
    char buf3[8] = {0};
    int r3 = esp_http_client_read(client, buf3, sizeof(buf3));
    TEST_ASSERT_EQUAL(0, r3);
    TEST_ASSERT_TRUE(esp_http_client_is_complete_data_received(client));

    TEST_ASSERT_EQUAL(ESP_OK, esp_http_client_close(client));
    TEST_ASSERT_EQUAL(ESP_OK, esp_http_client_cleanup(client));
}

/* TC-HTTP-10: Header 事件派发空指针防守 */
void test_http_header_event_null_pointer_safety(void) {
    static sim_http_route_t route;
    memset(&route, 0, sizeof(route));
    strncpy(route.url_prefix, "http://example.com/headers_safety", sizeof(route.url_prefix) - 1);
    route.resp.status_code = 200;
    route.resp.body_data = (const uint8_t *)"ok";
    route.resp.body_len = 2;
    strcpy(route.resp.headers[0].key, "X-Custom");
    strcpy(route.resp.headers[0].value, "HeaderVal");
    route.resp.header_count = 1;
    TEST_ASSERT_EQUAL(ESP_OK, sim_http_responder_register_route(&route));

    esp_http_client_config_t cfg = {
        .url = "http://example.com/headers_safety",
        .event_handler = http_test_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    TEST_ASSERT_NOT_NULL(client);

    TEST_ASSERT_EQUAL(ESP_OK, esp_http_client_perform(client));
    TEST_ASSERT_TRUE(s_event_on_header_count > 0);
    TEST_ASSERT_FALSE(s_header_key_null_detected);
    TEST_ASSERT_FALSE(s_header_val_null_detected);

    TEST_ASSERT_EQUAL(ESP_OK, esp_http_client_cleanup(client));
}

/* TC-HTTP-11: Chunked 响应状态查询 */
void test_http_chunked_response_query(void) {
    esp_http_client_config_t cfg = {
        .url = "http://example.com/chunked",
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    TEST_ASSERT_NOT_NULL(client);

    TEST_ASSERT_FALSE(esp_http_client_is_chunked_response(client));
    TEST_ASSERT_EQUAL(ESP_OK, esp_http_client_cleanup(client));
}

/* TC-HTTP-12: 未注册路由显式红灯 (Fail-Loud, ADR-0012) */
void test_http_unmapped_url_fails_loud(void) {
    esp_http_client_config_t cfg = {
        .url = "http://unknown-server.internal/unmapped",
        .event_handler = http_test_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    TEST_ASSERT_NOT_NULL(client);

    /* 必须显式返回 ESP_ERR_HTTP_CONNECT，严禁静默伪造 200 或 404 */
    TEST_ASSERT_EQUAL(ESP_ERR_HTTP_CONNECT, esp_http_client_perform(client));
    TEST_ASSERT_EQUAL(1, s_event_error_count);

    TEST_ASSERT_EQUAL(ESP_OK, esp_http_client_cleanup(client));
}

/* TC-HTTP-13: 64位 fetch_headers 与流式读取有界流 */
void test_http_fetch_headers_64bit_content_length(void) {
    static const sim_http_route_t route = {
        .url_prefix = "http://example.com/stream64",
        .resp = {
            .status_code = 200,
            .body_data = (const uint8_t *)"0123456789abcdef",
            .body_len = 16,
        },
    };
    TEST_ASSERT_EQUAL(ESP_OK, sim_http_responder_register_route(&route));

    esp_http_client_config_t cfg = {
        .url = "http://example.com/stream64",
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    TEST_ASSERT_NOT_NULL(client);

    TEST_ASSERT_EQUAL(ESP_OK, esp_http_client_open(client, 0));
    int64_t clen = esp_http_client_fetch_headers(client);
    TEST_ASSERT_EQUAL_INT64(16, clen);
    TEST_ASSERT_EQUAL_INT64(16, esp_http_client_get_content_length(client));

    char buf[32] = {0};
    int rlen = esp_http_client_read(client, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(16, rlen);
    TEST_ASSERT_EQUAL_STRING("0123456789abcdef", buf);

    TEST_ASSERT_EQUAL(ESP_OK, esp_http_client_close(client));
    TEST_ASSERT_EQUAL(ESP_OK, esp_http_client_cleanup(client));
}

/* --------------------------------------------------------------------------
 * Unity Main Runner
 * -------------------------------------------------------------------------- */
int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_http_basic_get_and_status_code);
    RUN_TEST(test_http_custom_url_and_path);
    RUN_TEST(test_http_custom_mock_response);
    RUN_TEST(test_http_post_field_set_and_get);
    RUN_TEST(test_http_headers_set_get_delete);
    RUN_TEST(test_http_multiple_consecutive_performs);
    RUN_TEST(test_http_null_and_invalid_arguments);
    RUN_TEST(test_http_cleanup_lifecycle);
    RUN_TEST(test_http_native_streaming_pipeline);
    RUN_TEST(test_http_header_event_null_pointer_safety);
    RUN_TEST(test_http_chunked_response_query);
    RUN_TEST(test_http_unmapped_url_fails_loud);
    RUN_TEST(test_http_fetch_headers_64bit_content_length);

    return UNITY_END();
}
