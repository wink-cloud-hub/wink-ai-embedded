/* SPDX-License-Identifier: LGPL-3.0-only */
#include "esp_log.h"
#include "esp_err.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "os/os_mbuf.h"
#include "esp_sim_fault.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

static const char *TAG = "esp_nimble";

#define SIM_BLE_MAX_SERVICES    8
#define SIM_BLE_MAX_CHRS        32
#define SIM_BLE_MAX_DSCS        32
#define SIM_BLE_MAX_NAME_LEN    32
#define SIM_BLE_MAX_VALUE_LEN   128

typedef struct {
    uint16_t handle;
    uint16_t val_handle;
    const ble_uuid_t *uuid;
    ble_gatt_access_fn *access_cb;
    void *arg;
    uint16_t flags;
    uint16_t *val_handle_ptr;
    const struct ble_gatt_chr_def *def;
    uint8_t value[SIM_BLE_MAX_VALUE_LEN];
    uint16_t value_len;
    bool is_subscribed_notify;
    bool is_subscribed_indicate;
    uint8_t svc_index;
} sim_ble_chr_record_t;

typedef struct {
    uint8_t type;
    const ble_uuid_t *uuid;
    uint16_t start_handle;
    uint16_t end_handle;
} sim_ble_svc_record_t;

typedef struct {
    uint16_t handle;
    const ble_uuid_t *uuid;
    ble_gatt_access_fn *access_cb;
    void *arg;
    uint8_t att_flags;
    uint8_t chr_index;
} sim_ble_dsc_record_t;

typedef struct {
    bool is_initialized;
    bool is_advertising;
    bool is_connected;
    char device_name[SIM_BLE_MAX_NAME_LEN];
    uint16_t device_appearance;

    /* GAP advertisement parameters & state */
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields adv_fields;
    struct ble_hs_adv_fields rsp_fields;
    ble_gap_event_fn *adv_cb;
    void *adv_cb_arg;

    /* GATT services and characteristics */
    sim_ble_svc_record_t svcs[SIM_BLE_MAX_SERVICES];
    uint8_t num_svcs;
    sim_ble_chr_record_t chrs[SIM_BLE_MAX_CHRS];
    uint8_t num_chrs;
    sim_ble_dsc_record_t dscs[SIM_BLE_MAX_DSCS];
    uint8_t num_dscs;
    uint16_t next_handle;

    /* Single virtual connection */
    struct ble_gap_conn_desc conn;

    /* UniSim hook */
    esp_nimble_sim_notify_hook_t notify_hook;
} sim_ble_state_t;

static sim_ble_state_t s_ble_state;
struct ble_hs_cfg ble_hs_cfg = {0};

/* ── UUID utilities ────────────────────────────────────────────────────────── */

int ble_uuid_cmp(const ble_uuid_t *a, const ble_uuid_t *b) {
    if (!a || !b) return (a == b) ? 0 : (a ? 1 : -1);
    if (a->type == b->type) {
        if (a->type == BLE_UUID_TYPE_16) {
            uint16_t va = ((const ble_uuid16_t *)a)->value;
            uint16_t vb = ((const ble_uuid16_t *)b)->value;
            return (va == vb) ? 0 : (va < vb ? -1 : 1);
        } else if (a->type == BLE_UUID_TYPE_32) {
            uint32_t va = ((const ble_uuid32_t *)a)->value;
            uint32_t vb = ((const ble_uuid32_t *)b)->value;
            return (va == vb) ? 0 : (va < vb ? -1 : 1);
        } else if (a->type == BLE_UUID_TYPE_128) {
            return memcmp(((const ble_uuid128_t *)a)->value,
                          ((const ble_uuid128_t *)b)->value, 16);
        }
    }

    static const uint8_t ble_base_uuid[16] = {
        0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
        0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    if (a->type == BLE_UUID_TYPE_16 && b->type == BLE_UUID_TYPE_128) {
        uint16_t va = ((const ble_uuid16_t *)a)->value;
        uint8_t a128[16];
        memcpy(a128, ble_base_uuid, 16);
        a128[12] = (uint8_t)(va & 0xff);
        a128[13] = (uint8_t)((va >> 8) & 0xff);
        return memcmp(a128, ((const ble_uuid128_t *)b)->value, 16);
    } else if (a->type == BLE_UUID_TYPE_128 && b->type == BLE_UUID_TYPE_16) {
        uint16_t vb = ((const ble_uuid16_t *)b)->value;
        uint8_t b128[16];
        memcpy(b128, ble_base_uuid, 16);
        b128[12] = (uint8_t)(vb & 0xff);
        b128[13] = (uint8_t)((vb >> 8) & 0xff);
        return memcmp(((const ble_uuid128_t *)a)->value, b128, 16);
    }
    return (a->type < b->type) ? -1 : 1;
}

char *ble_uuid_to_str(const ble_uuid_t *uuid, char *dst) {
    if (!uuid || !dst) return dst;
    if (uuid->type == BLE_UUID_TYPE_16) {
        snprintf(dst, BLE_UUID_STR_LEN, "0x%04x", ((const ble_uuid16_t *)uuid)->value);
    } else if (uuid->type == BLE_UUID_TYPE_32) {
        snprintf(dst, BLE_UUID_STR_LEN, "0x%08x", (unsigned int)((const ble_uuid32_t *)uuid)->value);
    } else if (uuid->type == BLE_UUID_TYPE_128) {
        const uint8_t *u8 = ((const ble_uuid128_t *)uuid)->value;
        snprintf(dst, BLE_UUID_STR_LEN,
                 "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                 u8[15], u8[14], u8[13], u8[12], u8[11], u8[10], u8[9], u8[8],
                 u8[7], u8[6], u8[5], u8[4], u8[3], u8[2], u8[1], u8[0]);
    } else {
        dst[0] = '\0';
    }
    return dst;
}

uint16_t ble_uuid_u16(const ble_uuid_t *uuid) {
    if (uuid && uuid->type == BLE_UUID_TYPE_16) {
        return ((const ble_uuid16_t *)uuid)->value;
    }
    return 0;
}

/* ── Host Core API ─────────────────────────────────────────────────────────── */

int ble_hs_init(void) {
    if (!s_ble_state.is_initialized) {
        nimble_port_init();
    }
    return 0;
}

int ble_hs_is_enabled(void) {
    return s_ble_state.is_initialized ? 1 : 0;
}

int ble_hs_mbuf_to_flat(const struct os_mbuf *om, void *flat, uint16_t max_len, uint16_t *out_len) {
    if (!om || !flat) return BLE_HS_EINVAL;
    uint16_t copy_len = om->om_len < max_len ? om->om_len : max_len;
    memcpy(flat, om->om_databuf, copy_len);
    if (out_len) {
        *out_len = copy_len;
    }
    return 0;
}

struct os_mbuf *ble_hs_mbuf_from_flat(const void *buf, uint16_t len) {
    static struct os_mbuf s_flat_mbuf;
    memset(&s_flat_mbuf, 0, sizeof(s_flat_mbuf));
    s_flat_mbuf.om_data = s_flat_mbuf.om_databuf;
    os_mbuf_append(&s_flat_mbuf, buf, len);
    return &s_flat_mbuf;
}

int ble_hs_id_infer_auto(int privacy, uint8_t *out_own_addr_type) {
    (void)privacy;
    if (out_own_addr_type) {
        *out_own_addr_type = BLE_ADDR_PUBLIC;
    }
    return 0;
}

int ble_hs_util_ensure_addr(int prefer_random) {
    (void)prefer_random;
    return 0;
}

/* ── NimBLE Port API ───────────────────────────────────────────────────────── */

esp_err_t nimble_port_init(void) {
    memset(&s_ble_state, 0, sizeof(s_ble_state));
    s_ble_state.is_initialized = true;
    s_ble_state.next_handle = 1;
    strncpy(s_ble_state.device_name, "Wink-BLE", sizeof(s_ble_state.device_name) - 1);

    s_ble_state.conn.conn_handle = 1;
    s_ble_state.conn.role = 0;
    s_ble_state.conn.conn_itvl = 16;
    s_ble_state.conn.conn_latency = 0;
    s_ble_state.conn.supervision_timeout = 256;
    return ESP_OK;
}

esp_err_t nimble_port_deinit(void) {
    s_ble_state.is_initialized = false;
    s_ble_state.is_advertising = false;
    s_ble_state.is_connected = false;
    return ESP_OK;
}

void nimble_port_run(void) {
    /* Cooperative scheduler: non-blocking single pulse */
    if (ble_hs_cfg.sync_cb) {
        ble_hs_cfg.sync_cb();
    }
}

esp_err_t nimble_port_stop(void) {
    return ESP_OK;
}

void nimble_port_freertos_init(void (*host_task_fn)(void *param)) {
    if (host_task_fn) {
        host_task_fn(NULL);
    }
}

void nimble_port_freertos_deinit(void) {
}

/* ── Standard GAP Services ─────────────────────────────────────────────────── */

void ble_svc_gap_init(void) {
}

int ble_svc_gap_device_name_set(const char *name) {
    if (!name) return BLE_HS_EINVAL;
    strncpy(s_ble_state.device_name, name, sizeof(s_ble_state.device_name) - 1);
    s_ble_state.device_name[sizeof(s_ble_state.device_name) - 1] = '\0';
    return 0;
}

const char *ble_svc_gap_device_name(void) {
    return s_ble_state.device_name;
}

int ble_svc_gap_device_appearance_set(uint16_t appearance) {
    s_ble_state.device_appearance = appearance;
    return 0;
}

uint16_t ble_svc_gap_device_appearance(void) {
    return s_ble_state.device_appearance;
}

void ble_svc_gatt_init(void) {
}

/* ── GATT Server API ───────────────────────────────────────────────────────── */

int ble_gatts_count_cfg(const struct ble_gatt_svc_def *defs) {
    if (!defs) return 0;
    uint8_t svcs_count = 0;
    uint8_t chrs_count = 0;
    uint8_t dscs_count = 0;
    for (const struct ble_gatt_svc_def *svc = defs; svc->type != BLE_GATT_SVC_TYPE_END && svc->uuid != NULL; svc++) {
        svcs_count++;
        if (svc->characteristics) {
            for (const struct ble_gatt_chr_def *chr = svc->characteristics; chr->uuid != NULL; chr++) {
                chrs_count++;
                if (chr->descriptors) {
                    for (const struct ble_gatt_dsc_def *dsc = chr->descriptors; dsc->uuid != NULL; dsc++) {
                        dscs_count++;
                    }
                }
            }
        }
    }
    if ((uint32_t)s_ble_state.num_svcs + svcs_count > SIM_BLE_MAX_SERVICES ||
        (uint32_t)s_ble_state.num_chrs + chrs_count > SIM_BLE_MAX_CHRS ||
        (uint32_t)s_ble_state.num_dscs + dscs_count > SIM_BLE_MAX_DSCS) {
        ESP_LOGE(TAG, "GATT capacity exceeded (svcs: %u+%u > %d, chrs: %u+%u > %d, dscs: %u+%u > %d)",
                 s_ble_state.num_svcs, svcs_count, SIM_BLE_MAX_SERVICES,
                 s_ble_state.num_chrs, chrs_count, SIM_BLE_MAX_CHRS,
                 s_ble_state.num_dscs, dscs_count, SIM_BLE_MAX_DSCS);
        return BLE_HS_ENOMEM;
    }
    return 0;
}

int ble_gatts_add_svcs(const struct ble_gatt_svc_def *svcs) {
    if (!svcs) return 0;
    int rc = ble_gatts_count_cfg(svcs);
    if (rc != 0) return rc;

    for (const struct ble_gatt_svc_def *svc = svcs; svc->type != BLE_GATT_SVC_TYPE_END && svc->uuid != NULL; svc++) {
        uint8_t s_idx = s_ble_state.num_svcs++;
        sim_ble_svc_record_t *s_rec = &s_ble_state.svcs[s_idx];
        s_rec->type = svc->type;
        s_rec->uuid = svc->uuid;
        s_rec->start_handle = s_ble_state.next_handle++;

        if (ble_hs_cfg.gatts_register_cb) {
            struct ble_gatt_register_ctxt ctxt;
            memset(&ctxt, 0, sizeof(ctxt));
            ctxt.op = BLE_GATT_REGISTER_OP_SVC;
            ctxt.svc.svc = svc;
            ctxt.svc.svc_def = svc;
            ctxt.svc.handle = s_rec->start_handle;
            ble_hs_cfg.gatts_register_cb(&ctxt, ble_hs_cfg.gatts_register_arg);
        }

        if (svc->characteristics) {
            for (const struct ble_gatt_chr_def *chr = svc->characteristics; chr->uuid != NULL; chr++) {
                uint8_t c_idx = s_ble_state.num_chrs++;
                sim_ble_chr_record_t *c_rec = &s_ble_state.chrs[c_idx];
                memset(c_rec, 0, sizeof(*c_rec));
                c_rec->handle = s_ble_state.next_handle++;
                c_rec->val_handle = s_ble_state.next_handle++;
                c_rec->uuid = chr->uuid;
                c_rec->access_cb = chr->access_cb;
                c_rec->arg = chr->arg;
                c_rec->flags = chr->flags;
                c_rec->val_handle_ptr = chr->val_handle;
                c_rec->svc_index = s_idx;
                c_rec->def = chr;

                if (chr->val_handle) {
                    *chr->val_handle = c_rec->val_handle;
                }

                if (ble_hs_cfg.gatts_register_cb) {
                    struct ble_gatt_register_ctxt ctxt;
                    memset(&ctxt, 0, sizeof(ctxt));
                    ctxt.op = BLE_GATT_REGISTER_OP_CHR;
                    ctxt.chr.chr = chr;
                    ctxt.chr.chr_def = chr;
                    ctxt.chr.handle = c_rec->handle;
                    ctxt.chr.def_handle = c_rec->handle;
                    ctxt.chr.val_handle = c_rec->val_handle;
                    ble_hs_cfg.gatts_register_cb(&ctxt, ble_hs_cfg.gatts_register_arg);
                }

                if (chr->descriptors) {
                    for (const struct ble_gatt_dsc_def *dsc = chr->descriptors; dsc->uuid != NULL; dsc++) {
                        uint8_t d_idx = s_ble_state.num_dscs++;
                        sim_ble_dsc_record_t *d_rec = &s_ble_state.dscs[d_idx];
                        d_rec->handle = s_ble_state.next_handle++;
                        d_rec->uuid = dsc->uuid;
                        d_rec->access_cb = dsc->access_cb;
                        d_rec->arg = dsc->arg;
                        d_rec->att_flags = dsc->att_flags;
                        d_rec->chr_index = c_idx;

                        if (ble_hs_cfg.gatts_register_cb) {
                            struct ble_gatt_register_ctxt ctxt;
                            memset(&ctxt, 0, sizeof(ctxt));
                            ctxt.op = BLE_GATT_REGISTER_OP_DSC;
                            ctxt.dsc.dsc = dsc;
                            ctxt.dsc.dsc_def = dsc;
                            ctxt.dsc.handle = d_rec->handle;
                            ble_hs_cfg.gatts_register_cb(&ctxt, ble_hs_cfg.gatts_register_arg);
                        }
                    }
                }
            }
        }
        s_rec->end_handle = s_ble_state.next_handle - 1;
    }
    return 0;
}

int ble_gatts_start(void) {
    return 0;
}

int ble_gatts_find_chr(const ble_uuid_t *svc_uuid, const ble_uuid_t *chr_uuid,
                       uint16_t *out_def_handle, uint16_t *out_val_handle) {
    if (!svc_uuid || !chr_uuid) return BLE_HS_EINVAL;
    for (uint8_t i = 0; i < s_ble_state.num_svcs; i++) {
        if (ble_uuid_cmp(s_ble_state.svcs[i].uuid, svc_uuid) == 0) {
            for (uint8_t j = 0; j < s_ble_state.num_chrs; j++) {
                if (s_ble_state.chrs[j].svc_index == i &&
                    ble_uuid_cmp(s_ble_state.chrs[j].uuid, chr_uuid) == 0) {
                    if (out_def_handle) *out_def_handle = s_ble_state.chrs[j].handle;
                    if (out_val_handle) *out_val_handle = s_ble_state.chrs[j].val_handle;
                    return 0;
                }
            }
        }
    }
    return BLE_HS_EINVAL;
}

int ble_gatts_chr_updated(uint16_t chr_def_handle) {
    sim_ble_chr_record_t *chr = NULL;
    for (uint8_t i = 0; i < s_ble_state.num_chrs; i++) {
        if (s_ble_state.chrs[i].handle == chr_def_handle ||
            s_ble_state.chrs[i].val_handle == chr_def_handle) {
            chr = &s_ble_state.chrs[i];
            break;
        }
    }
    if (!chr) {
        ESP_LOGE(TAG, "Characteristic handle 0x%04x not found in updated", chr_def_handle);
        return BLE_HS_EINVAL;
    }

    if (!chr->is_subscribed_notify && !chr->is_subscribed_indicate) {
        return 0;
    }

    if (s_ble_state.notify_hook) {
        if (chr->access_cb) {
            struct os_mbuf om;
            memset(&om, 0, sizeof(om));
            om.om_data = om.om_databuf;

            struct ble_gatt_access_ctxt ctxt;
            memset(&ctxt, 0, sizeof(ctxt));
            ctxt.op = BLE_GATT_ACCESS_OP_READ_CHR;
            ctxt.chr = chr->def;
            ctxt.om = &om;

            int rc = chr->access_cb(1, chr->val_handle, &ctxt, chr->arg);
            if (rc == 0) {
                s_ble_state.notify_hook(1, chr->val_handle, om.om_databuf, om.om_len);
            }
        } else {
            s_ble_state.notify_hook(1, chr->val_handle, chr->value, chr->value_len);
        }
    }
    return 0;
}

int ble_gatts_notify(uint16_t conn_handle, uint16_t chr_val_handle) {
    return ble_gatts_chr_updated(chr_val_handle);
    (void)conn_handle;
}

int ble_gatts_notify_custom(uint16_t conn_handle, uint16_t val_handle, struct os_mbuf *om) {
    sim_ble_chr_record_t *chr = NULL;
    for (uint8_t i = 0; i < s_ble_state.num_chrs; i++) {
        if (s_ble_state.chrs[i].val_handle == val_handle) {
            chr = &s_ble_state.chrs[i];
            break;
        }
    }
    if (!chr) {
        ESP_LOGE(TAG, "Characteristic val_handle 0x%04x not found in notify_custom", val_handle);
        return BLE_HS_EINVAL;
    }

    if (s_ble_state.notify_hook && om) {
        s_ble_state.notify_hook(conn_handle, val_handle, om->om_databuf, om->om_len);
    }
#if defined(__EMSCRIPTEN__)
    if (om) {
        EM_ASM({
            if (typeof globalThis !== 'undefined' && typeof globalThis.__wink_ble_notify_hook === 'function') {
                globalThis.__wink_ble_notify_hook($0, $1, $2, $3);
            }
        }, conn_handle, val_handle, om->om_databuf, om->om_len);
    }
#endif
    return 0;
}

/* ── GAP API ───────────────────────────────────────────────────────────────── */

int ble_gap_adv_start(uint8_t own_addr_type, const ble_addr_t *direct_addr,
                      int32_t duration_ms, const struct ble_gap_adv_params *adv_params,
                      ble_gap_event_fn *cb, void *cb_arg) {
    (void)own_addr_type;
    (void)direct_addr;
    (void)duration_ms;

    if (sim_esp_fault_is_active(ESP_FAULT_DOMAIN_BLE, ESP_FAULT_BLE_ADV_REJECT)) {
        return BLE_HS_EINVAL;
    }

    if (adv_params) {
        s_ble_state.adv_params = *adv_params;
    }
    s_ble_state.adv_cb = cb;
    s_ble_state.adv_cb_arg = cb_arg;
    s_ble_state.is_advertising = true;
    return 0;
}

int ble_gap_adv_stop(void) {
    s_ble_state.is_advertising = false;
    return 0;
}

int ble_gap_adv_active(void) {
    return s_ble_state.is_advertising ? 1 : 0;
}

int ble_gap_adv_set_fields(const struct ble_hs_adv_fields *adv_fields) {
    if (!adv_fields) return BLE_HS_EINVAL;
    s_ble_state.adv_fields = *adv_fields;
    return 0;
}

int ble_gap_adv_rsp_set_fields(const struct ble_hs_adv_fields *rsp_fields) {
    if (!rsp_fields) return BLE_HS_EINVAL;
    s_ble_state.rsp_fields = *rsp_fields;
    return 0;
}

int ble_gap_conn_find(uint16_t conn_handle, struct ble_gap_conn_desc *out_desc) {
    if (conn_handle != 1 || !s_ble_state.is_connected) {
        return BLE_HS_ENOTCONN;
    }
    if (out_desc) {
        *out_desc = s_ble_state.conn;
    }
    return 0;
}

int ble_gap_terminate(uint16_t conn_handle, uint8_t hci_reason) {
    (void)hci_reason;
    if (conn_handle != 1 || !s_ble_state.is_connected) {
        return BLE_HS_ENOTCONN;
    }
    return esp_nimble_sim_disconnect();
}

/* ── UniSim Simulation & Bridge API ────────────────────────────────────────── */

void esp_nimble_sim_set_notify_hook(esp_nimble_sim_notify_hook_t hook) {
    s_ble_state.notify_hook = hook;
}

int esp_nimble_sim_get_service_count(void) {
    return (int)s_ble_state.num_svcs;
}

int esp_nimble_sim_get_service_info(uint8_t index, sim_ble_service_info_t *out_info) {
    if (index >= s_ble_state.num_svcs || !out_info) return BLE_HS_EINVAL;
    memset(out_info, 0, sizeof(*out_info));
    out_info->handle = s_ble_state.svcs[index].start_handle;
    out_info->type = s_ble_state.svcs[index].type;
    const ble_uuid_t *u = s_ble_state.svcs[index].uuid;
    if (u) {
        out_info->uuid_type = u->type;
        if (u->type == BLE_UUID_TYPE_16) {
            uint16_t v = ((const ble_uuid16_t *)u)->value;
            memcpy(out_info->uuid_bytes, &v, sizeof(v));
        } else if (u->type == BLE_UUID_TYPE_32) {
            uint32_t v = ((const ble_uuid32_t *)u)->value;
            memcpy(out_info->uuid_bytes, &v, sizeof(v));
        } else if (u->type == BLE_UUID_TYPE_128) {
            memcpy(out_info->uuid_bytes, ((const ble_uuid128_t *)u)->value, 16);
        }
    }
    return 0;
}

int esp_nimble_sim_get_char_count(uint8_t svc_index) {
    if (svc_index >= s_ble_state.num_svcs) return 0;
    int count = 0;
    for (uint8_t i = 0; i < s_ble_state.num_chrs; i++) {
        if (s_ble_state.chrs[i].svc_index == svc_index) {
            count++;
        }
    }
    return count;
}

int esp_nimble_sim_get_char_info(uint8_t svc_index, uint8_t chr_index, sim_ble_chr_info_t *out_info) {
    if (svc_index >= s_ble_state.num_svcs || !out_info) return BLE_HS_EINVAL;
    uint8_t match = 0;
    for (uint8_t i = 0; i < s_ble_state.num_chrs; i++) {
        if (s_ble_state.chrs[i].svc_index == svc_index) {
            if (match == chr_index) {
                const sim_ble_chr_record_t *chr = &s_ble_state.chrs[i];
                memset(out_info, 0, sizeof(*out_info));
                out_info->handle = chr->handle;
                out_info->val_handle = chr->val_handle;
                out_info->flags = chr->flags;
                out_info->is_subscribed_notify = chr->is_subscribed_notify;
                out_info->is_subscribed_indicate = chr->is_subscribed_indicate;
                const ble_uuid_t *u = chr->uuid;
                if (u) {
                    out_info->uuid_type = u->type;
                    if (u->type == BLE_UUID_TYPE_16) {
                        uint16_t v = ((const ble_uuid16_t *)u)->value;
                        memcpy(out_info->uuid_bytes, &v, sizeof(v));
                    } else if (u->type == BLE_UUID_TYPE_32) {
                        uint32_t v = ((const ble_uuid32_t *)u)->value;
                        memcpy(out_info->uuid_bytes, &v, sizeof(v));
                    } else if (u->type == BLE_UUID_TYPE_128) {
                        memcpy(out_info->uuid_bytes, ((const ble_uuid128_t *)u)->value, 16);
                    }
                }
                return 0;
            }
            match++;
        }
    }
    return BLE_HS_EINVAL;
}

int esp_nimble_sim_is_advertising(void) {
    return s_ble_state.is_advertising ? 1 : 0;
}

int esp_nimble_sim_get_device_name(char *out_buf, size_t max_len) {
    if (!out_buf || max_len == 0) return BLE_HS_EINVAL;
    strncpy(out_buf, s_ble_state.device_name, max_len - 1);
    out_buf[max_len - 1] = '\0';
    return 0;
}

int esp_nimble_sim_connect(void) {
    if (!s_ble_state.is_initialized) return BLE_HS_EINVAL;
    s_ble_state.is_advertising = false;
    s_ble_state.is_connected = true;
    s_ble_state.conn.conn_handle = 1;
    s_ble_state.conn.role = 0;
    s_ble_state.conn.conn_itvl = 16;
    s_ble_state.conn.conn_latency = 0;
    s_ble_state.conn.supervision_timeout = 256;

    if (s_ble_state.adv_cb) {
        struct ble_gap_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = BLE_GAP_EVENT_CONNECT;
        ev.connect.status = 0;
        ev.connect.conn = s_ble_state.conn;
        s_ble_state.adv_cb(&ev, s_ble_state.adv_cb_arg);

        memset(&ev, 0, sizeof(ev));
        ev.type = BLE_GAP_EVENT_MTU;
        ev.mtu.conn_handle = s_ble_state.conn.conn_handle;
        ev.mtu.channel_id = 4;
        ev.mtu.value = 256;
        s_ble_state.adv_cb(&ev, s_ble_state.adv_cb_arg);
    }
    return 0;
}

int esp_nimble_sim_disconnect(void) {
    if (!s_ble_state.is_connected) return BLE_HS_ENOTCONN;
    s_ble_state.is_connected = false;

    if (s_ble_state.adv_cb) {
        struct ble_gap_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = BLE_GAP_EVENT_DISCONNECT;
        ev.disconnect.reason = 0;
        ev.disconnect.conn = s_ble_state.conn;
        s_ble_state.adv_cb(&ev, s_ble_state.adv_cb_arg);
    }
    return 0;
}

int esp_nimble_sim_read_chr(uint16_t conn_handle, uint16_t val_handle, void *out_buf, uint16_t max_len, uint16_t *out_len) {
    if (!out_buf || !out_len) return BLE_HS_EINVAL;
    sim_ble_chr_record_t *chr = NULL;
    for (uint8_t i = 0; i < s_ble_state.num_chrs; i++) {
        if (s_ble_state.chrs[i].val_handle == val_handle) {
            chr = &s_ble_state.chrs[i];
            break;
        }
    }
    if (!chr) {
        ESP_LOGE(TAG, "Characteristic val_handle 0x%04x not found for read", val_handle);
        return BLE_HS_EINVAL;
    }
    if (!(chr->flags & BLE_GATT_CHR_F_READ)) {
        ESP_LOGE(TAG, "Characteristic val_handle 0x%04x not readable", val_handle);
        return BLE_HS_ENOTSUP;
    }

    if (chr->access_cb) {
        struct os_mbuf om;
        memset(&om, 0, sizeof(om));
        om.om_data = om.om_databuf;

        struct ble_gatt_access_ctxt ctxt;
        memset(&ctxt, 0, sizeof(ctxt));
        ctxt.op = BLE_GATT_ACCESS_OP_READ_CHR;
        ctxt.chr = chr->def;
        ctxt.om = &om;

        int rc = chr->access_cb(conn_handle, val_handle, &ctxt, chr->arg);
        if (rc != 0) return rc;

        uint16_t copy_len = om.om_len < max_len ? om.om_len : max_len;
        memcpy(out_buf, om.om_databuf, copy_len);
        *out_len = copy_len;
        if (copy_len <= sizeof(chr->value)) {
            memcpy(chr->value, om.om_databuf, copy_len);
            chr->value_len = copy_len;
        }
        return 0;
    } else {
        uint16_t copy_len = chr->value_len < max_len ? chr->value_len : max_len;
        memcpy(out_buf, chr->value, copy_len);
        *out_len = copy_len;
        return 0;
    }
}

int esp_nimble_sim_write_chr(uint16_t conn_handle, uint16_t val_handle, const void *data, uint16_t len) {
    if (!data && len > 0) return BLE_HS_EINVAL;
    sim_ble_chr_record_t *chr = NULL;
    for (uint8_t i = 0; i < s_ble_state.num_chrs; i++) {
        if (s_ble_state.chrs[i].val_handle == val_handle) {
            chr = &s_ble_state.chrs[i];
            break;
        }
    }
    if (!chr) {
        ESP_LOGE(TAG, "Characteristic val_handle 0x%04x not found for write", val_handle);
        return BLE_HS_EINVAL;
    }
    if (!(chr->flags & (BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP))) {
        ESP_LOGE(TAG, "Characteristic val_handle 0x%04x not writable", val_handle);
        return BLE_HS_ENOTSUP;
    }

    if (len <= sizeof(chr->value)) {
        memcpy(chr->value, data, len);
        chr->value_len = len;
    }

    if (chr->access_cb) {
        struct os_mbuf om;
        memset(&om, 0, sizeof(om));
        om.om_data = om.om_databuf;
        os_mbuf_append(&om, data, len);

        struct ble_gatt_access_ctxt ctxt;
        memset(&ctxt, 0, sizeof(ctxt));
        ctxt.op = BLE_GATT_ACCESS_OP_WRITE_CHR;
        ctxt.chr = chr->def;
        ctxt.om = &om;

        int rc = chr->access_cb(conn_handle, val_handle, &ctxt, chr->arg);
        return rc;
    }
    return 0;
}

int esp_nimble_sim_subscribe(uint16_t conn_handle, uint16_t val_handle, bool notify, bool indicate) {
    sim_ble_chr_record_t *chr = NULL;
    for (uint8_t i = 0; i < s_ble_state.num_chrs; i++) {
        if (s_ble_state.chrs[i].val_handle == val_handle) {
            chr = &s_ble_state.chrs[i];
            break;
        }
    }
    if (!chr) {
        ESP_LOGE(TAG, "Characteristic val_handle 0x%04x not found for subscribe", val_handle);
        return BLE_HS_EINVAL;
    }

    bool prev_notify = chr->is_subscribed_notify;
    bool prev_indicate = chr->is_subscribed_indicate;
    chr->is_subscribed_notify = notify;
    chr->is_subscribed_indicate = indicate;

    if (s_ble_state.adv_cb) {
        struct ble_gap_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = BLE_GAP_EVENT_SUBSCRIBE;
        ev.subscribe.conn_handle = conn_handle;
        ev.subscribe.attr_handle = val_handle;
        ev.subscribe.reason = 0;
        ev.subscribe.prev_notify = prev_notify ? 1 : 0;
        ev.subscribe.cur_notify = notify ? 1 : 0;
        ev.subscribe.prev_indicate = prev_indicate ? 1 : 0;
        ev.subscribe.cur_indicate = indicate ? 1 : 0;
        s_ble_state.adv_cb(&ev, s_ble_state.adv_cb_arg);
    }
    return 0;
}

int ble_hs_id_copy_addr(uint8_t id_addr_type, uint8_t *out_id_addr, int *out_is_nrpa) {
    (void)id_addr_type;
    if (out_id_addr) {
        memset(out_id_addr, 0x11, 6);
    }
    if (out_is_nrpa) {
        *out_is_nrpa = 0;
    }
    return 0;
}

void ble_store_util_status_rr(void *event, void *arg) {
    (void)event;
    (void)arg;
}

void ble_store_config_init(void) {
    /* No-op in headless simulation: NVS key store mock */
}

void esp_nimble_sim_reset(void) {
    memset(&s_ble_state, 0, sizeof(s_ble_state));
    memset(&ble_hs_cfg, 0, sizeof(ble_hs_cfg));
    s_ble_state.next_handle = 1;
}

#if defined(__EMSCRIPTEN__)
#  include <emscripten.h>
#  define WINK_SIM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#  define WINK_SIM_EXPORT
#endif

WINK_SIM_EXPORT int sim_ble_get_adv_state(void) {
    return s_ble_state.is_advertising ? 1 : 0;
}

WINK_SIM_EXPORT const char* sim_ble_get_device_name(void) {
    return s_ble_state.device_name;
}

WINK_SIM_EXPORT int sim_ble_get_num_services(void) {
    return (int)s_ble_state.num_svcs;
}

WINK_SIM_EXPORT int sim_ble_get_num_characteristics(void) {
    return (int)s_ble_state.num_chrs;
}

