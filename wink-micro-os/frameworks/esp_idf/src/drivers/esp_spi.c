// SPDX-License-Identifier: LGPL-3.0-only
#include "driver/spi_master.h"
#include "hal/pal_spi.h"
#include "esp_log.h"
#include "esp_sim_handle.h"
#include "esp_idf_wink.h"
#include "esp_fault.h"
#include <string.h>

#define MAX_SPI_DEVS 8

#define EEPROM_AT93C46D_SIZE 128

struct spi_device_t {
    bool in_use;
    pal_spi_device_handle_t pal_handle;
    uint32_t token;
    spi_device_interface_config_t cfg;
    uint8_t pal_bus;
};

static struct spi_device_t s_spis[MAX_SPI_DEVS];
static uint8_t s_eeprom_mem[EEPROM_AT93C46D_SIZE];
static bool s_eeprom_write_enabled = false;

static struct spi_device_t *resolve_spi_device(spi_device_handle_t handle) {
    if (!handle) {
        return NULL;
    }
    uint32_t slot = 0;
    if (!esp_sim_handle_decode(handle, ESP_SIM_HANDLE_SPI, MAX_SPI_DEVS, &slot)) {
        return NULL;
    }
    struct spi_device_t *dev = &s_spis[slot];
    if (!dev->in_use || dev->token != (uint32_t)(uintptr_t)handle) {
        return NULL;
    }
    return dev;
}

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
    wink_status_t st = pal_spi_init_bus(&pcfg);
    return esp_err_from_wink(st);
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
            wink_status_t st = pal_spi_add_device(pal_bus, &dcfg, &pal_dev);
            if (st != WINK_OK) {
                return esp_err_from_wink(st);
            }
            uint32_t token = esp_sim_handle_issue(ESP_SIM_HANDLE_SPI, (uint32_t)i);
            if (!token) {
                pal_spi_remove_device(pal_dev);
                return ESP_ERR_NO_MEM;
            }
            s_spis[i].in_use = true;
            s_spis[i].pal_handle = pal_dev;
            s_spis[i].token = token;
            s_spis[i].cfg = *dev_config;
            s_spis[i].pal_bus = pal_bus;
            *handle = (spi_device_handle_t)(uintptr_t)token;
            return ESP_OK;
        }
    }
    return ESP_ERR_NO_MEM;
}

esp_err_t spi_bus_remove_device(spi_device_handle_t handle) {
    struct spi_device_t *dev = resolve_spi_device(handle);
    if (!dev) {
        return ESP_ERR_INVALID_ARG;
    }
    pal_spi_remove_device(dev->pal_handle);
    dev->in_use = false;
    dev->pal_handle = NULL;
    dev->token = 0;
    return ESP_OK;
}

esp_err_t spi_device_transmit(spi_device_handle_t handle, spi_transaction_t *trans_desc) {
    struct spi_device_t *dev = resolve_spi_device(handle);
    if (!dev || !trans_desc) {
        return ESP_ERR_INVALID_ARG;
    }
    if (sim_esp_fault_is_active(ESP_FAULT_DOMAIN_SPI, ESP_FAULT_SPI_TRANSFER_FAIL)) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    /* AT93C46D Half-Duplex EEPROM emulation (8-bit mode) */
    if (trans_desc->cmd != 0) {
        uint16_t op = trans_desc->cmd & 0x380;
        uint8_t addr = (uint8_t)(trans_desc->cmd & 0x7F);

        if (op == 0x300) {
            /* CMD_READ (Opcode 10): Read byte from EEPROM */
            uint8_t rval = (addr < EEPROM_AT93C46D_SIZE) ? s_eeprom_mem[addr] : 0xFF;
            if (trans_desc->flags & SPI_TRANS_USE_RXDATA) {
                trans_desc->rx_data[0] = rval;
            } else if (trans_desc->rx_buffer) {
                ((uint8_t *)trans_desc->rx_buffer)[0] = rval;
            }
        } else if (op == 0x280) {
            /* CMD_WRITE (Opcode 01): Write byte to EEPROM */
            uint8_t wval = (trans_desc->flags & SPI_TRANS_USE_TXDATA)
                ? trans_desc->tx_data[0]
                : (trans_desc->tx_buffer ? ((const uint8_t *)trans_desc->tx_buffer)[0] : 0);
            if (s_eeprom_write_enabled && addr < EEPROM_AT93C46D_SIZE) {
                s_eeprom_mem[addr] = wval;
            }
        } else if (trans_desc->cmd == 0x260) {
            /* CMD_EWEN (Opcode 00, Addr 11xxxx): Erase/Write Enable */
            s_eeprom_write_enabled = true;
        } else if (trans_desc->cmd == 0x200) {
            /* CMD_EWDS (Opcode 00, Addr 00xxxx): Erase/Write Disable */
            s_eeprom_write_enabled = false;
        } else if (op == 0x380) {
            /* CMD_ERASE (Opcode 11): Erase byte */
            if (s_eeprom_write_enabled && addr < EEPROM_AT93C46D_SIZE) {
                s_eeprom_mem[addr] = 0xFF;
            }
        }
    }

    size_t byte_len = (trans_desc->length > trans_desc->rxlength ? trans_desc->length : trans_desc->rxlength + 7) / 8;
    if (byte_len == 0 && trans_desc->cmd != 0) {
        byte_len = 2; // Pass 10-bit command for physical bus wave/trace
    }

    // 关键防护 R-009：支持 SPI_TRANS_USE_TXDATA 内部数组
    const uint8_t *tx = (trans_desc->flags & SPI_TRANS_USE_TXDATA)
        ? trans_desc->tx_data
        : (const uint8_t *)trans_desc->tx_buffer;
    uint8_t *rx = (trans_desc->flags & SPI_TRANS_USE_RXDATA)
        ? trans_desc->rx_data
        : (uint8_t *)trans_desc->rx_buffer;

    uint8_t dummy_tx[2] = { (uint8_t)(trans_desc->cmd >> 8), (uint8_t)(trans_desc->cmd & 0xFF) };
    if (tx == NULL && rx == NULL) {
        tx = dummy_tx;
    }

    wink_status_t st = WINK_OK;
    if (byte_len > 0) {
        st = pal_spi_transfer_polling(dev->pal_handle, tx, rx, byte_len);
    }
    return esp_err_from_wink(st);
}

esp_err_t spi_device_polling_transmit(spi_device_handle_t handle, spi_transaction_t *trans_desc) {
    struct spi_device_t *dev = resolve_spi_device(handle);
    if (!dev || !trans_desc) {
        return ESP_ERR_INVALID_ARG;
    }

    if (dev->cfg.pre_cb) {
        dev->cfg.pre_cb(trans_desc);
    }

    esp_err_t ret = spi_device_transmit(handle, trans_desc);

    if (dev->cfg.post_cb) {
        dev->cfg.post_cb(trans_desc);
    }

    return ret;
}

esp_err_t spi_device_acquire_bus(spi_device_handle_t device, uint32_t wait) {
    (void)wait;
    struct spi_device_t *dev = resolve_spi_device(device);
    if (!dev) {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

void spi_device_release_bus(spi_device_handle_t dev) {
    (void)dev;
}

void esp_spi_reset(void) {
    for (int i = 0; i < MAX_SPI_DEVS; i++) {
        if (s_spis[i].in_use) {
            pal_spi_remove_device(s_spis[i].pal_handle);
            s_spis[i].in_use = false;
            s_spis[i].pal_handle = NULL;
            s_spis[i].token = 0;
            memset(&s_spis[i].cfg, 0, sizeof(s_spis[i].cfg));
        }
    }
    memset(s_eeprom_mem, 0xFF, sizeof(s_eeprom_mem));
    s_eeprom_write_enabled = false;
    pal_spi_deinit_bus(0);
    pal_spi_deinit_bus(1);
}

