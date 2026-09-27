/* SPDX-License-Identifier: GPL-3.0-only */
#include "unity.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "os/os_mbuf.h"

void setUp(void) {
    esp_nimble_sim_reset();
    nimble_port_init();
}

void tearDown(void) {
    nimble_port_deinit();
    esp_nimble_sim_reset();
}

/* ── TC-BLE-01: Port 生命周期与 Sync 回调 ───────────────────────────────────── */
static bool s_tc01_sync_called = false;
static void tc01_sync_cb(void) {
    s_tc01_sync_called = true;
}

static void test_ble_port_lifecycle_and_sync(void) {
    s_tc01_sync_called = false;
    ble_hs_cfg.sync_cb = tc01_sync_cb;
    TEST_ASSERT_EQUAL(1, ble_hs_is_enabled());

    nimble_port_run();
    TEST_ASSERT_TRUE(s_tc01_sync_called);

    TEST_ASSERT_EQUAL(ESP_OK, nimble_port_stop());
    TEST_ASSERT_EQUAL(ESP_OK, nimble_port_deinit());
    TEST_ASSERT_EQUAL(0, ble_hs_is_enabled());
}

/* ── TC-BLE-02: GAP 设备名称与外观读写 ───────────────────────────────────────── */
static void test_ble_gap_device_name_and_appearance(void) {
    TEST_ASSERT_EQUAL(0, ble_svc_gap_device_name_set("Wink-BLE-Dev"));
    TEST_ASSERT_EQUAL_STRING("Wink-BLE-Dev", ble_svc_gap_device_name());

    TEST_ASSERT_EQUAL(0, ble_svc_gap_device_appearance_set(0x0340));
    TEST_ASSERT_EQUAL_HEX16(0x0340, ble_svc_gap_device_appearance());

    /* Null name defensive check */
    TEST_ASSERT_EQUAL(BLE_HS_EINVAL, ble_svc_gap_device_name_set(NULL));
}

/* ── TC-BLE-03: GATT 资源静态计数与越界防护 (ADR-0012 / ADR-0045) ───────────── */
static void test_ble_gatt_capacity_guard(void) {
    const struct ble_gatt_svc_def ok_svcs[] = {
        {
            .type = BLE_GATT_SVC_TYPE_PRIMARY,
            .uuid = BLE_UUID16_DECLARE(0x1800),
            .characteristics = (struct ble_gatt_chr_def[]) {
                { .uuid = BLE_UUID16_DECLARE(0x2a00), .flags = BLE_GATT_CHR_F_READ },
                { 0 }
            }
        },
        { 0 }
    };
    TEST_ASSERT_EQUAL(0, ble_gatts_count_cfg(ok_svcs));

    /* Exceeding SIM_BLE_MAX_SERVICES (8) */
    const struct ble_gatt_svc_def excess_svcs[10] = {
        { .type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = BLE_UUID16_DECLARE(0x1801) },
        { .type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = BLE_UUID16_DECLARE(0x1802) },
        { .type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = BLE_UUID16_DECLARE(0x1803) },
        { .type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = BLE_UUID16_DECLARE(0x1804) },
        { .type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = BLE_UUID16_DECLARE(0x1805) },
        { .type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = BLE_UUID16_DECLARE(0x1806) },
        { .type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = BLE_UUID16_DECLARE(0x1807) },
        { .type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = BLE_UUID16_DECLARE(0x1808) },
        { .type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = BLE_UUID16_DECLARE(0x1809) },
        { 0 }
    };
    TEST_ASSERT_EQUAL(BLE_HS_ENOMEM, ble_gatts_count_cfg(excess_svcs));
    TEST_ASSERT_EQUAL(BLE_HS_ENOMEM, ble_gatts_add_svcs(excess_svcs));
}

/* ── TC-BLE-04: 服务与特征值句柄规范分配 ─────────────────────────────────────── */
static uint16_t s_tc04_val1 = 0;
static uint16_t s_tc04_val2 = 0;
static void test_ble_gatt_handle_allocation(void) {
    s_tc04_val1 = 0;
    s_tc04_val2 = 0;
    const struct ble_gatt_svc_def svcs[] = {
        {
            .type = BLE_GATT_SVC_TYPE_PRIMARY,
            .uuid = BLE_UUID16_DECLARE(0xff00),
            .characteristics = (struct ble_gatt_chr_def[]) {
                {
                    .uuid = BLE_UUID16_DECLARE(0xff01),
                    .flags = BLE_GATT_CHR_F_READ,
                    .val_handle = &s_tc04_val1,
                },
                {
                    .uuid = BLE_UUID16_DECLARE(0xff02),
                    .flags = BLE_GATT_CHR_F_WRITE,
                    .val_handle = &s_tc04_val2,
                },
                { 0 }
            }
        },
        { 0 }
    };
    TEST_ASSERT_EQUAL(0, ble_gatts_add_svcs(svcs));
    TEST_ASSERT_GREATER_THAN(0, s_tc04_val1);
    TEST_ASSERT_GREATER_THAN(s_tc04_val1, s_tc04_val2);

    uint16_t def_h = 0, val_h = 0;
    TEST_ASSERT_EQUAL(0, ble_gatts_find_chr(BLE_UUID16_DECLARE(0xff00),
                                            BLE_UUID16_DECLARE(0xff01),
                                            &def_h, &val_h));
    TEST_ASSERT_EQUAL_UINT16(s_tc04_val1, val_h);
}

/* ── TC-BLE-05: UUID 16-bit 与 128-bit 比较 ─────────────────────────────────── */
static void test_ble_uuid_cmp_and_str(void) {
    const ble_uuid16_t u16_a = BLE_UUID16_INIT(0x180d);
    const ble_uuid16_t u16_b = BLE_UUID16_INIT(0x180d);
    const ble_uuid16_t u16_c = BLE_UUID16_INIT(0x180a);

    TEST_ASSERT_EQUAL(0, ble_uuid_cmp(&u16_a.u, &u16_b.u));
    TEST_ASSERT_NOT_EQUAL(0, ble_uuid_cmp(&u16_a.u, &u16_c.u));

    /* 128-bit Bluetooth base UUID matching 0x180d */
    const ble_uuid128_t u128_base_180d = BLE_UUID128_INIT(
        0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
        0x00, 0x10, 0x00, 0x00, 0x0d, 0x18, 0x00, 0x00
    );
    TEST_ASSERT_EQUAL(0, ble_uuid_cmp(&u16_a.u, &u128_base_180d.u));
    TEST_ASSERT_EQUAL(0, ble_uuid_cmp(&u128_base_180d.u, &u16_a.u));

    char str_buf[BLE_UUID_STR_LEN];
    ble_uuid_to_str(&u16_a.u, str_buf);
    TEST_ASSERT_EQUAL_STRING("0x180d", str_buf);
}

/* ── TC-BLE-06: GAP 广播与扫描响应流转 ───────────────────────────────────────── */
static void test_ble_gap_adv_and_rsp_flow(void) {
    struct ble_gap_adv_params params;
    memset(&params, 0, sizeof(params));
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));
    const char *adv_name = "WinkAdv";
    fields.name = (const uint8_t *)adv_name;
    fields.name_len = strlen(adv_name);
    fields.name_is_complete = 1;

    TEST_ASSERT_EQUAL(0, ble_gap_adv_set_fields(&fields));
    TEST_ASSERT_EQUAL(0, ble_gap_adv_rsp_set_fields(&fields));

    TEST_ASSERT_EQUAL(0, ble_gap_adv_start(BLE_ADDR_PUBLIC, NULL, -1, &params, NULL, NULL));
    TEST_ASSERT_EQUAL(1, ble_gap_adv_active());

    TEST_ASSERT_EQUAL(0, ble_gap_adv_stop());
    TEST_ASSERT_EQUAL(0, ble_gap_adv_active());
}

/* ── TC-BLE-07: 虚拟连接事件建立与 MTU 协商 ──────────────────────────────────── */
static int s_tc07_gap_events[4];
static int s_tc07_event_count = 0;
static uint16_t s_tc07_mtu = 0;
static int tc07_gap_cb(struct ble_gap_event *ev, void *arg) {
    (void)arg;
    if (s_tc07_event_count < 4) {
        s_tc07_gap_events[s_tc07_event_count++] = ev->type;
    }
    if (ev->type == BLE_GAP_EVENT_MTU) {
        s_tc07_mtu = ev->mtu.value;
    }
    return 0;
}

static void test_ble_virtual_connect_and_mtu(void) {
    s_tc07_event_count = 0;
    s_tc07_mtu = 0;

    struct ble_gap_adv_params params;
    memset(&params, 0, sizeof(params));
    TEST_ASSERT_EQUAL(0, ble_gap_adv_start(BLE_ADDR_PUBLIC, NULL, -1, &params, tc07_gap_cb, NULL));
    TEST_ASSERT_EQUAL(1, ble_gap_adv_active());

    TEST_ASSERT_EQUAL(0, esp_nimble_sim_connect());
    /* Connection automatically stops advertisement */
    TEST_ASSERT_EQUAL(0, ble_gap_adv_active());
    TEST_ASSERT_EQUAL(2, s_tc07_event_count);
    TEST_ASSERT_EQUAL(BLE_GAP_EVENT_CONNECT, s_tc07_gap_events[0]);
    TEST_ASSERT_EQUAL(BLE_GAP_EVENT_MTU, s_tc07_gap_events[1]);
    TEST_ASSERT_EQUAL_UINT16(256, s_tc07_mtu);

    struct ble_gap_conn_desc desc;
    TEST_ASSERT_EQUAL(0, ble_gap_conn_find(1, &desc));
    TEST_ASSERT_EQUAL_UINT16(1, desc.conn_handle);
}

/* ── TC-BLE-08: 虚拟断开连接事件与重播流转 ──────────────────────────────────── */
static void test_ble_virtual_disconnect_and_replay(void) {
    s_tc07_event_count = 0;
    struct ble_gap_adv_params params;
    memset(&params, 0, sizeof(params));
    ble_gap_adv_start(BLE_ADDR_PUBLIC, NULL, -1, &params, tc07_gap_cb, NULL);
    esp_nimble_sim_connect();

    s_tc07_event_count = 0;
    TEST_ASSERT_EQUAL(0, esp_nimble_sim_disconnect());
    TEST_ASSERT_EQUAL(1, s_tc07_event_count);
    TEST_ASSERT_EQUAL(BLE_GAP_EVENT_DISCONNECT, s_tc07_gap_events[0]);

    struct ble_gap_conn_desc desc;
    TEST_ASSERT_EQUAL(BLE_HS_ENOTCONN, ble_gap_conn_find(1, &desc));

    /* Allowed to re-advertise after disconnection */
    TEST_ASSERT_EQUAL(0, ble_gap_adv_start(BLE_ADDR_PUBLIC, NULL, -1, &params, tc07_gap_cb, NULL));
    TEST_ASSERT_EQUAL(1, ble_gap_adv_active());
}

/* ── TC-BLE-09: GATT 特征值读回调执行 ────────────────────────────────────────── */
static int tc09_access_cb(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    (void)conn; (void)attr; (void)arg;
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        const char *msg = "Antigravity";
        return os_mbuf_append(ctxt->om, msg, strlen(msg));
    }
    return BLE_HS_EAPP;
}

static void test_ble_gatt_read_chr_callback(void) {
    uint16_t val_h = 0;
    const struct ble_gatt_svc_def svcs[] = {
        {
            .type = BLE_GATT_SVC_TYPE_PRIMARY,
            .uuid = BLE_UUID16_DECLARE(0x180f),
            .characteristics = (struct ble_gatt_chr_def[]) {
                {
                    .uuid = BLE_UUID16_DECLARE(0x2a19),
                    .flags = BLE_GATT_CHR_F_READ,
                    .access_cb = tc09_access_cb,
                    .val_handle = &val_h,
                },
                { 0 }
            }
        },
        { 0 }
    };
    TEST_ASSERT_EQUAL(0, ble_gatts_add_svcs(svcs));

    char buf[32] = {0};
    uint16_t out_len = 0;
    TEST_ASSERT_EQUAL(0, esp_nimble_sim_read_chr(1, val_h, buf, sizeof(buf) - 1, &out_len));
    TEST_ASSERT_EQUAL_STRING("Antigravity", buf);
}

/* ── TC-BLE-10: GATT 特征值写回调与 mbuf 提取 ───────────────────────────────── */
static char s_tc10_written[32];
static int tc10_access_cb(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    (void)conn; (void)attr; (void)arg;
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
        if (len >= sizeof(s_tc10_written)) len = sizeof(s_tc10_written) - 1;
        os_mbuf_copydata(ctxt->om, 0, len, s_tc10_written);
        s_tc10_written[len] = '\0';
        return 0;
    }
    return BLE_HS_EAPP;
}

static void test_ble_gatt_write_chr_and_mbuf_unpack(void) {
    memset(s_tc10_written, 0, sizeof(s_tc10_written));
    uint16_t val_h = 0;
    const struct ble_gatt_svc_def svcs[] = {
        {
            .type = BLE_GATT_SVC_TYPE_PRIMARY,
            .uuid = BLE_UUID16_DECLARE(0xff10),
            .characteristics = (struct ble_gatt_chr_def[]) {
                {
                    .uuid = BLE_UUID16_DECLARE(0xff11),
                    .flags = BLE_GATT_CHR_F_WRITE,
                    .access_cb = tc10_access_cb,
                    .val_handle = &val_h,
                },
                { 0 }
            }
        },
        { 0 }
    };
    TEST_ASSERT_EQUAL(0, ble_gatts_add_svcs(svcs));

    const char *payload = "Speed-100";
    TEST_ASSERT_EQUAL(0, esp_nimble_sim_write_chr(1, val_h, payload, strlen(payload)));
    TEST_ASSERT_EQUAL_STRING("Speed-100", s_tc10_written);
}

/* ── TC-BLE-11: 特征值读写权限守卫 ───────────────────────────────────────────── */
static void test_ble_gatt_permission_guard(void) {
    uint16_t r_val = 0;
    uint16_t w_val = 0;
    const struct ble_gatt_svc_def svcs[] = {
        {
            .type = BLE_GATT_SVC_TYPE_PRIMARY,
            .uuid = BLE_UUID16_DECLARE(0xff20),
            .characteristics = (struct ble_gatt_chr_def[]) {
                {
                    .uuid = BLE_UUID16_DECLARE(0xff21),
                    .flags = BLE_GATT_CHR_F_READ,
                    .val_handle = &r_val,
                },
                {
                    .uuid = BLE_UUID16_DECLARE(0xff22),
                    .flags = BLE_GATT_CHR_F_WRITE,
                    .val_handle = &w_val,
                },
                { 0 }
            }
        },
        { 0 }
    };
    TEST_ASSERT_EQUAL(0, ble_gatts_add_svcs(svcs));

    /* Writing to read-only characteristic must fail with ENOTSUP */
    TEST_ASSERT_EQUAL(BLE_HS_ENOTSUP, esp_nimble_sim_write_chr(1, r_val, "data", 4));

    /* Reading from write-only characteristic must fail with ENOTSUP */
    char buf[16];
    uint16_t len = 0;
    TEST_ASSERT_EQUAL(BLE_HS_ENOTSUP, esp_nimble_sim_read_chr(1, w_val, buf, sizeof(buf), &len));
}

/* ── TC-BLE-12: 订阅与 chr_updated 主动推送 ──────────────────────────────────── */
static uint16_t s_tc12_hook_val_handle = 0;
static uint8_t s_tc12_hook_data[32];
static uint16_t s_tc12_hook_len = 0;
static void tc12_notify_hook(uint16_t conn, uint16_t attr, const uint8_t *data, uint16_t len) {
    (void)conn;
    s_tc12_hook_val_handle = attr;
    s_tc12_hook_len = len;
    if (len < sizeof(s_tc12_hook_data)) {
        memcpy(s_tc12_hook_data, data, len);
    }
}

static int tc12_access_cb(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    (void)conn; (void)attr; (void)arg;
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uint8_t hrm[2] = { 0x00, 75 };
        return os_mbuf_append(ctxt->om, hrm, sizeof(hrm));
    }
    return 0;
}

static void test_ble_subscribe_and_chr_updated(void) {
    s_tc12_hook_len = 0;
    esp_nimble_sim_set_notify_hook(tc12_notify_hook);

    uint16_t val_h = 0;
    const struct ble_gatt_svc_def svcs[] = {
        {
            .type = BLE_GATT_SVC_TYPE_PRIMARY,
            .uuid = BLE_UUID16_DECLARE(0x180d),
            .characteristics = (struct ble_gatt_chr_def[]) {
                {
                    .uuid = BLE_UUID16_DECLARE(0x2a37),
                    .flags = BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_READ,
                    .access_cb = tc12_access_cb,
                    .val_handle = &val_h,
                },
                { 0 }
            }
        },
        { 0 }
    };
    TEST_ASSERT_EQUAL(0, ble_gatts_add_svcs(svcs));

    /* Before subscription, updated triggers nothing */
    TEST_ASSERT_EQUAL(0, ble_gatts_chr_updated(val_h));
    TEST_ASSERT_EQUAL_UINT16(0, s_tc12_hook_len);

    /* Subscribe notify */
    TEST_ASSERT_EQUAL(0, esp_nimble_sim_subscribe(1, val_h, true, false));
    TEST_ASSERT_EQUAL(0, ble_gatts_chr_updated(val_h));
    TEST_ASSERT_EQUAL_UINT16(2, s_tc12_hook_len);
    TEST_ASSERT_EQUAL_UINT16(val_h, s_tc12_hook_val_handle);
    TEST_ASSERT_EQUAL_HEX8(75, s_tc12_hook_data[1]);
}

/* ── TC-BLE-13: 非法句柄防御（Fail-Loud） ────────────────────────────────────── */
static void test_ble_invalid_handle_defense(void) {
    char buf[16];
    uint16_t len = 0;
    TEST_ASSERT_EQUAL(BLE_HS_EINVAL, esp_nimble_sim_read_chr(1, 0x9999, buf, sizeof(buf), &len));
    TEST_ASSERT_EQUAL(BLE_HS_EINVAL, esp_nimble_sim_write_chr(1, 0x9999, "data", 4));
    TEST_ASSERT_EQUAL(BLE_HS_EINVAL, esp_nimble_sim_subscribe(1, 0x9999, true, false));
    TEST_ASSERT_EQUAL(BLE_HS_EINVAL, ble_gatts_chr_updated(0x9999));
}

/* ── TC-BLE-14: 仿真符号导出保全（Wasm DCE） ─────────────────────────────────── */
static void test_ble_sim_export_symbols_callable(void) {
    TEST_ASSERT_NOT_NULL(esp_nimble_sim_connect);
    TEST_ASSERT_NOT_NULL(esp_nimble_sim_disconnect);
    TEST_ASSERT_NOT_NULL(esp_nimble_sim_read_chr);
    TEST_ASSERT_NOT_NULL(esp_nimble_sim_write_chr);
    TEST_ASSERT_NOT_NULL(esp_nimble_sim_subscribe);
    TEST_ASSERT_NOT_NULL(esp_nimble_sim_get_service_count);
    TEST_ASSERT_NOT_NULL(esp_nimble_sim_get_service_info);
    TEST_ASSERT_NOT_NULL(esp_nimble_sim_get_char_count);
    TEST_ASSERT_NOT_NULL(esp_nimble_sim_get_char_info);
}

/* ── TC-BLE-15: gatts_register_cb 注册钩子通知 ──────────────────────────────── */
static int s_tc15_svc_registered = 0;
static int s_tc15_chr_registered = 0;
static int s_tc15_dsc_registered = 0;
static void tc15_reg_cb(struct ble_gatt_register_ctxt *ctxt, void *arg) {
    (void)arg;
    if (ctxt->op == BLE_GATT_REGISTER_OP_SVC) s_tc15_svc_registered++;
    if (ctxt->op == BLE_GATT_REGISTER_OP_CHR) s_tc15_chr_registered++;
    if (ctxt->op == BLE_GATT_REGISTER_OP_DSC) s_tc15_dsc_registered++;
}

static void test_ble_gatts_register_callback(void) {
    s_tc15_svc_registered = 0;
    s_tc15_chr_registered = 0;
    s_tc15_dsc_registered = 0;
    ble_hs_cfg.gatts_register_cb = tc15_reg_cb;

    uint16_t val_h = 0;
    const struct ble_gatt_svc_def svcs[] = {
        {
            .type = BLE_GATT_SVC_TYPE_PRIMARY,
            .uuid = BLE_UUID16_DECLARE(0xff30),
            .characteristics = (struct ble_gatt_chr_def[]) {
                {
                    .uuid = BLE_UUID16_DECLARE(0xff31),
                    .flags = BLE_GATT_CHR_F_READ,
                    .val_handle = &val_h,
                    .descriptors = (struct ble_gatt_dsc_def[]) {
                        {
                            .uuid = BLE_UUID16_DECLARE(0x2901),
                            .att_flags = 0,
                        },
                        { 0 }
                    }
                },
                { 0 }
            }
        },
        { 0 }
    };
    TEST_ASSERT_EQUAL(0, ble_gatts_add_svcs(svcs));
    TEST_ASSERT_EQUAL(1, s_tc15_svc_registered);
    TEST_ASSERT_EQUAL(1, s_tc15_chr_registered);
    TEST_ASSERT_EQUAL(1, s_tc15_dsc_registered);
}

/* ── TC-BLE-16: BLE_GAP_EVENT_SUBSCRIBE 双向事件 ────────────────────────────── */
static bool s_tc16_subscribe_received = false;
static uint8_t s_tc16_cur_notify = 0;
static int tc16_gap_cb(struct ble_gap_event *ev, void *arg) {
    (void)arg;
    if (ev->type == BLE_GAP_EVENT_SUBSCRIBE) {
        s_tc16_subscribe_received = true;
        s_tc16_cur_notify = ev->subscribe.cur_notify;
    }
    return 0;
}

static void test_ble_gap_event_subscribe_bidirectional(void) {
    s_tc16_subscribe_received = false;
    s_tc16_cur_notify = 0;

    struct ble_gap_adv_params params;
    memset(&params, 0, sizeof(params));
    ble_gap_adv_start(BLE_ADDR_PUBLIC, NULL, -1, &params, tc16_gap_cb, NULL);

    uint16_t val_h = 0;
    const struct ble_gatt_svc_def svcs[] = {
        {
            .type = BLE_GATT_SVC_TYPE_PRIMARY,
            .uuid = BLE_UUID16_DECLARE(0xff40),
            .characteristics = (struct ble_gatt_chr_def[]) {
                {
                    .uuid = BLE_UUID16_DECLARE(0xff41),
                    .flags = BLE_GATT_CHR_F_NOTIFY,
                    .val_handle = &val_h,
                },
                { 0 }
            }
        },
        { 0 }
    };
    ble_gatts_add_svcs(svcs);

    TEST_ASSERT_EQUAL(0, esp_nimble_sim_subscribe(1, val_h, true, false));
    TEST_ASSERT_TRUE(s_tc16_subscribe_received);
    TEST_ASSERT_EQUAL_HEX8(1, s_tc16_cur_notify);
}

/* ── TC-BLE-17: ble_gatts_notify_custom 数据直推 ─────────────────────────────── */
static void test_ble_gatts_notify_custom(void) {
    s_tc12_hook_len = 0;
    esp_nimble_sim_set_notify_hook(tc12_notify_hook);

    uint16_t val_h = 0;
    const struct ble_gatt_svc_def svcs[] = {
        {
            .type = BLE_GATT_SVC_TYPE_PRIMARY,
            .uuid = BLE_UUID16_DECLARE(0xff50),
            .characteristics = (struct ble_gatt_chr_def[]) {
                {
                    .uuid = BLE_UUID16_DECLARE(0xff51),
                    .flags = BLE_GATT_CHR_F_NOTIFY,
                    .val_handle = &val_h,
                },
                { 0 }
            }
        },
        { 0 }
    };
    ble_gatts_add_svcs(svcs);

    struct os_mbuf om;
    memset(&om, 0, sizeof(om));
    om.om_data = om.om_databuf;
    const char *custom_data = "CustomAlert";
    os_mbuf_append(&om, custom_data, strlen(custom_data));

    TEST_ASSERT_EQUAL(0, ble_gatts_notify_custom(1, val_h, &om));
    TEST_ASSERT_EQUAL_UINT16(strlen(custom_data), s_tc12_hook_len);
    TEST_ASSERT_EQUAL_UINT16(val_h, s_tc12_hook_val_handle);
    TEST_ASSERT_EQUAL_STRING_LEN(custom_data, (char *)s_tc12_hook_data, s_tc12_hook_len);
}

/* ── TC-BLE-18: UniSim 前端 GATT 树遍历发现 ─────────────────────────────────── */
static void test_ble_unisim_tree_discovery(void) {
    uint16_t val_h1 = 0, val_h2 = 0;
    const struct ble_gatt_svc_def svcs[] = {
        {
            .type = BLE_GATT_SVC_TYPE_PRIMARY,
            .uuid = BLE_UUID16_DECLARE(0x180d),
            .characteristics = (struct ble_gatt_chr_def[]) {
                {
                    .uuid = BLE_UUID16_DECLARE(0x2a37),
                    .flags = BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_READ,
                    .val_handle = &val_h1,
                },
                {
                    .uuid = BLE_UUID16_DECLARE(0x2a38),
                    .flags = BLE_GATT_CHR_F_READ,
                    .val_handle = &val_h2,
                },
                { 0 }
            }
        },
        { 0 }
    };
    ble_gatts_add_svcs(svcs);

    TEST_ASSERT_EQUAL(1, esp_nimble_sim_get_service_count());

    sim_ble_service_info_t sinfo;
    TEST_ASSERT_EQUAL(0, esp_nimble_sim_get_service_info(0, &sinfo));
    TEST_ASSERT_EQUAL(BLE_GATT_SVC_TYPE_PRIMARY, sinfo.type);
    TEST_ASSERT_EQUAL(BLE_UUID_TYPE_16, sinfo.uuid_type);
    uint16_t s_uuid = *(uint16_t *)sinfo.uuid_bytes;
    TEST_ASSERT_EQUAL_HEX16(0x180d, s_uuid);

    TEST_ASSERT_EQUAL(2, esp_nimble_sim_get_char_count(0));

    sim_ble_chr_info_t cinfo;
    TEST_ASSERT_EQUAL(0, esp_nimble_sim_get_char_info(0, 0, &cinfo));
    TEST_ASSERT_EQUAL_UINT16(val_h1, cinfo.val_handle);
    TEST_ASSERT_EQUAL(BLE_UUID_TYPE_16, cinfo.uuid_type);
    uint16_t c1_uuid = *(uint16_t *)cinfo.uuid_bytes;
    TEST_ASSERT_EQUAL_HEX16(0x2a37, c1_uuid);
    TEST_ASSERT_FALSE(cinfo.is_subscribed_notify);

    /* Subscribe and re-query */
    esp_nimble_sim_subscribe(1, val_h1, true, false);
    TEST_ASSERT_EQUAL(0, esp_nimble_sim_get_char_info(0, 0, &cinfo));
    TEST_ASSERT_TRUE(cinfo.is_subscribed_notify);

    TEST_ASSERT_EQUAL(0, esp_nimble_sim_get_char_info(0, 1, &cinfo));
    TEST_ASSERT_EQUAL_UINT16(val_h2, cinfo.val_handle);
    uint16_t c2_uuid = *(uint16_t *)cinfo.uuid_bytes;
    TEST_ASSERT_EQUAL_HEX16(0x2a38, c2_uuid);
}

/* ── Main Runner ───────────────────────────────────────────────────────────── */
int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_ble_port_lifecycle_and_sync);
    RUN_TEST(test_ble_gap_device_name_and_appearance);
    RUN_TEST(test_ble_gatt_capacity_guard);
    RUN_TEST(test_ble_gatt_handle_allocation);
    RUN_TEST(test_ble_uuid_cmp_and_str);
    RUN_TEST(test_ble_gap_adv_and_rsp_flow);
    RUN_TEST(test_ble_virtual_connect_and_mtu);
    RUN_TEST(test_ble_virtual_disconnect_and_replay);
    RUN_TEST(test_ble_gatt_read_chr_callback);
    RUN_TEST(test_ble_gatt_write_chr_and_mbuf_unpack);
    RUN_TEST(test_ble_gatt_permission_guard);
    RUN_TEST(test_ble_subscribe_and_chr_updated);
    RUN_TEST(test_ble_invalid_handle_defense);
    RUN_TEST(test_ble_sim_export_symbols_callable);
    RUN_TEST(test_ble_gatts_register_callback);
    RUN_TEST(test_ble_gap_event_subscribe_bidirectional);
    RUN_TEST(test_ble_gatts_notify_custom);
    RUN_TEST(test_ble_unisim_tree_discovery);

    return UNITY_END();
}
