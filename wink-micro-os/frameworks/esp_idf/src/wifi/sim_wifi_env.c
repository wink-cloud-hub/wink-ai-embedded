/* SPDX-License-Identifier: LGPL-3.0-only */
#include "sim_wifi_env.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static sim_wifi_ap_t s_aps[SIM_WIFI_MAX_APS];
static bool s_custom_injected = false;
static wifi_ap_record_t s_scan_records[SIM_WIFI_MAX_APS];
static uint16_t s_scan_count = 0;
static bool s_scan_consumed = true;

static int s_sta_state = 0;
static esp_ip4_addr_t s_current_sta_ip = {0};

static sim_wifi_beacon_drop_cb_t s_beacon_drop_cb = NULL;

static void init_default_aps(void) {
    memset(s_aps, 0, sizeof(s_aps));

    /* AP 0: matches official wifi_sta carrier sdkconfig.h (myssid / mypassword) */
    s_aps[0].in_use = true;
    strncpy(s_aps[0].ssid, "myssid", sizeof(s_aps[0].ssid) - 1);
    strncpy(s_aps[0].password, "mypassword", sizeof(s_aps[0].password) - 1);
    s_aps[0].channel = 1;
    s_aps[0].rssi = -45;
    s_aps[0].authmode = WIFI_AUTH_WPA2_PSK;
    s_aps[0].bssid[0] = 0x00; s_aps[0].bssid[1] = 0x11; s_aps[0].bssid[2] = 0x22;
    s_aps[0].bssid[3] = 0x33; s_aps[0].bssid[4] = 0x44; s_aps[0].bssid[5] = 0x55;
    s_aps[0].dhcp_info.ip.addr = ESP_IP4TOADDR(192, 168, 1, 100);
    s_aps[0].dhcp_info.netmask.addr = ESP_IP4TOADDR(255, 255, 255, 0);
    s_aps[0].dhcp_info.gw.addr = ESP_IP4TOADDR(192, 168, 1, 1);

    /* AP 1: matches existing test_esp_wifi.c unit tests (TestSSID / TestPass) */
    s_aps[1].in_use = true;
    strncpy(s_aps[1].ssid, "TestSSID", sizeof(s_aps[1].ssid) - 1);
    strncpy(s_aps[1].password, "TestPass", sizeof(s_aps[1].password) - 1);
    s_aps[1].channel = 6;
    s_aps[1].rssi = -50;
    s_aps[1].authmode = WIFI_AUTH_WPA2_PSK;
    s_aps[1].bssid[0] = 0x00; s_aps[1].bssid[1] = 0x11; s_aps[1].bssid[2] = 0x22;
    s_aps[1].bssid[3] = 0x33; s_aps[1].bssid[4] = 0x44; s_aps[1].bssid[5] = 0x60;
    s_aps[1].dhcp_info.ip.addr = ESP_IP4TOADDR(192, 168, 4, 2);
    s_aps[1].dhcp_info.netmask.addr = ESP_IP4TOADDR(255, 255, 255, 0);
    s_aps[1].dhcp_info.gw.addr = ESP_IP4TOADDR(192, 168, 4, 1);

    /* AP 2: open AP for scan tests */
    s_aps[2].in_use = true;
    strncpy(s_aps[2].ssid, "Nearby_AP", sizeof(s_aps[2].ssid) - 1);
    s_aps[2].password[0] = '\0';
    s_aps[2].channel = 6;
    s_aps[2].rssi = -75;
    s_aps[2].authmode = WIFI_AUTH_OPEN;
    s_aps[2].bssid[0] = 0x00; s_aps[2].bssid[1] = 0x11; s_aps[2].bssid[2] = 0x22;
    s_aps[2].bssid[3] = 0x33; s_aps[2].bssid[4] = 0x44; s_aps[2].bssid[5] = 0x56;
    s_aps[2].dhcp_info.ip.addr = ESP_IP4TOADDR(192, 168, 6, 2);
    s_aps[2].dhcp_info.netmask.addr = ESP_IP4TOADDR(255, 255, 255, 0);
    s_aps[2].dhcp_info.gw.addr = ESP_IP4TOADDR(192, 168, 6, 1);
}

void sim_wifi_env_reset(void) {
    s_custom_injected = false;
    init_default_aps();
    s_scan_count = 0;
    s_scan_consumed = true;
    s_sta_state = 0;
    s_current_sta_ip.addr = 0;
    s_beacon_drop_cb = NULL;
}

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

static bool parse_mac_hex(const char *str, uint8_t mac[6]) {
    if (!str || strlen(str) < 17) return false;
    int values[6];
    int parsed = sscanf(str, "%x:%x:%x:%x:%x:%x",
                        &values[0], &values[1], &values[2],
                        &values[3], &values[4], &values[5]);
    if (parsed == 6) {
        for (int i = 0; i < 6; i++) {
            mac[i] = (uint8_t)values[i];
        }
        return true;
    }
    return false;
}

static bool parse_ip_dots(const char *str, esp_ip4_addr_t *out) {
    if (!str || !out) return false;
    int a = 0, b = 0, c = 0, d = 0;
    if (sscanf(str, "%d.%d.%d.%d", &a, &b, &c, &d) == 4) {
        out->addr = ESP_IP4TOADDR(a, b, c, d);
        return true;
    }
    return false;
}

static wifi_auth_mode_t parse_auth_mode_str(const char *str) {
    if (!str) return WIFI_AUTH_WPA2_PSK;
    if (strstr(str, "OPEN")) return WIFI_AUTH_OPEN;
    if (strstr(str, "WEP")) return WIFI_AUTH_WEP;
    if (strstr(str, "WPA3_PSK")) return WIFI_AUTH_WPA3_PSK;
    if (strstr(str, "WPA2_PSK")) return WIFI_AUTH_WPA2_PSK;
    if (strstr(str, "WPA_PSK")) return WIFI_AUTH_WPA_PSK;
    return WIFI_AUTH_WPA2_PSK;
}

static int parse_single_ap_obj(const char *obj_start, const char *obj_end, sim_wifi_ap_t *ap) {
    size_t len = (size_t)(obj_end - obj_start);
    if (len >= 1024) len = 1023;
    char buf[1024];
    memcpy(buf, obj_start, len);
    buf[len] = '\0';

    char ssid[33] = {0};
    if (!json_get_str(buf, "ssid", ssid, sizeof(ssid))) {
        return -1;
    }
    memset(ap, 0, sizeof(*ap));
    ap->in_use = true;
    strncpy(ap->ssid, ssid, sizeof(ap->ssid) - 1);

    char pwd[65] = {0};
    if (json_get_str(buf, "password", pwd, sizeof(pwd))) {
        strncpy(ap->password, pwd, sizeof(ap->password) - 1);
    }

    char bssid_str[32] = {0};
    if (json_get_str(buf, "bssid", bssid_str, sizeof(bssid_str))) {
        parse_mac_hex(bssid_str, ap->bssid);
    }

    int rssi = -50;
    if (json_get_int(buf, "rssi", &rssi)) {
        ap->rssi = (int8_t)rssi;
    } else {
        ap->rssi = -50;
    }

    int channel = 1;
    if (json_get_int(buf, "channel", &channel)) {
        ap->channel = (uint8_t)channel;
    } else {
        ap->channel = 1;
    }

    char auth[32] = {0};
    if (json_get_str(buf, "authMode", auth, sizeof(auth))) {
        ap->authmode = parse_auth_mode_str(auth);
    } else {
        ap->authmode = WIFI_AUTH_WPA2_PSK;
    }

    /* DHCP */
    char ip_str[32] = {0};
    if (json_get_str(buf, "assignedIp", ip_str, sizeof(ip_str))) {
        parse_ip_dots(ip_str, &ap->dhcp_info.ip);
    }
    char mask_str[32] = {0};
    if (json_get_str(buf, "netmask", mask_str, sizeof(mask_str))) {
        parse_ip_dots(mask_str, &ap->dhcp_info.netmask);
    }
    char gw_str[32] = {0};
    if (json_get_str(buf, "gateway", gw_str, sizeof(gw_str))) {
        parse_ip_dots(gw_str, &ap->dhcp_info.gw);
    }

    return 0;
}

int sim_wifi_env_inject_ap(const char *json_str) {
    if (!json_str) return -1;

    const char *array_start = strstr(json_str, "\"accessPoints\"");
    if (array_start) {
        /* Multi-AP array */
        const char *p = strchr(array_start, '[');
        if (!p) return -1;
        p++;

        if (!s_custom_injected) {
            memset(s_aps, 0, sizeof(s_aps));
            s_custom_injected = true;
        }

        size_t count = 0;
        while (*p && count < SIM_WIFI_MAX_APS) {
            while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ',') p++;
            if (*p == ']' || *p == '\0') break;
            if (*p == '{') {
                const char *obj_start = p;
                int brace_depth = 1;
                p++;
                while (*p && brace_depth > 0) {
                    if (*p == '{') brace_depth++;
                    else if (*p == '}') brace_depth--;
                    p++;
                }
                const char *obj_end = p;
                if (parse_single_ap_obj(obj_start, obj_end, &s_aps[count]) == 0) {
                    count++;
                }
            } else {
                p++;
            }
        }
        return (count > 0) ? 0 : -1;
    } else {
        /* Single AP object */
        if (!s_custom_injected) {
            memset(s_aps, 0, sizeof(s_aps));
            s_custom_injected = true;
        }
        size_t slot = 0;
        for (; slot < SIM_WIFI_MAX_APS; slot++) {
            if (!s_aps[slot].in_use) break;
        }
        if (slot >= SIM_WIFI_MAX_APS) slot = SIM_WIFI_MAX_APS - 1;
        return parse_single_ap_obj(json_str, json_str + strlen(json_str), &s_aps[slot]);
    }
}

int sim_wifi_env_inject_fault(const char *json_str) {
    if (!json_str) return -1;
    char target_ssid[33] = {0};
    char action[32] = {0};
    char reason_str[32] = {0};

    json_get_str(json_str, "targetSsid", target_ssid, sizeof(target_ssid));
    json_get_str(json_str, "action", action, sizeof(action));
    json_get_str(json_str, "reason", reason_str, sizeof(reason_str));

    uint8_t reason_code = WIFI_REASON_BEACON_TIMEOUT;
    if (strstr(reason_str, "AUTH_FAIL")) {
        reason_code = WIFI_REASON_AUTH_FAIL;
    } else if (strstr(reason_str, "4WAY") || strstr(reason_str, "HANDSHAKE")) {
        reason_code = WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT;
    } else if (strstr(reason_str, "NO_AP")) {
        reason_code = WIFI_REASON_NO_AP_FOUND;
    }

    if (strcmp(action, "DROP_BEACON") == 0 || action[0] == '\0') {
        for (size_t i = 0; i < SIM_WIFI_MAX_APS; i++) {
            if (s_aps[i].in_use && (target_ssid[0] == '\0' || strcmp(s_aps[i].ssid, target_ssid) == 0)) {
                s_aps[i].drop_beacon = true;
                s_aps[i].fault_reason = reason_code;
            }
        }
        if (s_beacon_drop_cb) {
            s_beacon_drop_cb(target_ssid, reason_code);
        }
        return 0;
    }
    return -1;
}

const sim_wifi_ap_t* sim_wifi_env_find_ap_by_ssid(const char *ssid) {
    if (!ssid || ssid[0] == '\0') return NULL;
    if (!s_custom_injected && !s_aps[0].in_use) {
        init_default_aps();
    }
    for (size_t i = 0; i < SIM_WIFI_MAX_APS; i++) {
        if (s_aps[i].in_use && strcmp(s_aps[i].ssid, ssid) == 0) {
            return &s_aps[i];
        }
    }
    return NULL;
}

esp_err_t sim_wifi_env_scan(const wifi_scan_config_t *config) {
    if (!s_custom_injected && !s_aps[0].in_use) {
        init_default_aps();
    }

    memset(s_scan_records, 0, sizeof(s_scan_records));
    s_scan_count = 0;

    const char *filter_ssid = (config && config->ssid) ? (const char*)config->ssid : NULL;
    uint8_t filter_channel = (config) ? config->channel : 0;

    for (size_t i = 0; i < SIM_WIFI_MAX_APS; i++) {
        if (!s_aps[i].in_use || s_aps[i].drop_beacon) continue;
        if (filter_ssid && filter_ssid[0] != '\0' && strcmp(s_aps[i].ssid, filter_ssid) != 0) {
            continue;
        }
        if (filter_channel > 0 && s_aps[i].channel != filter_channel) {
            continue;
        }

        wifi_ap_record_t *rec = &s_scan_records[s_scan_count];
        memcpy(rec->bssid, s_aps[i].bssid, 6);
        strncpy((char*)rec->ssid, s_aps[i].ssid, sizeof(rec->ssid) - 1);
        rec->primary = s_aps[i].channel;
        rec->rssi = s_aps[i].rssi;
        rec->authmode = s_aps[i].authmode;
        s_scan_count++;
    }

    /* Stable sort by RSSI descending (higher RSSI first: -45 > -75) */
    for (size_t i = 1; i < s_scan_count; i++) {
        wifi_ap_record_t key = s_scan_records[i];
        int j = (int)i - 1;
        while (j >= 0 && s_scan_records[j].rssi < key.rssi) {
            s_scan_records[j + 1] = s_scan_records[j];
            j--;
        }
        s_scan_records[j + 1] = key;
    }

    s_scan_consumed = false;
    return ESP_OK;
}

uint16_t sim_wifi_env_get_scan_num(void) {
    if (s_scan_consumed) {
        return 0;
    }
    return s_scan_count;
}

esp_err_t sim_wifi_env_get_scan_records(uint16_t *number, wifi_ap_record_t *ap_records) {
    if (!number || !ap_records) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_scan_consumed || s_scan_count == 0) {
        *number = 0;
        return ESP_ERR_WIFI_NOT_INIT;
    }
    uint16_t to_copy = (*number < s_scan_count) ? *number : s_scan_count;
    memcpy(ap_records, s_scan_records, to_copy * sizeof(wifi_ap_record_t));
    *number = to_copy;
    s_scan_consumed = true;
    return ESP_OK;
}

const char* sim_wifi_env_get_state_str(void) {
    switch (s_sta_state) {
        case 0: return "OFF";
        case 1: return "INIT";
        case 2: return "STARTED";
        case 3: return "CONNECTING";
        case 4: return "CONNECTED";
        case 5: return "GOT_IP";
        case 6: return "DISCONNECTED";
        default: return "UNKNOWN";
    }
}

void sim_wifi_env_set_state(int state) {
    s_sta_state = state;
}

void sim_wifi_env_get_sta_ip_str(char *buf, size_t maxlen) {
    if (!buf || maxlen == 0) return;
    const uint8_t *p = (const uint8_t*)&s_current_sta_ip.addr;
    snprintf(buf, maxlen, "%u.%u.%u.%u", p[0], p[1], p[2], p[3]);
}

static char s_sta_ip_str_buf[32];
const char* sim_wifi_env_get_sta_ip_ptr(void) {
    const uint8_t *p = (const uint8_t*)&s_current_sta_ip.addr;
    snprintf(s_sta_ip_str_buf, sizeof(s_sta_ip_str_buf), "%u.%u.%u.%u", p[0], p[1], p[2], p[3]);
    return s_sta_ip_str_buf;
}

void sim_wifi_env_set_sta_ip(esp_ip4_addr_t ip) {
    s_current_sta_ip = ip;
}

void sim_wifi_env_set_beacon_drop_cb(sim_wifi_beacon_drop_cb_t cb) {
    s_beacon_drop_cb = cb;
}
