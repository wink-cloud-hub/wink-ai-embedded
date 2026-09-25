// SPDX-License-Identifier: LGPL-3.0-only
#include "driver/i2c.h"
#include "hal/pal_i2c.h"
#include "esp_log.h"
#include <string.h>
#include <assert.h>

#define I2C_CMD_BUFFER_SIZE 256
#define MAX_CMD_LINKS 4

typedef enum {
    CMD_START,
    CMD_WRITE,
    CMD_READ,
    CMD_STOP
} cmd_type_t;

typedef struct {
    cmd_type_t type;
    uint8_t *data_ptr;
    uint32_t total_bytes;
    bool ack_en;
} cmd_entry_t;

typedef struct {
    bool in_use;
    cmd_entry_t entries[16];
    uint32_t entry_count;
    uint8_t write_storage[I2C_CMD_BUFFER_SIZE];
    uint32_t write_offset;
} esp_i2c_cmd_link_t;

static esp_i2c_cmd_link_t s_cmd_links[MAX_CMD_LINKS];

esp_err_t i2c_driver_install(i2c_port_t i2c_num, i2c_mode_t mode, size_t slv_rx_buf_len, size_t slv_tx_buf_len, int intr_alloc_flags) {
    (void)slv_rx_buf_len;
    (void)slv_tx_buf_len;
    (void)intr_alloc_flags;
    if (i2c_num >= SOC_HP_I2C_NUM) {
        return ESP_ERR_INVALID_ARG;
    }
    // 严格红线 6：PAL 仅支持主机模式，从机直接 Fail-Loud
    if (mode != I2C_MODE_MASTER) {
        ESP_LOGE("esp_i2c", "I2C slave mode is not supported in simulation");
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

esp_err_t i2c_driver_delete(i2c_port_t i2c_num) {
    if (i2c_num >= SOC_HP_I2C_NUM) {
        return ESP_ERR_INVALID_ARG;
    }
    wink_status_t st = pal_i2c_bus_deinit((uint8_t)i2c_num);
    (void)st;
    return ESP_OK;
}

esp_err_t i2c_param_config(i2c_port_t i2c_num, const i2c_config_t *i2c_conf) {
    if (i2c_num >= SOC_HP_I2C_NUM || !i2c_conf) {
        return ESP_ERR_INVALID_ARG;
    }
    if (i2c_conf->mode != I2C_MODE_MASTER) {
        ESP_LOGE("esp_i2c", "I2C slave mode is not supported in simulation");
        return ESP_ERR_NOT_SUPPORTED;
    }
    uint32_t speed = (i2c_conf->master.clk_speed > 0) ? i2c_conf->master.clk_speed : 400000;
    wink_status_t st = pal_i2c_bus_init((uint8_t)i2c_num, (wink_pin_t)i2c_conf->sda_io_num, (wink_pin_t)i2c_conf->scl_io_num, speed);
    return (st == WINK_OK) ? ESP_OK : ESP_FAIL;
}

i2c_cmd_handle_t i2c_cmd_link_create(void) {
    for (int i = 0; i < MAX_CMD_LINKS; i++) {
        if (!s_cmd_links[i].in_use) {
            s_cmd_links[i].in_use = true;
            s_cmd_links[i].entry_count = 0;
            s_cmd_links[i].write_offset = 0;
            return (i2c_cmd_handle_t)&s_cmd_links[i];
        }
    }
    return NULL;
}

void i2c_cmd_link_delete(i2c_cmd_handle_t cmd_handle) {
    if (!cmd_handle) {
        return;
    }
    esp_i2c_cmd_link_t *link = (esp_i2c_cmd_link_t *)cmd_handle;
    link->in_use = false;
}

esp_err_t i2c_master_start(i2c_cmd_handle_t cmd_handle) {
    esp_i2c_cmd_link_t *link = (esp_i2c_cmd_link_t *)cmd_handle;
    if (!link || link->entry_count >= 16) {
        return ESP_ERR_INVALID_ARG;
    }
    link->entries[link->entry_count++].type = CMD_START;
    return ESP_OK;
}

esp_err_t i2c_master_write_byte(i2c_cmd_handle_t cmd_handle, uint8_t data, bool ack_en) {
    return i2c_master_write(cmd_handle, &data, 1, ack_en);
}

esp_err_t i2c_master_write(i2c_cmd_handle_t cmd_handle, const uint8_t *data, size_t data_len, bool ack_en) {
    esp_i2c_cmd_link_t *link = (esp_i2c_cmd_link_t *)cmd_handle;
    if (!link || !data || link->entry_count >= 16 || (link->write_offset + data_len > I2C_CMD_BUFFER_SIZE)) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t *dest = &link->write_storage[link->write_offset];
    memcpy(dest, data, data_len);
    link->write_offset += data_len;

    cmd_entry_t *e = &link->entries[link->entry_count++];
    e->type = CMD_WRITE;
    e->data_ptr = dest;
    e->total_bytes = data_len;
    e->ack_en = ack_en;
    return ESP_OK;
}

esp_err_t i2c_master_read_byte(i2c_cmd_handle_t cmd_handle, uint8_t *data, i2c_ack_type_t ack) {
    return i2c_master_read(cmd_handle, data, 1, ack);
}

esp_err_t i2c_master_read(i2c_cmd_handle_t cmd_handle, uint8_t *data, size_t data_len, i2c_ack_type_t ack) {
    esp_i2c_cmd_link_t *link = (esp_i2c_cmd_link_t *)cmd_handle;
    if (!link || !data || link->entry_count >= 16) {
        return ESP_ERR_INVALID_ARG;
    }

    cmd_entry_t *e = &link->entries[link->entry_count++];
    e->type = CMD_READ;
    e->data_ptr = data;
    e->total_bytes = data_len;
    e->ack_en = (ack != I2C_MASTER_LAST_NACK);
    return ESP_OK;
}

esp_err_t i2c_master_stop(i2c_cmd_handle_t cmd_handle) {
    esp_i2c_cmd_link_t *link = (esp_i2c_cmd_link_t *)cmd_handle;
    if (!link || link->entry_count >= 16) {
        return ESP_ERR_INVALID_ARG;
    }
    link->entries[link->entry_count++].type = CMD_STOP;
    return ESP_OK;
}

// 状态机驱动的复合时序折叠引擎：解析 Start -> W_Addr -> W_Data -> (Rep_Start -> R_Addr) -> R_Data -> Stop
esp_err_t i2c_master_cmd_begin(i2c_port_t i2c_num, i2c_cmd_handle_t cmd_handle, TickType_t ticks_to_wait) {
    esp_i2c_cmd_link_t *link = (esp_i2c_cmd_link_t *)cmd_handle;
    if (!link || i2c_num >= SOC_HP_I2C_NUM || link->entry_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t timeout_ms = (ticks_to_wait == portMAX_DELAY) ? PAL_I2C_DEFAULT_TIMEOUT_MS : (ticks_to_wait * 10);
    if (timeout_ms == 0) {
        timeout_ms = PAL_I2C_DEFAULT_TIMEOUT_MS;
    }

    uint16_t dev_addr = 0;
    const uint8_t *tx_buf = NULL;
    uint32_t tx_len = 0;
    uint8_t *rx_buf = NULL;
    uint32_t rx_len = 0;
    bool in_start = false;
    bool stop_seen = false;

    for (uint32_t i = 0; i < link->entry_count; i++) {
        cmd_entry_t *e = &link->entries[i];
        if (e->type == CMD_START) {
            if (stop_seen) {
                ESP_LOGE("esp_i2c", "Multiple independent I2C transactions in single command link not supported");
                return ESP_ERR_NOT_SUPPORTED;
            }
            in_start = true;
        } else if (e->type == CMD_WRITE && e->total_bytes > 0) {
            if (in_start) {
                // START 后的第一个写操作的首字节必定为从机地址
                dev_addr = (uint16_t)(e->data_ptr[0] >> 1);
                if (e->total_bytes > 1) {
                    tx_buf = &e->data_ptr[1];
                    tx_len = e->total_bytes - 1;
                }
                in_start = false;
            } else {
                // 后续普通写数据追加
                if (tx_buf == NULL) {
                    tx_buf = e->data_ptr;
                    tx_len = e->total_bytes;
                } else {
                    // 显式边界校验：确保折叠追加的数据在写存储池中保持内存物理连续 (消除 assert 隐患)
                    if (e->data_ptr != tx_buf + tx_len) {
                        ESP_LOGE("esp_i2c", "Non-contiguous write buffer in I2C folding");
                        return ESP_ERR_INVALID_ARG;
                    }
                    tx_len += e->total_bytes;
                }
            }
        } else if (e->type == CMD_READ && e->total_bytes > 0) {
            rx_buf = e->data_ptr;
            rx_len = e->total_bytes;
            in_start = false;
        } else if (e->type == CMD_STOP) {
            stop_seen = true;
        }
    }

    if (dev_addr == 0 && tx_len == 0 && rx_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    wink_status_t st = pal_i2c_transfer_timeout((uint8_t)i2c_num, dev_addr, tx_buf, tx_len, rx_buf, rx_len, timeout_ms);
    return (st == WINK_OK) ? ESP_OK : (st == WINK_ERR_TIMEOUT ? ESP_ERR_TIMEOUT : ESP_FAIL);
}

// --- Legacy Timing & Slave Stubs (Tier-B Corpus Compatibility) ---
static int s_i2c_high_period[SOC_HP_I2C_NUM];
static int s_i2c_low_period[SOC_HP_I2C_NUM];
static int s_i2c_start_setup[SOC_HP_I2C_NUM];
static int s_i2c_start_hold[SOC_HP_I2C_NUM];
static int s_i2c_stop_setup[SOC_HP_I2C_NUM];
static int s_i2c_stop_hold[SOC_HP_I2C_NUM];
static int s_i2c_data_sample[SOC_HP_I2C_NUM];
static int s_i2c_data_hold[SOC_HP_I2C_NUM];
static int s_i2c_timeout[SOC_HP_I2C_NUM];

esp_err_t i2c_set_pin(i2c_port_t i2c_num, int sda_io_num, int scl_io_num, bool sda_pullup_en, bool scl_pullup_en, i2c_mode_t mode) {
    (void)sda_pullup_en; (void)scl_pullup_en; (void)mode;
    if (i2c_num >= (i2c_port_t)SOC_HP_I2C_NUM) return ESP_ERR_INVALID_ARG;
    if (sda_io_num == scl_io_num) return ESP_ERR_INVALID_ARG;
    return ESP_OK;
}

esp_err_t i2c_reset_tx_fifo(i2c_port_t i2c_num) {
    if (i2c_num >= (i2c_port_t)SOC_HP_I2C_NUM) return ESP_ERR_INVALID_ARG;
    return ESP_OK;
}

esp_err_t i2c_reset_rx_fifo(i2c_port_t i2c_num) {
    if (i2c_num >= (i2c_port_t)SOC_HP_I2C_NUM) return ESP_ERR_INVALID_ARG;
    return ESP_OK;
}

int i2c_slave_write_buffer(i2c_port_t i2c_num, const uint8_t *data, int size, TickType_t ticks_to_wait) {
    (void)i2c_num; (void)data; (void)size; (void)ticks_to_wait;
    ESP_LOGE("ESP_I2C_LEGACY", "i2c_slave_write_buffer: slave mode not supported in simulation");
    return -1;
}

int i2c_slave_read_buffer(i2c_port_t i2c_num, uint8_t *data, size_t max_size, TickType_t ticks_to_wait) {
    (void)i2c_num; (void)data; (void)max_size; (void)ticks_to_wait;
    ESP_LOGE("ESP_I2C_LEGACY", "i2c_slave_read_buffer: slave mode not supported in simulation");
    return -1;
}

esp_err_t i2c_set_period(i2c_port_t i2c_num, int high_period, int low_period) {
    if (i2c_num >= (i2c_port_t)SOC_HP_I2C_NUM) return ESP_ERR_INVALID_ARG;
    if (high_period > 0x3FF || low_period > 0x3FF || high_period <= 0 || low_period <= 0) {
        return ESP_ERR_INVALID_ARG;
    }
    s_i2c_high_period[i2c_num] = high_period;
    s_i2c_low_period[i2c_num] = low_period;
    return ESP_OK;
}

esp_err_t i2c_get_period(i2c_port_t i2c_num, int *high_period, int *low_period) {
    if (i2c_num >= (i2c_port_t)SOC_HP_I2C_NUM) return ESP_ERR_INVALID_ARG;
    if (high_period) *high_period = s_i2c_high_period[i2c_num];
    if (low_period) *low_period = s_i2c_low_period[i2c_num];
    return ESP_OK;
}

esp_err_t i2c_set_start_timing(i2c_port_t i2c_num, int setup_time, int hold_time) {
    if (i2c_num >= (i2c_port_t)SOC_HP_I2C_NUM) return ESP_ERR_INVALID_ARG;
    s_i2c_start_setup[i2c_num] = setup_time;
    s_i2c_start_hold[i2c_num] = hold_time;
    return ESP_OK;
}

esp_err_t i2c_get_start_timing(i2c_port_t i2c_num, int *setup_time, int *hold_time) {
    if (i2c_num >= (i2c_port_t)SOC_HP_I2C_NUM) return ESP_ERR_INVALID_ARG;
    if (setup_time) *setup_time = s_i2c_start_setup[i2c_num];
    if (hold_time) *hold_time = s_i2c_start_hold[i2c_num];
    return ESP_OK;
}

esp_err_t i2c_set_stop_timing(i2c_port_t i2c_num, int setup_time, int hold_time) {
    if (i2c_num >= (i2c_port_t)SOC_HP_I2C_NUM) return ESP_ERR_INVALID_ARG;
    s_i2c_stop_setup[i2c_num] = setup_time;
    s_i2c_stop_hold[i2c_num] = hold_time;
    return ESP_OK;
}

esp_err_t i2c_get_stop_timing(i2c_port_t i2c_num, int *setup_time, int *hold_time) {
    if (i2c_num >= (i2c_port_t)SOC_HP_I2C_NUM) return ESP_ERR_INVALID_ARG;
    if (setup_time) *setup_time = s_i2c_stop_setup[i2c_num];
    if (hold_time) *hold_time = s_i2c_stop_hold[i2c_num];
    return ESP_OK;
}

esp_err_t i2c_set_data_timing(i2c_port_t i2c_num, int sample_time, int hold_time) {
    if (i2c_num >= (i2c_port_t)SOC_HP_I2C_NUM) return ESP_ERR_INVALID_ARG;
    s_i2c_data_sample[i2c_num] = sample_time;
    s_i2c_data_hold[i2c_num] = hold_time;
    return ESP_OK;
}

esp_err_t i2c_get_data_timing(i2c_port_t i2c_num, int *sample_time, int *hold_time) {
    if (i2c_num >= (i2c_port_t)SOC_HP_I2C_NUM) return ESP_ERR_INVALID_ARG;
    if (sample_time) *sample_time = s_i2c_data_sample[i2c_num];
    if (hold_time) *hold_time = s_i2c_data_hold[i2c_num];
    return ESP_OK;
}

esp_err_t i2c_set_timeout(i2c_port_t i2c_num, int tout_cycle) {
    if (i2c_num >= (i2c_port_t)SOC_HP_I2C_NUM) return ESP_ERR_INVALID_ARG;
    s_i2c_timeout[i2c_num] = tout_cycle;
    return ESP_OK;
}

esp_err_t i2c_get_timeout(i2c_port_t i2c_num, int *tout_cycle) {
    if (i2c_num >= (i2c_port_t)SOC_HP_I2C_NUM) return ESP_ERR_INVALID_ARG;
    if (tout_cycle) *tout_cycle = s_i2c_timeout[i2c_num];
    return ESP_OK;
}

void esp_i2c_legacy_reset(void) {
    for (int i = 0; i < MAX_CMD_LINKS; i++) {
        s_cmd_links[i].in_use = false;
        s_cmd_links[i].entry_count = 0;
        s_cmd_links[i].write_offset = 0;
    }
    for (int p = 0; p < (int)SOC_HP_I2C_NUM; p++) {
        s_i2c_high_period[p] = 0;
        s_i2c_low_period[p] = 0;
        s_i2c_start_setup[p] = 0;
        s_i2c_start_hold[p] = 0;
        s_i2c_stop_setup[p] = 0;
        s_i2c_stop_hold[p] = 0;
        s_i2c_data_sample[p] = 0;
        s_i2c_data_hold[p] = 0;
        s_i2c_timeout[p] = 0;
    }
}

