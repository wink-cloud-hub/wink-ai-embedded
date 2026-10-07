// SPDX-License-Identifier: LGPL-3.0-only
#include "driver/dedic_gpio.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "../core/esp_sim_fault.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAX_DEDIC_GPIO_BUNDLES 8
#define MAX_GPIOS_PER_BUNDLE   32
#define SOFT_UART_FIFO_SIZE    256

static const char *TAG = "esp_dedic_gpio";

struct dedic_gpio_bundle_t {
    int gpios[MAX_GPIOS_PER_BUNDLE];
    size_t array_size;
    dedic_gpio_bundle_config_t config;
    uint32_t out_mask;
    uint32_t in_mask;
    uint32_t out_offset;
    uint32_t in_offset;
    bool in_use;
};

static struct dedic_gpio_bundle_t s_bundles[MAX_DEDIC_GPIO_BUNDLES];

/* Soft UART software simulation buffers */
static uint8_t s_soft_uart_tx_buf[SOFT_UART_FIFO_SIZE];
static size_t  s_soft_uart_tx_len = 0;
static uint8_t s_soft_uart_rx_fifo[SOFT_UART_FIFO_SIZE];
static size_t  s_soft_uart_rx_head = 0;
static size_t  s_soft_uart_rx_tail = 0;
static size_t  s_soft_uart_rx_count = 0;
static bool    s_soft_uart_loopback = true;

/***** DEDIC_GPIO Facade Implementation *****/

esp_err_t dedic_gpio_new_bundle(const dedic_gpio_bundle_config_t *config, dedic_gpio_bundle_handle_t *ret_bundle)
{
    if (config == NULL || ret_bundle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (config->gpio_array == NULL || config->array_size == 0 || config->array_size > MAX_GPIOS_PER_BUNDLE) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Fault injection hook for negative test verification */
    if (sim_esp_fault_is_active(ESP_FAULT_DOMAIN_DEDIC_GPIO, ESP_FAULT_DEDIC_GPIO_ALLOC_FAIL)) {
        ESP_LOGE(TAG, "Fault injection active: dedic_gpio_new_bundle failed");
        return ESP_ERR_INVALID_STATE;
    }

    struct dedic_gpio_bundle_t *bundle = NULL;
    for (int i = 0; i < MAX_DEDIC_GPIO_BUNDLES; i++) {
        if (!s_bundles[i].in_use) {
            bundle = &s_bundles[i];
            break;
        }
    }
    if (bundle == NULL) {
        return ESP_ERR_NO_MEM;
    }

    memset(bundle, 0, sizeof(*bundle));
    bundle->array_size = config->array_size;
    bundle->config = *config;

    for (size_t i = 0; i < config->array_size; i++) {
        int pin = config->gpio_array[i];
        bundle->gpios[i] = pin;
        if (config->flags.out_en) {
            (void)gpio_set_direction(pin, GPIO_MODE_OUTPUT);
        }
        if (config->flags.in_en) {
            (void)gpio_set_direction(pin, GPIO_MODE_INPUT);
        }
    }

    bundle->out_mask = (config->array_size == 32) ? 0xFFFFFFFFU : ((1U << config->array_size) - 1U);
    bundle->in_mask = bundle->out_mask;
    bundle->out_offset = 0;
    bundle->in_offset = 0;
    bundle->in_use = true;

    *ret_bundle = bundle;
    return ESP_OK;
}

esp_err_t dedic_gpio_del_bundle(dedic_gpio_bundle_handle_t bundle)
{
    if (bundle == NULL || !bundle->in_use) {
        return ESP_ERR_INVALID_ARG;
    }
    bundle->in_use = false;
    return ESP_OK;
}

esp_err_t dedic_gpio_get_out_offset(dedic_gpio_bundle_handle_t bundle, uint32_t *offset)
{
    if (bundle == NULL || offset == NULL || !bundle->in_use) {
        return ESP_ERR_INVALID_ARG;
    }
    *offset = bundle->out_offset;
    return ESP_OK;
}

esp_err_t dedic_gpio_get_in_offset(dedic_gpio_bundle_handle_t bundle, uint32_t *offset)
{
    if (bundle == NULL || offset == NULL || !bundle->in_use) {
        return ESP_ERR_INVALID_ARG;
    }
    *offset = bundle->in_offset;
    return ESP_OK;
}

esp_err_t dedic_gpio_get_out_mask(dedic_gpio_bundle_handle_t bundle, uint32_t *mask)
{
    if (bundle == NULL || mask == NULL || !bundle->in_use) {
        return ESP_ERR_INVALID_ARG;
    }
    *mask = bundle->out_mask;
    return ESP_OK;
}

esp_err_t dedic_gpio_get_in_mask(dedic_gpio_bundle_handle_t bundle, uint32_t *mask)
{
    if (bundle == NULL || mask == NULL || !bundle->in_use) {
        return ESP_ERR_INVALID_ARG;
    }
    *mask = bundle->in_mask;
    return ESP_OK;
}

void esp_dedic_gpio_reset(void)
{
    memset(s_bundles, 0, sizeof(s_bundles));
    s_soft_uart_tx_len = 0;
    s_soft_uart_rx_head = 0;
    s_soft_uart_rx_tail = 0;
    s_soft_uart_rx_count = 0;
    s_soft_uart_loopback = true;
    memset(s_soft_uart_tx_buf, 0, sizeof(s_soft_uart_tx_buf));
    memset(s_soft_uart_rx_fifo, 0, sizeof(s_soft_uart_rx_fifo));
}

/***** Software UART Simulation Bit-Banging Routine (C Emulation) *****/

void IRAM_ATTR emulate_uart_send(const uint8_t *tx_msg, uint32_t tx_size, uint32_t tx_bit, uint32_t baudrate)
{
    (void)tx_bit;
    (void)baudrate;
    if (tx_msg == NULL || tx_size == 0) {
        return;
    }

    size_t copy_len = tx_size;
    if (s_soft_uart_tx_len + copy_len > sizeof(s_soft_uart_tx_buf)) {
        copy_len = sizeof(s_soft_uart_tx_buf) - s_soft_uart_tx_len;
    }
    memcpy(s_soft_uart_tx_buf + s_soft_uart_tx_len, tx_msg, copy_len);
    s_soft_uart_tx_len += copy_len;

    /* If loopback is enabled, push valid bytes to RX FIFO */
    if (s_soft_uart_loopback) {
        for (uint32_t i = 0; i < tx_size; i++) {
            /* If dummy byte (single byte 0x00 sent alone), skip loopbacking it */
            if (tx_size == 1 && tx_msg[0] == 0) {
                continue;
            }
            if (s_soft_uart_rx_count < SOFT_UART_FIFO_SIZE) {
                s_soft_uart_rx_fifo[s_soft_uart_rx_tail] = tx_msg[i];
                s_soft_uart_rx_tail = (s_soft_uart_rx_tail + 1) % SOFT_UART_FIFO_SIZE;
                s_soft_uart_rx_count++;
            }
        }
    }
}

void IRAM_ATTR emulate_uart_receive(uint8_t *rx_msg, uint32_t rx_size, uint32_t rx_bit, uint32_t baudrate)
{
    (void)rx_bit;
    (void)baudrate;
    if (rx_msg == NULL || rx_size == 0) {
        return;
    }

    /* Consume bytes from RX FIFO if available */
    uint32_t read_bytes = 0;
    while (read_bytes < rx_size && s_soft_uart_rx_count > 0) {
        rx_msg[read_bytes++] = s_soft_uart_rx_fifo[s_soft_uart_rx_head];
        s_soft_uart_rx_head = (s_soft_uart_rx_head + 1) % SOFT_UART_FIFO_SIZE;
        s_soft_uart_rx_count--;
    }

    /* If FIFO didn't have enough, fill remaining with zero or loopback fallback */
    while (read_bytes < rx_size) {
        rx_msg[read_bytes++] = 0;
    }
}

/* Simulation helper APIs for testing and headless scenarios */
void sim_soft_uart_inject_rx(const uint8_t *data, size_t len)
{
    if (data == NULL || len == 0) return;
    for (size_t i = 0; i < len; i++) {
        if (s_soft_uart_rx_count < SOFT_UART_FIFO_SIZE) {
            s_soft_uart_rx_fifo[s_soft_uart_rx_tail] = data[i];
            s_soft_uart_rx_tail = (s_soft_uart_rx_tail + 1) % SOFT_UART_FIFO_SIZE;
            s_soft_uart_rx_count++;
        }
    }
}

void sim_soft_uart_get_tx(uint8_t *out_buf, size_t *out_len)
{
    if (out_buf == NULL || out_len == NULL) return;
    size_t copy = (*out_len < s_soft_uart_tx_len) ? *out_len : s_soft_uart_tx_len;
    memcpy(out_buf, s_soft_uart_tx_buf, copy);
    *out_len = copy;
}

void sim_soft_uart_set_loopback(bool enabled)
{
    s_soft_uart_loopback = enabled;
}
