// SPDX-License-Identifier: LGPL-3.0-only
#include "driver/uart.h"
#include "hal/pal_uart.h"
#include "osal/pal_osal.h"
#include "osal/pal_deferred.h"
#include "wink_sim_scheduler.h"
#include <string.h>

#define UART_RING_BUF_SIZE 512
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
        for (size_t i = 0; i < len; i++) {
            if (u->rx_count < UART_RING_BUF_SIZE) {
                u->rx_buf[u->rx_head] = data[i];
                u->rx_head = (u->rx_head + 1) % UART_RING_BUF_SIZE;
                u->rx_count++;
                pushed++;
            }
        }
        if (u->event_queue && pushed > 0) {
            uart_event_t q_evt = { .type = UART_DATA, .size = pushed, .timeout_flag = false };
            BaseType_t woken = pdFALSE;
            xQueueSendFromISR(u->event_queue, &q_evt, &woken);
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
    } else if (event == PAL_UART_EVENT_BUFFER_FULL && u->event_queue) {
        uart_event_t q_evt = { .type = UART_BUFFER_FULL, .size = 0, .timeout_flag = false };
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

esp_err_t uart_set_pin(uart_port_t uart_num, int tx_io_num, int rx_io_num, int rts_io_num, int cts_io_num) {
    (void)rts_io_num;
    (void)cts_io_num;
    if (uart_num >= UART_NUM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_uart_port_t *u = &s_uarts[uart_num];
    if (tx_io_num != UART_PIN_NO_CHANGE) {
        u->tx_pin = (wink_pin_t)tx_io_num;
    }
    if (rx_io_num != UART_PIN_NO_CHANGE) {
        u->rx_pin = (wink_pin_t)rx_io_num;
    }
    if (u->pal_initialized) {
        pal_uart_deinit((uint8_t)uart_num);
        wink_status_t st = pal_uart_init((uint8_t)uart_num, u->tx_pin, u->rx_pin, u->baud_rate > 0 ? u->baud_rate : 115200);
        if (st != WINK_OK) {
            u->pal_initialized = false;
            return ESP_FAIL;
        }
    }
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
            return ESP_FAIL;
        }
        u->pal_initialized = true;
    }

    u->rx_head = u->rx_tail = u->rx_count = 0;
    u->waiting_task_id = -1;
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
    return ESP_OK;
}

esp_err_t uart_get_buffered_data_len(uart_port_t uart_num, size_t *size) {
    if (uart_num >= UART_NUM_MAX || !size) {
        return ESP_ERR_INVALID_ARG;
    }
    *size = s_uarts[uart_num].rx_count;
    return ESP_OK;
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
        s_uarts[i].baud_rate = 115200;
        s_uarts[i].waiting_task_id = -1;
    }
}
