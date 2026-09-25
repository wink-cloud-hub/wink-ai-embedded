/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef _SOC_GPIO_SIG_MAP_H_
#define _SOC_GPIO_SIG_MAP_H_
#ifndef __WINK_HARVESTED_SOC_GPIO_SIG_MAP_H__
#define __WINK_HARVESTED_SOC_GPIO_SIG_MAP_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef ANT_SEL0_IDX
#define ANT_SEL0_IDX 216
#endif
#ifndef ANT_SEL1_IDX
#define ANT_SEL1_IDX 217
#endif
#ifndef ANT_SEL2_IDX
#define ANT_SEL2_IDX 218
#endif
#ifndef ANT_SEL3_IDX
#define ANT_SEL3_IDX 219
#endif
#ifndef ANT_SEL4_IDX
#define ANT_SEL4_IDX 220
#endif
#ifndef ANT_SEL5_IDX
#define ANT_SEL5_IDX 221
#endif
#ifndef ANT_SEL6_IDX
#define ANT_SEL6_IDX 222
#endif
#ifndef ANT_SEL7_IDX
#define ANT_SEL7_IDX 223
#endif
#ifndef BB_DIAG0_IDX
#define BB_DIAG0_IDX 41
#endif
#ifndef BB_DIAG10_IDX
#define BB_DIAG10_IDX 51
#endif
#ifndef BB_DIAG11_IDX
#define BB_DIAG11_IDX 52
#endif
#ifndef BB_DIAG12_IDX
#define BB_DIAG12_IDX 53
#endif
#ifndef BB_DIAG13_IDX
#define BB_DIAG13_IDX 54
#endif
#ifndef BB_DIAG14_IDX
#define BB_DIAG14_IDX 55
#endif
#ifndef BB_DIAG15_IDX
#define BB_DIAG15_IDX 56
#endif
#ifndef BB_DIAG16_IDX
#define BB_DIAG16_IDX 57
#endif
#ifndef BB_DIAG17_IDX
#define BB_DIAG17_IDX 58
#endif
#ifndef BB_DIAG18_IDX
#define BB_DIAG18_IDX 59
#endif
#ifndef BB_DIAG19_IDX
#define BB_DIAG19_IDX 60
#endif
#ifndef BB_DIAG1_IDX
#define BB_DIAG1_IDX 42
#endif
#ifndef BB_DIAG2_IDX
#define BB_DIAG2_IDX 43
#endif
#ifndef BB_DIAG3_IDX
#define BB_DIAG3_IDX 44
#endif
#ifndef BB_DIAG4_IDX
#define BB_DIAG4_IDX 45
#endif
#ifndef BB_DIAG5_IDX
#define BB_DIAG5_IDX 46
#endif
#ifndef BB_DIAG6_IDX
#define BB_DIAG6_IDX 47
#endif
#ifndef BB_DIAG7_IDX
#define BB_DIAG7_IDX 48
#endif
#ifndef BB_DIAG8_IDX
#define BB_DIAG8_IDX 49
#endif
#ifndef BB_DIAG9_IDX
#define BB_DIAG9_IDX 50
#endif
#ifndef BLE_AUDIO0_IRQ_IDX
#define BLE_AUDIO0_IRQ_IDX 207
#endif
#ifndef BLE_AUDIO1_IRQ_IDX
#define BLE_AUDIO1_IRQ_IDX 208
#endif
#ifndef BLE_AUDIO2_IRQ_IDX
#define BLE_AUDIO2_IRQ_IDX 209
#endif
#ifndef BLE_AUDIO_SYNC0_P_IDX
#define BLE_AUDIO_SYNC0_P_IDX 213
#endif
#ifndef BLE_AUDIO_SYNC1_P_IDX
#define BLE_AUDIO_SYNC1_P_IDX 214
#endif
#ifndef BLE_AUDIO_SYNC2_P_IDX
#define BLE_AUDIO_SYNC2_P_IDX 215
#endif
#ifndef BT_AUDIO0_IRQ_IDX
#define BT_AUDIO0_IRQ_IDX 204
#endif
#ifndef BT_AUDIO1_IRQ_IDX
#define BT_AUDIO1_IRQ_IDX 205
#endif
#ifndef BT_AUDIO2_IRQ_IDX
#define BT_AUDIO2_IRQ_IDX 206
#endif
#ifndef CAN_BUS_OFF_ON_IDX
#define CAN_BUS_OFF_ON_IDX TWAI_BUS_OFF_ON_IDX
#endif
#ifndef CAN_CLKOUT_IDX
#define CAN_CLKOUT_IDX TWAI_CLKOUT_IDX
#endif
#ifndef CAN_RX_IDX
#define CAN_RX_IDX TWAI_RX_IDX
#endif
#ifndef CAN_TX_IDX
#define CAN_TX_IDX TWAI_TX_IDX
#endif
#ifndef EMAC_COL_I_IDX
#define EMAC_COL_I_IDX 203
#endif
#ifndef EMAC_COL_O_IDX
#define EMAC_COL_O_IDX 203
#endif
#ifndef EMAC_CRS_I_IDX
#define EMAC_CRS_I_IDX 202
#endif
#ifndef EMAC_CRS_O_IDX
#define EMAC_CRS_O_IDX 202
#endif
#ifndef EMAC_MDC_I_IDX
#define EMAC_MDC_I_IDX 200
#endif
#ifndef EMAC_MDC_O_IDX
#define EMAC_MDC_O_IDX 200
#endif
#ifndef EMAC_MDI_I_IDX
#define EMAC_MDI_I_IDX 201
#endif
#ifndef EMAC_MDO_O_IDX
#define EMAC_MDO_O_IDX 201
#endif
#ifndef EXT_ADC_START_IDX
#define EXT_ADC_START_IDX 93
#endif
#ifndef EXT_I2C_SCL_O_IDX
#define EXT_I2C_SCL_O_IDX 21
#endif
#ifndef EXT_I2C_SDA_I_IDX
#define EXT_I2C_SDA_I_IDX 22
#endif
#ifndef EXT_I2C_SDA_O_IDX
#define EXT_I2C_SDA_O_IDX 22
#endif
#ifndef GPIO_BT_ACTIVE_IDX
#define GPIO_BT_ACTIVE_IDX 37
#endif
#ifndef GPIO_BT_PRIORITY_IDX
#define GPIO_BT_PRIORITY_IDX 38
#endif
#ifndef GPIO_SD0_OUT_IDX
#define GPIO_SD0_OUT_IDX 100
#endif
#ifndef GPIO_SD1_OUT_IDX
#define GPIO_SD1_OUT_IDX 101
#endif
#ifndef GPIO_SD2_OUT_IDX
#define GPIO_SD2_OUT_IDX 102
#endif
#ifndef GPIO_SD3_OUT_IDX
#define GPIO_SD3_OUT_IDX 103
#endif
#ifndef GPIO_SD4_OUT_IDX
#define GPIO_SD4_OUT_IDX 104
#endif
#ifndef GPIO_SD5_OUT_IDX
#define GPIO_SD5_OUT_IDX 105
#endif
#ifndef GPIO_SD6_OUT_IDX
#define GPIO_SD6_OUT_IDX 106
#endif
#ifndef GPIO_SD7_OUT_IDX
#define GPIO_SD7_OUT_IDX 107
#endif
#ifndef GPIO_WLAN_ACTIVE_IDX
#define GPIO_WLAN_ACTIVE_IDX 40
#endif
#ifndef HOST_CARD_DETECT_N_1_IDX
#define HOST_CARD_DETECT_N_1_IDX 97
#endif
#ifndef HOST_CARD_DETECT_N_2_IDX
#define HOST_CARD_DETECT_N_2_IDX 98
#endif
#ifndef HOST_CARD_INT_N_1_IDX
#define HOST_CARD_INT_N_1_IDX 101
#endif
#ifndef HOST_CARD_INT_N_2_IDX
#define HOST_CARD_INT_N_2_IDX 102
#endif
#ifndef HOST_CARD_WRITE_PRT_1_IDX
#define HOST_CARD_WRITE_PRT_1_IDX 99
#endif
#ifndef HOST_CARD_WRITE_PRT_2_IDX
#define HOST_CARD_WRITE_PRT_2_IDX 100
#endif
#ifndef HOST_CCMD_OD_PULLUP_EN_N_IDX
#define HOST_CCMD_OD_PULLUP_EN_N_IDX 97
#endif
#ifndef HOST_RST_N_1_IDX
#define HOST_RST_N_1_IDX 98
#endif
#ifndef HOST_RST_N_2_IDX
#define HOST_RST_N_2_IDX 99
#endif
#ifndef HSPICLK_IN_IDX
#define HSPICLK_IN_IDX 8
#endif
#ifndef HSPICLK_OUT_IDX
#define HSPICLK_OUT_IDX 8
#endif
#ifndef HSPICS0_IN_IDX
#define HSPICS0_IN_IDX 11
#endif
#ifndef HSPICS0_OUT_IDX
#define HSPICS0_OUT_IDX 11
#endif
#ifndef HSPICS1_IN_IDX
#define HSPICS1_IN_IDX 61
#endif
#ifndef HSPICS1_OUT_IDX
#define HSPICS1_OUT_IDX 61
#endif
#ifndef HSPICS2_IN_IDX
#define HSPICS2_IN_IDX 62
#endif
#ifndef HSPICS2_OUT_IDX
#define HSPICS2_OUT_IDX 62
#endif
#ifndef HSPID4_IN_IDX
#define HSPID4_IN_IDX 132
#endif
#ifndef HSPID4_OUT_IDX
#define HSPID4_OUT_IDX 132
#endif
#ifndef HSPID5_IN_IDX
#define HSPID5_IN_IDX 133
#endif
#ifndef HSPID5_OUT_IDX
#define HSPID5_OUT_IDX 133
#endif
#ifndef HSPID6_IN_IDX
#define HSPID6_IN_IDX 134
#endif
#ifndef HSPID6_OUT_IDX
#define HSPID6_OUT_IDX 134
#endif
#ifndef HSPID7_IN_IDX
#define HSPID7_IN_IDX 135
#endif
#ifndef HSPID7_OUT_IDX
#define HSPID7_OUT_IDX 135
#endif
#ifndef HSPID_IN_IDX
#define HSPID_IN_IDX 10
#endif
#ifndef HSPID_OUT_IDX
#define HSPID_OUT_IDX 10
#endif
#ifndef HSPIHD_IN_IDX
#define HSPIHD_IN_IDX 12
#endif
#ifndef HSPIHD_OUT_IDX
#define HSPIHD_OUT_IDX 12
#endif
#ifndef HSPIQ_IN_IDX
#define HSPIQ_IN_IDX 9
#endif
#ifndef HSPIQ_OUT_IDX
#define HSPIQ_OUT_IDX 9
#endif
#ifndef HSPIWP_IN_IDX
#define HSPIWP_IN_IDX 13
#endif
#ifndef HSPIWP_OUT_IDX
#define HSPIWP_OUT_IDX 13
#endif
#ifndef I2CEXT0_SCL_IN_IDX
#define I2CEXT0_SCL_IN_IDX 29
#endif
#ifndef I2CEXT0_SCL_OUT_IDX
#define I2CEXT0_SCL_OUT_IDX 29
#endif
#ifndef I2CEXT0_SDA_IN_IDX
#define I2CEXT0_SDA_IN_IDX 30
#endif
#ifndef I2CEXT0_SDA_OUT_IDX
#define I2CEXT0_SDA_OUT_IDX 30
#endif
#ifndef I2CEXT1_SCL_IN_IDX
#define I2CEXT1_SCL_IN_IDX 95
#endif
#ifndef I2CEXT1_SCL_OUT_IDX
#define I2CEXT1_SCL_OUT_IDX 95
#endif
#ifndef I2CEXT1_SDA_IN_IDX
#define I2CEXT1_SDA_IN_IDX 96
#endif
#ifndef I2CEXT1_SDA_OUT_IDX
#define I2CEXT1_SDA_OUT_IDX 96
#endif
#ifndef I2CM_SCL_O_IDX
#define I2CM_SCL_O_IDX 19
#endif
#ifndef I2CM_SDA_I_IDX
#define I2CM_SDA_I_IDX 20
#endif
#ifndef I2CM_SDA_O_IDX
#define I2CM_SDA_O_IDX 20
#endif
#ifndef I2S0I_BCK_IN_IDX
#define I2S0I_BCK_IN_IDX 27
#endif
#ifndef I2S0I_BCK_OUT_IDX
#define I2S0I_BCK_OUT_IDX 27
#endif
#ifndef I2S0I_DATA_IN0_IDX
#define I2S0I_DATA_IN0_IDX 140
#endif
#ifndef I2S0I_DATA_IN10_IDX
#define I2S0I_DATA_IN10_IDX 150
#endif
#ifndef I2S0I_DATA_IN11_IDX
#define I2S0I_DATA_IN11_IDX 151
#endif
#ifndef I2S0I_DATA_IN12_IDX
#define I2S0I_DATA_IN12_IDX 152
#endif
#ifndef I2S0I_DATA_IN13_IDX
#define I2S0I_DATA_IN13_IDX 153
#endif
#ifndef I2S0I_DATA_IN14_IDX
#define I2S0I_DATA_IN14_IDX 154
#endif
#ifndef I2S0I_DATA_IN15_IDX
#define I2S0I_DATA_IN15_IDX 155
#endif
#ifndef I2S0I_DATA_IN1_IDX
#define I2S0I_DATA_IN1_IDX 141
#endif
#ifndef I2S0I_DATA_IN2_IDX
#define I2S0I_DATA_IN2_IDX 142
#endif
#ifndef I2S0I_DATA_IN3_IDX
#define I2S0I_DATA_IN3_IDX 143
#endif
#ifndef I2S0I_DATA_IN4_IDX
#define I2S0I_DATA_IN4_IDX 144
#endif
#ifndef I2S0I_DATA_IN5_IDX
#define I2S0I_DATA_IN5_IDX 145
#endif
#ifndef I2S0I_DATA_IN6_IDX
#define I2S0I_DATA_IN6_IDX 146
#endif
#ifndef I2S0I_DATA_IN7_IDX
#define I2S0I_DATA_IN7_IDX 147
#endif
#ifndef I2S0I_DATA_IN8_IDX
#define I2S0I_DATA_IN8_IDX 148
#endif
#ifndef I2S0I_DATA_IN9_IDX
#define I2S0I_DATA_IN9_IDX 149
#endif
#ifndef I2S0I_H_ENABLE_IDX
#define I2S0I_H_ENABLE_IDX 192
#endif
#ifndef I2S0I_H_SYNC_IDX
#define I2S0I_H_SYNC_IDX 190
#endif
#ifndef I2S0I_V_SYNC_IDX
#define I2S0I_V_SYNC_IDX 191
#endif
#ifndef I2S0I_WS_IN_IDX
#define I2S0I_WS_IN_IDX 28
#endif
#ifndef I2S0I_WS_OUT_IDX
#define I2S0I_WS_OUT_IDX 28
#endif
#ifndef I2S0O_BCK_IN_IDX
#define I2S0O_BCK_IN_IDX 23
#endif
#ifndef I2S0O_BCK_OUT_IDX
#define I2S0O_BCK_OUT_IDX 23
#endif
#ifndef I2S0O_DATA_OUT0_IDX
#define I2S0O_DATA_OUT0_IDX 140
#endif
#ifndef I2S0O_DATA_OUT10_IDX
#define I2S0O_DATA_OUT10_IDX 150
#endif
#ifndef I2S0O_DATA_OUT11_IDX
#define I2S0O_DATA_OUT11_IDX 151
#endif
#ifndef I2S0O_DATA_OUT12_IDX
#define I2S0O_DATA_OUT12_IDX 152
#endif
#ifndef I2S0O_DATA_OUT13_IDX
#define I2S0O_DATA_OUT13_IDX 153
#endif
#ifndef I2S0O_DATA_OUT14_IDX
#define I2S0O_DATA_OUT14_IDX 154
#endif
#ifndef I2S0O_DATA_OUT15_IDX
#define I2S0O_DATA_OUT15_IDX 155
#endif
#ifndef I2S0O_DATA_OUT16_IDX
#define I2S0O_DATA_OUT16_IDX 156
#endif
#ifndef I2S0O_DATA_OUT17_IDX
#define I2S0O_DATA_OUT17_IDX 157
#endif
#ifndef I2S0O_DATA_OUT18_IDX
#define I2S0O_DATA_OUT18_IDX 158
#endif
#ifndef I2S0O_DATA_OUT19_IDX
#define I2S0O_DATA_OUT19_IDX 159
#endif
#ifndef I2S0O_DATA_OUT1_IDX
#define I2S0O_DATA_OUT1_IDX 141
#endif
#ifndef I2S0O_DATA_OUT20_IDX
#define I2S0O_DATA_OUT20_IDX 160
#endif
#ifndef I2S0O_DATA_OUT21_IDX
#define I2S0O_DATA_OUT21_IDX 161
#endif
#ifndef I2S0O_DATA_OUT22_IDX
#define I2S0O_DATA_OUT22_IDX 162
#endif
#ifndef I2S0O_DATA_OUT23_IDX
#define I2S0O_DATA_OUT23_IDX 163
#endif
#ifndef I2S0O_DATA_OUT2_IDX
#define I2S0O_DATA_OUT2_IDX 142
#endif
#ifndef I2S0O_DATA_OUT3_IDX
#define I2S0O_DATA_OUT3_IDX 143
#endif
#ifndef I2S0O_DATA_OUT4_IDX
#define I2S0O_DATA_OUT4_IDX 144
#endif
#ifndef I2S0O_DATA_OUT5_IDX
#define I2S0O_DATA_OUT5_IDX 145
#endif
#ifndef I2S0O_DATA_OUT6_IDX
#define I2S0O_DATA_OUT6_IDX 146
#endif
#ifndef I2S0O_DATA_OUT7_IDX
#define I2S0O_DATA_OUT7_IDX 147
#endif
#ifndef I2S0O_DATA_OUT8_IDX
#define I2S0O_DATA_OUT8_IDX 148
#endif
#ifndef I2S0O_DATA_OUT9_IDX
#define I2S0O_DATA_OUT9_IDX 149
#endif
#ifndef I2S0O_WS_IN_IDX
#define I2S0O_WS_IN_IDX 25
#endif
#ifndef I2S0O_WS_OUT_IDX
#define I2S0O_WS_OUT_IDX 25
#endif
#ifndef I2S1I_BCK_IN_IDX
#define I2S1I_BCK_IN_IDX 164
#endif
#ifndef I2S1I_BCK_OUT_IDX
#define I2S1I_BCK_OUT_IDX 164
#endif
#ifndef I2S1I_DATA_IN0_IDX
#define I2S1I_DATA_IN0_IDX 166
#endif
#ifndef I2S1I_DATA_IN10_IDX
#define I2S1I_DATA_IN10_IDX 176
#endif
#ifndef I2S1I_DATA_IN11_IDX
#define I2S1I_DATA_IN11_IDX 177
#endif
#ifndef I2S1I_DATA_IN12_IDX
#define I2S1I_DATA_IN12_IDX 178
#endif
#ifndef I2S1I_DATA_IN13_IDX
#define I2S1I_DATA_IN13_IDX 179
#endif
#ifndef I2S1I_DATA_IN14_IDX
#define I2S1I_DATA_IN14_IDX 180
#endif
#ifndef I2S1I_DATA_IN15_IDX
#define I2S1I_DATA_IN15_IDX 181
#endif
#ifndef I2S1I_DATA_IN1_IDX
#define I2S1I_DATA_IN1_IDX 167
#endif
#ifndef I2S1I_DATA_IN2_IDX
#define I2S1I_DATA_IN2_IDX 168
#endif
#ifndef I2S1I_DATA_IN3_IDX
#define I2S1I_DATA_IN3_IDX 169
#endif
#ifndef I2S1I_DATA_IN4_IDX
#define I2S1I_DATA_IN4_IDX 170
#endif
#ifndef I2S1I_DATA_IN5_IDX
#define I2S1I_DATA_IN5_IDX 171
#endif
#ifndef I2S1I_DATA_IN6_IDX
#define I2S1I_DATA_IN6_IDX 172
#endif
#ifndef I2S1I_DATA_IN7_IDX
#define I2S1I_DATA_IN7_IDX 173
#endif
#ifndef I2S1I_DATA_IN8_IDX
#define I2S1I_DATA_IN8_IDX 174
#endif
#ifndef I2S1I_DATA_IN9_IDX
#define I2S1I_DATA_IN9_IDX 175
#endif
#ifndef I2S1I_H_ENABLE_IDX
#define I2S1I_H_ENABLE_IDX 195
#endif
#ifndef I2S1I_H_SYNC_IDX
#define I2S1I_H_SYNC_IDX 193
#endif
#ifndef I2S1I_V_SYNC_IDX
#define I2S1I_V_SYNC_IDX 194
#endif
#ifndef I2S1I_WS_IN_IDX
#define I2S1I_WS_IN_IDX 165
#endif
#ifndef I2S1I_WS_OUT_IDX
#define I2S1I_WS_OUT_IDX 165
#endif
#ifndef I2S1O_BCK_IN_IDX
#define I2S1O_BCK_IN_IDX 24
#endif
#ifndef I2S1O_BCK_OUT_IDX
#define I2S1O_BCK_OUT_IDX 24
#endif
#ifndef I2S1O_DATA_OUT0_IDX
#define I2S1O_DATA_OUT0_IDX 166
#endif
#ifndef I2S1O_DATA_OUT10_IDX
#define I2S1O_DATA_OUT10_IDX 176
#endif
#ifndef I2S1O_DATA_OUT11_IDX
#define I2S1O_DATA_OUT11_IDX 177
#endif
#ifndef I2S1O_DATA_OUT12_IDX
#define I2S1O_DATA_OUT12_IDX 178
#endif
#ifndef I2S1O_DATA_OUT13_IDX
#define I2S1O_DATA_OUT13_IDX 179
#endif
#ifndef I2S1O_DATA_OUT14_IDX
#define I2S1O_DATA_OUT14_IDX 180
#endif
#ifndef I2S1O_DATA_OUT15_IDX
#define I2S1O_DATA_OUT15_IDX 181
#endif
#ifndef I2S1O_DATA_OUT16_IDX
#define I2S1O_DATA_OUT16_IDX 182
#endif
#ifndef I2S1O_DATA_OUT17_IDX
#define I2S1O_DATA_OUT17_IDX 183
#endif
#ifndef I2S1O_DATA_OUT18_IDX
#define I2S1O_DATA_OUT18_IDX 184
#endif
#ifndef I2S1O_DATA_OUT19_IDX
#define I2S1O_DATA_OUT19_IDX 185
#endif
#ifndef I2S1O_DATA_OUT1_IDX
#define I2S1O_DATA_OUT1_IDX 167
#endif
#ifndef I2S1O_DATA_OUT20_IDX
#define I2S1O_DATA_OUT20_IDX 186
#endif
#ifndef I2S1O_DATA_OUT21_IDX
#define I2S1O_DATA_OUT21_IDX 187
#endif
#ifndef I2S1O_DATA_OUT22_IDX
#define I2S1O_DATA_OUT22_IDX 188
#endif
#ifndef I2S1O_DATA_OUT23_IDX
#define I2S1O_DATA_OUT23_IDX 189
#endif
#ifndef I2S1O_DATA_OUT2_IDX
#define I2S1O_DATA_OUT2_IDX 168
#endif
#ifndef I2S1O_DATA_OUT3_IDX
#define I2S1O_DATA_OUT3_IDX 169
#endif
#ifndef I2S1O_DATA_OUT4_IDX
#define I2S1O_DATA_OUT4_IDX 170
#endif
#ifndef I2S1O_DATA_OUT5_IDX
#define I2S1O_DATA_OUT5_IDX 171
#endif
#ifndef I2S1O_DATA_OUT6_IDX
#define I2S1O_DATA_OUT6_IDX 172
#endif
#ifndef I2S1O_DATA_OUT7_IDX
#define I2S1O_DATA_OUT7_IDX 173
#endif
#ifndef I2S1O_DATA_OUT8_IDX
#define I2S1O_DATA_OUT8_IDX 174
#endif
#ifndef I2S1O_DATA_OUT9_IDX
#define I2S1O_DATA_OUT9_IDX 175
#endif
#ifndef I2S1O_WS_IN_IDX
#define I2S1O_WS_IN_IDX 26
#endif
#ifndef I2S1O_WS_OUT_IDX
#define I2S1O_WS_OUT_IDX 26
#endif
#ifndef LEDC_HS_SIG_OUT0_IDX
#define LEDC_HS_SIG_OUT0_IDX 71
#endif
#ifndef LEDC_HS_SIG_OUT1_IDX
#define LEDC_HS_SIG_OUT1_IDX 72
#endif
#ifndef LEDC_HS_SIG_OUT2_IDX
#define LEDC_HS_SIG_OUT2_IDX 73
#endif
#ifndef LEDC_HS_SIG_OUT3_IDX
#define LEDC_HS_SIG_OUT3_IDX 74
#endif
#ifndef LEDC_HS_SIG_OUT4_IDX
#define LEDC_HS_SIG_OUT4_IDX 75
#endif
#ifndef LEDC_HS_SIG_OUT5_IDX
#define LEDC_HS_SIG_OUT5_IDX 76
#endif
#ifndef LEDC_HS_SIG_OUT6_IDX
#define LEDC_HS_SIG_OUT6_IDX 77
#endif
#ifndef LEDC_HS_SIG_OUT7_IDX
#define LEDC_HS_SIG_OUT7_IDX 78
#endif
#ifndef LEDC_LS_SIG_OUT0_IDX
#define LEDC_LS_SIG_OUT0_IDX 79
#endif
#ifndef LEDC_LS_SIG_OUT1_IDX
#define LEDC_LS_SIG_OUT1_IDX 80
#endif
#ifndef LEDC_LS_SIG_OUT2_IDX
#define LEDC_LS_SIG_OUT2_IDX 81
#endif
#ifndef LEDC_LS_SIG_OUT3_IDX
#define LEDC_LS_SIG_OUT3_IDX 82
#endif
#ifndef LEDC_LS_SIG_OUT4_IDX
#define LEDC_LS_SIG_OUT4_IDX 83
#endif
#ifndef LEDC_LS_SIG_OUT5_IDX
#define LEDC_LS_SIG_OUT5_IDX 84
#endif
#ifndef LEDC_LS_SIG_OUT6_IDX
#define LEDC_LS_SIG_OUT6_IDX 85
#endif
#ifndef LEDC_LS_SIG_OUT7_IDX
#define LEDC_LS_SIG_OUT7_IDX 86
#endif
#ifndef PCMCLK_IN_IDX
#define PCMCLK_IN_IDX 205
#endif
#ifndef PCMCLK_OUT_IDX
#define PCMCLK_OUT_IDX 211
#endif
#ifndef PCMDIN_IDX
#define PCMDIN_IDX 206
#endif
#ifndef PCMDOUT_IDX
#define PCMDOUT_IDX 212
#endif
#ifndef PCMFSYNC_IN_IDX
#define PCMFSYNC_IN_IDX 204
#endif
#ifndef PCMFSYNC_OUT_IDX
#define PCMFSYNC_OUT_IDX 210
#endif
#ifndef PCNT_CTRL_CH0_IN0_IDX
#define PCNT_CTRL_CH0_IN0_IDX 41
#endif
#ifndef PCNT_CTRL_CH0_IN1_IDX
#define PCNT_CTRL_CH0_IN1_IDX 45
#endif
#ifndef PCNT_CTRL_CH0_IN2_IDX
#define PCNT_CTRL_CH0_IN2_IDX 49
#endif
#ifndef PCNT_CTRL_CH0_IN3_IDX
#define PCNT_CTRL_CH0_IN3_IDX 53
#endif
#ifndef PCNT_CTRL_CH0_IN4_IDX
#define PCNT_CTRL_CH0_IN4_IDX 57
#endif
#ifndef PCNT_CTRL_CH0_IN5_IDX
#define PCNT_CTRL_CH0_IN5_IDX 73
#endif
#ifndef PCNT_CTRL_CH0_IN6_IDX
#define PCNT_CTRL_CH0_IN6_IDX 77
#endif
#ifndef PCNT_CTRL_CH0_IN7_IDX
#define PCNT_CTRL_CH0_IN7_IDX 81
#endif
#ifndef PCNT_CTRL_CH1_IN0_IDX
#define PCNT_CTRL_CH1_IN0_IDX 42
#endif
#ifndef PCNT_CTRL_CH1_IN1_IDX
#define PCNT_CTRL_CH1_IN1_IDX 46
#endif
#ifndef PCNT_CTRL_CH1_IN2_IDX
#define PCNT_CTRL_CH1_IN2_IDX 50
#endif
#ifndef PCNT_CTRL_CH1_IN3_IDX
#define PCNT_CTRL_CH1_IN3_IDX 54
#endif
#ifndef PCNT_CTRL_CH1_IN4_IDX
#define PCNT_CTRL_CH1_IN4_IDX 58
#endif
#ifndef PCNT_CTRL_CH1_IN5_IDX
#define PCNT_CTRL_CH1_IN5_IDX 74
#endif
#ifndef PCNT_CTRL_CH1_IN6_IDX
#define PCNT_CTRL_CH1_IN6_IDX 78
#endif
#ifndef PCNT_CTRL_CH1_IN7_IDX
#define PCNT_CTRL_CH1_IN7_IDX 82
#endif
#ifndef PCNT_SIG_CH0_IN0_IDX
#define PCNT_SIG_CH0_IN0_IDX 39
#endif
#ifndef PCNT_SIG_CH0_IN1_IDX
#define PCNT_SIG_CH0_IN1_IDX 43
#endif
#ifndef PCNT_SIG_CH0_IN2_IDX
#define PCNT_SIG_CH0_IN2_IDX 47
#endif
#ifndef PCNT_SIG_CH0_IN3_IDX
#define PCNT_SIG_CH0_IN3_IDX 51
#endif
#ifndef PCNT_SIG_CH0_IN4_IDX
#define PCNT_SIG_CH0_IN4_IDX 55
#endif
#ifndef PCNT_SIG_CH0_IN5_IDX
#define PCNT_SIG_CH0_IN5_IDX 71
#endif
#ifndef PCNT_SIG_CH0_IN6_IDX
#define PCNT_SIG_CH0_IN6_IDX 75
#endif
#ifndef PCNT_SIG_CH0_IN7_IDX
#define PCNT_SIG_CH0_IN7_IDX 79
#endif
#ifndef PCNT_SIG_CH1_IN0_IDX
#define PCNT_SIG_CH1_IN0_IDX 40
#endif
#ifndef PCNT_SIG_CH1_IN1_IDX
#define PCNT_SIG_CH1_IN1_IDX 44
#endif
#ifndef PCNT_SIG_CH1_IN2_IDX
#define PCNT_SIG_CH1_IN2_IDX 48
#endif
#ifndef PCNT_SIG_CH1_IN3_IDX
#define PCNT_SIG_CH1_IN3_IDX 52
#endif
#ifndef PCNT_SIG_CH1_IN4_IDX
#define PCNT_SIG_CH1_IN4_IDX 56
#endif
#ifndef PCNT_SIG_CH1_IN5_IDX
#define PCNT_SIG_CH1_IN5_IDX 72
#endif
#ifndef PCNT_SIG_CH1_IN6_IDX
#define PCNT_SIG_CH1_IN6_IDX 76
#endif
#ifndef PCNT_SIG_CH1_IN7_IDX
#define PCNT_SIG_CH1_IN7_IDX 80
#endif
#ifndef PWM0_CAP0_IN_IDX
#define PWM0_CAP0_IN_IDX 109
#endif
#ifndef PWM0_CAP1_IN_IDX
#define PWM0_CAP1_IN_IDX 110
#endif
#ifndef PWM0_CAP2_IN_IDX
#define PWM0_CAP2_IN_IDX 111
#endif
#ifndef PWM0_F0_IN_IDX
#define PWM0_F0_IN_IDX 34
#endif
#ifndef PWM0_F1_IN_IDX
#define PWM0_F1_IN_IDX 35
#endif
#ifndef PWM0_F2_IN_IDX
#define PWM0_F2_IN_IDX 36
#endif
#ifndef PWM0_OUT0A_IDX
#define PWM0_OUT0A_IDX 32
#endif
#ifndef PWM0_OUT0B_IDX
#define PWM0_OUT0B_IDX 33
#endif
#ifndef PWM0_OUT1A_IDX
#define PWM0_OUT1A_IDX 34
#endif
#ifndef PWM0_OUT1B_IDX
#define PWM0_OUT1B_IDX 35
#endif
#ifndef PWM0_OUT2A_IDX
#define PWM0_OUT2A_IDX 36
#endif
#ifndef PWM0_OUT2B_IDX
#define PWM0_OUT2B_IDX 37
#endif
#ifndef PWM0_SYNC0_IN_IDX
#define PWM0_SYNC0_IN_IDX 31
#endif
#ifndef PWM0_SYNC1_IN_IDX
#define PWM0_SYNC1_IN_IDX 32
#endif
#ifndef PWM0_SYNC2_IN_IDX
#define PWM0_SYNC2_IN_IDX 33
#endif
#ifndef PWM1_CAP0_IN_IDX
#define PWM1_CAP0_IN_IDX 112
#endif
#ifndef PWM1_CAP1_IN_IDX
#define PWM1_CAP1_IN_IDX 113
#endif
#ifndef PWM1_CAP2_IN_IDX
#define PWM1_CAP2_IN_IDX 114
#endif
#ifndef PWM1_F0_IN_IDX
#define PWM1_F0_IN_IDX 106
#endif
#ifndef PWM1_F1_IN_IDX
#define PWM1_F1_IN_IDX 107
#endif
#ifndef PWM1_F2_IN_IDX
#define PWM1_F2_IN_IDX 108
#endif
#ifndef PWM1_OUT0A_IDX
#define PWM1_OUT0A_IDX 108
#endif
#ifndef PWM1_OUT0B_IDX
#define PWM1_OUT0B_IDX 109
#endif
#ifndef PWM1_OUT1A_IDX
#define PWM1_OUT1A_IDX 110
#endif
#ifndef PWM1_OUT1B_IDX
#define PWM1_OUT1B_IDX 111
#endif
#ifndef PWM1_OUT2A_IDX
#define PWM1_OUT2A_IDX 112
#endif
#ifndef PWM1_OUT2B_IDX
#define PWM1_OUT2B_IDX 113
#endif
#ifndef PWM1_SYNC0_IN_IDX
#define PWM1_SYNC0_IN_IDX 103
#endif
#ifndef PWM1_SYNC1_IN_IDX
#define PWM1_SYNC1_IN_IDX 104
#endif
#ifndef PWM1_SYNC2_IN_IDX
#define PWM1_SYNC2_IN_IDX 105
#endif
#ifndef RMT_SIG_IN0_IDX
#define RMT_SIG_IN0_IDX 83
#endif
#ifndef RMT_SIG_IN1_IDX
#define RMT_SIG_IN1_IDX 84
#endif
#ifndef RMT_SIG_IN2_IDX
#define RMT_SIG_IN2_IDX 85
#endif
#ifndef RMT_SIG_IN3_IDX
#define RMT_SIG_IN3_IDX 86
#endif
#ifndef RMT_SIG_IN4_IDX
#define RMT_SIG_IN4_IDX 87
#endif
#ifndef RMT_SIG_IN5_IDX
#define RMT_SIG_IN5_IDX 88
#endif
#ifndef RMT_SIG_IN6_IDX
#define RMT_SIG_IN6_IDX 89
#endif
#ifndef RMT_SIG_IN7_IDX
#define RMT_SIG_IN7_IDX 90
#endif
#ifndef RMT_SIG_OUT0_IDX
#define RMT_SIG_OUT0_IDX 87
#endif
#ifndef RMT_SIG_OUT1_IDX
#define RMT_SIG_OUT1_IDX 88
#endif
#ifndef RMT_SIG_OUT2_IDX
#define RMT_SIG_OUT2_IDX 89
#endif
#ifndef RMT_SIG_OUT3_IDX
#define RMT_SIG_OUT3_IDX 90
#endif
#ifndef RMT_SIG_OUT4_IDX
#define RMT_SIG_OUT4_IDX 91
#endif
#ifndef RMT_SIG_OUT5_IDX
#define RMT_SIG_OUT5_IDX 92
#endif
#ifndef RMT_SIG_OUT6_IDX
#define RMT_SIG_OUT6_IDX 93
#endif
#ifndef RMT_SIG_OUT7_IDX
#define RMT_SIG_OUT7_IDX 94
#endif
#ifndef SDIO_TOHOST_INT_OUT_IDX
#define SDIO_TOHOST_INT_OUT_IDX 31
#endif
#ifndef SIG_GPIO_OUT_IDX
#define SIG_GPIO_OUT_IDX 256
#endif
#ifndef SIG_IN_FUNC224_IDX
#define SIG_IN_FUNC224_IDX 224
#endif
#ifndef SIG_IN_FUNC225_IDX
#define SIG_IN_FUNC225_IDX 225
#endif
#ifndef SIG_IN_FUNC226_IDX
#define SIG_IN_FUNC226_IDX 226
#endif
#ifndef SIG_IN_FUNC227_IDX
#define SIG_IN_FUNC227_IDX 227
#endif
#ifndef SIG_IN_FUNC228_IDX
#define SIG_IN_FUNC228_IDX 228
#endif
#ifndef SPICLK_IN_IDX
#define SPICLK_IN_IDX 0
#endif
#ifndef SPICLK_OUT_IDX
#define SPICLK_OUT_IDX 0
#endif
#ifndef SPICS0_IN_IDX
#define SPICS0_IN_IDX 5
#endif
#ifndef SPICS0_OUT_IDX
#define SPICS0_OUT_IDX 5
#endif
#ifndef SPICS1_IN_IDX
#define SPICS1_IN_IDX 6
#endif
#ifndef SPICS1_OUT_IDX
#define SPICS1_OUT_IDX 6
#endif
#ifndef SPICS2_IN_IDX
#define SPICS2_IN_IDX 7
#endif
#ifndef SPICS2_OUT_IDX
#define SPICS2_OUT_IDX 7
#endif
#ifndef SPID4_IN_IDX
#define SPID4_IN_IDX 128
#endif
#ifndef SPID4_OUT_IDX
#define SPID4_OUT_IDX 128
#endif
#ifndef SPID5_IN_IDX
#define SPID5_IN_IDX 129
#endif
#ifndef SPID5_OUT_IDX
#define SPID5_OUT_IDX 129
#endif
#ifndef SPID6_IN_IDX
#define SPID6_IN_IDX 130
#endif
#ifndef SPID6_OUT_IDX
#define SPID6_OUT_IDX 130
#endif
#ifndef SPID7_IN_IDX
#define SPID7_IN_IDX 131
#endif
#ifndef SPID7_OUT_IDX
#define SPID7_OUT_IDX 131
#endif
#ifndef SPID_IN_IDX
#define SPID_IN_IDX 2
#endif
#ifndef SPID_OUT_IDX
#define SPID_OUT_IDX 2
#endif
#ifndef SPIHD_IN_IDX
#define SPIHD_IN_IDX 3
#endif
#ifndef SPIHD_OUT_IDX
#define SPIHD_OUT_IDX 3
#endif
#ifndef SPIQ_IN_IDX
#define SPIQ_IN_IDX 1
#endif
#ifndef SPIQ_OUT_IDX
#define SPIQ_OUT_IDX 1
#endif
#ifndef SPIWP_IN_IDX
#define SPIWP_IN_IDX 4
#endif
#ifndef SPIWP_OUT_IDX
#define SPIWP_OUT_IDX 4
#endif
#ifndef TWAI_BUS_OFF_ON_IDX
#define TWAI_BUS_OFF_ON_IDX 124
#endif
#ifndef TWAI_CLKOUT_IDX
#define TWAI_CLKOUT_IDX 125
#endif
#ifndef TWAI_RX_IDX
#define TWAI_RX_IDX 94
#endif
#ifndef TWAI_TX_IDX
#define TWAI_TX_IDX 123
#endif
#ifndef U0CTS_IN_IDX
#define U0CTS_IN_IDX 15
#endif
#ifndef U0DSR_IN_IDX
#define U0DSR_IN_IDX 16
#endif
#ifndef U0DTR_OUT_IDX
#define U0DTR_OUT_IDX 16
#endif
#ifndef U0RTS_OUT_IDX
#define U0RTS_OUT_IDX 15
#endif
#ifndef U0RXD_IN_IDX
#define U0RXD_IN_IDX 14
#endif
#ifndef U0TXD_OUT_IDX
#define U0TXD_OUT_IDX 14
#endif
#ifndef U1CTS_IN_IDX
#define U1CTS_IN_IDX 18
#endif
#ifndef U1RTS_OUT_IDX
#define U1RTS_OUT_IDX 18
#endif
#ifndef U1RXD_IN_IDX
#define U1RXD_IN_IDX 17
#endif
#ifndef U1TXD_OUT_IDX
#define U1TXD_OUT_IDX 17
#endif
#ifndef U2CTS_IN_IDX
#define U2CTS_IN_IDX 199
#endif
#ifndef U2RTS_OUT_IDX
#define U2RTS_OUT_IDX 199
#endif
#ifndef U2RXD_IN_IDX
#define U2RXD_IN_IDX 198
#endif
#ifndef U2TXD_OUT_IDX
#define U2TXD_OUT_IDX 198
#endif
#ifndef VSPICLK_IN_IDX
#define VSPICLK_IN_IDX 63
#endif
#ifndef VSPICLK_OUT_IDX
#define VSPICLK_OUT_IDX 63
#endif
#ifndef VSPICS0_IN_IDX
#define VSPICS0_IN_IDX 68
#endif
#ifndef VSPICS0_OUT_IDX
#define VSPICS0_OUT_IDX 68
#endif
#ifndef VSPICS1_IN_IDX
#define VSPICS1_IN_IDX 69
#endif
#ifndef VSPICS1_OUT_IDX
#define VSPICS1_OUT_IDX 69
#endif
#ifndef VSPICS2_IN_IDX
#define VSPICS2_IN_IDX 70
#endif
#ifndef VSPICS2_OUT_IDX
#define VSPICS2_OUT_IDX 70
#endif
#ifndef VSPID4_IN_IDX
#define VSPID4_IN_IDX 136
#endif
#ifndef VSPID4_OUT_IDX
#define VSPID4_OUT_IDX 136
#endif
#ifndef VSPID5_IN_IDX
#define VSPID5_IN_IDX 137
#endif
#ifndef VSPID5_OUT_IDX
#define VSPID5_OUT_IDX 137
#endif
#ifndef VSPID6_IN_IDX
#define VSPID6_IN_IDX 138
#endif
#ifndef VSPID6_OUT_IDX
#define VSPID6_OUT_IDX 138
#endif
#ifndef VSPID7_IN_IDX
#define VSPID7_IN_IDX 139
#endif
#ifndef VSPID7_OUT_IDX
#define VSPID7_OUT_IDX 139
#endif
#ifndef VSPID_IN_IDX
#define VSPID_IN_IDX 65
#endif
#ifndef VSPID_OUT_IDX
#define VSPID_OUT_IDX 65
#endif
#ifndef VSPIHD_IN_IDX
#define VSPIHD_IN_IDX 66
#endif
#ifndef VSPIHD_OUT_IDX
#define VSPIHD_OUT_IDX 66
#endif
#ifndef VSPIQ_IN_IDX
#define VSPIQ_IN_IDX 64
#endif
#ifndef VSPIQ_OUT_IDX
#define VSPIQ_OUT_IDX 64
#endif
#ifndef VSPIWP_IN_IDX
#define VSPIWP_IN_IDX 67
#endif
#ifndef VSPIWP_OUT_IDX
#define VSPIWP_OUT_IDX 67
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_SOC_GPIO_SIG_MAP_H__ */
#endif /* _SOC_GPIO_SIG_MAP_H_ */
