/* SPDX-License-Identifier: Apache-2.0 */
#ifndef H_BLE_
#define H_BLE_
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifndef MYNEWT_VAL
#define MYNEWT_VAL_BLE_GATTS         1
#define MYNEWT_VAL_BLE_EATT_CHAN_NUM 0
#define MYNEWT_VAL_BLE_POWER_CONTROL 0
#define MYNEWT_VAL_BLE_CONN_SUBRATING 0
#define MYNEWT_VAL_STATIC_PASSKEY    0
#define MYNEWT_VAL(x)                MYNEWT_VAL_ ## x
#endif

#define BLE_HS_ADV_TYPE_FLAGS        0x01
#define BLE_HS_ADV_TYPE_COMP_UUIDS16 0x03

#define BLE_HS_ADV_TX_PWR_LVL_AUTO   (-128)
#define BLE_HS_FOREVER               (-1)

#endif
