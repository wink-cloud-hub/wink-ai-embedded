/**
 * @file pal_hal_esp32_rmt.c
 * @brief ESP32 RMT 硬件脉冲捕获（超声波专用）。
 *
 * ⚠️ @verified: SOURCE-EDITED -- RMT timeout disable/enable reset applied; idf.py compile verification
 *    DEFERRED (no ESP-IDF in this session). Hardware validation still required: oscilloscope ISR latency < 10us and HC-SR04 accuracy.
 *
 * 使用 RMT (Remote Control) 外设实现超声波脉冲测量，替换 pal_hal_esp32.c 中的 busy-wait。
 *
 * 设计要点：
 * - RMT 时钟 80MHz，分频因子 80 → 1MHz 分辨率 (1us/ tick)
 * - 双沿捕获：上升沿开始计数，下降沿停止
 * - 与 HC-SR04 工作流程完美匹配：TRIG 输出 → RMT 接收 ECHO 脉宽
 *
 * 使用方法（替代 pal_gpio_pulse_in）：
 *   pal_rmt_ultrasonic_init(echo_pin);
 *   pal_gpio_write(trig_pin, true); pal_os_busy_wait_us(10); pal_gpio_write(trig_pin, false);
 *   uint32_t pulse_us;
 *   if (pal_rmt_ultrasonic_measure(30000, &pulse_us) == WINK_OK) { ... }
 *
 * TODO: Phase 4 将此接口标准化为 PAL 层非阻塞捕获 API。
 */

#include "pal_hal.h"

#if defined(ESP_PLATFORM)
#include "driver/rmt_rx.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define RMT_CLK_DIV             80      /* 80MHz / 80 = 1MHz → 1us resolution */
#define RMT_MEM_BLOCK_SYMB      64      /* 每个 memory block 64 symbols */
#define RMT_RX_MAX_BYTES        1024    /* Ring buffer size */

/* HC-SR04 有效脉冲范围（对应 2cm ~ 400cm 测距范围）
 * 声速 ~343m/s，往返距离 = 2 × 实测距离
 * 最小脉冲: 2cm × 2 / 343m/s ≈ 117us
 * 最大脉冲: 400cm × 2 / 343m/s ≈ 23324us
 */
#define MIN_VALID_PULSE_US      100     /* 略小于理论最小值，留余量 */
#define MAX_VALID_PULSE_US      25000   /* 略大于理论最大值，留余量 */

/* 全局状态（单实例超声波，MVP 阶段暂不支持多路）。
 * s_rx_buf 是【用户拥有】的接收缓冲：rmt_receive 把捕获的符号直接写入其中，完成回调经
 * s_rx_num_symbols 报告数量（回调入参 edata->received_symbols 即指向 s_rx_buf，ESP-IDF v5.x 契约）。
 * 评审 P0-2：不再用 rmt_rx_done_event_data_t 的 const 指针字段当可写缓冲。 */
#define RMT_RX_SYMBOLS 64                       /* 与 mem_block_symbols 对齐，单次接收上限 */
static rmt_channel_handle_t   s_rmt_rx_chan = NULL;
static rmt_symbol_word_t      s_rx_buf[RMT_RX_SYMBOLS];
static volatile size_t        s_rx_num_symbols = 0;
static SemaphoreHandle_t      s_rx_done_sem = NULL;
static uint16_t               s_echo_pin = 0xFFFF;

/* ─────────────────────────────────────────────────────────
 * RMT RX 完成回调 (ISR context)
 * ───────────────────────────────────────────────────────── */

static bool IRAM_ATTR rmt_rx_done_callback(rmt_channel_handle_t channel,
                                            const rmt_rx_done_event_data_t *edata,
                                            void *user_data) {
    BaseType_t high_task_wakeup = pdFALSE;
    /* 符号已由 rmt_receive 写入 s_rx_buf（edata->received_symbols 指向它）；仅记录数量即可 */
    s_rx_num_symbols = edata->num_symbols;
    xSemaphoreGiveFromISR(s_rx_done_sem, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}

/* ─────────────────────────────────────────────────────────
 * 初始化 RMT 超声波捕获通道
 * ───────────────────────────────────────────────────────── */

wink_status_t pal_rmt_ultrasonic_init(uint16_t echo_pin) {
    if (s_rmt_rx_chan != NULL) {
        return WINK_OK;  /* 已初始化 */
    }

    s_echo_pin = echo_pin;

    /* 创建 RMT RX channel */
    rmt_rx_channel_config_t rx_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000,  /* 1MHz = 1us/tick */
        .mem_block_symbols = RMT_MEM_BLOCK_SYMB,
        .gpio_num = echo_pin,
        .flags.invert_in = false,
        .flags.with_dma = false,
        /* .flags.io_loop_back - REMOVED IN ESP-IDF v6.x */
    };
    esp_err_t err = rmt_new_rx_channel(&rx_cfg, &s_rmt_rx_chan);
    if (err != ESP_OK) {
        return WINK_ERR_HARDWARE;
    }

    /* 创建完成信号量 */
    s_rx_done_sem = xSemaphoreCreateBinary();
    if (s_rx_done_sem == NULL) {
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    /* 注册回调 */
    rmt_rx_event_callbacks_t cbs = {
        .on_recv_done = rmt_rx_done_callback,
    };
    err = rmt_rx_register_event_callbacks(s_rmt_rx_chan, &cbs, NULL);
    if (err != ESP_OK) {
        return WINK_ERR_HARDWARE;
    }

    /* 启用 RMT */
    err = rmt_enable(s_rmt_rx_chan);
    if (err != ESP_OK) {
        return WINK_ERR_HARDWARE;
    }

    /* 架构评审修复 #6：RMT 中断优先级说明
     *
     * ESP32 中断优先级：数值越大优先级越高（0=最低，7=NMI最高）
     * WiFi/BT 中断优先级通常为 3~4。
     *
     * 验证方法（验收标准：中断响应延迟 < 10us）：
     *   1. 在 TRIG 引脚输出时同时翻转另一 GPIO（打勾）
     *   2. RMT ISR 中立即翻转另一 GPIO
     *   3. 示波器测量两 GPIO 的上升沿间隔
     *
     * 注：ESP-IDF 5.x RMT new_channel 会自动分配合理优先级，
     *     如需手动调整，需通过 CONFIG_RMT_INTERRUPT_PRIORITY kconfig。
     */

    return WINK_OK;
}

/* ─────────────────────────────────────────────────────────
 * 执行一次超声波脉宽测量（非阻塞，由 RMT 硬件完成）
 * ───────────────────────────────────────────────────────── */

wink_status_t pal_rmt_ultrasonic_measure(uint32_t timeout_us, uint32_t *pulse_us) {
    if (pulse_us == NULL || s_rmt_rx_chan == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    /* 清空信号量 */
    xSemaphoreTake(s_rx_done_sem, 0);

    /* 启动 RMT 接收：符号写入【用户拥有的】s_rx_buf（ESP-IDF v5.x rmt_receive 契约——
     * 第二参数须为调用方持有的可写缓冲，第三参数为其字节数，缓冲须保持有效至 done 事件）。 */
    rmt_receive_config_t recv_cfg = {
        .signal_range_min_ns = 1000,     /* 1us, 过滤毛刺 */
        .signal_range_max_ns = (uint32_t)((uint64_t)timeout_us * 1000),  /* 超时对应最大脉宽 */
    };
    esp_err_t err = rmt_receive(s_rmt_rx_chan, s_rx_buf, sizeof(s_rx_buf), &recv_cfg);
    if (err != ESP_OK) {
        return WINK_ERR_HARDWARE;
    }

    /* 等待 RMT 捕获完成（阻塞但不消耗 CPU，由 FreeRTOS 调度） */
    BaseType_t ok = xSemaphoreTake(s_rx_done_sem, pdMS_TO_TICKS((timeout_us + 999) / 1000 + 1));
    if (ok != pdPASS) {
        /* 超时恢复：disable → enable 复位 RMT RX 状态机。
         * 信号量残留：超时后 s_rx_done_sem 可能有残留 Give，但下一次 measure 入口的
         * xSemaphoreTake(s_rx_done_sem, 0) 会清空，故此处不必额外 Take。
         * 旧实现误用 rmt_receive(NULL,...) 取消——违反 v5.x RX 契约，改为状态机复位。*/
        rmt_disable(s_rmt_rx_chan);
        esp_err_t start_err = rmt_enable(s_rmt_rx_chan);
        if (start_err != ESP_OK) {
            return WINK_ERR_HARDWARE;
        }
        return WINK_ERR_TIMEOUT;
    }

    /* 解析 RMT symbols → 超声波脉宽
     *
     * 鲁棒解析策略：
     *   1. 遍历所有捕获的符号
     *   2. 找最长的高电平脉冲（抗串扰噪声、边沿抖动）
     *   3. 校验脉冲在有效范围内（过滤无效信号）
     *
     * HC-SR04 典型波形：
     *   symbol[N].level0=0, duration0=低电平等待时间
     *   symbol[N].level1=1, duration1=ECHO 脉冲宽度（有效值）
     */
    /* 解析 s_rx_buf 中的符号 → 超声波脉宽（捕获数量先读入局部，避免反复读 volatile + 限定上界） */
    size_t num = s_rx_num_symbols;
    if (num >= 1 && num <= RMT_RX_SYMBOLS) {
        uint32_t max_high_duration = 0;

        /* 搜索所有符号，找最长的高电平脉冲（抗串扰噪声、边沿抖动） */
        for (size_t i = 0; i < num; i++) {
            const rmt_symbol_word_t *sym = &s_rx_buf[i];

            /* 每个符号含 level0 + level1 两段，取高电平（==1）时长最大值 */
            if (sym->level0 == 1 && sym->duration0 > max_high_duration) {
                max_high_duration = sym->duration0;
            }
            if (sym->level1 == 1 && sym->duration1 > max_high_duration) {
                max_high_duration = sym->duration1;
            }
        }

        /* 校验脉冲在有效范围内（过滤噪声和超时信号） */
        if (max_high_duration >= MIN_VALID_PULSE_US &&
            max_high_duration <= MAX_VALID_PULSE_US) {
            *pulse_us = max_high_duration;
            return WINK_OK;
        }
    }

    return WINK_ERR_TIMEOUT;
}

/* ─────────────────────────────────────────────────────────
 * 反初始化 RMT 通道
 * ───────────────────────────────────────────────────────── */

void pal_rmt_ultrasonic_deinit(void) {
    if (s_rmt_rx_chan != NULL) {
        rmt_disable(s_rmt_rx_chan);
        rmt_del_channel(s_rmt_rx_chan);
        s_rmt_rx_chan = NULL;
    }
    if (s_rx_done_sem != NULL) {
        vSemaphoreDelete(s_rx_done_sem);
        s_rx_done_sem = NULL;
    }
}

#else  /* !ESP_PLATFORM - stub implementations for static analysis */

wink_status_t pal_rmt_ultrasonic_init(uint16_t echo_pin) {
    (void)echo_pin; return WINK_ERR_UNSUPPORTED;
}

wink_status_t pal_rmt_ultrasonic_measure(uint32_t timeout_us, uint32_t *pulse_us) {
    (void)timeout_us; (void)pulse_us; return WINK_ERR_UNSUPPORTED;
}

void pal_rmt_ultrasonic_deinit(void) {}

#endif  /* ESP_PLATFORM */
