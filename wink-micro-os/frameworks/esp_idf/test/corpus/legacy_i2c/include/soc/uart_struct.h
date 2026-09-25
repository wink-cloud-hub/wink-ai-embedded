/* SPDX-License-Identifier: CC0-1.0 */
#ifndef SOC_UART_STRUCT_H_
#define SOC_UART_STRUCT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t dummy;
} uart_dev_t;

extern uart_dev_t UART1;

#ifdef __cplusplus
}
#endif

#endif /* SOC_UART_STRUCT_H_ */
