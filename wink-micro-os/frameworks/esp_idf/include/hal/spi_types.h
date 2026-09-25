/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef HAL_SPI_TYPES_H_
#define HAL_SPI_TYPES_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SPI1_HOST = 0,
    SPI2_HOST = 1,
    SPI3_HOST = 2,
    SPI_HOST_MAX,
} spi_host_device_t;

#define SPI_HOST  SPI1_HOST
#define HSPI_HOST SPI2_HOST
#define VSPI_HOST SPI3_HOST

typedef int spi_clock_source_t;
#define SPI_CLK_SRC_DEFAULT 0

typedef enum {
    SPI_SAMPLING_POINT_PHASE_0 = 0,
    SPI_SAMPLING_POINT_PHASE_1 = 1,
} spi_sampling_point_t;

typedef enum {
    SPI_DMA_DISABLED = 0,
    SPI_DMA_CH1 = 1,
    SPI_DMA_CH2 = 2,
    SPI_DMA_CH_AUTO = 3,
} spi_dma_chan_t;

#ifdef __cplusplus
}
#endif

#endif /* HAL_SPI_TYPES_H_ */
