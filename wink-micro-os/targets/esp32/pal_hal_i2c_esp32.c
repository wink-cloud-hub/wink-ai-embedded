// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_hal_i2c_esp32.c
 * @brief ESP32 target PAL HAL I2C subsystem implementation.
 */
#include "pal_hal.h"
#include "hal/pal_i2c.h"

#include <string.h>

#if defined(ESP_PLATFORM)
#include "esp_err.h"
#include "esp_idf_version.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    #include "driver/i2c_master.h"
    #define WINK_I2C_USE_V6_API  1
#else
    #include "driver/i2c.h"
    #define WINK_I2C_USE_V6_API  0
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(7, 0, 0)
    #error "ESP-IDF v7.x I2C API compatibility not verified yet. " \
           "Please update tech-designs/pal-i2c-v6-compatibility.md first."
#endif

#if defined(CONFIG_WINK_I2C_FORCE_V5_API) && CONFIG_WINK_I2C_FORCE_V5_API
    #undef WINK_I2C_USE_V6_API
    #define WINK_I2C_USE_V6_API  0
    #pragma message "Wink I2C: forced to use v5.x compatible API per Kconfig"
#endif

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "wink_pal_i2c";

__attribute__((weak)) const wink_pin_t pal_i2c_pin_map[PAL_I2C_PORTS][2] = {
    {21, 22},
    {33, 32}
};

#define I2C_MAX_DEVICES      4
#define I2C_TRANSFER_TIMEOUT_MS  1000

static bool s_i2c_initialized[PAL_I2C_PORTS] = {false};

static SemaphoreHandle_t s_i2c_mutex = NULL;
static StaticSemaphore_t s_i2c_mutex_buf;
static portMUX_TYPE      s_i2c_mutex_init_lock = portMUX_INITIALIZER_UNLOCKED;

static inline SemaphoreHandle_t pal_i2c_get_mutex(void) {
    if (s_i2c_mutex != NULL) {
        return s_i2c_mutex;
    }
    portENTER_CRITICAL(&s_i2c_mutex_init_lock);
    if (s_i2c_mutex == NULL) {
        s_i2c_mutex = xSemaphoreCreateMutexStatic(&s_i2c_mutex_buf);
    }
    portEXIT_CRITICAL(&s_i2c_mutex_init_lock);
    return s_i2c_mutex;
}

#if WINK_I2C_USE_V6_API
static i2c_master_bus_handle_t s_i2c_bus[PAL_I2C_PORTS] = {NULL};

typedef struct {
    i2c_master_dev_handle_t handle;
    uint16_t                dev_addr;
} i2c_dev_cache_entry_t;

static i2c_dev_cache_entry_t s_i2c_dev_cache[PAL_I2C_PORTS][I2C_MAX_DEVICES] = {
    {{NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}},
    {{NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}}
};

static inline wink_status_t pal_i2c_map_esp_err(esp_err_t esp_err)
{
    if (esp_err == ESP_OK) {
        return WINK_OK;
    }
    if (esp_err == ESP_ERR_TIMEOUT) {
        return WINK_ERR_TIMEOUT;
    }
    if (esp_err == ESP_ERR_NOT_FOUND) {
        return WINK_ERR_DISCONNECTED;
    }
    if (esp_err == ESP_ERR_INVALID_ARG) {
        return WINK_ERR_INVALID_ARG;
    }
    if (esp_err == ESP_ERR_NO_MEM) {
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }
    if (esp_err == ESP_ERR_INVALID_STATE) {
        return WINK_ERR_NOT_INITIALIZED;
    }
    ESP_LOGD(TAG, "unmapped esp_err: %s", esp_err_to_name(esp_err));
    return WINK_ERR_HARDWARE;
}

static inline void pal_i2c_lru_touch(uint8_t port, int hit_idx)
{
    i2c_dev_cache_entry_t hit = s_i2c_dev_cache[port][hit_idx];
    for (int i = hit_idx; i < I2C_MAX_DEVICES - 1; i++) {
        if (s_i2c_dev_cache[port][i + 1].dev_addr == 0) {
            s_i2c_dev_cache[port][i] = hit;
            return;
        }
        s_i2c_dev_cache[port][i] = s_i2c_dev_cache[port][i + 1];
    }
    s_i2c_dev_cache[port][I2C_MAX_DEVICES - 1] = hit;
}

static wink_status_t pal_i2c_get_or_create_device(uint8_t port, uint16_t dev_addr,
                                                   i2c_master_dev_handle_t *out_handle)
{
    int free_slot = -1;
    for (int i = 0; i < I2C_MAX_DEVICES; i++) {
        if (s_i2c_dev_cache[port][i].dev_addr == dev_addr) {
            i2c_master_dev_handle_t handle = s_i2c_dev_cache[port][i].handle;
            pal_i2c_lru_touch(port, i);
            *out_handle = handle;
            return WINK_OK;
        }
        if (s_i2c_dev_cache[port][i].dev_addr == 0 && free_slot == -1) {
            free_slot = i;
        }
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev_addr,
        .scl_speed_hz = 100000,
    };

    i2c_master_dev_handle_t temp_handle = NULL;
    esp_err_t err = i2c_master_bus_add_device(s_i2c_bus[port], &dev_cfg, &temp_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to add I2C device addr 0x%02X: %s",
                 dev_addr, esp_err_to_name(err));
        return pal_i2c_map_esp_err(err);
    }

    if (free_slot == -1) {
        ESP_LOGW(TAG, "port %d device cache full, LRU evicting addr 0x%02X",
                 port, s_i2c_dev_cache[port][0].dev_addr);

        esp_err_t rm_err = i2c_master_bus_rm_device(s_i2c_dev_cache[port][0].handle);
        if (rm_err != ESP_OK) {
            ESP_LOGW(TAG, "evict device 0x%02X failed: %s (ignoring)",
                     s_i2c_dev_cache[port][0].dev_addr, esp_err_to_name(rm_err));
        }

        for (int i = 0; i < I2C_MAX_DEVICES - 1; i++) {
            s_i2c_dev_cache[port][i] = s_i2c_dev_cache[port][i + 1];
        }
        free_slot = I2C_MAX_DEVICES - 1;
    }

    s_i2c_dev_cache[port][free_slot].handle = temp_handle;
    s_i2c_dev_cache[port][free_slot].dev_addr = dev_addr;
    *out_handle = temp_handle;
    return WINK_OK;
}
#else
static inline wink_status_t pal_i2c_map_esp_err(esp_err_t esp_err)
{
    if (esp_err == ESP_OK) {
        return WINK_OK;
    }
    if (esp_err == ESP_ERR_TIMEOUT) {
        return WINK_ERR_TIMEOUT;
    }
    if (esp_err == ESP_ERR_INVALID_ARG) {
        return WINK_ERR_INVALID_ARG;
    }
    if (esp_err == ESP_ERR_INVALID_STATE) {
        return WINK_ERR_NOT_INITIALIZED;
    }
    if (esp_err == ESP_FAIL) {
        return WINK_ERR_DISCONNECTED;
    }
    ESP_LOGD(TAG, "unmapped esp_err: %s", esp_err_to_name(esp_err));
    return WINK_ERR_HARDWARE;
}
#endif

wink_status_t pal_i2c_transfer(uint8_t port, uint16_t dev_addr,
                      const uint8_t *write_buf, uint32_t write_len,
                      uint8_t *read_buf, uint32_t read_len) {
    if (port >= PAL_I2C_PORTS) { return WINK_ERR_INVALID_ARG; }

    if (write_len == 0 && read_len == 0) { return WINK_ERR_INVALID_ARG; }
    if (write_len > 0 && write_buf == NULL) { return WINK_ERR_INVALID_ARG; }
    if (read_len  > 0 && read_buf  == NULL) { return WINK_ERR_INVALID_ARG; }

    SemaphoreHandle_t mutex = pal_i2c_get_mutex();
    if (mutex == NULL) {
        ESP_LOGE(TAG, "I2C mutex allocation failed");
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "I2C mutex timeout");
        return WINK_ERR_BUSY;
    }

    if (!s_i2c_initialized[port]) {
        ESP_LOGW(TAG, "I2C port %d transfer called before bus init, lazy initializing (deprecated path)", port);
#if WINK_I2C_USE_V6_API
        i2c_master_bus_config_t bus_cfg = {
            .i2c_port = (i2c_port_t)port,
            .sda_io_num = pal_i2c_pin_map[port][0],
            .scl_io_num = pal_i2c_pin_map[port][1],
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true,
        };
        esp_err_t err = i2c_new_master_bus(&bus_cfg, &s_i2c_bus[port]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "failed to create I2C master bus port %d: %s",
                     port, esp_err_to_name(err));
            xSemaphoreGive(s_i2c_mutex);
            return WINK_ERR_HARDWARE;
        }
#else
        i2c_config_t cfg = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = pal_i2c_pin_map[port][0],
            .scl_io_num = pal_i2c_pin_map[port][1],
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = 100000,
        };
        esp_err_t err = i2c_param_config((i2c_port_t)port, &cfg);
        if (err != ESP_OK) {
            xSemaphoreGive(s_i2c_mutex);
            return WINK_ERR_HARDWARE;
        }
        err = i2c_driver_install((i2c_port_t)port, cfg.mode, 0, 0, 0);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            xSemaphoreGive(s_i2c_mutex);
            return WINK_ERR_HARDWARE;
        }
#endif
        s_i2c_initialized[port] = true;
    }

    wink_status_t rs = WINK_OK;
    esp_err_t err = ESP_OK;

#if WINK_I2C_USE_V6_API
    i2c_master_dev_handle_t dev_handle = NULL;
    rs = pal_i2c_get_or_create_device(port, dev_addr, &dev_handle);
    if (wink_status_is_error(rs)) {
        xSemaphoreGive(s_i2c_mutex);
        return rs;
    }
#endif

#if WINK_I2C_USE_V6_API
    if (write_buf != NULL && write_len > 0) {
        if (read_buf != NULL && read_len > 0) {
            err = i2c_master_transmit_receive(dev_handle,
                                              write_buf, (size_t)write_len,
                                              read_buf, (size_t)read_len,
                                              I2C_TRANSFER_TIMEOUT_MS);
        } else {
            err = i2c_master_transmit(dev_handle, write_buf, (size_t)write_len,
                                      I2C_TRANSFER_TIMEOUT_MS);
        }
    } else if (read_buf != NULL && read_len > 0) {
        err = i2c_master_receive(dev_handle, read_buf, (size_t)read_len,
                                 I2C_TRANSFER_TIMEOUT_MS);
    }
#else
    err = i2c_master_write_read_device(
        (i2c_port_t)port, dev_addr,
        write_buf, (size_t)write_len,
        read_buf, (size_t)read_len,
        pdMS_TO_TICKS(I2C_TRANSFER_TIMEOUT_MS)
    );
#endif

    xSemaphoreGive(s_i2c_mutex);

    if (err != ESP_OK) {
        return pal_i2c_map_esp_err(err);
    }
    return WINK_OK;
}

wink_status_t pal_i2c_port_pins(uint8_t port, wink_pin_t *out_sda, wink_pin_t *out_scl) {
    if (out_sda == NULL && out_scl == NULL) { return WINK_ERR_INVALID_ARG; }
    if (port >= PAL_I2C_PORTS) { return WINK_ERR_INVALID_ARG; }
    if (out_sda) { *out_sda = pal_i2c_pin_map[port][0]; }
    if (out_scl) { *out_scl = pal_i2c_pin_map[port][1]; }
    return WINK_OK;
}

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
wink_status_t pal_i2c_scan(uint8_t port, uint8_t start_addr, uint8_t end_addr,
                            uint8_t *out_found_bitmap, size_t bitmap_bytes) {
    if (out_found_bitmap == NULL || bitmap_bytes < 16) { return WINK_ERR_INVALID_ARG; }
    if (port >= PAL_I2C_PORTS) { return WINK_ERR_INVALID_ARG; }
    if (start_addr > end_addr || end_addr > 0x7F) { return WINK_ERR_INVALID_ARG; }
    memset(out_found_bitmap, 0, 16);

    uint8_t lo = start_addr < 0x03 ? 0x03 : start_addr;
    uint8_t hi = end_addr   > 0x77 ? 0x77 : end_addr;

    uint8_t dummy;
    wink_status_t probe_st = pal_i2c_transfer(port, lo, NULL, 0, &dummy, 1);
    (void)probe_st;

    for (uint16_t addr = lo; addr <= hi; addr++) {
#if WINK_I2C_USE_V6_API
        SemaphoreHandle_t mutex = pal_i2c_get_mutex();
        if (mutex == NULL || xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            continue;
        }
        esp_err_t err = ESP_FAIL;
        if (s_i2c_initialized[port]) {
            err = i2c_master_probe(s_i2c_bus[port], addr, 50);
        }
        xSemaphoreGive(mutex);
        if (err == ESP_OK) {
            uint8_t byte_idx = (uint8_t)(addr >> 3);
            uint8_t bit_idx  = (uint8_t)(addr & 0x7);
            out_found_bitmap[byte_idx] |= (uint8_t)(1u << bit_idx);
        }
#else
        wink_status_t st = pal_i2c_transfer(port, (uint8_t)addr, NULL, 0, &dummy, 1);
        if (st == WINK_OK) {
            uint8_t byte_idx = (uint8_t)(addr >> 3);
            uint8_t bit_idx  = (uint8_t)(addr & 0x7);
            out_found_bitmap[byte_idx] |= (uint8_t)(1u << bit_idx);
        }
#endif
    }
    return WINK_OK;
}
wink_status_t pal_i2c_bus_init(uint8_t port, uint8_t sda, uint8_t scl, uint32_t hz) {
    if (port >= PAL_I2C_PORTS) { return WINK_ERR_INVALID_ARG; }

    SemaphoreHandle_t mutex = pal_i2c_get_mutex();
    if (mutex == NULL) {
        ESP_LOGE(TAG, "I2C mutex allocation failed");
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "I2C mutex timeout");
        return WINK_ERR_BUSY;
    }

    if (s_i2c_initialized[port]) {
        xSemaphoreGive(mutex);
        return WINK_OK;
    }

#if WINK_I2C_USE_V6_API
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = (i2c_port_t)port,
        .sda_io_num = (gpio_num_t)sda,
        .scl_io_num = (gpio_num_t)scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &s_i2c_bus[port]);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to create I2C master bus port %d: %s",
                 port, esp_err_to_name(err));
        xSemaphoreGive(mutex);
        return pal_i2c_map_esp_err(err);
    }
#else
    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = (gpio_num_t)sda,
        .scl_io_num = (gpio_num_t)scl,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = hz,
    };
    esp_err_t err = i2c_param_config((i2c_port_t)port, &cfg);
    if (err != ESP_OK) {
        xSemaphoreGive(mutex);
        return pal_i2c_map_esp_err(err);
    }
    err = i2c_driver_install((i2c_port_t)port, cfg.mode, 0, 0, 0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        xSemaphoreGive(mutex);
        return pal_i2c_map_esp_err(err);
    }
#endif

    s_i2c_initialized[port] = true;
    xSemaphoreGive(mutex);
    return WINK_OK;
}

void pal_i2c_bus_deinit(uint8_t port) {
    if (port >= PAL_I2C_PORTS) { return; }

    SemaphoreHandle_t mutex = pal_i2c_get_mutex();
    if (mutex == NULL || xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "I2C mutex timeout in bus_deinit");
        return;
    }

    if (!s_i2c_initialized[port]) {
        xSemaphoreGive(mutex);
        return;
    }

#if WINK_I2C_USE_V6_API
    for (int i = 0; i < I2C_MAX_DEVICES; i++) {
        if (s_i2c_dev_cache[port][i].dev_addr != 0) {
            esp_err_t rm_err = i2c_master_bus_rm_device(s_i2c_dev_cache[port][i].handle);
            if (rm_err != ESP_OK) {
                ESP_LOGW(TAG, "remove device 0x%02X in bus_deinit failed: %s",
                         s_i2c_dev_cache[port][i].dev_addr, esp_err_to_name(rm_err));
            }
            s_i2c_dev_cache[port][i].handle = NULL;
            s_i2c_dev_cache[port][i].dev_addr = 0;
        }
    }

    esp_err_t clear_err = i2c_master_bus_reset(s_i2c_bus[port]);
    if (clear_err != ESP_OK) {
        ESP_LOGW(TAG, "clear bus port %d in bus_deinit failed: %s", port, esp_err_to_name(clear_err));
    }

    esp_err_t err = i2c_del_master_bus(s_i2c_bus[port]);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to delete I2C master bus port %d: %s", port, esp_err_to_name(err));
    }
    s_i2c_bus[port] = NULL;
#else
    esp_err_t err = i2c_driver_delete((i2c_port_t)port);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to delete I2C driver port %d: %s", port, esp_err_to_name(err));
    }
#endif

    s_i2c_initialized[port] = false;
    xSemaphoreGive(mutex);
}
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic pop
#endif

#else

wink_status_t pal_i2c_bus_init(uint8_t port, uint8_t sda, uint8_t scl, uint32_t hz) {
    (void)port; (void)sda; (void)scl; (void)hz;
    return WINK_ERR_UNSUPPORTED;
}

void pal_i2c_bus_deinit(uint8_t port) {
    (void)port;
}

wink_status_t pal_i2c_transfer(uint8_t port, uint16_t dev_addr,
                      const uint8_t *write_buf, uint32_t write_len,
                      uint8_t *read_buf, uint32_t read_len)
{
    (void)port; (void)dev_addr; (void)write_buf; (void)write_len;
    (void)read_buf; (void)read_len;
    return WINK_ERR_UNSUPPORTED;
}

wink_status_t pal_i2c_port_pins(uint8_t port, wink_pin_t *out_sda, wink_pin_t *out_scl)
{ (void)port; (void)out_sda; (void)out_scl; return WINK_ERR_UNSUPPORTED; }

wink_status_t pal_i2c_scan(uint8_t port, uint8_t start_addr, uint8_t end_addr,
                            uint8_t *out_found_bitmap, size_t bitmap_bytes)
{ (void)port; (void)start_addr; (void)end_addr; (void)out_found_bitmap; (void)bitmap_bytes; return WINK_ERR_UNSUPPORTED; }

#endif
