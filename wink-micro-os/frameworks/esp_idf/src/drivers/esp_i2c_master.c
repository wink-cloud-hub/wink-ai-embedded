// SPDX-License-Identifier: LGPL-3.0-only
#include "driver/i2c_master.h"
#include "hal/pal_i2c.h"
#include <string.h>

#define MAX_MASTER_BUSES PAL_I2C_PORT_MAX
#define MAX_MASTER_DEVICES 8

struct i2c_master_bus_t {
    bool in_use;
    uint8_t port;
};

struct i2c_master_dev_t {
    bool in_use;
    struct i2c_master_bus_t *bus;
    uint16_t addr;
    uint32_t speed_hz;
};

static struct i2c_master_bus_t s_buses[MAX_MASTER_BUSES];
static struct i2c_master_dev_t s_devices[MAX_MASTER_DEVICES];

esp_err_t i2c_new_master_bus(const i2c_master_bus_config_t *bus_config, i2c_master_bus_handle_t *ret_bus_handle) {
    if (!bus_config || !ret_bus_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t port = 0;
    if (bus_config->i2c_port == -1) {
        // Auto select first free port
        bool found = false;
        for (uint8_t i = 0; i < MAX_MASTER_BUSES; i++) {
            if (!s_buses[i].in_use) {
                port = i;
                found = true;
                break;
            }
        }
        if (!found) {
            return ESP_ERR_NOT_FOUND;
        }
    } else {
        if (bus_config->i2c_port < 0 || (uint32_t)bus_config->i2c_port >= MAX_MASTER_BUSES) {
            return ESP_ERR_INVALID_ARG;
        }
        port = (uint8_t)bus_config->i2c_port;
    }

    if (s_buses[port].in_use) {
        return ESP_ERR_INVALID_STATE;
    }

    wink_status_t st = pal_i2c_bus_init(port, (wink_pin_t)bus_config->sda_io_num, (wink_pin_t)bus_config->scl_io_num, 400000);
    if (st != WINK_OK) {
        return ESP_FAIL;
    }

    s_buses[port].in_use = true;
    s_buses[port].port = port;
    *ret_bus_handle = &s_buses[port];
    return ESP_OK;
}

esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t bus_handle, const i2c_device_config_t *dev_config, i2c_master_dev_handle_t *ret_handle) {
    if (!bus_handle || !dev_config || !ret_handle || !bus_handle->in_use) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < MAX_MASTER_DEVICES; i++) {
        if (!s_devices[i].in_use) {
            s_devices[i].in_use = true;
            s_devices[i].bus = bus_handle;
            s_devices[i].addr = dev_config->device_address;
            s_devices[i].speed_hz = dev_config->scl_speed_hz;
            *ret_handle = &s_devices[i];
            return ESP_OK;
        }
    }
    return ESP_ERR_NO_MEM;
}

esp_err_t i2c_master_transmit(i2c_master_dev_handle_t handle, const uint8_t *write_buffer, size_t write_size, int xfer_timeout_ms) {
    if (!handle || !handle->in_use || !handle->bus || !handle->bus->in_use) {
        return ESP_ERR_INVALID_ARG;
    }
    uint32_t t_ms = (xfer_timeout_ms < 0) ? PAL_I2C_DEFAULT_TIMEOUT_MS : (uint32_t)xfer_timeout_ms;
    wink_status_t st = pal_i2c_transfer_timeout(handle->bus->port, handle->addr, write_buffer, (uint32_t)write_size, NULL, 0, t_ms);
    return (st == WINK_OK) ? ESP_OK : ESP_FAIL;
}

esp_err_t i2c_master_receive(i2c_master_dev_handle_t handle, uint8_t *read_buffer, size_t read_size, int xfer_timeout_ms) {
    if (!handle || !handle->in_use || !handle->bus || !handle->bus->in_use) {
        return ESP_ERR_INVALID_ARG;
    }
    uint32_t t_ms = (xfer_timeout_ms < 0) ? PAL_I2C_DEFAULT_TIMEOUT_MS : (uint32_t)xfer_timeout_ms;
    wink_status_t st = pal_i2c_transfer_timeout(handle->bus->port, handle->addr, NULL, 0, read_buffer, (uint32_t)read_size, t_ms);
    return (st == WINK_OK) ? ESP_OK : ESP_FAIL;
}

esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t handle, const uint8_t *write_buffer, size_t write_size, uint8_t *read_buffer, size_t read_size, int xfer_timeout_ms) {
    if (!handle || !handle->in_use || !handle->bus || !handle->bus->in_use) {
        return ESP_ERR_INVALID_ARG;
    }
    uint32_t t_ms = (xfer_timeout_ms < 0) ? PAL_I2C_DEFAULT_TIMEOUT_MS : (uint32_t)xfer_timeout_ms;
    wink_status_t st = pal_i2c_transfer_timeout(handle->bus->port, handle->addr, write_buffer, (uint32_t)write_size, read_buffer, (uint32_t)read_size, t_ms);
    return (st == WINK_OK) ? ESP_OK : ESP_FAIL;
}

esp_err_t i2c_master_probe(i2c_master_bus_handle_t bus_handle, uint16_t address, int xfer_timeout_ms) {
    if (!bus_handle || !bus_handle->in_use) {
        return ESP_ERR_INVALID_ARG;
    }
    uint32_t t_ms = (xfer_timeout_ms < 0) ? PAL_I2C_DEFAULT_TIMEOUT_MS : (uint32_t)xfer_timeout_ms;
    wink_status_t st = pal_i2c_transfer_timeout(bus_handle->port, address, NULL, 0, NULL, 0, t_ms);
    return (st == WINK_OK) ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t i2c_del_master_bus(i2c_master_bus_handle_t bus_handle) {
    if (!bus_handle || !bus_handle->in_use) {
        return ESP_ERR_INVALID_ARG;
    }
    for (int i = 0; i < MAX_MASTER_DEVICES; i++) {
        if (s_devices[i].in_use && s_devices[i].bus == bus_handle) {
            s_devices[i].in_use = false;
            s_devices[i].bus = NULL;
        }
    }
    pal_i2c_bus_deinit(bus_handle->port);
    bus_handle->in_use = false;
    return ESP_OK;
}

esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t handle) {
    if (!handle || !handle->in_use) {
        return ESP_ERR_INVALID_ARG;
    }
    handle->in_use = false;
    handle->bus = NULL;
    return ESP_OK;
}

void esp_i2c_master_reset(void) {
    for (int i = 0; i < MAX_MASTER_DEVICES; i++) {
        s_devices[i].in_use = false;
        s_devices[i].bus = NULL;
    }
    for (int i = 0; i < MAX_MASTER_BUSES; i++) {
        if (s_buses[i].in_use) {
            pal_i2c_bus_deinit((uint8_t)i);
            s_buses[i].in_use = false;
        }
    }
}
