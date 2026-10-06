// SPDX-License-Identifier: LGPL-3.0-only
#include "driver/i2c_master.h"
#include "hal/pal_i2c.h"
#include "esp_sim_handle.h"
#include "esp_idf_wink.h"
#include "esp_sim_fault.h"
#include <string.h>

#define MAX_MASTER_BUSES SOC_HP_I2C_NUM
#define MAX_MASTER_DEVICES 8

struct i2c_master_bus_t {
    bool in_use;
    uint8_t port;
    uint32_t token;
};

struct i2c_master_dev_t {
    bool in_use;
    struct i2c_master_bus_t *bus;
    uint16_t addr;
    uint32_t speed_hz;
    uint32_t token;
};

static struct i2c_master_bus_t s_buses[MAX_MASTER_BUSES];
static struct i2c_master_dev_t s_devices[MAX_MASTER_DEVICES];

static struct i2c_master_bus_t *resolve_i2c_bus(i2c_master_bus_handle_t handle) {
    if (!handle) {
        return NULL;
    }
    uint32_t slot = 0;
    if (!esp_sim_handle_decode(handle, ESP_SIM_HANDLE_I2C_BUS, MAX_MASTER_BUSES, &slot)) {
        return NULL;
    }
    struct i2c_master_bus_t *bus = &s_buses[slot];
    if (!bus->in_use || bus->token != (uint32_t)(uintptr_t)handle) {
        return NULL;
    }
    return bus;
}

static struct i2c_master_dev_t *resolve_i2c_dev(i2c_master_dev_handle_t handle) {
    if (!handle) {
        return NULL;
    }
    uint32_t slot = 0;
    if (!esp_sim_handle_decode(handle, ESP_SIM_HANDLE_I2C_DEV, MAX_MASTER_DEVICES, &slot)) {
        return NULL;
    }
    struct i2c_master_dev_t *dev = &s_devices[slot];
    if (!dev->in_use || dev->token != (uint32_t)(uintptr_t)handle) {
        return NULL;
    }
    return dev;
}

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
        return esp_err_from_wink(st);
    }

    uint32_t token = esp_sim_handle_issue(ESP_SIM_HANDLE_I2C_BUS, port);
    if (!token) {
        wink_status_t dst = pal_i2c_bus_deinit(port);
        (void)dst;
        return ESP_ERR_NO_MEM;
    }

    s_buses[port].in_use = true;
    s_buses[port].port = port;
    s_buses[port].token = token;
    *ret_bus_handle = (i2c_master_bus_handle_t)(uintptr_t)token;
    return ESP_OK;
}

esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t bus_handle, const i2c_device_config_t *dev_config, i2c_master_dev_handle_t *ret_handle) {
    struct i2c_master_bus_t *bus = resolve_i2c_bus(bus_handle);
    if (!bus || !dev_config || !ret_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < MAX_MASTER_DEVICES; i++) {
        if (!s_devices[i].in_use) {
            uint32_t token = esp_sim_handle_issue(ESP_SIM_HANDLE_I2C_DEV, (uint32_t)i);
            if (!token) {
                return ESP_ERR_NO_MEM;
            }
            s_devices[i].in_use = true;
            s_devices[i].bus = bus;
            s_devices[i].addr = dev_config->device_address;
            s_devices[i].speed_hz = dev_config->scl_speed_hz;
            s_devices[i].token = token;
            *ret_handle = (i2c_master_dev_handle_t)(uintptr_t)token;
            return ESP_OK;
        }
    }
    return ESP_ERR_NO_MEM;
}

esp_err_t i2c_master_transmit(i2c_master_dev_handle_t handle, const uint8_t *write_buffer, size_t write_size, int xfer_timeout_ms) {
    struct i2c_master_dev_t *dev = resolve_i2c_dev(handle);
    if (!dev || !dev->bus || !dev->bus->in_use) {
        return ESP_ERR_INVALID_ARG;
    }
    if (sim_esp_fault_is_active(ESP_FAULT_DOMAIN_I2C, ESP_FAULT_I2C_NACK)) {
        uint32_t p = sim_esp_fault_get_param(ESP_FAULT_DOMAIN_I2C, ESP_FAULT_I2C_NACK);
        if (p == 0 || p == dev->addr) {
            return ESP_ERR_NOT_FOUND;
        }
    }
    if (sim_esp_fault_is_active(ESP_FAULT_DOMAIN_I2C, ESP_FAULT_I2C_TIMEOUT)) {
        uint32_t p = sim_esp_fault_get_param(ESP_FAULT_DOMAIN_I2C, ESP_FAULT_I2C_TIMEOUT);
        if (p == 0 || p == dev->addr) {
            return ESP_ERR_TIMEOUT;
        }
    }
    uint32_t t_ms = (xfer_timeout_ms < 0) ? PAL_I2C_DEFAULT_TIMEOUT_MS : (uint32_t)xfer_timeout_ms;
    wink_status_t st = pal_i2c_transfer_timeout(dev->bus->port, dev->addr, write_buffer, (uint32_t)write_size, NULL, 0, t_ms);
    return esp_err_from_wink(st);
}

esp_err_t i2c_master_receive(i2c_master_dev_handle_t handle, uint8_t *read_buffer, size_t read_size, int xfer_timeout_ms) {
    struct i2c_master_dev_t *dev = resolve_i2c_dev(handle);
    if (!dev || !dev->bus || !dev->bus->in_use) {
        return ESP_ERR_INVALID_ARG;
    }
    if (sim_esp_fault_is_active(ESP_FAULT_DOMAIN_I2C, ESP_FAULT_I2C_NACK)) {
        uint32_t p = sim_esp_fault_get_param(ESP_FAULT_DOMAIN_I2C, ESP_FAULT_I2C_NACK);
        if (p == 0 || p == dev->addr) {
            return ESP_ERR_NOT_FOUND;
        }
    }
    if (sim_esp_fault_is_active(ESP_FAULT_DOMAIN_I2C, ESP_FAULT_I2C_TIMEOUT)) {
        uint32_t p = sim_esp_fault_get_param(ESP_FAULT_DOMAIN_I2C, ESP_FAULT_I2C_TIMEOUT);
        if (p == 0 || p == dev->addr) {
            return ESP_ERR_TIMEOUT;
        }
    }
    uint32_t t_ms = (xfer_timeout_ms < 0) ? PAL_I2C_DEFAULT_TIMEOUT_MS : (uint32_t)xfer_timeout_ms;
    wink_status_t st = pal_i2c_transfer_timeout(dev->bus->port, dev->addr, NULL, 0, read_buffer, (uint32_t)read_size, t_ms);
    return esp_err_from_wink(st);
}

esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t handle, const uint8_t *write_buffer, size_t write_size, uint8_t *read_buffer, size_t read_size, int xfer_timeout_ms) {
    struct i2c_master_dev_t *dev = resolve_i2c_dev(handle);
    if (!dev || !dev->bus || !dev->bus->in_use) {
        return ESP_ERR_INVALID_ARG;
    }
    if (sim_esp_fault_is_active(ESP_FAULT_DOMAIN_I2C, ESP_FAULT_I2C_NACK)) {
        uint32_t p = sim_esp_fault_get_param(ESP_FAULT_DOMAIN_I2C, ESP_FAULT_I2C_NACK);
        if (p == 0 || p == dev->addr) {
            return ESP_ERR_NOT_FOUND;
        }
    }
    if (sim_esp_fault_is_active(ESP_FAULT_DOMAIN_I2C, ESP_FAULT_I2C_TIMEOUT)) {
        uint32_t p = sim_esp_fault_get_param(ESP_FAULT_DOMAIN_I2C, ESP_FAULT_I2C_TIMEOUT);
        if (p == 0 || p == dev->addr) {
            return ESP_ERR_TIMEOUT;
        }
    }
    uint32_t t_ms = (xfer_timeout_ms < 0) ? PAL_I2C_DEFAULT_TIMEOUT_MS : (uint32_t)xfer_timeout_ms;
    wink_status_t st = pal_i2c_transfer_timeout(dev->bus->port, dev->addr, write_buffer, (uint32_t)write_size, read_buffer, (uint32_t)read_size, t_ms);
    return esp_err_from_wink(st);
}

esp_err_t i2c_master_probe(i2c_master_bus_handle_t bus_handle, uint16_t address, int xfer_timeout_ms) {
    struct i2c_master_bus_t *bus = resolve_i2c_bus(bus_handle);
    if (!bus) {
        return ESP_ERR_INVALID_ARG;
    }
    if (sim_esp_fault_is_active(ESP_FAULT_DOMAIN_I2C, ESP_FAULT_I2C_NACK)) {
        uint32_t p = sim_esp_fault_get_param(ESP_FAULT_DOMAIN_I2C, ESP_FAULT_I2C_NACK);
        if (p == 0 || p == address) {
            return ESP_ERR_NOT_FOUND;
        }
    }
    if (sim_esp_fault_is_active(ESP_FAULT_DOMAIN_I2C, ESP_FAULT_I2C_TIMEOUT)) {
        uint32_t p = sim_esp_fault_get_param(ESP_FAULT_DOMAIN_I2C, ESP_FAULT_I2C_TIMEOUT);
        if (p == 0 || p == address) {
            return ESP_ERR_TIMEOUT;
        }
    }
    uint32_t t_ms = (xfer_timeout_ms < 0) ? PAL_I2C_DEFAULT_TIMEOUT_MS : (uint32_t)xfer_timeout_ms;
    wink_status_t st = pal_i2c_transfer_timeout(bus->port, address, NULL, 0, NULL, 0, t_ms);
    if (st == WINK_OK) {
        return ESP_OK;
    }
    if (st == WINK_ERR_TIMEOUT) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t i2c_del_master_bus(i2c_master_bus_handle_t bus_handle) {
    struct i2c_master_bus_t *bus = resolve_i2c_bus(bus_handle);
    if (!bus) {
        return ESP_ERR_INVALID_ARG;
    }
    for (int i = 0; i < MAX_MASTER_DEVICES; i++) {
        if (s_devices[i].in_use && s_devices[i].bus == bus) {
            s_devices[i].in_use = false;
            s_devices[i].bus = NULL;
            s_devices[i].token = 0;
        }
    }
    wink_status_t st = pal_i2c_bus_deinit(bus->port);
    (void)st;
    bus->in_use = false;
    bus->token = 0;
    return ESP_OK;
}

esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t handle) {
    struct i2c_master_dev_t *dev = resolve_i2c_dev(handle);
    if (!dev) {
        return ESP_ERR_INVALID_ARG;
    }
    dev->in_use = false;
    dev->bus = NULL;
    dev->token = 0;
    return ESP_OK;
}

void esp_i2c_master_reset(void) {
    for (uint32_t i = 0; i < MAX_MASTER_DEVICES; i++) {
        s_devices[i].in_use = false;
        s_devices[i].bus = NULL;
        s_devices[i].token = 0;
    }
    for (uint32_t i = 0; i < MAX_MASTER_BUSES; i++) {
        if (s_buses[i].in_use) {
            wink_status_t st = pal_i2c_bus_deinit((uint8_t)i);
            (void)st;
            s_buses[i].in_use = false;
            s_buses[i].token = 0;
        }
    }
}
