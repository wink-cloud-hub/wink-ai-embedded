/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef DRIVER_SPI_MASTER_H_
#define DRIVER_SPI_MASTER_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "hal/spi_types.h"
#include "driver/spi_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SPI_TRANS_USE_RXDATA (1<<2)
#define SPI_TRANS_USE_TXDATA (1<<3)

#define SPI_DEVICE_TXBIT_LSBFIRST          (1<<0)
#define SPI_DEVICE_RXBIT_LSBFIRST          (1<<1)
#define SPI_DEVICE_BIT_LSBFIRST            (SPI_DEVICE_TXBIT_LSBFIRST|SPI_DEVICE_RXBIT_LSBFIRST)
#define SPI_DEVICE_3WIRE                   (1<<2)
#define SPI_DEVICE_POSITIVE_CS             (1<<3)
#define SPI_DEVICE_HALFDUPLEX              (1<<4)
#define SPI_DEVICE_CLK_AS_CS               (1<<5)
#define SPI_DEVICE_NO_DUMMY                (1<<6)
#define SPI_DEVICE_DDR                     (1<<7)

typedef struct spi_transaction_t spi_transaction_t;

struct spi_transaction_t {
    uint32_t flags;
    uint16_t cmd;
    uint64_t addr;
    size_t length;
    size_t rxlength;
    void *user;
    union {
        const void *tx_buffer;
        uint8_t tx_data[4];
    };
    union {
        void *rx_buffer;
        uint8_t rx_data[4];
    };
};

typedef struct {
    uint8_t command_bits;
    uint8_t address_bits;
    uint8_t dummy_bits;
    uint8_t mode;
    int clock_speed_hz;
    int spics_io_num;
    uint32_t flags;
    int queue_size;
} spi_device_interface_config_t;

typedef struct spi_device_t *spi_device_handle_t;

esp_err_t spi_bus_add_device(spi_host_device_t host_id, const spi_device_interface_config_t *dev_config, spi_device_handle_t *handle);
esp_err_t spi_bus_remove_device(spi_device_handle_t handle);
esp_err_t spi_device_transmit(spi_device_handle_t handle, spi_transaction_t *trans_desc);

void esp_spi_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_SPI_MASTER_H_ */
