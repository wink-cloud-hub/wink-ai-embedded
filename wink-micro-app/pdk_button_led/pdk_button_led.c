/* SPDX-License-Identifier: Apache-2.0
 * PDK Button-drives-LED — Standard Padauk (PFS154 / PMS150C) user source.
 *
 * Production wasm-sim app:
 * Reads a push button on PA.5 (active-low with internal pull-up enabled).
 * When pressed (PA.5 = 0), drives LED on PA.4 to ON (active-high, 1 = lit).
 * When released (PA.5 = 1), drives LED on PA.4 to OFF (0 = unlit).
 */

#include <stdint.h>
#include <device.h>

// Button on PA.5 (Port A, Bit 5) - Active Low with internal pull-up
#define BTN_BIT             5
#define is_button_pressed() (!(PA & (1 << BTN_BIT)))

// LED on PA.4 (Port A, Bit 4) - Active High
#define LED_BIT             4
#define set_led_on()        (PA |= (1 << LED_BIT))
#define set_led_off()       (PA &= ~(1 << LED_BIT))

void main(void) {
    // 1. 初始化按键引脚 PA.5
    PADIER |= (1 << BTN_BIT);       // 使能 PA.5 为数字输入模式
    PAPH   |= (1 << BTN_BIT);       // 使能 PA.5 内部硬件上拉电阻
    PAC    &= ~(1 << BTN_BIT);      // 设为输入方向

    // 2. 初始化 LED 引脚 PA.4
    PAC    |= (1 << LED_BIT);       // 设为输出方向
    set_led_off();                  // 初始状态：熄灭

    // 3. 主轮询循环
    while (1) {
        if (is_button_pressed()) {
            set_led_on();           // 按下时点亮
        } else {
            set_led_off();          // 松开时熄灭
        }
        __asm__("nop");             // 协作式微步推进
    }
}
