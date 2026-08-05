// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_rmt_esp32.c
 * @brief ESP32 target PAL RMT pulse capture subsystem implementation.
 */
#include "pal_hal.h"
#include "hal/pal_rmt.h"
#include "pal_log.h"

#if defined(ESP_PLATFORM)
#include "driver/rmt_rx.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define RMT_CLK_DIV             80
#define RMT_MEM_BLOCK_SYMB      64
#define RMT_RX_MAX_BYTES        1024

#define MIN_VALID_PULSE_US      100
#define MAX_VALID_PULSE_US      25000

#define RMT_RX_SYMBOLS 64
static rmt_channel_handle_t   s_rmt_rx_chan = NULL;
static rmt_symbol_word_t      s_rx_buf[RMT_RX_SYMBOLS];
static volatile size_t        s_rx_num_symbols = 0;
static SemaphoreHandle_t      s_rx_done_sem = NULL;
static wink_pin_t             s_capture_pin = -1;

static bool IRAM_ATTR rmt_rx_done_callback(rmt_channel_handle_t channel,
                                            const rmt_rx_done_event_data_t *edata,
                                            void *user_data) {
    (void)channel;
    (void)user_data;
    BaseType_t high_task_wakeup = pdFALSE;
    s_rx_num_symbols = edata->num_symbols;
    static volatile int s_isr_log = 0;
    int n = s_isr_log++;
    if (n < 8) {
        esp_rom_printf("[rmt] ISR done num_sym=%lu pin=%d\n",
                       (unsigned long)edata->num_symbols, (int)s_capture_pin);
    }
    xSemaphoreGiveFromISR(s_rx_done_sem, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}

wink_status_t pal_rmt_pulse_capture_init(wink_pin_t pin, pal_rmt_edge_t start_edge) {
    if (start_edge != PAL_RMT_EDGE_RISING && start_edge != PAL_RMT_EDGE_FALLING) {
        return WINK_ERR_INVALID_ARG;
    }
    if (pin < 0) {
        return WINK_ERR_INVALID_ARG;
    }

    if (s_rmt_rx_chan != NULL) {
        if (s_capture_pin == pin) {
            return WINK_OK;
        }
        esp_rom_printf("[rmt] init: switching pin %d -> %d, deinit old chan\n",
                       (int)s_capture_pin, (int)pin);
        pal_rmt_pulse_capture_deinit();
    }

    rmt_rx_channel_config_t rx_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000,
        .mem_block_symbols = RMT_MEM_BLOCK_SYMB,
        .gpio_num = pin,
        .flags.invert_in = false,
        .flags.with_dma = false,
    };
    esp_err_t err = rmt_new_rx_channel(&rx_cfg, &s_rmt_rx_chan);
    if (err != ESP_OK) {
        esp_rom_printf("[rmt] init: rmt_new_rx_channel(pin=%d) err=%d\n", (int)pin, (int)err);
        s_rmt_rx_chan = NULL;
        return WINK_ERR_HARDWARE;
    }

    s_rx_done_sem = xSemaphoreCreateBinary();
    if (s_rx_done_sem == NULL) {
        rmt_del_channel(s_rmt_rx_chan);
        s_rmt_rx_chan = NULL;
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    rmt_rx_event_callbacks_t cbs = {
        .on_recv_done = rmt_rx_done_callback,
    };
    err = rmt_rx_register_event_callbacks(s_rmt_rx_chan, &cbs, NULL);
    if (err != ESP_OK) {
        vSemaphoreDelete(s_rx_done_sem);
        s_rx_done_sem = NULL;
        rmt_del_channel(s_rmt_rx_chan);
        s_rmt_rx_chan = NULL;
        esp_rom_printf("[rmt] init: register_callbacks(pin=%d) err=%d\n", (int)pin, (int)err);
        return WINK_ERR_HARDWARE;
    }

    err = rmt_enable(s_rmt_rx_chan);
    if (err != ESP_OK) {
        vSemaphoreDelete(s_rx_done_sem);
        s_rx_done_sem = NULL;
        rmt_del_channel(s_rmt_rx_chan);
        s_rmt_rx_chan = NULL;
        esp_rom_printf("[rmt] init: rmt_enable(pin=%d) err=%d\n", (int)pin, (int)err);
        return WINK_ERR_HARDWARE;
    }

    s_capture_pin = pin;
    s_rx_num_symbols = 0;
    esp_rom_printf("[rmt] init: OK pin=%d chan=%p\n", (int)pin, s_rmt_rx_chan);
    return WINK_OK;
}

wink_status_t pal_rmt_pulse_capture_arm(void) {
    if (s_rmt_rx_chan == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_rx_done_sem, 0);

    rmt_receive_config_t recv_cfg = {
        .signal_range_min_ns = 1000,
        .signal_range_max_ns = (uint32_t)((uint64_t)MAX_VALID_PULSE_US * 1000),
    };
    esp_err_t err = rmt_receive(s_rmt_rx_chan, s_rx_buf, sizeof(s_rx_buf), &recv_cfg);
    if (err != ESP_OK) {
        esp_rom_printf("[rmt] arm: rmt_receive err=%d pin=%d\n", (int)err, (int)s_capture_pin);
        return WINK_ERR_HARDWARE;
    }
    static int s_arm_log = 0;
    if (s_arm_log++ < 8) {
        esp_rom_printf("[rmt] arm OK pin=%d chan=%p\n", (int)s_capture_pin, s_rmt_rx_chan);
    }
    return WINK_OK;
}

wink_status_t pal_rmt_pulse_capture_wait_armed(uint32_t timeout_us, uint32_t *pulse_us_out) {
    if (pulse_us_out == NULL || s_rmt_rx_chan == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    *pulse_us_out = 0;

    TickType_t wait_ticks = pdMS_TO_TICKS((timeout_us + 999) / 1000 + 1);
    BaseType_t ok = xSemaphoreTake(s_rx_done_sem, wait_ticks);
    static int s_wait_log = 0;
    static int s_timeout_log = 0;
    if (ok != pdPASS) {
        int pin_level = gpio_get_level((gpio_num_t)s_capture_pin);
        if (s_timeout_log++ < 3) {
            esp_rom_printf("[rmt] wait_armed TIMEOUT num_sym=%lu pin=%d level=%d\n",
                           (unsigned long)s_rx_num_symbols, (int)s_capture_pin, pin_level);
            LOG_E("rmt: wait_armed timeout (%lu us, wait_ticks=%lu), s_rx_num_symbols=%lu, pin=%d",
                  (unsigned long)timeout_us, (unsigned long)wait_ticks,
                  (unsigned long)s_rx_num_symbols, (int)s_capture_pin);
        }
        rmt_disable(s_rmt_rx_chan);
        esp_err_t start_err = rmt_enable(s_rmt_rx_chan);
        if (start_err != ESP_OK) {
            return WINK_ERR_HARDWARE;
        }
        return WINK_ERR_TIMEOUT;
    }
    if (s_wait_log++ < 8) {
        esp_rom_printf("[rmt] wait_armed DONE num_sym=%lu pin=%d\n",
                       (unsigned long)s_rx_num_symbols, (int)s_capture_pin);
        for (size_t i = 0; i < s_rx_num_symbols && i < 4; i++) {
            esp_rom_printf("[rmt] sym[%lu]: L0=%u D0=%u L1=%u D1=%u\n",
                           (unsigned long)i,
                           (unsigned)s_rx_buf[i].level0, (unsigned)s_rx_buf[i].duration0,
                           (unsigned)s_rx_buf[i].level1, (unsigned)s_rx_buf[i].duration1);
        }
    }

    size_t num = s_rx_num_symbols;
    if (num >= 1 && num <= RMT_RX_SYMBOLS) {
        uint32_t max_high_duration = 0;

        for (size_t i = 0; i < num; i++) {
            const rmt_symbol_word_t *sym = &s_rx_buf[i];

            if (sym->level0 == 1 && sym->duration0 > max_high_duration) {
                max_high_duration = sym->duration0;
            }
            if (sym->level1 == 1 && sym->duration1 > max_high_duration) {
                max_high_duration = sym->duration1;
            }
        }

        if (max_high_duration >= MIN_VALID_PULSE_US &&
            max_high_duration <= MAX_VALID_PULSE_US) {
            *pulse_us_out = max_high_duration;
            return WINK_OK;
        }

        LOG_E("rmt: %lu symbols captured but high pulse=%luus out of [%u,%u]; first 4 syms:",
              (unsigned long)num, (unsigned long)max_high_duration,
              (unsigned)MIN_VALID_PULSE_US, (unsigned)MAX_VALID_PULSE_US);
        for (size_t i = 0; i < num && i < 4; i++) {
            LOG_E("  sym[%lu]: L0=%u D0=%u  L1=%u D1=%u",
                  (unsigned long)i,
                  (unsigned)s_rx_buf[i].level0, (unsigned)s_rx_buf[i].duration0,
                  (unsigned)s_rx_buf[i].level1, (unsigned)s_rx_buf[i].duration1);
        }
    } else {
        LOG_E("rmt: done ISR fired but num_symbols=%lu (invalid or zero)", (unsigned long)num);
    }

    return WINK_ERR_TIMEOUT;
}

wink_status_t pal_rmt_pulse_capture_wait(uint32_t timeout_us, uint32_t *pulse_us_out) {
    if (pulse_us_out == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    *pulse_us_out = 0;
    wink_status_t s = pal_rmt_pulse_capture_arm();
    if (wink_status_is_error(s)) {
        return s;
    }

    static int s_wait_diag_log = 0;
    if (s_wait_diag_log < 8) {
        s_wait_diag_log++;
        int lvl_start = gpio_get_level((gpio_num_t)s_capture_pin);
        esp_rom_printf("[rmt] wait pin=%d: start level=%d (first 10ms):",
                       (int)s_capture_pin, lvl_start);
        uint32_t trace = 0;
        for (int i = 0; i < 20; i++) {
            esp_rom_delay_us(500);
            int l = gpio_get_level((gpio_num_t)s_capture_pin);
            trace = (trace << 1) | (l & 1u);
        }
        int lvl_end = gpio_get_level((gpio_num_t)s_capture_pin);
        esp_rom_printf(" trace=%05lx end=%d\n",
                       (unsigned long)trace, lvl_end);
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    return pal_rmt_pulse_capture_wait_armed(timeout_us, pulse_us_out);
#pragma GCC diagnostic pop
}

void pal_rmt_pulse_capture_deinit(void) {
    if (s_rmt_rx_chan != NULL) {
        rmt_disable(s_rmt_rx_chan);
        rmt_del_channel(s_rmt_rx_chan);
        s_rmt_rx_chan = NULL;
    }
    if (s_rx_done_sem != NULL) {
        vSemaphoreDelete(s_rx_done_sem);
        s_rx_done_sem = NULL;
    }
    s_capture_pin = -1;
    s_rx_num_symbols = 0;
}

bool pal_rmt_pulse_capture_is_active(void) {
    return s_rmt_rx_chan != NULL;
}

#else

wink_status_t pal_rmt_pulse_capture_init(wink_pin_t pin, pal_rmt_edge_t start_edge) {
    (void)pin; (void)start_edge; return WINK_ERR_UNSUPPORTED;
}

wink_status_t pal_rmt_pulse_capture_arm(void) {
    return WINK_ERR_UNSUPPORTED;
}

wink_status_t pal_rmt_pulse_capture_wait_armed(uint32_t timeout_us, uint32_t *pulse_us_out) {
    if (pulse_us_out != NULL) { *pulse_us_out = 0; }
    (void)timeout_us; return WINK_ERR_UNSUPPORTED;
}

wink_status_t pal_rmt_pulse_capture_wait(uint32_t timeout_us, uint32_t *pulse_us_out) {
    if (pulse_us_out != NULL) { *pulse_us_out = 0; }
    (void)timeout_us; return WINK_ERR_UNSUPPORTED;
}

void pal_rmt_pulse_capture_deinit(void) {}

bool pal_rmt_pulse_capture_is_active(void) {
    return false;
}

#endif
