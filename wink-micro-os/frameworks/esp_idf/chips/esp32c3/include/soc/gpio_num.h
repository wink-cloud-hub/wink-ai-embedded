/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef SOC_GPIO_NUM_ESP32C3_H_
#define SOC_GPIO_NUM_ESP32C3_H_

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
    GPIO_NUM_20 = 20, GPIO_NUM_21 = 21,
    GPIO_NUM_MAX = 22,
} gpio_num_t;

#ifdef __cplusplus
}
#endif

#endif /* SOC_GPIO_NUM_ESP32C3_H_ */
