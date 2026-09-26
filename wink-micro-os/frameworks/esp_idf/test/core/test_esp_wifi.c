/* SPDX-License-Identifier: GPL-3.0-only */
#include "unity.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_idf_wink.h"
#include "wink_sim_scheduler.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern void sim_set_mono_time_us(uint64_t us);

void setUp(void) {
    sim_set_mono_time_us(0);
    sim_scheduler_reset(42);
    esp_freertos_pools_reset();
    esp_event_loop_sim_reset();
    esp_wifi_sim_reset();
    esp_netif_init();
}

void tearDown(void) {
    sim_set_mono_time_us(0);
    sim_scheduler_reset(0);
    esp_freertos_pools_reset();
    esp_event_loop_sim_reset();
    esp_wifi_sim_reset();
    esp_netif_deinit();
}

/* --------------------------------------------------------------------------
 * Event counters and handlers for testing
 * -------------------------------------------------------------------------- */
static int s_sta_start_count = 0;
static int s_sta_connected_count = 0;
static int s_sta_disconnected_count = 0;
static int s_got_ip_count = 0;
static uint32_t s_last_got_ip_addr = 0;

static void wifi_test_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg;
    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START) {
            s_sta_start_count++;
        } else if (id == WIFI_EVENT_STA_CONNECTED) {
            s_sta_connected_count++;
            if (data) {
                wifi_event_sta_connected_t *ev = (wifi_event_sta_connected_t*)data;
                TEST_ASSERT_EQUAL_UINT8(6, ev->channel);
            }
        } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
            s_sta_disconnected_count++;
        }
    } else if (base == IP_EVENT) {
        if (id == IP_EVENT_STA_GOT_IP) {
            s_got_ip_count++;
            if (data) {
                ip_event_got_ip_t *ip_ev = (ip_event_got_ip_t*)data;
                s_last_got_ip_addr = ip_ev->ip_info.ip.addr;
            }
        }
    }
}

/* TC-WIFI-01: init -> start -> connect (via pal_sim_scheduler_run 100ms) -> GOT_IP */
void test_wifi_init_start_connect_got_ip(void) {
    s_sta_start_count = 0;
    s_sta_connected_count = 0;
    s_got_ip_count = 0;
    s_last_got_ip_addr = 0;

    esp_netif_t sta_netif = esp_netif_create_default_wifi_sta();
    TEST_ASSERT_NOT_NULL(sta_netif);

    TEST_ASSERT_EQUAL(ESP_OK, esp_event_loop_create_default());
    TEST_ASSERT_EQUAL(ESP_OK, esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_test_event_handler, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_test_event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_init(&cfg));
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t conf = {
        .sta = {
            .ssid = "TestSSID",
            .password = "TestPass",
        }
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_set_config(WIFI_IF_STA, &conf));
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_start());
    TEST_ASSERT_EQUAL(1, s_sta_start_count);

    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_connect());
    TEST_ASSERT_FALSE(esp_wifi_sim_is_connected());

    /* Advance simulation time through scheduler steps to complete 100ms delay */
    wink_status_t st = pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 50);
    TEST_ASSERT_EQUAL(WINK_OK, st);

    TEST_ASSERT_TRUE(esp_wifi_sim_is_connected());
    TEST_ASSERT_EQUAL(1, s_sta_connected_count);
    TEST_ASSERT_EQUAL(1, s_got_ip_count);
    TEST_ASSERT_EQUAL_UINT32(ESP_IP4TOADDR(192, 168, 4, 2), s_last_got_ip_addr);
}

/* TC-WIFI-02: start() without init() fails with ESP_ERR_INVALID_STATE */
void test_wifi_start_before_init_fails(void) {
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_wifi_start());
}

/* TC-WIFI-03: double init() returns ESP_ERR_INVALID_STATE */
void test_wifi_double_init_fails(void) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_init(&cfg));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_wifi_init(&cfg));
}

/* TC-WIFI-04: set_mode(AP) returns ESP_ERR_NOT_SUPPORTED */
void test_wifi_ap_mode_not_supported(void) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_init(&cfg));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_wifi_set_mode(WIFI_MODE_AP));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_wifi_set_mode(WIFI_MODE_APSTA));
}

/* TC-WIFI-05: get_mac(STA) returns DE:AD:BE:EF:00:01 */
void test_wifi_get_mac_sta(void) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_init(&cfg));

    uint8_t mac[6] = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_get_mac(WIFI_IF_STA, mac));
    uint8_t expected[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, mac, 6);

    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_wifi_get_mac(WIFI_IF_AP, mac));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_get_mac(WIFI_IF_STA, NULL));
}

/* TC-WIFI-06: scan APIs return ESP_ERR_NOT_SUPPORTED */
void test_wifi_scan_not_supported(void) {
    wifi_scan_config_t scfg = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_wifi_scan_start(&scfg, false));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_wifi_scan_stop());

    uint16_t ap_num = 10;
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_wifi_scan_get_ap_records(&ap_num, NULL));
    TEST_ASSERT_EQUAL_UINT16(0, ap_num);
}

/* TC-WIFI-07: disconnect then reconnect */
void test_wifi_disconnect_and_reconnect(void) {
    s_sta_connected_count = 0;
    s_sta_disconnected_count = 0;
    s_got_ip_count = 0;

    esp_event_loop_create_default();
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_test_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_test_event_handler, NULL);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    esp_wifi_connect();
    pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 50);
    TEST_ASSERT_TRUE(esp_wifi_sim_is_connected());
    TEST_ASSERT_EQUAL(1, s_sta_connected_count);
    TEST_ASSERT_EQUAL(1, s_got_ip_count);

    /* Disconnect */
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_disconnect());
    TEST_ASSERT_FALSE(esp_wifi_sim_is_connected());
    TEST_ASSERT_EQUAL(1, s_sta_disconnected_count);

    /* Reconnect */
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_connect());
    pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 50);
    TEST_ASSERT_TRUE(esp_wifi_sim_is_connected());
    TEST_ASSERT_EQUAL(2, s_sta_connected_count);
    TEST_ASSERT_EQUAL(2, s_got_ip_count);
}

/* TC-WIFI-08: disconnect during CONNECTING cancels in-flight task (no ghost event) */
void test_wifi_disconnect_during_connecting_cancels_task(void) {
    s_sta_connected_count = 0;
    s_got_ip_count = 0;

    esp_event_loop_create_default();
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_test_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_test_event_handler, NULL);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    /* Trigger connect (task created with token 1) */
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_connect());

    /* Cancel immediately before 100ms expires */
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_disconnect());
    TEST_ASSERT_FALSE(esp_wifi_sim_is_connected());

    /* Step scheduler: wifi_connect_task wakes up after delay, notices invalid token, exits silently */
    pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 50);

    TEST_ASSERT_FALSE(esp_wifi_sim_is_connected());
    TEST_ASSERT_EQUAL(0, s_sta_connected_count);
    TEST_ASSERT_EQUAL(0, s_got_ip_count);
}

/* TC-WIFI-09: re-entrant connect() returns ESP_ERR_WIFI_CONN */
void test_wifi_reentrant_connect_returns_conn_err(void) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    /* First connect: OK */
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_connect());

    /* While CONNECTING: returns ESP_ERR_WIFI_CONN */
    TEST_ASSERT_EQUAL(ESP_ERR_WIFI_CONN, esp_wifi_connect());

    /* Step to complete connection */
    pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 50);
    TEST_ASSERT_TRUE(esp_wifi_sim_is_connected());

    /* While CONNECTED: returns ESP_ERR_WIFI_CONN */
    TEST_ASSERT_EQUAL(ESP_ERR_WIFI_CONN, esp_wifi_connect());
}

/* --------------------------------------------------------------------------
 * Event system tests
 * -------------------------------------------------------------------------- */
static int s_simple_evt_count = 0;
static int32_t s_simple_evt_last_id = -1;

static void simple_evt_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg;
    (void)base;
    (void)data;
    s_simple_evt_count++;
    s_simple_evt_last_id = id;
}

/* TC-EVT-01: register + post -> handler called with matched args */
void test_event_register_and_post(void) {
    s_simple_evt_count = 0;
    s_simple_evt_last_id = -1;

    TEST_ASSERT_EQUAL(ESP_OK, esp_event_loop_create_default());
    TEST_ASSERT_EQUAL(ESP_OK, esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, simple_evt_handler, NULL));

    int dummy = 42;
    TEST_ASSERT_EQUAL(ESP_OK, esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_START, &dummy, sizeof(dummy), 0));
    TEST_ASSERT_EQUAL(1, s_simple_evt_count);
    TEST_ASSERT_EQUAL(WIFI_EVENT_STA_START, s_simple_evt_last_id);

    /* Post different event id: should not trigger */
    esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_STOP, NULL, 0, 0);
    TEST_ASSERT_EQUAL(1, s_simple_evt_count);
}

/* Dummy handlers for pool filling */
static void pool_filler_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg; (void)base; (void)id; (void)data;
}

/* TC-EVT-02: 16 full pool + 17th register returns ESP_ERR_NO_MEM */
void test_event_full_pool_returns_no_mem(void) {
    esp_event_loop_create_default();
    for (int i = 0; i < 16; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, esp_event_handler_register(WIFI_EVENT, i, pool_filler_handler, (void*)(uintptr_t)i));
    }
    /* 17th registration should fail */
    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, esp_event_handler_register(WIFI_EVENT, 999, pool_filler_handler, NULL));
}

/* TC-EVT-03: ANY_BASE + ANY_ID wildcard captures all events */
void test_event_wildcard_any_base_and_id(void) {
    s_simple_evt_count = 0;
    esp_event_loop_create_default();
    TEST_ASSERT_EQUAL(ESP_OK, esp_event_handler_register(ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID, simple_evt_handler, NULL));

    esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_START, NULL, 0, 0);
    esp_event_post(IP_EVENT, IP_EVENT_STA_GOT_IP, NULL, 0, 0);
    esp_event_post("CUSTOM_BASE", 123, NULL, 0, 0);

    TEST_ASSERT_EQUAL(3, s_simple_evt_count);
}

/* TC-EVT-04: unregister stops callbacks */
void test_event_unregister_stops_callbacks(void) {
    s_simple_evt_count = 0;
    esp_event_loop_create_default();
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, simple_evt_handler, NULL);

    esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_START, NULL, 0, 0);
    TEST_ASSERT_EQUAL(1, s_simple_evt_count);

    TEST_ASSERT_EQUAL(ESP_OK, esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_START, simple_evt_handler));
    esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_START, NULL, 0, 0);
    TEST_ASSERT_EQUAL(1, s_simple_evt_count);
}

/* Shared handler for multi-registration test */
static int s_multi_wifi_count = 0;
static int s_multi_ip_count = 0;

static void multi_reg_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg; (void)id; (void)data;
    if (base == WIFI_EVENT) {
        s_multi_wifi_count++;
    } else if (base == IP_EVENT) {
        s_multi_ip_count++;
    }
}

/* TC-EVT-05: same handler registered to multiple events, instance handles unregister independently */
void test_event_instance_independent_unregister(void) {
    s_multi_wifi_count = 0;
    s_multi_ip_count = 0;
    esp_event_loop_create_default();

    esp_event_handler_instance_t inst_wifi = NULL;
    esp_event_handler_instance_t inst_ip = NULL;

    TEST_ASSERT_EQUAL(ESP_OK, esp_event_handler_instance_register(
        WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, multi_reg_handler, NULL, &inst_wifi));
    TEST_ASSERT_EQUAL(ESP_OK, esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, multi_reg_handler, NULL, &inst_ip));

    TEST_ASSERT_NOT_NULL(inst_wifi);
    TEST_ASSERT_NOT_NULL(inst_ip);
    TEST_ASSERT_NOT_EQUAL(inst_wifi, inst_ip);

    /* Post both */
    esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, NULL, 0, 0);
    esp_event_post(IP_EVENT, IP_EVENT_STA_GOT_IP, NULL, 0, 0);
    TEST_ASSERT_EQUAL(1, s_multi_wifi_count);
    TEST_ASSERT_EQUAL(1, s_multi_ip_count);

    /* Unregister ONLY wifi instance */
    TEST_ASSERT_EQUAL(ESP_OK, esp_event_handler_instance_unregister(
        WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, inst_wifi));

    /* Post both again */
    esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, NULL, 0, 0);
    esp_event_post(IP_EVENT, IP_EVENT_STA_GOT_IP, NULL, 0, 0);
    TEST_ASSERT_EQUAL(1, s_multi_wifi_count); /* Wifi count unchanged */
    TEST_ASSERT_EQUAL(2, s_multi_ip_count);   /* IP count incremented */
}

/* Self-unregistering handler */
static int s_self_unreg_call_count = 0;

static void self_unreg_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg; (void)data;
    s_self_unreg_call_count++;
    esp_event_handler_unregister(base, id, self_unreg_handler);
}

/* TC-EVT-06: handler unregisters itself in callback (re-entrancy protection) */
void test_event_self_unregister_in_callback(void) {
    s_self_unreg_call_count = 0;
    esp_event_loop_create_default();

    esp_event_handler_register(WIFI_EVENT, 1, self_unreg_handler, NULL);

    /* First post executes handler and it unregisters itself */
    TEST_ASSERT_EQUAL(ESP_OK, esp_event_post(WIFI_EVENT, 1, NULL, 0, 0));
    TEST_ASSERT_EQUAL(1, s_self_unreg_call_count);

    /* Second post should not invoke handler */
    TEST_ASSERT_EQUAL(ESP_OK, esp_event_post(WIFI_EVENT, 1, NULL, 0, 0));
    TEST_ASSERT_EQUAL(1, s_self_unreg_call_count);
}

/* --------------------------------------------------------------------------
 * Netif tests
 * -------------------------------------------------------------------------- */

/* TC-NET-01: esp_netif when connected returns ip=192.168.4.2, gw=192.168.4.1 */
void test_netif_get_ip_info_when_connected(void) {
    esp_netif_t netif = esp_netif_create_default_wifi_sta();
    TEST_ASSERT_NOT_NULL(netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    esp_wifi_connect();
    pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 50);

    TEST_ASSERT_TRUE(esp_wifi_sim_is_connected());

    esp_netif_ip_info_t ip_info;
    TEST_ASSERT_EQUAL(ESP_OK, esp_netif_get_ip_info(netif, &ip_info));
    TEST_ASSERT_EQUAL_UINT32(ESP_IP4TOADDR(192, 168, 4, 2), ip_info.ip.addr);
    TEST_ASSERT_EQUAL_UINT32(ESP_IP4TOADDR(255, 255, 255, 0), ip_info.netmask.addr);
    TEST_ASSERT_EQUAL_UINT32(ESP_IP4TOADDR(192, 168, 4, 1), ip_info.gw.addr);
}

/* TC-NET-02: esp_netif when not connected returns all zeros */
void test_netif_get_ip_info_when_not_connected(void) {
    esp_netif_t netif = esp_netif_create_default_wifi_sta();
    TEST_ASSERT_NOT_NULL(netif);

    esp_netif_ip_info_t ip_info;
    TEST_ASSERT_EQUAL(ESP_OK, esp_netif_get_ip_info(netif, &ip_info));
    TEST_ASSERT_EQUAL_UINT32(0, ip_info.ip.addr);
    TEST_ASSERT_EQUAL_UINT32(0, ip_info.netmask.addr);
    TEST_ASSERT_EQUAL_UINT32(0, ip_info.gw.addr);
}

/* --------------------------------------------------------------------------
 * Unity Main Runner
 * -------------------------------------------------------------------------- */
int main(void) {
    UNITY_BEGIN();

    /* Wi-Fi FSM & Lifecycle */
    RUN_TEST(test_wifi_init_start_connect_got_ip);
    RUN_TEST(test_wifi_start_before_init_fails);
    RUN_TEST(test_wifi_double_init_fails);
    RUN_TEST(test_wifi_ap_mode_not_supported);
    RUN_TEST(test_wifi_get_mac_sta);
    RUN_TEST(test_wifi_scan_not_supported);
    RUN_TEST(test_wifi_disconnect_and_reconnect);
    RUN_TEST(test_wifi_disconnect_during_connecting_cancels_task);
    RUN_TEST(test_wifi_reentrant_connect_returns_conn_err);

    /* Event Loop & Snapshot Dispatch */
    RUN_TEST(test_event_register_and_post);
    RUN_TEST(test_event_full_pool_returns_no_mem);
    RUN_TEST(test_event_wildcard_any_base_and_id);
    RUN_TEST(test_event_unregister_stops_callbacks);
    RUN_TEST(test_event_instance_independent_unregister);
    RUN_TEST(test_event_self_unregister_in_callback);

    /* Netif Shim */
    RUN_TEST(test_netif_get_ip_info_when_connected);
    RUN_TEST(test_netif_get_ip_info_when_not_connected);

    return UNITY_END();
}
