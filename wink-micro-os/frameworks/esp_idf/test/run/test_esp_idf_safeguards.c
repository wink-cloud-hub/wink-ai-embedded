/* SPDX-License-Identifier: GPL-3.0-only */
#include "unity.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>

#include "sim_responder.h"
#include "esp_partition.h"
#include "esp_partition_sim.h"
#include "esp_vfs_ram.h"
#include "esp_http_server.h"
#include "esp_sntp.h"
#include "esp_netif_sntp.h"
#include "esp_netif.h"
#include "sim_network_broker.h"

int sim_http_server_dispatch_request(const char *method_str, const char *raw_uri, const char *body, size_t body_len);
int sim_http_server_dispatch_request_with_host(const char *method_str, const char *raw_uri, const char *body, size_t body_len, const char *host_header);
int sim_http_server_inject_json(const char *json_str);
int sim_http_server_dispatch_ws_frame(httpd_ws_type_t type, const char *raw_uri, const uint8_t *payload, size_t len);
void sim_http_server_reset(void);
int sim_http_server_get_state(void);
void esp_sntp_sim_reset(void);
void sim_sntp_set_fault(int fault);
void pal_wasm_target_clear_pending_reset(void);

void setUp(void) {
    sim_responder_reset_all();
    esp_partition_sim_reset();
    esp_vfs_ram_reset();
    sim_http_server_reset();
    esp_sntp_sim_reset();
    sim_network_broker_reset();
    esp_netif_init();
}

void tearDown(void) {
    sim_responder_reset_all();
    esp_partition_sim_reset();
    esp_vfs_ram_reset();
    sim_http_server_reset();
    esp_sntp_sim_reset();
    sim_network_broker_reset();
    esp_netif_deinit();
}

void test_sim_responder_at24c02_eeprom(void) {
    static sim_i2c_eeprom_at24c02_t eeprom;
    wink_status_t st = sim_i2c_eeprom_at24c02_init(&eeprom, 0u, 0x50u);
    TEST_ASSERT_EQUAL(WINK_OK, st);

    /* 1. Probe transaction */
    st = sim_responder_dispatch(SIM_BUS_TYPE_I2C, 0u, 0x50u, NULL, 0u, NULL, 0u);
    TEST_ASSERT_EQUAL(WINK_OK, st);

    /* 2. Unregistered device probe returns NOT_FOUND */
    st = sim_responder_dispatch(SIM_BUS_TYPE_I2C, 0u, 0x42u, NULL, 0u, NULL, 0u);
    TEST_ASSERT_EQUAL(WINK_ERR_NOT_FOUND, st);

    /* 3. Write data to EEPROM at address 0x10 */
    uint8_t write_payload[] = { 0x10u, 0xDEu, 0xADu, 0xBEu, 0xEFu };
    st = sim_responder_dispatch(SIM_BUS_TYPE_I2C, 0u, 0x50u,
                                write_payload, sizeof(write_payload), NULL, 0u);
    TEST_ASSERT_EQUAL(WINK_OK, st);

    /* 4. Set read address pointer to 0x10 and read 4 bytes */
    uint8_t addr_select[] = { 0x10u };
    uint8_t read_back[4] = { 0 };
    st = sim_responder_dispatch(SIM_BUS_TYPE_I2C, 0u, 0x50u,
                                addr_select, sizeof(addr_select), read_back, sizeof(read_back));
    TEST_ASSERT_EQUAL(WINK_OK, st);
    TEST_ASSERT_EQUAL_HEX8(0xDE, read_back[0]);
    TEST_ASSERT_EQUAL_HEX8(0xAD, read_back[1]);
    TEST_ASSERT_EQUAL_HEX8(0xBE, read_back[2]);
    TEST_ASSERT_EQUAL_HEX8(0xEF, read_back[3]);
}

void test_esp_partition_sim_in_memory_crud(void) {
    /* 1. Find NVS partition */
    const esp_partition_t *nvs = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                          ESP_PARTITION_SUBTYPE_DATA_NVS,
                                                          "nvs");
    TEST_ASSERT_NOT_NULL(nvs);
    TEST_ASSERT_EQUAL_STRING("nvs", nvs->label);
    TEST_ASSERT_EQUAL_UINT32(0x6000, nvs->size);

    /* 2. Unwritten partition reads as 0xFF */
    uint8_t buf[8] = { 0 };
    esp_err_t err = esp_partition_read(nvs, 0, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(ESP_OK, err);
    for (size_t i = 0; i < sizeof(buf); i++) {
        TEST_ASSERT_EQUAL_HEX8(0xFF, buf[i]);
    }

    /* 3. Write test pattern */
    uint8_t test_data[] = { 0x12, 0x34, 0x56, 0x78 };
    err = esp_partition_write(nvs, 4096, test_data, sizeof(test_data));
    TEST_ASSERT_EQUAL(ESP_OK, err);

    /* 4. Read back */
    uint8_t read_data[4] = { 0 };
    err = esp_partition_read(nvs, 4096, read_data, sizeof(read_data));
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(test_data, read_data, sizeof(test_data));

    /* 5. Erase sector */
    err = esp_partition_erase_range(nvs, 4096, 4096);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = esp_partition_read(nvs, 4096, read_data, sizeof(read_data));
    TEST_ASSERT_EQUAL(ESP_OK, err);
    for (size_t i = 0; i < sizeof(read_data); i++) {
        TEST_ASSERT_EQUAL_HEX8(0xFF, read_data[i]);
    }
}

void test_esp_vfs_ram_pure_memory_sandbox(void) {
    /* 1. Create directory */
    int res = esp_vfs_ram_mkdir("/spiffs", 0755);
    TEST_ASSERT_EQUAL(0, res);

    /* Duplicate mkdir fails with EEXIST */
    res = esp_vfs_ram_mkdir("/spiffs", 0755);
    TEST_ASSERT_EQUAL(-1, res);

    /* 2. Open file with O_CREAT */
    int fd = esp_vfs_ram_open("/spiffs/test.txt", O_CREAT | O_RDWR, 0644);
    TEST_ASSERT_GREATER_OR_EQUAL(3, fd);

    /* 3. Write data */
    const char msg[] = "WinkMicroOS VFS Sandbox";
    ssize_t written = esp_vfs_ram_write(fd, msg, strlen(msg));
    TEST_ASSERT_EQUAL(strlen(msg), (size_t)written);

    /* 4. Seek to start and read */
    off_t pos = esp_vfs_ram_lseek(fd, 0, SEEK_SET);
    TEST_ASSERT_EQUAL(0, pos);

    char read_buf[32] = { 0 };
    ssize_t n_read = esp_vfs_ram_read(fd, read_buf, sizeof(read_buf));
    TEST_ASSERT_EQUAL(strlen(msg), (size_t)n_read);
    TEST_ASSERT_EQUAL_STRING(msg, read_buf);

    /* 5. Close file */
    res = esp_vfs_ram_close(fd);
    TEST_ASSERT_EQUAL(0, res);

    /* 6. Stat file */
    struct stat st;
    res = esp_vfs_ram_stat("/spiffs/test.txt", &st);
    TEST_ASSERT_EQUAL(0, res);
    TEST_ASSERT_EQUAL(strlen(msg), (size_t)st.st_size);

    /* 7. Unlink */
    res = esp_vfs_ram_unlink("/spiffs/test.txt");
    TEST_ASSERT_EQUAL(0, res);

    /* 8. Re-open fails with ENOENT */
    fd = esp_vfs_ram_open("/spiffs/test.txt", O_RDONLY, 0);
    TEST_ASSERT_EQUAL(-1, fd);
}

static httpd_req_t *s_test_ws_req = NULL;

static esp_err_t test_ws_handler(httpd_req_t *req) {
    s_test_ws_req = req;
    return ESP_OK;
}

void test_wave1_r1_websocket_bounds_and_metadata(void) {
    httpd_handle_t hd = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    esp_err_t err = httpd_start(&hd, &config);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_NOT_NULL(hd);

    httpd_uri_t ws_uri = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = test_ws_handler,
        .user_ctx = NULL,
        .is_websocket = true
    };
    err = httpd_register_uri_handler(hd, &ws_uri);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    /* --- W-01: Allocate 17 bytes, declare capacity 16, guard at [16]=0xAA --- */
    {
        const char *payload16 = "0123456789ABCDEF"; /* 16th char is 'F' != 0xAA */
        s_test_ws_req = NULL;
        int status = sim_http_server_dispatch_request("GET", "/ws", payload16, 16);
        TEST_ASSERT_EQUAL(200, status);
        TEST_ASSERT_NOT_NULL(s_test_ws_req);

        uint8_t storage[17];
        memset(storage, 0, sizeof(storage));
        storage[16] = 0xAA;

        httpd_ws_frame_t pkt;
        memset(&pkt, 0, sizeof(pkt));
        pkt.payload = storage;

        err = httpd_ws_recv_frame(s_test_ws_req, &pkt, 16);
        TEST_ASSERT_EQUAL(ESP_OK, err);
        TEST_ASSERT_EQUAL_UINT32(16, pkt.len);
        TEST_ASSERT_EQUAL_UINT32(0, pkt.left_len);
        TEST_ASSERT_EQUAL_MEMORY(payload16, storage, 16);
        TEST_ASSERT_EQUAL_HEX8(0xAA, storage[16]); /* Guard MUST NOT be overwritten with NUL */
    }

    /* --- W-02: 12-byte frame with 8-byte buffer -> ESP_ERR_INVALID_SIZE, no truncation --- */
    {
        const char *payload12 = "HELLO_WORLD!";
        s_test_ws_req = NULL;
        int status = sim_http_server_dispatch_request("GET", "/ws", payload12, 12);
        TEST_ASSERT_EQUAL(200, status);
        TEST_ASSERT_NOT_NULL(s_test_ws_req);

        uint8_t buf8[8] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
        httpd_ws_frame_t pkt;
        memset(&pkt, 0, sizeof(pkt));
        pkt.payload = buf8;

        err = httpd_ws_recv_frame(s_test_ws_req, &pkt, 8);
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, err);
        TEST_ASSERT_EQUAL_UINT32(12, pkt.len); /* Total length remains 12 */
        TEST_ASSERT_EQUAL_HEX8(0xFF, buf8[0]); /* Prefix was not copied */

        /* Retry with sufficient capacity */
        uint8_t buf16[16] = {0};
        pkt.payload = buf16;
        err = httpd_ws_recv_frame(s_test_ws_req, &pkt, 16);
        TEST_ASSERT_EQUAL(ESP_OK, err);
        TEST_ASSERT_EQUAL_UINT32(12, pkt.len);
        TEST_ASSERT_EQUAL_UINT32(0, pkt.left_len);
        TEST_ASSERT_EQUAL_MEMORY(payload12, buf16, 12);
    }

    /* --- W-03: max_len = 0 query metadata without consuming payload --- */
    {
        const char *payload10 = "0123456789";
        s_test_ws_req = NULL;
        int status = sim_http_server_dispatch_request("GET", "/ws", payload10, 10);
        TEST_ASSERT_EQUAL(200, status);
        TEST_ASSERT_NOT_NULL(s_test_ws_req);

        httpd_ws_frame_t pkt;
        memset(&pkt, 0, sizeof(pkt));
        pkt.payload = NULL;

        err = httpd_ws_recv_frame(s_test_ws_req, &pkt, 0);
        TEST_ASSERT_EQUAL(ESP_OK, err);
        TEST_ASSERT_EQUAL_UINT32(10, pkt.len);
        TEST_ASSERT_EQUAL_UINT32(10, pkt.left_len);
        TEST_ASSERT_EQUAL(HTTPD_WS_TYPE_TEXT, pkt.type);
        TEST_ASSERT_TRUE(pkt.final);
        TEST_ASSERT_FALSE(pkt.fragmented);

        /* Consecutive query does not consume */
        err = httpd_ws_recv_frame(s_test_ws_req, &pkt, 0);
        TEST_ASSERT_EQUAL(ESP_OK, err);
        TEST_ASSERT_EQUAL_UINT32(10, pkt.len);
        TEST_ASSERT_EQUAL_UINT32(10, pkt.left_len);
    }

    /* --- W-04: 12 bytes read in 5/5/2 chunks with part API, midway query no rewind --- */
    {
        const char *payload12 = "ABCDEFGHIJKL";
        s_test_ws_req = NULL;
        int status = sim_http_server_dispatch_request("GET", "/ws", payload12, 12);
        TEST_ASSERT_EQUAL(200, status);
        TEST_ASSERT_NOT_NULL(s_test_ws_req);

        uint8_t c1[5] = {0};
        httpd_ws_frame_t pkt;
        memset(&pkt, 0, sizeof(pkt));
        pkt.payload = c1;

        err = httpd_ws_recv_frame_part(s_test_ws_req, &pkt, 5);
        TEST_ASSERT_EQUAL(ESP_OK, err);
        TEST_ASSERT_EQUAL_UINT32(12, pkt.len);
        TEST_ASSERT_EQUAL_UINT32(7, pkt.left_len);
        TEST_ASSERT_EQUAL_MEMORY("ABCDE", c1, 5);

        /* Midway query: max_len = 0 must NOT rewind body_read_pos */
        err = httpd_ws_recv_frame_part(s_test_ws_req, &pkt, 0);
        TEST_ASSERT_EQUAL(ESP_OK, err);
        TEST_ASSERT_EQUAL_UINT32(12, pkt.len);
        TEST_ASSERT_EQUAL_UINT32(7, pkt.left_len);

        /* Second chunk: 5 bytes */
        uint8_t c2[5] = {0};
        pkt.payload = c2;
        err = httpd_ws_recv_frame_part(s_test_ws_req, &pkt, 5);
        TEST_ASSERT_EQUAL(ESP_OK, err);
        TEST_ASSERT_EQUAL_UINT32(12, pkt.len);
        TEST_ASSERT_EQUAL_UINT32(2, pkt.left_len);
        TEST_ASSERT_EQUAL_MEMORY("FGHIJ", c2, 5);

        /* Third chunk: remaining 2 bytes */
        uint8_t c3[5] = {0};
        pkt.payload = c3;
        err = httpd_ws_recv_frame_part(s_test_ws_req, &pkt, 5);
        TEST_ASSERT_EQUAL(ESP_OK, err);
        TEST_ASSERT_EQUAL_UINT32(12, pkt.len);
        TEST_ASSERT_EQUAL_UINT32(0, pkt.left_len);
        TEST_ASSERT_EQUAL_MEMORY("KL", c3, 2);
    }

    /* --- W-05: Empty frame + NULL payload, non-empty frame + NULL payload, NULL req/pkt --- */
    {
        /* NULL req/pkt validation */
        httpd_ws_frame_t pkt;
        memset(&pkt, 0, sizeof(pkt));
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, httpd_ws_recv_frame(NULL, &pkt, 10));
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, httpd_ws_recv_frame_part(NULL, &pkt, 10));
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, httpd_ws_recv_frame(s_test_ws_req, NULL, 10));
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, httpd_ws_recv_frame_part(s_test_ws_req, NULL, 10));

        /* Empty frame */
        s_test_ws_req = NULL;
        int status = sim_http_server_dispatch_request("GET", "/ws", "", 0);
        TEST_ASSERT_EQUAL(200, status);
        TEST_ASSERT_NOT_NULL(s_test_ws_req);

        pkt.payload = NULL;
        err = httpd_ws_recv_frame(s_test_ws_req, &pkt, 10);
        TEST_ASSERT_EQUAL(ESP_OK, err);
        TEST_ASSERT_EQUAL_UINT32(0, pkt.len);
        TEST_ASSERT_EQUAL_UINT32(0, pkt.left_len);

        /* Non-empty frame with NULL payload -> ESP_FAIL */
        status = sim_http_server_dispatch_request("GET", "/ws", "TEST_DATA", 9);
        TEST_ASSERT_EQUAL(200, status);
        pkt.payload = NULL;
        err = httpd_ws_recv_frame(s_test_ws_req, &pkt, 9);
        TEST_ASSERT_EQUAL(ESP_FAIL, err);

        err = httpd_ws_recv_frame_part(s_test_ws_req, &pkt, 5);
        TEST_ASSERT_EQUAL(ESP_FAIL, err);
    }

    /* --- W-06: Embedded NUL bytes, exact capacity, consecutive requests --- */
    {
        const char raw_with_nul[5] = { 'A', '\0', 'B', '\0', 'C' };
        s_test_ws_req = NULL;
        int status = sim_http_server_dispatch_request("GET", "/ws", raw_with_nul, 5);
        TEST_ASSERT_EQUAL(200, status);
        TEST_ASSERT_NOT_NULL(s_test_ws_req);

        uint8_t buf5[5] = {0};
        httpd_ws_frame_t pkt;
        memset(&pkt, 0, sizeof(pkt));
        pkt.payload = buf5;

        err = httpd_ws_recv_frame(s_test_ws_req, &pkt, 5);
        TEST_ASSERT_EQUAL(ESP_OK, err);
        TEST_ASSERT_EQUAL_UINT32(5, pkt.len);
        TEST_ASSERT_EQUAL_UINT32(0, pkt.left_len);
        TEST_ASSERT_EQUAL_MEMORY(raw_with_nul, buf5, 5);

        /* Consecutive new request clears old offset */
        status = sim_http_server_dispatch_request("GET", "/ws", "XYZ", 3);
        TEST_ASSERT_EQUAL(200, status);
        uint8_t buf3[3] = {0};
        pkt.payload = buf3;
        err = httpd_ws_recv_frame(s_test_ws_req, &pkt, 3);
        TEST_ASSERT_EQUAL(ESP_OK, err);
        TEST_ASSERT_EQUAL_UINT32(3, pkt.len);
        TEST_ASSERT_EQUAL_UINT32(0, pkt.left_len);
        TEST_ASSERT_EQUAL_MEMORY("XYZ", buf3, 3);
    }

    httpd_stop(hd);
}

/* --- H-01 ~ H-06 Test Helpers & Handlers --- */
static int s_h1_handler1_calls = 0;
static int s_h1_handler2_calls = 0;
static void *s_h1_last_ctx = NULL;
static int s_h1_err404_1_calls = 0;
static int s_h1_err404_2_calls = 0;

static esp_err_t h1_handler1(httpd_req_t *req) {
    s_h1_handler1_calls++;
    s_h1_last_ctx = req->user_ctx;
    return httpd_resp_sendstr(req, "H1_OLD");
}

static esp_err_t h1_handler2(httpd_req_t *req) {
    s_h1_handler2_calls++;
    s_h1_last_ctx = req->user_ctx;
    return httpd_resp_sendstr(req, "H1_NEW");
}

static esp_err_t h1_err404_1(httpd_req_t *req, httpd_err_code_t err) {
    (void)err;
    s_h1_err404_1_calls++;
    return httpd_resp_send_custom_err(req, HTTPD_404, "ERR404_1");
}

static esp_err_t h1_err404_2(httpd_req_t *req, httpd_err_code_t err) {
    (void)err;
    s_h1_err404_2_calls++;
    return httpd_resp_send_custom_err(req, HTTPD_404, "ERR404_2");
}

static int s_h3_user_free_count = 0;
static int s_h3_trans_free_count = 0;
static esp_err_t s_h3_reentrant_start_err = ESP_OK;
static esp_err_t s_h3_reentrant_stop_err = ESP_OK;
static httpd_handle_t s_h3_active_hd = NULL;

static void h3_user_free_cb(void *ctx) {
    s_h3_user_free_count++;
    (void)ctx;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t dummy = NULL;
    s_h3_reentrant_start_err = httpd_start(&dummy, &cfg);
    s_h3_reentrant_stop_err = httpd_stop(s_h3_active_hd);
}

static void h3_trans_free_cb(void *ctx) {
    s_h3_trans_free_count++;
    (void)ctx;
}

static char s_h4_recorded_host[64];
static esp_err_t h4_whoami_handler(httpd_req_t *req) {
    memset(s_h4_recorded_host, 0, sizeof(s_h4_recorded_host));
    esp_err_t ret = httpd_req_get_hdr_value_str(req, "Host", s_h4_recorded_host, sizeof(s_h4_recorded_host));
    if (ret != ESP_OK) {
        strcpy(s_h4_recorded_host, "NONE");
    }
    return httpd_resp_sendstr(req, s_h4_recorded_host);
}

static int s_h5_patch_calls = 0;
static int s_h5_get_calls = 0;
static int s_h5_err405_calls = 0;
static esp_err_t h5_patch_handler(httpd_req_t *req) {
    s_h5_patch_calls++;
    return httpd_resp_sendstr(req, "PATCH_OK");
}
static esp_err_t h5_get_handler(httpd_req_t *req) {
    s_h5_get_calls++;
    return httpd_resp_sendstr(req, "GET_OK");
}
static esp_err_t h5_err405_handler(httpd_req_t *req, httpd_err_code_t err) {
    (void)err;
    s_h5_err405_calls++;
    return httpd_resp_send_custom_err(req, HTTPD_405, "CUSTOM_405");
}

static size_t s_h6_recv_len = 0;
static char s_h6_recv_buf[128];
static esp_err_t h6_echo_handler(httpd_req_t *req) {
    memset(s_h6_recv_buf, 0, sizeof(s_h6_recv_buf));
    int r = httpd_req_recv(req, s_h6_recv_buf, sizeof(s_h6_recv_buf) - 1);
    s_h6_recv_len = (r > 0) ? (size_t)r : 0;
    return httpd_resp_send(req, s_h6_recv_buf, s_h6_recv_len);
}

void test_wave1_r1_http_server_lifecycle_and_contracts(void) {
    /* --- H-01: Lifecycle restart & handler unregistration --- */
    {
        s_h1_handler1_calls = 0;
        s_h1_handler2_calls = 0;
        s_h1_last_ctx = NULL;
        s_h1_err404_1_calls = 0;
        s_h1_err404_2_calls = 0;

        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        httpd_handle_t hd = NULL;
        TEST_ASSERT_EQUAL(ESP_OK, httpd_start(&hd, &config));

        httpd_uri_t u1 = {
            .uri = "/h1",
            .method = HTTP_GET,
            .handler = h1_handler1,
            .user_ctx = (void *)0x1111
        };
        TEST_ASSERT_EQUAL(ESP_OK, httpd_register_uri_handler(hd, &u1));
        TEST_ASSERT_EQUAL(ESP_OK, httpd_register_err_handler(hd, HTTPD_404_NOT_FOUND, h1_err404_1));

        int st = sim_http_server_dispatch_request("GET", "/h1", NULL, 0);
        TEST_ASSERT_EQUAL(200, st);
        TEST_ASSERT_EQUAL(1, s_h1_handler1_calls);
        TEST_ASSERT_EQUAL_PTR((void *)0x1111, s_h1_last_ctx);

        st = sim_http_server_dispatch_request("GET", "/missing", NULL, 0);
        TEST_ASSERT_EQUAL(404, st);
        TEST_ASSERT_EQUAL(1, s_h1_err404_1_calls);

        /* Stop server */
        TEST_ASSERT_EQUAL(ESP_OK, httpd_stop(hd));

        /* Start server again */
        hd = NULL;
        TEST_ASSERT_EQUAL(ESP_OK, httpd_start(&hd, &config));

        /* Re-register /h1 with new handler and new ctx */
        httpd_uri_t u2 = {
            .uri = "/h1",
            .method = HTTP_GET,
            .handler = h1_handler2,
            .user_ctx = (void *)0x2222
        };
        TEST_ASSERT_EQUAL(ESP_OK, httpd_register_uri_handler(hd, &u2));
        TEST_ASSERT_EQUAL(ESP_OK, httpd_register_err_handler(hd, HTTPD_404_NOT_FOUND, h1_err404_2));

        st = sim_http_server_dispatch_request("GET", "/h1", NULL, 0);
        TEST_ASSERT_EQUAL(200, st);
        TEST_ASSERT_EQUAL(1, s_h1_handler1_calls); /* Old handler NOT called again */
        TEST_ASSERT_EQUAL(1, s_h1_handler2_calls); /* New handler called */
        TEST_ASSERT_EQUAL_PTR((void *)0x2222, s_h1_last_ctx);

        st = sim_http_server_dispatch_request("GET", "/missing", NULL, 0);
        TEST_ASSERT_EQUAL(404, st);
        TEST_ASSERT_EQUAL(1, s_h1_err404_1_calls); /* Old err handler NOT called again */
        TEST_ASSERT_EQUAL(1, s_h1_err404_2_calls); /* New err handler called */

        TEST_ASSERT_EQUAL(ESP_OK, httpd_stop(hd));
    }

    /* --- H-02: Invalid start params, concurrent start, foreign/NULL handle, duplicate stop --- */
    {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        httpd_handle_t hd = NULL;
        httpd_handle_t hd2 = NULL;

        /* NULL handle or NULL config */
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, httpd_start(NULL, &config));
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, httpd_start(&hd, NULL));

        /* Start valid instance */
        TEST_ASSERT_EQUAL(ESP_OK, httpd_start(&hd, &config));

        /* Active server start attempt -> ESP_ERR_HTTPD_ALLOC_MEM */
        TEST_ASSERT_EQUAL(ESP_ERR_HTTPD_ALLOC_MEM, httpd_start(&hd2, &config));

        /* Invalid stop calls */
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, httpd_stop(NULL));
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, httpd_stop((httpd_handle_t)0xdeadbeef));

        /* Verify active instance is not corrupted */
        httpd_uri_t u = { .uri = "/ping", .method = HTTP_GET, .handler = h1_handler1 };
        TEST_ASSERT_EQUAL(ESP_OK, httpd_register_uri_handler(hd, &u));
        int st = sim_http_server_dispatch_request("GET", "/ping", NULL, 0);
        TEST_ASSERT_EQUAL(200, st);

        /* Stop valid instance */
        TEST_ASSERT_EQUAL(ESP_OK, httpd_stop(hd));

        /* Duplicate stop -> ESP_ERR_INVALID_STATE */
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, httpd_stop(hd));
    }

    /* --- H-03: Context free callbacks and reentrancy guard --- */
    {
        s_h3_user_free_count = 0;
        s_h3_trans_free_count = 0;
        s_h3_reentrant_start_err = ESP_OK;
        s_h3_reentrant_stop_err = ESP_OK;

        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.global_user_ctx = (void *)0x8888;
        config.global_user_ctx_free_fn = h3_user_free_cb;
        config.global_transport_ctx = (void *)0x9999;
        config.global_transport_ctx_free_fn = h3_trans_free_cb;

        httpd_handle_t hd = NULL;
        TEST_ASSERT_EQUAL(ESP_OK, httpd_start(&hd, &config));
        s_h3_active_hd = hd;

        /* Register URI with borrowed context */
        httpd_uri_t u = {
            .uri = "/borrowed",
            .method = HTTP_GET,
            .handler = h1_handler1,
            .user_ctx = (void *)0x5555
        };
        TEST_ASSERT_EQUAL(ESP_OK, httpd_register_uri_handler(hd, &u));

        /* Stop should trigger free callbacks exactly once */
        TEST_ASSERT_EQUAL(ESP_OK, httpd_stop(hd));
        TEST_ASSERT_EQUAL(1, s_h3_user_free_count);
        TEST_ASSERT_EQUAL(1, s_h3_trans_free_count);

        /* Reentrancy attempts during callback should have been rejected */
        TEST_ASSERT_NOT_EQUAL(ESP_OK, s_h3_reentrant_start_err);
        TEST_ASSERT_NOT_EQUAL(ESP_OK, s_h3_reentrant_stop_err);
    }

    /* --- H-04: Host header isolation across requests and stop/start --- */
    {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        httpd_handle_t hd = NULL;
        TEST_ASSERT_EQUAL(ESP_OK, httpd_start(&hd, &config));

        httpd_uri_t u = { .uri = "/whoami", .method = HTTP_GET, .handler = h4_whoami_handler };
        TEST_ASSERT_EQUAL(ESP_OK, httpd_register_uri_handler(hd, &u));

        /* 1. Request with Host "api.local" */
        int st = sim_http_server_dispatch_request_with_host("GET", "/whoami", NULL, 0, "api.local");
        TEST_ASSERT_EQUAL(200, st);
        TEST_ASSERT_EQUAL_STRING("api.local", s_h4_recorded_host);

        /* 2. Request without Host -> MUST NOT inherit previous "api.local" */
        st = sim_http_server_dispatch_request("GET", "/whoami", NULL, 0);
        TEST_ASSERT_EQUAL(200, st);
        TEST_ASSERT_EQUAL_STRING("NONE", s_h4_recorded_host);

        /* 3. Request with Host "backup.local" */
        st = sim_http_server_dispatch_request_with_host("GET", "/whoami", NULL, 0, "backup.local");
        TEST_ASSERT_EQUAL(200, st);
        TEST_ASSERT_EQUAL_STRING("backup.local", s_h4_recorded_host);

        /* Stop and restart */
        TEST_ASSERT_EQUAL(ESP_OK, httpd_stop(hd));
        hd = NULL;
        TEST_ASSERT_EQUAL(ESP_OK, httpd_start(&hd, &config));
        TEST_ASSERT_EQUAL(ESP_OK, httpd_register_uri_handler(hd, &u));

        /* 4. Request without Host after restart */
        st = sim_http_server_dispatch_request("GET", "/whoami", NULL, 0);
        TEST_ASSERT_EQUAL(200, st);
        TEST_ASSERT_EQUAL_STRING("NONE", s_h4_recorded_host);

        /* 5. Request with Host "final.local" */
        st = sim_http_server_dispatch_request_with_host("GET", "/whoami", NULL, 0, "final.local");
        TEST_ASSERT_EQUAL(200, st);
        TEST_ASSERT_EQUAL_STRING("final.local", s_h4_recorded_host);

        TEST_ASSERT_EQUAL(ESP_OK, httpd_stop(hd));
    }

    /* --- H-05: PATCH routing, method 405, missing 404, unknown verb 501, invalid json 400 --- */
    {
        s_h5_patch_calls = 0;
        s_h5_get_calls = 0;
        s_h5_err405_calls = 0;

        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        httpd_handle_t hd = NULL;
        TEST_ASSERT_EQUAL(ESP_OK, httpd_start(&hd, &config));

        httpd_uri_t u_patch = { .uri = "/api/res", .method = HTTP_PATCH, .handler = h5_patch_handler };
        httpd_uri_t u_get = { .uri = "/api/res", .method = HTTP_GET, .handler = h5_get_handler };
        TEST_ASSERT_EQUAL(ESP_OK, httpd_register_uri_handler(hd, &u_patch));
        TEST_ASSERT_EQUAL(ESP_OK, httpd_register_uri_handler(hd, &u_get));
        TEST_ASSERT_EQUAL(ESP_OK, httpd_register_err_handler(hd, HTTPD_405_METHOD_NOT_ALLOWED, h5_err405_handler));

        /* PATCH dispatch */
        int st = sim_http_server_dispatch_request("PATCH", "/api/res", NULL, 0);
        TEST_ASSERT_EQUAL(200, st);
        TEST_ASSERT_EQUAL(1, s_h5_patch_calls);
        TEST_ASSERT_EQUAL(0, s_h5_get_calls);

        /* POST to /api/res -> method not registered for URI -> 405 */
        st = sim_http_server_dispatch_request("POST", "/api/res", NULL, 0);
        TEST_ASSERT_EQUAL(405, st);
        TEST_ASSERT_EQUAL(1, s_h5_err405_calls);
        TEST_ASSERT_EQUAL(0, s_h5_get_calls); /* MUST NOT call GET instead! */

        /* Unknown method verb -> 501 */
        st = sim_http_server_dispatch_request("BREW", "/api/res", NULL, 0);
        TEST_ASSERT_EQUAL(501, st);
        TEST_ASSERT_EQUAL(0, s_h5_get_calls);

        /* Non-existent path -> 404 */
        st = sim_http_server_dispatch_request("GET", "/nonexistent", NULL, 0);
        TEST_ASSERT_EQUAL(404, st);

        /* Invalid JSON syntax injection -> 400 */
        st = sim_http_server_inject_json("not valid json");
        TEST_ASSERT_EQUAL(400, st);

        TEST_ASSERT_EQUAL(ESP_OK, httpd_stop(hd));
    }

    /* --- H-06: Sequential requests & stop/start state isolation --- */
    {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        httpd_handle_t hd = NULL;
        TEST_ASSERT_EQUAL(ESP_OK, httpd_start(&hd, &config));

        httpd_uri_t u = { .uri = "/echo", .method = HTTP_POST, .handler = h6_echo_handler };
        TEST_ASSERT_EQUAL(ESP_OK, httpd_register_uri_handler(hd, &u));

        /* Request 1: 50 bytes payload */
        char payload50[50];
        memset(payload50, 'A', sizeof(payload50));
        int st = sim_http_server_dispatch_request("POST", "/echo", payload50, sizeof(payload50));
        TEST_ASSERT_EQUAL(200, st);
        TEST_ASSERT_EQUAL_UINT32(50, s_h6_recv_len);
        TEST_ASSERT_EQUAL_MEMORY(payload50, s_h6_recv_buf, 50);

        /* Stop and restart */
        TEST_ASSERT_EQUAL(ESP_OK, httpd_stop(hd));
        hd = NULL;
        TEST_ASSERT_EQUAL(ESP_OK, httpd_start(&hd, &config));
        TEST_ASSERT_EQUAL(ESP_OK, httpd_register_uri_handler(hd, &u));

        /* Request 2: 10 bytes payload */
        char payload10[10];
        memset(payload10, 'Z', sizeof(payload10));
        st = sim_http_server_dispatch_request("POST", "/echo", payload10, sizeof(payload10));
        TEST_ASSERT_EQUAL(200, st);
        TEST_ASSERT_EQUAL_UINT32(10, s_h6_recv_len);
        TEST_ASSERT_EQUAL_MEMORY(payload10, s_h6_recv_buf, 10);

        TEST_ASSERT_EQUAL(ESP_OK, httpd_stop(hd));
    }
}

/* --------------------------------------------------------------------------
 * Wave 1 R2: SNTP Lifecycle, Fault Injection, and Monotonic Sync
 * -------------------------------------------------------------------------- */
static int s_sntp_cb_called = 0;
static void sntp_test_sync_cb(struct timeval *tv) {
    (void)tv;
    s_sntp_cb_called++;
}

void test_wave1_r2_sntp_lifecycle_fault_and_timezone(void) {
    esp_sntp_sim_reset();
    s_sntp_cb_called = 0;

    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    cfg.sync_cb = sntp_test_sync_cb;
    cfg.start = true;

    TEST_ASSERT_EQUAL(ESP_OK, esp_netif_sntp_init(&cfg));
    TEST_ASSERT_TRUE(esp_sntp_enabled());

    /* Successful sync wait */
    TEST_ASSERT_EQUAL(ESP_OK, esp_netif_sntp_sync_wait(50));
    TEST_ASSERT_EQUAL(1, s_sntp_cb_called);
    TEST_ASSERT_EQUAL_HEX8(0xFF, esp_sntp_getreachability(0));
    TEST_ASSERT_EQUAL(SNTP_SYNC_STATUS_COMPLETED, sntp_get_sync_status());

    /* Fault injection: simulate network timeout */
    esp_sntp_sim_reset();
    s_sntp_cb_called = 0;
    sim_sntp_set_fault(1);
    TEST_ASSERT_EQUAL(ESP_OK, esp_netif_sntp_init(&cfg));
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, esp_netif_sntp_sync_wait(10));
    TEST_ASSERT_EQUAL(0, s_sntp_cb_called);
    TEST_ASSERT_EQUAL_HEX8(0x00, esp_sntp_getreachability(0));

    /* Recovery: clear fault */
    sim_sntp_set_fault(0);
    esp_sntp_sim_reset();
    TEST_ASSERT_EQUAL(ESP_OK, esp_netif_sntp_init(&cfg));
    TEST_ASSERT_EQUAL(ESP_OK, esp_netif_sntp_sync_wait(50));
    TEST_ASSERT_EQUAL(1, s_sntp_cb_called);

    esp_netif_sntp_deinit();
}

/* --------------------------------------------------------------------------
 * Wave 1 R2: HTTP/WS Typed Control Frames and Async Work Queue
 * -------------------------------------------------------------------------- */
static int s_work_exec_count = 0;
static void dummy_http_work(void *arg) {
    int *c = (int *)arg;
    if (c) (*c)++;
}

static httpd_ws_type_t s_last_ws_type = HTTPD_WS_TYPE_CONTINUE;
static size_t s_last_ws_len = 0;
static esp_err_t ws_echo_handler(httpd_req_t *req) {
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(ws_pkt));
    uint8_t buf[128];
    ws_pkt.payload = buf;
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, sizeof(buf));
    if (ret == ESP_OK) {
        s_last_ws_type = ws_pkt.type;
        s_last_ws_len = ws_pkt.len;
        if (ws_pkt.type == HTTPD_WS_TYPE_PING) {
            httpd_ws_frame_t pong = {
                .final = true,
                .fragmented = false,
                .type = HTTPD_WS_TYPE_PONG,
                .payload = ws_pkt.payload,
                .len = ws_pkt.len
            };
            return httpd_ws_send_frame(req, &pong);
        } else {
            return httpd_ws_send_frame(req, &ws_pkt);
        }
    }
    return ret;
}

void test_wave1_r2_http_ws_frames_and_async_work_queue(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t hd = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, httpd_start(&hd, &config));

    /* Work queue validation */
    s_work_exec_count = 0;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, httpd_queue_work(hd, NULL, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, httpd_queue_work(hd, dummy_http_work, &s_work_exec_count));
    TEST_ASSERT_EQUAL(1, s_work_exec_count);

    /* Register WebSocket echo handler */
    httpd_uri_t ws_uri = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = ws_echo_handler,
        .is_websocket = true
    };
    TEST_ASSERT_EQUAL(ESP_OK, httpd_register_uri_handler(hd, &ws_uri));

    /* Dispatch Binary frame */
    uint8_t bin_data[] = { 0x01, 0x02, 0x03, 0x04 };
    s_last_ws_type = HTTPD_WS_TYPE_CONTINUE;
    int st = sim_http_server_dispatch_ws_frame(HTTPD_WS_TYPE_BINARY, "/ws", bin_data, sizeof(bin_data));
    TEST_ASSERT_EQUAL(200, st);
    TEST_ASSERT_EQUAL(HTTPD_WS_TYPE_BINARY, s_last_ws_type);
    TEST_ASSERT_EQUAL(sizeof(bin_data), s_last_ws_len);

    /* Dispatch Ping frame */
    uint8_t ping_data[] = "ping";
    st = sim_http_server_dispatch_ws_frame(HTTPD_WS_TYPE_PING, "/ws", ping_data, 4);
    TEST_ASSERT_EQUAL(200, st);
    TEST_ASSERT_EQUAL(HTTPD_WS_TYPE_PING, s_last_ws_type);

    /* Stop server */
    TEST_ASSERT_EQUAL(ESP_OK, httpd_stop(hd));

    /* Attempting work queue after stop must be rejected with ESP_ERR_INVALID_STATE */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, httpd_queue_work(hd, dummy_http_work, &s_work_exec_count));
}

/* --------------------------------------------------------------------------
 * Wave 1 R2: SoftAP+STA Routing and Netif Data Plane
 * -------------------------------------------------------------------------- */
void test_wave1_r2_network_broker_routing_and_netifs(void) {
    sim_network_broker_reset();
    TEST_ASSERT_FALSE(sim_network_broker_is_ready());

    esp_netif_t *sta = esp_netif_create_default_wifi_sta();
    esp_netif_t *ap  = esp_netif_create_default_wifi_ap();
    TEST_ASSERT_NOT_NULL(sta);
    TEST_ASSERT_NOT_NULL(ap);

    sim_network_broker_set_netif_ready(sta, true);
    sim_network_broker_set_netif_ready(ap, true);
    TEST_ASSERT_TRUE(sim_network_broker_is_netif_ready(sta));
    TEST_ASSERT_TRUE(sim_network_broker_is_netif_ready(ap));

    /* Route simulated packet from SoftAP client across STA to upstream broker */
    uint8_t frame[] = "SoftAP-to-STA-Forwarded-Packet";
    int routed = sim_network_broker_route_packet(ap, sta, frame, sizeof(frame));
    TEST_ASSERT_EQUAL((int)sizeof(frame), routed);
    TEST_ASSERT_EQUAL(sizeof(frame), sim_network_broker_get_routed_bytes());
    TEST_ASSERT_EQUAL(1, sim_network_broker_get_routed_packets());

    /* Disable routing and observe intentional drop */
    sim_network_broker_set_routing(false);
    TEST_ASSERT_FALSE(sim_network_broker_is_routing_enabled());
    routed = sim_network_broker_route_packet(ap, sta, frame, sizeof(frame));
    TEST_ASSERT_EQUAL(-1, routed);

    /* Re-enable and reset */
    sim_network_broker_set_routing(true);
    sim_network_broker_reset();
    TEST_ASSERT_EQUAL(0, sim_network_broker_get_routed_bytes());
}

/* --------------------------------------------------------------------------
 * Wave 1 R4: Composite Network Stack Application (SoftAP + STA + HTTP/WS + SNTP)
 * -------------------------------------------------------------------------- */
void test_wave1_r4_composite_mesh_application(void) {
    /* 1. Initialize composite network stack: SoftAP + STA + HTTP Server + SNTP */
    sim_network_broker_reset();
    esp_sntp_sim_reset();
    sim_http_server_reset();

    esp_netif_t *sta = esp_netif_create_default_wifi_sta();
    esp_netif_t *ap  = esp_netif_create_default_wifi_ap();
    sim_network_broker_set_netif_ready(sta, true);
    sim_network_broker_set_netif_ready(ap, true);

    /* 2. Synchronize SNTP time while AP and STA are up */
    esp_sntp_config_t sntp_cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("time.google.com");
    sntp_cfg.start = true;
    TEST_ASSERT_EQUAL(ESP_OK, esp_netif_sntp_init(&sntp_cfg));
    TEST_ASSERT_EQUAL(ESP_OK, esp_netif_sntp_sync_wait(50));
    TEST_ASSERT_EQUAL(SNTP_SYNC_STATUS_COMPLETED, sntp_get_sync_status());

    /* 3. Start HTTP/WS server and serve requests */
    httpd_config_t http_cfg = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t hd = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, httpd_start(&hd, &http_cfg));

    httpd_uri_t echo_uri = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = ws_echo_handler
    };
    TEST_ASSERT_EQUAL(ESP_OK, httpd_register_uri_handler(hd, &echo_uri));

    int st = sim_http_server_dispatch_request("GET", "/api/status", NULL, 0);
    TEST_ASSERT_EQUAL(200, st);

    /* 4. Forward packets through data plane */
    uint8_t payload[] = "MESH-DATA-PACKET";
    TEST_ASSERT_EQUAL((int)sizeof(payload), sim_network_broker_route_packet(ap, sta, payload, sizeof(payload)));

    /* 5. Trigger Reset DAG: all subsystems must return to clean baseline */
    TEST_ASSERT_EQUAL(ESP_OK, httpd_stop(hd));
    pal_wasm_target_clear_pending_reset();

    TEST_ASSERT_EQUAL(0, sim_http_server_get_state());
    TEST_ASSERT_EQUAL(SNTP_SYNC_STATUS_RESET, sntp_get_sync_status());
    TEST_ASSERT_EQUAL(0, sim_network_broker_get_routed_bytes());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sim_responder_at24c02_eeprom);
    RUN_TEST(test_esp_partition_sim_in_memory_crud);
    RUN_TEST(test_esp_vfs_ram_pure_memory_sandbox);
    RUN_TEST(test_wave1_r1_websocket_bounds_and_metadata);
    RUN_TEST(test_wave1_r1_http_server_lifecycle_and_contracts);
    RUN_TEST(test_wave1_r2_sntp_lifecycle_fault_and_timezone);
    RUN_TEST(test_wave1_r2_http_ws_frames_and_async_work_queue);
    RUN_TEST(test_wave1_r2_network_broker_routing_and_netifs);
    RUN_TEST(test_wave1_r4_composite_mesh_application);
    return UNITY_END();
}
