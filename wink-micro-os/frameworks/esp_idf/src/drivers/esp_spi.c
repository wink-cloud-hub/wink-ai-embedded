// SPDX-License-Identifier: LGPL-3.0-only
#include "driver/spi_master.h"
#include "hal/pal_spi.h"
#include "esp_log.h"
#include <string.h>

#define MAX_SPI_DEVS 8

struct spi_device_t {
    bool in_use;
    pal_spi_device_handle_t pal_handle;
};

static struct spi_device_t s_spis[MAX_SPI_DEVS];

esp_err_t spi_bus_initialize(spi_host_device_t host_id, const spi_bus_config_t *bus_config, spi_dma_chan_t dma_chan) {
    (void)dma_chan;
    if (!bus_config) {
        return ESP_ERR_INVALID_ARG;
    }
    /* ADR-0085 D1：SPI_HOST 枚举保持全集，SoC 端口上限运行期 Fail-Loud（C3/C6 仅 2 个 SPI） */
    if (host_id >= SOC_SPI_PERIPH_NUM) {
        ESP_LOGE("esp_spi", "SPI host %d not available on current SoC (max %d)",
                 (int)host_id, (int)SOC_SPI_PERIPH_NUM);
        return ESP_ERR_INVALID_ARG;
    }
    // 关键防护 R-009：IDF SPI2_HOST=1, SPI3_HOST=2 映射至 PAL 0 与 1
    uint8_t pal_bus = 255;
    if (host_id == SPI2_HOST) {
        pal_bus = 0;
    } else if (host_id == SPI3_HOST) {
        pal_bus = 1;
    } else {
        return ESP_ERR_INVALID_ARG;
    }

    pal_spi_bus_config_t pcfg = {
        .spi_bus = pal_bus,
        .sclk = (wink_pin_t)bus_config->sclk_io_num,
        .mosi = (wink_pin_t)bus_config->mosi_io_num,
        .miso = (wink_pin_t)bus_config->miso_io_num,
        .clock_hz = 10000000,
        .mode = 0,
        .dma_enabled = false,
        .timeout_ms = PAL_SPI_DEFAULT_TIMEOUT_MS
    };
    return (pal_spi_init_bus(&pcfg) == WINK_OK) ? ESP_OK : ESP_FAIL;
}

esp_err_t spi_bus_free(spi_host_device_t host_id) {
    uint8_t pal_bus = (host_id == SPI2_HOST) ? 0 : (host_id == SPI3_HOST ? 1 : 255);
    if (pal_bus == 255) {
        return ESP_ERR_INVALID_ARG;
    }
    pal_spi_deinit_bus(pal_bus);
    return ESP_OK;
}

esp_err_t spi_bus_add_device(spi_host_device_t host_id, const spi_device_interface_config_t *dev_config, spi_device_handle_t *handle) {
    if (!dev_config || !handle) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t pal_bus = (host_id == SPI2_HOST) ? 0 : (host_id == SPI3_HOST ? 1 : 255);
    if (pal_bus == 255) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < MAX_SPI_DEVS; i++) {
        if (!s_spis[i].in_use) {
            pal_spi_device_config_t dcfg = {
                .cs_pin = (wink_pin_t)dev_config->spics_io_num,
                .clock_hz = (uint32_t)dev_config->clock_speed_hz,
                .mode = (uint8_t)dev_config->mode,
                .cs_active_high = (dev_config->flags & SPI_DEVICE_POSITIVE_CS) ? true : false,
                .cs_setup_ns = 0,
                .cs_hold_ns = 0
            };
            pal_spi_device_handle_t pal_dev = NULL;
            if (pal_spi_add_device(pal_bus, &dcfg, &pal_dev) != WINK_OK) {
                return ESP_FAIL;
            }
            s_spis[i].in_use = true;
            s_spis[i].pal_handle = pal_dev;
            *handle = &s_spis[i];
            return ESP_OK;
        }
    }
    return ESP_ERR_NO_MEM;
}

esp_err_t spi_bus_remove_device(spi_device_handle_t handle) {
    if (!handle || !handle->in_use) {
        return ESP_ERR_INVALID_ARG;
    }
    pal_spi_remove_device(handle->pal_handle);
    handle->in_use = false;
    handle->pal_handle = NULL;
    return ESP_OK;
}

esp_err_t spi_device_transmit(spi_device_handle_t handle, spi_transaction_t *trans_desc) {
    if (!handle || !handle->in_use || !trans_desc) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t byte_len = (trans_desc->length + 7) / 8;

    // 关键防护 R-009：支持 SPI_TRANS_USE_TXDATA 内部数组
    const uint8_t *tx = (trans_desc->flags & SPI_TRANS_USE_TXDATA)
        ? trans_desc->tx_data
        : (const uint8_t *)trans_desc->tx_buffer;
    uint8_t *rx = (trans_desc->flags & SPI_TRANS_USE_RXDATA)
        ? trans_desc->rx_data
        : (uint8_t *)trans_desc->rx_buffer;

    wink_status_t st = pal_spi_transfer_polling(handle->pal_handle, tx, rx, byte_len);
    return (st == WINK_OK) ? ESP_OK : ESP_FAIL;
}

void esp_spi_reset(void) {
    for (int i = 0; i < MAX_SPI_DEVS; i++) {
        if (s_spis[i].in_use) {
            pal_spi_remove_device(s_spis[i].pal_handle);
            s_spis[i].in_use = false;
            s_spis[i].pal_handle = NULL;
        }
    }
    pal_spi_deinit_bus(0);
    pal_spi_deinit_bus(1);
}
