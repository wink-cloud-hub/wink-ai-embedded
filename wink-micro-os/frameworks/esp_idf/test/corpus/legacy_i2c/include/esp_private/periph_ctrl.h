/* SPDX-License-Identifier: CC0-1.0 */
#ifndef ESP_PRIVATE_PERIPH_CTRL_H_
#define ESP_PRIVATE_PERIPH_CTRL_H_

#define PERIPH_RCC_ATOMIC() if (1)

typedef enum {
    PERIPH_I2C0_MODULE = 0,
    PERIPH_I2C1_MODULE,
    PERIPH_UART1_MODULE,
} periph_module_t;

static inline void periph_module_enable(periph_module_t periph) { (void)periph; }
static inline void periph_module_disable(periph_module_t periph) { (void)periph; }
static inline void periph_module_reset(periph_module_t periph) { (void)periph; }

#endif /* ESP_PRIVATE_PERIPH_CTRL_H_ */
