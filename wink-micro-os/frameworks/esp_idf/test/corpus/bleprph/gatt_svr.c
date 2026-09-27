/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "bleprph.h"

static const char *manuf_name = "Apache NimBLE";
static const char *model_num = "BLE Peripheral";

static uint16_t hrs_hrm_val_handle;

static int
gatt_svr_chr_access_heart_rate(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

static int
gatt_svr_chr_access_device_info(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        /*** Service: Heart Rate. */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(GATT_SVR_SVC_HEART_RATE_UUID),
        .characteristics = (struct ble_gatt_chr_def[]) { {
            /*** Characteristic: Heart Rate Measurement. */
            .uuid = BLE_UUID16_DECLARE(GATT_SVR_CHR_HEART_RATE_MEASUREMENT_UUID),
            .access_cb = gatt_svr_chr_access_heart_rate,
            .val_handle = &hrs_hrm_val_handle,
            .flags = BLE_GATT_CHR_F_NOTIFY,
        }, {
            /*** Characteristic: Body Sensor Location. */
            .uuid = BLE_UUID16_DECLARE(GATT_SVR_CHR_BODY_SENSOR_LOCATION_UUID),
            .access_cb = gatt_svr_chr_access_heart_rate,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            0, /* No more characteristics in this service. */
        } },
    },

    {
        /*** Service: Device Information. */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(GATT_SVR_SVC_DEVICE_INFO_UUID),
        .characteristics = (struct ble_gatt_chr_def[]) { {
            /*** Characteristic: Manufacturer Name. */
            .uuid = BLE_UUID16_DECLARE(GATT_SVR_CHR_MANUFACTURER_NAME_UUID),
            .access_cb = gatt_svr_chr_access_device_info,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            /*** Characteristic: Model Number. */
            .uuid = BLE_UUID16_DECLARE(GATT_SVR_CHR_MODEL_NUMBER_UUID),
            .access_cb = gatt_svr_chr_access_device_info,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            0, /* No more characteristics in this service. */
        } },
    },

    {
        0, /* No more services. */
    },
};

static int
gatt_svr_chr_access_heart_rate(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;
    uint16_t uuid16 = ble_uuid_u16(ctxt->chr->uuid);
    int rc;

    switch (uuid16) {
    case GATT_SVR_CHR_HEART_RATE_MEASUREMENT_UUID: {
        uint8_t hrm[2] = { 0x00, 80 };
        rc = os_mbuf_append(ctxt->om, hrm, sizeof(hrm));
        return rc == 0 ? 0 : BLE_HS_ENOMEM;
    }
    case GATT_SVR_CHR_BODY_SENSOR_LOCATION_UUID: {
        uint8_t body_sensor_loc = 0x01; /* Chest */
        rc = os_mbuf_append(ctxt->om, &body_sensor_loc, sizeof(body_sensor_loc));
        return rc == 0 ? 0 : BLE_HS_ENOMEM;
    }
    default:
        return BLE_HS_EINVAL;
    }
}

static int
gatt_svr_chr_access_device_info(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;
    uint16_t uuid16 = ble_uuid_u16(ctxt->chr->uuid);
    int rc;

    switch (uuid16) {
    case GATT_SVR_CHR_MANUFACTURER_NAME_UUID:
        rc = os_mbuf_append(ctxt->om, manuf_name, strlen(manuf_name));
        return rc == 0 ? 0 : BLE_HS_ENOMEM;
    case GATT_SVR_CHR_MODEL_NUMBER_UUID:
        rc = os_mbuf_append(ctxt->om, model_num, strlen(model_num));
        return rc == 0 ? 0 : BLE_HS_ENOMEM;
    default:
        return BLE_HS_EINVAL;
    }
}

void
gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    (void)ctxt;
    (void)arg;
}

int
gatt_svr_init(void)
{
    int rc;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    return 0;
}
