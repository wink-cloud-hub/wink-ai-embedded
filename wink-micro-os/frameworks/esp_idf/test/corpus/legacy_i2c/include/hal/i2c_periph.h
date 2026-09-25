/* SPDX-License-Identifier: CC0-1.0 */
#ifndef HAL_I2C_PERIPH_H_
#define HAL_I2C_PERIPH_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define I2C_SCL_HIGH_PERIOD_V 0x3FF

typedef struct {
    struct {
        uint32_t ms_mode : 1;
        uint32_t reserved : 31;
    } ctr;
    struct {
        uint32_t rx_fifo_rst : 1;
        uint32_t tx_fifo_rst : 1;
        uint32_t reserved : 30;
    } fifo_conf;
    uint32_t timeout;
    uint32_t scl_low_period;
    uint32_t scl_high_period;
    struct {
        uint32_t rx_fifo_cnt : 6;
        uint32_t reserved : 26;
    } rxfifo_st;
} i2c_dev_t;

extern i2c_dev_t I2C0;
extern i2c_dev_t I2C1;

typedef struct {
    int scl_out_sig;
    int sda_out_sig;
    int scl_in_sig;
    int sda_in_sig;
} i2c_periph_signal_t;

extern const i2c_periph_signal_t i2c_periph_signal[];

#ifdef __cplusplus
}
#endif

#endif /* HAL_I2C_PERIPH_H_ */
