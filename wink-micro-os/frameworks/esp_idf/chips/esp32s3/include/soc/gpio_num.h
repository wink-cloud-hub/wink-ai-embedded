/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef SOC_GPIO_NUM_ESP32S3_H_
#define SOC_GPIO_NUM_ESP32S3_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GPIO_NUM_NC = -1,
    GPIO_NUM_0 = 0,   GPIO_NUM_1 = 1,   GPIO_NUM_2 = 2,   GPIO_NUM_3 = 3,
    GPIO_NUM_4 = 4,   GPIO_NUM_5 = 5,   GPIO_NUM_6 = 6,   GPIO_NUM_7 = 7,
    GPIO_NUM_8 = 8,   GPIO_NUM_9 = 9,   GPIO_NUM_10 = 10, GPIO_NUM_11 = 11,
    GPIO_NUM_12 = 12, GPIO_NUM_13 = 13, GPIO_NUM_14 = 14, GPIO_NUM_15 = 15,
    GPIO_NUM_16 = 16, GPIO_NUM_17 = 17, GPIO_NUM_18 = 18, GPIO_NUM_19 = 19,
    GPIO_NUM_20 = 20, GPIO_NUM_21 = 21, /* 22~25 物理不存在，官方枚举跳过 */
    GPIO_NUM_26 = 26, GPIO_NUM_27 = 27, GPIO_NUM_28 = 28, GPIO_NUM_29 = 29,
    GPIO_NUM_30 = 30, GPIO_NUM_31 = 31, GPIO_NUM_32 = 32, GPIO_NUM_33 = 33,
    GPIO_NUM_34 = 34, GPIO_NUM_35 = 35, GPIO_NUM_36 = 36, GPIO_NUM_37 = 37,
    GPIO_NUM_38 = 38, GPIO_NUM_39 = 39, GPIO_NUM_40 = 40, GPIO_NUM_41 = 41,
    GPIO_NUM_42 = 42, GPIO_NUM_43 = 43, GPIO_NUM_44 = 44, GPIO_NUM_45 = 45,
    GPIO_NUM_46 = 46, GPIO_NUM_47 = 47, GPIO_NUM_48 = 48,
    GPIO_NUM_MAX = 49,
} gpio_num_t;

#ifdef __cplusplus
}
#endif

#endif /* SOC_GPIO_NUM_ESP32S3_H_ */
