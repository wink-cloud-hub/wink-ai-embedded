/* SPDX-License-Identifier: Apache-2.0 */
#ifndef H_BLE_
#define H_BLE_
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifndef MYNEWT_VAL
#define MYNEWT_VAL(x) 0
#endif

#define BLE_HS_ADV_TYPE_FLAGS        0x01
#define BLE_HS_ADV_TYPE_COMP_UUIDS16 0x03

#define BLE_HS_ADV_TX_PWR_LVL_AUTO   (-128)
#define BLE_HS_FOREVER               (-1)

#endif
