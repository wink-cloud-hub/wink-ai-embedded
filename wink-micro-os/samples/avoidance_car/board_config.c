/**
 * @file board_config.c
 * @brief 板级硬件路由（物理引脚映射）—— codegen 产物占位。
 *
 * 与 device_tree.c 分离：device_tree.c 描述逻辑设备实例（servo/ultrasonic），
 * 本文件描述 PWM channel→GPIO 的物理路由。仅物理 firmware 链接；host/wasm 不引用。
 */
#include "pal_hal.h"

/* 强定义，覆盖 esp32 target 的弱默认。*/
const uint16_t pal_pwm_pin_map[PAL_PWM_CHANNELS] = {2, 4, 5, 18, 19, 21, 22, 23};

/* avoidance_car I2C 路由：I2C0 接 OLED（SDA=21, SCL=22），I2C1 预留（SDA=33, SCL=32）。
 * 强定义，覆盖 esp32 target 的弱默认 pal_i2c_pin_map。*/
const uint16_t pal_i2c_pin_map[PAL_I2C_PORTS][2] = {
    {21, 22},
    {33, 32}
};
