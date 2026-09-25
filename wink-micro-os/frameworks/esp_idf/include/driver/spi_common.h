/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef DRIVER_SPI_COMMON_H_
#define DRIVER_SPI_COMMON_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "hal/spi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int mosi_io_num;
    int miso_io_num;
    int sclk_io_num;
    int quadwp_io_num;
    int quadhd_io_num;
    int max_transfer_sz;
    uint32_t flags;
    int intr_flags;
} spi_bus_config_t;

esp_err_t spi_bus_initialize(spi_host_device_t host_id, const spi_bus_config_t *bus_config, spi_dma_chan_t dma_chan);
esp_err_t spi_bus_free(spi_host_device_t host_id);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_SPI_COMMON_H_ */
