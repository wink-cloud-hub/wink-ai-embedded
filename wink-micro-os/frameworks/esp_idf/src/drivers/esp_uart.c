// SPDX-License-Identifier: LGPL-3.0-only
#include "driver/uart.h"
#include "hal/pal_uart.h"
#include "osal/pal_osal.h"
#include "osal/pal_deferred.h"
#include "wink_sim_scheduler.h"
#include "esp_idf_wink.h"
#include "freertos_sync.h"
#include <string.h>

#define UART_RING_BUF_SIZE 512
#define UART_PATTERN_QUEUE_MAX 32
#define RES_UART_TAG 0x08u

typedef struct {
    bool installed;
    bool pal_initialized;
    wink_pin_t tx_pin;
    wink_pin_t rx_pin;
    uint32_t baud_rate;
    QueueHandle_t event_queue;
    uint8_t rx_buf[UART_RING_BUF_SIZE];
    uint32_t rx_head;
    uint32_t rx_tail;
    uint32_t rx_count;
    int waiting_task_id;

    /* Pattern detection */
    bool pattern_enabled;
    char pattern_chr;
    uint8_t pattern_chr_num;
    uint8_t pattern_match_count;
    int pattern_positions[UART_PATTERN_QUEUE_MAX];
    int pattern_head;
    int pattern_tail;
    int pattern_count;
    int pattern_queue_max;

    /* RS485 & Mode configuration */
    uart_mode_t mode;
    uint8_t rx_tout_thresh;
    wink_pin_t rts_pin;
    wink_pin_t cts_pin;
} esp_uart_port_t;

static esp_uart_port_t s_uarts[UART_NUM_MAX];

static void on_pal_uart_event(uint8_t port, pal_uart_event_t event, const uint8_t *data, size_t len, void *arg) {
    if (port >= UART_NUM_MAX) {
        return;
    }
    esp_uart_port_t *u = &s_uarts[port];
    if (!u->installed) {
        return;
    }

    if (event == PAL_UART_EVENT_RX_DATA && data && len > 0) {
        size_t pushed = 0;
        bool pattern_hit = false;
        for (size_t i = 0; i < len; i++) {
            if (u->rx_count < UART_RING_BUF_SIZE) {
                u->rx_buf[u->rx_head] = data[i];
                u->rx_head = (u->rx_head + 1) % UART_RING_BUF_SIZE;
                u->rx_count++;
                pushed++;

                if (u->pattern_enabled && data[i] == (uint8_t)u->pattern_chr) {
                    u->pattern_match_count++;
                    if (u->pattern_match_count == u->pattern_chr_num) {
                        pattern_hit = true;
                        int pos = (int)u->rx_count - (int)u->pattern_chr_num;
                        if (pos < 0) {
                            pos = 0;
                        }
                        int q_max = u->pattern_queue_max > 0 ? u->pattern_queue_max : UART_PATTERN_QUEUE_MAX;
                        if (u->pattern_count < q_max) {
                            u->pattern_positions[u->pattern_head] = pos;
                            u->pattern_head = (u->pattern_head + 1) % q_max;
                            u->pattern_count++;
                        }
                        u->pattern_match_count = 0;
                    }
                } else if (u->pattern_enabled) {
                    if (u->pattern_match_count > 0 && u->event_queue) {
                        uart_event_t q_evt = { .type = UART_DATA, .size = u->pattern_match_count, .timeout_flag = false };
                        BaseType_t woken = pdFALSE;
                        xQueueSendFromISR(u->event_queue, &q_evt, &woken);
                    }
                    u->pattern_match_count = 0;
                }
            }
        }
        if (u->event_queue && pushed > 0) {
            BaseType_t woken = pdFALSE;
            if (pattern_hit) {
                uart_event_t q_evt = { .type = UART_PATTERN_DET, .size = pushed, .timeout_flag = false };
                xQueueSendFromISR(u->event_queue, &q_evt, &woken);
            } else if (u->pattern_enabled && u->pattern_match_count > 0) {
                /* Hold back event while pattern is accumulating */
            } else {
                uart_event_t q_evt = { .type = UART_DATA, .size = pushed, .timeout_flag = false };
                xQueueSendFromISR(u->event_queue, &q_evt, &woken);
            }
        }
        if (pushed < len && u->event_queue) {
            uart_event_t q_evt = { .type = UART_BUFFER_FULL, .size = 0, .timeout_flag = false };
            BaseType_t woken = pdFALSE;
            xQueueSendFromISR(u->event_queue, &q_evt, &woken);
        }
        if (u->waiting_task_id >= 0) {
            sim_scheduler_resume((uint32_t)u->waiting_task_id);
            u->waiting_task_id = -1;
        }
    } else if (u->event_queue) {
        uart_event_type_t type;
        switch (event) {
            case PAL_UART_EVENT_RX_FIFO_OVF:
                type = UART_FIFO_OVF;
                break;
            case PAL_UART_EVENT_BUFFER_FULL:
                type = UART_BUFFER_FULL;
                break;
            case PAL_UART_EVENT_BREAK:
                type = UART_BREAK;
                break;
            case PAL_UART_EVENT_PARITY_ERR:
                type = UART_PARITY_ERR;
                break;
            case PAL_UART_EVENT_FRAME_ERR:
                type = UART_FRAME_ERR;
                break;
            default:
                return;
        }
        uart_event_t q_evt = { .type = type, .size = 0, .timeout_flag = false };
        BaseType_t woken = pdFALSE;
        xQueueSendFromISR(u->event_queue, &q_evt, &woken);
    }
}

esp_err_t uart_param_config(uart_port_t uart_num, const uart_config_t *uart_config) {
    if (uart_num >= UART_NUM_MAX || !uart_config) {
        return ESP_ERR_INVALID_ARG;
    }
    s_uarts[uart_num].baud_rate = (uint32_t)uart_config->baud_rate;
    return ESP_OK;
}

static esp_err_t _uart_set_pin_internal(uart_port_t uart_num, int tx_io_num, int rx_io_num,
                                        int rts_io_num, int cts_io_num, int dtr_io_num, int dsr_io_num) {
    if (uart_num >= SOC_UART_HP_NUM || uart_num >= UART_NUM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    /* Fail-Loud on unsupported non-UART0 DTR/DSR pins if explicitly requested on ESP32 */
    if ((dtr_io_num != UART_PIN_NO_CHANGE && dtr_io_num >= 0) ||
        (dsr_io_num != UART_PIN_NO_CHANGE && dsr_io_num >= 0)) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    esp_uart_port_t *u = &s_uarts[uart_num];
    if (tx_io_num != UART_PIN_NO_CHANGE) {
        u->tx_pin = (wink_pin_t)tx_io_num;
    }
    if (rx_io_num != UART_PIN_NO_CHANGE) {
        u->rx_pin = (wink_pin_t)rx_io_num;
    }
    if (rts_io_num != UART_PIN_NO_CHANGE) {
        u->rts_pin = (wink_pin_t)rts_io_num;
    }
    if (cts_io_num != UART_PIN_NO_CHANGE) {
        u->cts_pin = (wink_pin_t)cts_io_num;
    }
    if (u->pal_initialized) {
        pal_uart_deinit((uint8_t)uart_num);
        wink_status_t st = pal_uart_init((uint8_t)uart_num, u->tx_pin, u->rx_pin, u->baud_rate > 0 ? u->baud_rate : 115200);
        if (st != WINK_OK) {
            u->pal_initialized = false;
            return esp_err_from_wink(st);
        }
        if (u->installed) {
            pal_uart_set_event_callback((uint8_t)uart_num, on_pal_uart_event, u);
        }
    }
    return ESP_OK;
}

esp_err_t _uart_set_pin4(uart_port_t uart_num, int tx_io_num, int rx_io_num, int rts_io_num, int cts_io_num) {
    return _uart_set_pin_internal(uart_num, tx_io_num, rx_io_num, rts_io_num, cts_io_num, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

esp_err_t _uart_set_pin6(uart_port_t uart_num, int tx_io_num, int rx_io_num, int rts_io_num, int cts_io_num, int dtr_io_num, int dsr_io_num) {
    return _uart_set_pin_internal(uart_num, tx_io_num, rx_io_num, rts_io_num, cts_io_num, dtr_io_num, dsr_io_num);
}

esp_err_t __uart_set_pin_invalid_args__(int dummy, ...) {
    (void)dummy;
    return ESP_ERR_INVALID_ARG;
}

#undef uart_set_pin
esp_err_t uart_set_pin(uart_port_t uart_num, int tx_io_num, int rx_io_num, int rts_io_num, int cts_io_num) {
    return _uart_set_pin4(uart_num, tx_io_num, rx_io_num, rts_io_num, cts_io_num);
}

esp_err_t uart_set_mode(uart_port_t uart_num, uart_mode_t mode) {
    if (uart_num >= SOC_UART_HP_NUM || uart_num >= UART_NUM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_uart_port_t *u = &s_uarts[uart_num];
    if (!u->installed) {
        return ESP_ERR_INVALID_STATE;
    }
    if (mode != UART_MODE_UART && mode != UART_MODE_RS485_HALF_DUPLEX) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    u->mode = mode;
    return ESP_OK;
}

esp_err_t uart_set_rx_timeout(uart_port_t uart_num, const uint8_t tout_thresh) {
    if (uart_num >= SOC_UART_HP_NUM || uart_num >= UART_NUM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_uart_port_t *u = &s_uarts[uart_num];
    if (!u->installed) {
        return ESP_ERR_INVALID_STATE;
    }
    /* ESP-IDF v6.1 limit is 126 symbols */
    if (tout_thresh > 126) {
        return ESP_ERR_INVALID_ARG;
    }
    u->rx_tout_thresh = tout_thresh;
    return ESP_OK;
}

esp_err_t uart_wait_tx_done(uart_port_t uart_num, uint32_t ticks_to_wait) {
    (void)ticks_to_wait;
    if (uart_num >= SOC_UART_HP_NUM || uart_num >= UART_NUM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_uart_port_t *u = &s_uarts[uart_num];
    if (!u->installed) {
        return ESP_ERR_INVALID_STATE;
    }
    /* In Wasm simulation, pal_uart_write completes synchronously to the bridge buffer */
    return ESP_OK;
}

esp_err_t uart_driver_install(uart_port_t uart_num, int rx_buffer_size, int tx_buffer_size, int queue_size, QueueHandle_t *uart_queue, int intr_alloc_flags) {
    (void)rx_buffer_size;
    (void)tx_buffer_size;
    (void)intr_alloc_flags;
    /* ADR-0085 D1：全 SoC 共用 UART 枚举，SoC 端口上限以 SOC_UART_HP_NUM 运行期 Fail-Loud 拦截 */
    _Static_assert(UART_NUM_MAX >= SOC_UART_HP_NUM,
                   "UART_NUM_MAX must cover all SoC UART ports");
    if (uart_num >= SOC_UART_HP_NUM || uart_num >= UART_NUM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_uart_port_t *u = &s_uarts[uart_num];
    if (u->installed) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!u->pal_initialized) {
        wink_status_t st = pal_uart_init((uint8_t)uart_num, u->tx_pin, u->rx_pin, u->baud_rate > 0 ? u->baud_rate : 115200);
        if (st != WINK_OK) {
            return esp_err_from_wink(st);
        }
        u->pal_initialized = true;
    }

    u->rx_head = u->rx_tail = u->rx_count = 0;
    u->waiting_task_id = -1;
    u->mode = UART_MODE_UART;
    u->rx_tout_thresh = 0;
    u->installed = true;

    if (queue_size > 0 && uart_queue != NULL) {
        u->event_queue = xQueueCreate((uint32_t)queue_size, sizeof(uart_event_t));
        *uart_queue = u->event_queue;
    } else {
        u->event_queue = NULL;
    }

    pal_uart_set_event_callback((uint8_t)uart_num, on_pal_uart_event, u);
    return ESP_OK;
}

esp_err_t uart_driver_delete(uart_port_t uart_num) {
    if (uart_num >= UART_NUM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_uart_port_t *u = &s_uarts[uart_num];
    if (!u->installed) {
        return ESP_ERR_INVALID_STATE;
    }
    if (u->event_queue) {
        vQueueDelete(u->event_queue);
        u->event_queue = NULL;
    }
    if (u->pal_initialized) {
        pal_uart_deinit((uint8_t)uart_num);
        u->pal_initialized = false;
    }
    u->installed = false;
    return ESP_OK;
}

int uart_write_bytes(uart_port_t uart_num, const void *src, size_t size) {
    if (uart_num >= UART_NUM_MAX || !src || size == 0) {
        return -1;
    }
    esp_uart_port_t *u = &s_uarts[uart_num];
    if (!u->installed) {
        return -1;
    }
    wink_status_t st = pal_uart_write((uint8_t)uart_num, (const uint8_t *)src, (uint32_t)size);
    return (st == WINK_OK) ? (int)size : -1;
}

int uart_read_bytes(uart_port_t uart_num, void *buf, uint32_t length, TickType_t ticks_to_wait) {
    if (uart_num >= UART_NUM_MAX || !buf || length == 0) {
        return -1;
    }
    esp_uart_port_t *u = &s_uarts[uart_num];
    if (!u->installed) {
        return -1;
    }

    TickType_t remaining = ticks_to_wait;
    while (u->rx_count == 0) {
        if (remaining == 0) {
            return 0;
        }
        uint32_t self = sim_scheduler_current_id();
        if (self == SIM_SCHED_NO_READY) {
            return 0;
        }

        /* 防御性并发读者检查：单端口目前仅支持单任务阻塞读取，若已有其他任务正在等待，立即拒绝以防孤儿任务覆写 */
        if (u->waiting_task_id >= 0 && u->waiting_task_id != (int)self) {
            return -1;
        }

        esp_freertos_assert_not_in_critical("uart_read_bytes");

        u->waiting_task_id = (int)self;
        uint32_t res_id = (RES_UART_TAG << 24) | (uint32_t)uart_num;
        uint64_t timeout_us = (remaining == portMAX_DELAY) ? 0ULL : ((uint64_t)remaining * (portTICK_PERIOD_MS * 1000ULL));
        uint64_t before_us = pal_os_get_us();

        sim_scheduler_block(self, res_id, before_us, timeout_us);
        sim_scheduler_yield_context();

        u->waiting_task_id = -1;
        const sim_task_t *t = sim_scheduler_get(self);
        if (t && t->timeout_fired && u->rx_count == 0) {
            return 0;
        }

        if (remaining != portMAX_DELAY) {
            uint64_t elapsed_us = pal_os_get_us() - before_us;
            TickType_t elapsed_ticks = (TickType_t)(elapsed_us / (portTICK_PERIOD_MS * 1000ULL));
            remaining = (elapsed_ticks >= remaining) ? 0 : (remaining - elapsed_ticks);
        }
    }

    uint32_t bytes_to_copy = (u->rx_count < length) ? u->rx_count : length;
    uint8_t *dst = (uint8_t *)buf;
    for (uint32_t i = 0; i < bytes_to_copy; i++) {
        dst[i] = u->rx_buf[u->rx_tail];
        u->rx_tail = (u->rx_tail + 1) % UART_RING_BUF_SIZE;
    }
    u->rx_count -= bytes_to_copy;
    return (int)bytes_to_copy;
}

esp_err_t uart_flush(uart_port_t uart_num) {
    if (uart_num >= UART_NUM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_uart_port_t *u = &s_uarts[uart_num];
    u->rx_head = u->rx_tail = u->rx_count = 0;
    u->pattern_head = u->pattern_tail = u->pattern_count = 0;
    u->pattern_match_count = 0;
    return ESP_OK;
}

esp_err_t uart_flush_input(uart_port_t uart_num) {
    return uart_flush(uart_num);
}

esp_err_t uart_get_buffered_data_len(uart_port_t uart_num, size_t *size) {
    if (uart_num >= UART_NUM_MAX || !size) {
        return ESP_ERR_INVALID_ARG;
    }
    *size = s_uarts[uart_num].rx_count;
    return ESP_OK;
}

esp_err_t uart_enable_pattern_det_baud_intr(uart_port_t uart_num, char pattern_chr, uint8_t chr_num, int chr_tout, int post_idle, int pre_idle) {
    (void)chr_tout;
    (void)post_idle;
    (void)pre_idle;
    if (uart_num >= UART_NUM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_uart_port_t *u = &s_uarts[uart_num];
    u->pattern_enabled = true;
    u->pattern_chr = pattern_chr;
    u->pattern_chr_num = chr_num;
    u->pattern_match_count = 0;
    return ESP_OK;
}

esp_err_t uart_disable_pattern_det_intr(uart_port_t uart_num) {
    if (uart_num >= UART_NUM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_uart_port_t *u = &s_uarts[uart_num];
    u->pattern_enabled = false;
    u->pattern_match_count = 0;
    return ESP_OK;
}

esp_err_t uart_pattern_queue_reset(uart_port_t uart_num, int queue_length) {
    if (uart_num >= UART_NUM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_uart_port_t *u = &s_uarts[uart_num];
    u->pattern_head = 0;
    u->pattern_tail = 0;
    u->pattern_count = 0;
    u->pattern_match_count = 0;
    u->pattern_queue_max = (queue_length > 0 && queue_length <= UART_PATTERN_QUEUE_MAX) ? queue_length : UART_PATTERN_QUEUE_MAX;
    return ESP_OK;
}

int uart_pattern_pop_pos(uart_port_t uart_num) {
    if (uart_num >= UART_NUM_MAX) {
        return -1;
    }
    esp_uart_port_t *u = &s_uarts[uart_num];
    if (u->pattern_count == 0) {
        return -1;
    }
    int pos = u->pattern_positions[u->pattern_tail];
    int q_max = u->pattern_queue_max > 0 ? u->pattern_queue_max : UART_PATTERN_QUEUE_MAX;
    u->pattern_tail = (u->pattern_tail + 1) % q_max;
    u->pattern_count--;
    return pos;
}

int uart_pattern_get_pos(uart_port_t uart_num) {
    if (uart_num >= UART_NUM_MAX) {
        return -1;
    }
    esp_uart_port_t *u = &s_uarts[uart_num];
    if (u->pattern_count == 0) {
        return -1;
    }
    return u->pattern_positions[u->pattern_tail];
}

void esp_uart_reset(void) {
    for (int i = 0; i < UART_NUM_MAX; i++) {
        if (s_uarts[i].event_queue) {
            vQueueDelete(s_uarts[i].event_queue);
            s_uarts[i].event_queue = NULL;
        }
        if (s_uarts[i].pal_initialized) {
            pal_uart_deinit((uint8_t)i);
        }
        memset(&s_uarts[i], 0, sizeof(esp_uart_port_t));
        s_uarts[i].tx_pin = WINK_PIN_NC;
        s_uarts[i].rx_pin = WINK_PIN_NC;
        s_uarts[i].rts_pin = WINK_PIN_NC;
        s_uarts[i].cts_pin = WINK_PIN_NC;
        s_uarts[i].baud_rate = 115200;
        s_uarts[i].mode = UART_MODE_UART;
        s_uarts[i].rx_tout_thresh = 0;
        s_uarts[i].waiting_task_id = -1;
    }
}
