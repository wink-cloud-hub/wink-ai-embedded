/**
 * @file pal_hal_esp32.c
 * @brief ESP32 PAL HAL implementation (ESP-IDF v5.x / v6.x compatible).
 *
 * ✅ @verified: HARDWARE-SMOKE-PASSED (DevKitC, 2026-06-27)
 *    - GPIO init/read/write: board LED + Boot button verified
 *    - GPIO ISR: uintptr_t callback arg round-trip verified
 *    - PWM: ch1/ch2 different timer allocation (LEDC router)
 *    - I2C: v6 master bus scan (3 addresses NACK, no panic)
 *    - RMT: still pending ultrasonic hardware (Wave B follow-up)
 *
 * MVP status:
 * - GPIO/PWM/I2C hardware drivers implemented
 * - Ultrasonic pulse capture: pal_gpio_pulse_in still uses busy-wait fallback;
 *   RMT hardware capture in pal_hal_esp32_rmt.c
 * - PWM/I2C pin routing via board_config.c (pal_pwm_pin_map / pal_i2c_pin_map
 *   strong definitions; this TU provides weak defaults so MVP samples without
 *   a board_config.c still link and run with sensible default pins).
 */
#include "pal_hal.h"
#include "pal_osal.h"       /* pal_get_us() (used in pal_gpio_pulse_in busy-wait) */
#include "pal_resource.h"
#include "pal_pwm_router.h"

#if defined(ESP_PLATFORM)
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_idf_version.h"

/* ─────────────────────────────────────────────────────────
 * I2C 版本门控：ESP-IDF v6.x 使用新的 driver/i2c_master.h
 * ───────────────────────────────────────────────────────── */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    /* v6.x 新 API：总线-设备二级模型 */
    #include "driver/i2c_master.h"
    #define WINK_I2C_USE_V6_API  1
#else
    /* v5.x 旧 API：单级 port 模型 */
    #include "driver/i2c.h"
    #define WINK_I2C_USE_V6_API  0
#endif

/* v7.0 前向保护：检测到未验证的版本时编译报错 */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(7, 0, 0)
    #error "ESP-IDF v7.x I2C API compatibility not verified yet. " \
           "Please update tech-designs/pal-i2c-v6-compatibility.md first."
#endif

/* 强制回退开关：Kconfig 可配置强制使用 v5.x API */
#if defined(CONFIG_WINK_I2C_FORCE_V5_API) && CONFIG_WINK_I2C_FORCE_V5_API
    #undef WINK_I2C_USE_V6_API
    #define WINK_I2C_USE_V6_API  0
    #pragma message "Wink I2C: forced to use v5.x compatible API per Kconfig"
#endif

#include "esp_log.h"
static const char *TAG = "wink_pal_i2c";

#else
/* 非 ESP32 编译环境：函数体保持 stub（供静态分析/代码扫描），
 * 真机链接时由 ESP-IDF 构建系统替换为真实实现。 */
typedef int esp_err_t;
#define GPIO_NUM_MAX 50
#define ESP_OK 0
#endif

/* ─────────────────────────────────────────────────────────
 * GPIO 实现
 * ───────────────────────────────────────────────────────── */

wink_status_t pal_gpio_init(uint16_t pin, pal_gpio_mode_t mode) {
    if (pin >= GPIO_NUM_MAX) { return WINK_ERR_INVALID_ARG; }

    wink_status_t rs = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, pin, "pal_hal_esp32");
    if (wink_status_is_error(rs)) { return rs; }

#if defined(ESP_PLATFORM)
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << pin,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    switch (mode) {
        case PAL_GPIO_INPUT:
            cfg.mode = GPIO_MODE_INPUT;
            break;
        case PAL_GPIO_INPUT_PULLUP:
            cfg.mode = GPIO_MODE_INPUT;
            cfg.pull_up_en = GPIO_PULLUP_ENABLE;
            break;
        case PAL_GPIO_INPUT_PULLDOWN:
            cfg.mode = GPIO_MODE_INPUT;
            cfg.pull_down_en = GPIO_PULLDOWN_ENABLE;
            break;
        case PAL_GPIO_OUTPUT_PUSH_PULL:
            cfg.mode = GPIO_MODE_OUTPUT;
            break;
        case PAL_GPIO_OUTPUT_OPEN_DRAIN:
            cfg.mode = GPIO_MODE_OUTPUT_OD;
            break;
        default:
            return WINK_ERR_INVALID_ARG;
    }

    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) { return WINK_ERR_HARDWARE; }
#endif
    return WINK_OK;
}

void pal_gpio_write(uint16_t pin, bool level) {
#if defined(ESP_PLATFORM)
    if (pin < GPIO_NUM_MAX) {
        gpio_set_level((gpio_num_t)pin, level ? 1 : 0);
    }
#else
    (void)pin; (void)level;
#endif
}

bool pal_gpio_read(uint16_t pin) {
#if defined(ESP_PLATFORM)
    if (pin >= GPIO_NUM_MAX) { return false; }
    return gpio_get_level((gpio_num_t)pin) != 0;
#else
    (void)pin; return false;
#endif
}

#if defined(ESP_PLATFORM)
static pal_gpio_isr_t s_gpio_isr[GPIO_NUM_MAX];
static void *s_gpio_isr_arg[GPIO_NUM_MAX];

static void IRAM_ATTR gpio_isr_wrapper(void *arg) {
    uint32_t pin = (uint32_t)(uintptr_t)arg;
    if (pin < GPIO_NUM_MAX && s_gpio_isr[pin] != NULL) {
        s_gpio_isr[pin](s_gpio_isr_arg[pin]);
    }
}
#endif

wink_status_t pal_gpio_enable_interrupt(uint16_t pin, pal_gpio_intr_t intr_type,
                                         pal_gpio_isr_t cb, void *arg) {
    if (pin >= GPIO_NUM_MAX || cb == NULL) { return WINK_ERR_INVALID_ARG; }

#if defined(ESP_PLATFORM)
    gpio_int_type_t esp_intr_type;
    switch (intr_type) {
        case PAL_GPIO_INTR_RISING_EDGE:
            esp_intr_type = GPIO_INTR_POSEDGE;
            break;
        case PAL_GPIO_INTR_FALLING_EDGE:
            esp_intr_type = GPIO_INTR_NEGEDGE;
            break;
        case PAL_GPIO_INTR_ANY_EDGE:
            esp_intr_type = GPIO_INTR_ANYEDGE;
            break;
        default:
            return WINK_ERR_INVALID_ARG;
    }

    static bool s_isr_service_installed = false;
    if (!s_isr_service_installed) {
        esp_err_t err = gpio_install_isr_service(0);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            return WINK_ERR_HARDWARE;
        }
        s_isr_service_installed = true;
    }

    s_gpio_isr[pin] = cb;
    s_gpio_isr_arg[pin] = arg;

    /* 经 uintptr_t 中转：uint16_t 可无损存入 void*，同时消除 -Wint-to-pointer-cast 警告 */
    esp_err_t err = gpio_isr_handler_add((gpio_num_t)pin, gpio_isr_wrapper, (void *)(uintptr_t)pin);
    if (err != ESP_OK) { return WINK_ERR_HARDWARE; }

    err = gpio_set_intr_type((gpio_num_t)pin, esp_intr_type);
    if (err != ESP_OK) { return WINK_ERR_HARDWARE; }
#else
    (void)intr_type; (void)cb; (void)arg;
#endif
    return WINK_OK;
}

wink_status_t pal_gpio_disable_interrupt(uint16_t pin) {
    if (pin >= GPIO_NUM_MAX) { return WINK_ERR_INVALID_ARG; }

#if defined(ESP_PLATFORM)
    esp_err_t err = gpio_set_intr_type((gpio_num_t)pin, GPIO_INTR_DISABLE);
    if (err != ESP_OK) { return WINK_ERR_HARDWARE; }
    gpio_isr_handler_remove((gpio_num_t)pin);
    s_gpio_isr[pin] = NULL;
#endif
    return WINK_OK;
}

/* ─────────────────────────────────────────────────────────
 * PWM (LEDC) 实现
 * ───────────────────────────────────────────────────────── */

#if defined(ESP_PLATFORM)
/* 板级路由弱默认：无 board_config.c 覆盖时使用，避免链接缺符号。
 * 强定义由 samples/<app>/board_config.c 提供。*/
__attribute__((weak)) const uint16_t pal_pwm_pin_map[PAL_PWM_CHANNELS] = {2, 4, 5, 18, 19, 21, 22, 23};

/* I2C 引脚弱默认：无 board_config.c 强覆盖时使用。
 * I2C0: SDA=21, SCL=22; I2C1: SDA=33, SCL=32 */
__attribute__((weak)) const uint16_t pal_i2c_pin_map[PAL_I2C_PORTS][2] = {
    {21, 22},
    {33, 32}
};
#endif

/* owner 字符串常量：claim/release 必须逐字一致，否则 release 静默 no-op。*/
static const char *const PWM_OWNER = "pal_hal_esp32";

wink_status_t pal_pwm_init(uint8_t channel, uint32_t freq_hz) {
    uint8_t timer_num = 0;
    wink_status_t rs = pal_pwm_router_acquire(channel, freq_hz, &timer_num);
    if (wink_status_is_error(rs)) { return rs; }

    rs = pal_resource_claim(PAL_RESOURCE_PWM_CHANNEL, channel, PWM_OWNER);
    if (wink_status_is_error(rs)) {
        pal_pwm_router_release(channel);
        return rs;
    }

#if defined(ESP_PLATFORM)
    /* router 分配 timer，不再写死 LEDC_TIMER_0：同频复用、异频隔离。*/
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = (ledc_timer_t)timer_num,
        .freq_hz = freq_hz,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer_cfg);
    if (err != ESP_OK) {
        pal_pwm_router_release(channel);
        /* gcc16/xtensa-gcc 不因 (void) 抑制 warn_unused_result：先赋值再丢弃，best-effort 释放。*/
        wink_status_t _rel = pal_resource_release(PAL_RESOURCE_PWM_CHANNEL, channel, PWM_OWNER);
        (void)_rel;
        return WINK_ERR_HARDWARE;
    }

    /* 物理路由来自 board_config.c 的强定义（无覆盖时回落至本 TU 弱默认 pal_pwm_pin_map）。*/
    ledc_channel_config_t ch_cfg = {
        .gpio_num = pal_pwm_pin_map[channel],
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = (ledc_channel_t)channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = (ledc_timer_t)timer_num,
        .duty = 0,
        .hpoint = 0,
    };
    err = ledc_channel_config(&ch_cfg);
    if (err != ESP_OK) {
        pal_pwm_router_release(channel);
        /* gcc16/xtensa-gcc 不因 (void) 抑制 warn_unused_result：先赋值再丢弃，best-effort 释放。*/
        wink_status_t _rel = pal_resource_release(PAL_RESOURCE_PWM_CHANNEL, channel, PWM_OWNER);
        (void)_rel;
        return WINK_ERR_HARDWARE;
    }
#else
    (void)freq_hz;
#endif
    return WINK_OK;
}

wink_status_t pal_pwm_set_duty(uint8_t channel, float duty_percent) {
    if (!pal_pwm_router_channel_ready(channel)) { return WINK_ERR_INVALID_ARG; }
    if (duty_percent < 0.0f) { duty_percent = 0.0f; }
    if (duty_percent > 100.0f) { duty_percent = 100.0f; }

#if defined(ESP_PLATFORM)
    uint32_t duty = (uint32_t)(duty_percent / 100.0f * 8191.0f); /* 13-bit = 8192 */
    esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, duty);
    if (err != ESP_OK) { return WINK_ERR_HARDWARE; }
    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
    if (err != ESP_OK) { return WINK_ERR_HARDWARE; }
#else
    (void)duty_percent;
#endif
    return WINK_OK;
}

void pal_pwm_deinit(uint8_t channel) {
    if (!pal_pwm_router_channel_ready(channel)) { return; }   /* no-op if uninitialized */
#if defined(ESP_PLATFORM)
    (void)ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, 0);
    (void)ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
#endif
    /* gcc16/xtensa-gcc 不因 (void) 抑制 warn_unused_result：先赋值再丢弃，best-effort 释放/deinit 不失败。*/
    wink_status_t _rel = pal_resource_release(PAL_RESOURCE_PWM_CHANNEL, channel, PWM_OWNER);
    (void)_rel;
    pal_pwm_router_release(channel);
}

/* ─────────────────────────────────────────────────────────
 * I2C 实现（v5.x / v6.x 双版本兼容）
 * ───────────────────────────────────────────────────────── */

#define I2C_MAX_DEVICES      4    /* MVP：每总线最多 4 个设备 */
#define I2C_TRANSFER_TIMEOUT_MS  1000

static bool s_i2c_initialized[PAL_I2C_PORTS] = {false};

#if defined(ESP_PLATFORM)
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/* 并发安全：静态互斥锁，保护初始化与设备缓存操作 */
static SemaphoreHandle_t s_i2c_mutex = NULL;
static StaticSemaphore_t s_i2c_mutex_buf;

#if WINK_I2C_USE_V6_API
/* v6.x：总线-设备二级模型 */
static i2c_master_bus_handle_t s_i2c_bus[PAL_I2C_PORTS] = {NULL};

typedef struct {
    i2c_master_dev_handle_t handle;
    uint16_t                dev_addr;    /* 0 = slot 空闲 */
} i2c_dev_cache_entry_t;

static i2c_dev_cache_entry_t s_i2c_dev_cache[PAL_I2C_PORTS][I2C_MAX_DEVICES] = {
    {{NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}},
    {{NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}}
};

/**
 * @brief ESP-IDF I2C 错误码精细映射
 */
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

/**
 * @brief 获取或创建 I2C 设备句柄（懒加载 + FIFO 替换）
 * @note 必须在持有 s_i2c_mutex 的情况下调用
 */
static wink_status_t pal_i2c_get_or_create_device(uint8_t port, uint16_t dev_addr,
                                                   i2c_master_dev_handle_t *out_handle)
{
    /* Step 1：线性扫描缓存，查找已存在的设备 */
    int free_slot = -1;
    for (int i = 0; i < I2C_MAX_DEVICES; i++) {
        if (s_i2c_dev_cache[port][i].dev_addr == dev_addr) {
            *out_handle = s_i2c_dev_cache[port][i].handle;
            return WINK_OK;
        }
        if (s_i2c_dev_cache[port][i].dev_addr == 0 && free_slot == -1) {
            free_slot = i;
        }
    }

    /* Step 2：未命中，需要创建新设备 */
    if (free_slot == -1) {
        /* 缓存已满：FIFO 替换策略，淘汰 index 0，整体前移 */
        ESP_LOGW(TAG, "port %d device cache full, evicting addr 0x%02X",
                 port, s_i2c_dev_cache[port][0].dev_addr);

        esp_err_t err = i2c_master_bus_rm_device(s_i2c_dev_cache[port][0].handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "failed to remove I2C device: %s", esp_err_to_name(err));
        }

        for (int i = 0; i < I2C_MAX_DEVICES - 1; i++) {
            s_i2c_dev_cache[port][i] = s_i2c_dev_cache[port][i + 1];
        }
        free_slot = I2C_MAX_DEVICES - 1;
    }

    /* Step 3：创建设备 */
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev_addr,
        .scl_speed_hz = 400000,
    };

    esp_err_t err = i2c_master_bus_add_device(s_i2c_bus[port], &dev_cfg,
                                               &s_i2c_dev_cache[port][free_slot].handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to add I2C device addr 0x%02X: %s",
                 dev_addr, esp_err_to_name(err));
        return WINK_ERR_HARDWARE;
    }

    s_i2c_dev_cache[port][free_slot].dev_addr = dev_addr;
    *out_handle = s_i2c_dev_cache[port][free_slot].handle;
    return WINK_OK;
}
#else  /* WINK_I2C_USE_V6_API == 0：v5.x 旧 API */
/**
 * @brief v5.x 错误码映射（复用相同的精细映射，v5.x 也受益）
 */
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
    /* v5.x i2c_master_write_read_device 对 NACK 返回 ESP_FAIL */
    if (esp_err == ESP_FAIL) {
        return WINK_ERR_DISCONNECTED;
    }
    ESP_LOGD(TAG, "unmapped esp_err: %s", esp_err_to_name(esp_err));
    return WINK_ERR_HARDWARE;
}
#endif /* WINK_I2C_USE_V6_API */
#endif /* ESP_PLATFORM */

wink_status_t pal_i2c_transfer(uint8_t port, uint16_t dev_addr,
                      const uint8_t *write_buf, uint32_t write_len,
                      uint8_t *read_buf, uint32_t read_len) {
    if (port >= PAL_I2C_PORTS) { return WINK_ERR_INVALID_ARG; }

#if defined(ESP_PLATFORM)
    /* 懒创建互斥锁 */
    if (s_i2c_mutex == NULL) {
        s_i2c_mutex = xSemaphoreCreateMutexStatic(&s_i2c_mutex_buf);
    }

    /* 临界区：初始化 + 设备缓存操作 */
    if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "I2C mutex timeout");
        return WINK_ERR_BUSY;
    }

    if (!s_i2c_initialized[port]) {
        /* SDA/SCL 物理路由来自 pal_i2c_pin_map（board_config.c 强定义，
         * 缺省时回落至本 TU 的弱默认值）。*/
#if WINK_I2C_USE_V6_API
        /* v6.x：总线初始化 */
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
        /* v5.x：旧 API 初始化 */
        i2c_config_t cfg = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = pal_i2c_pin_map[port][0],
            .scl_io_num = pal_i2c_pin_map[port][1],
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = 400000,
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
#endif /* WINK_I2C_USE_V6_API */
        s_i2c_initialized[port] = true;
    }

    wink_status_t rs = WINK_OK;
    esp_err_t err = ESP_OK;

#if WINK_I2C_USE_V6_API
    /* v6.x：先获取/创建设备句柄（在临界区内） */
    i2c_master_dev_handle_t dev_handle = NULL;
    rs = pal_i2c_get_or_create_device(port, dev_addr, &dev_handle);
    if (wink_status_is_error(rs)) {
        xSemaphoreGive(s_i2c_mutex);
        return rs;
    }
#endif /* WINK_I2C_USE_V6_API */

    /* 实际数据传输：持有 s_i2c_mutex 锁以防止并发时设备被驱逐引致 UAF 漏洞 */
#if WINK_I2C_USE_V6_API
    if (write_buf != NULL && write_len > 0) {
        if (read_buf != NULL && read_len > 0) {
            /* 写+读 组合传输（repeated START） */
            err = i2c_master_transmit_receive(dev_handle,
                                              write_buf, (size_t)write_len,
                                              read_buf, (size_t)read_len,
                                              I2C_TRANSFER_TIMEOUT_MS);
        } else {
            /* 只写 */
            err = i2c_master_transmit(dev_handle, write_buf, (size_t)write_len,
                                      I2C_TRANSFER_TIMEOUT_MS);
        }
    } else if (read_buf != NULL && read_len > 0) {
        /* 只读 */
        err = i2c_master_receive(dev_handle, read_buf, (size_t)read_len,
                                 I2C_TRANSFER_TIMEOUT_MS);
    }
    /* else: 空操作，直接返回 OK */
#else
    /* v5.x：旧 API 传输 */
    err = i2c_master_write_read_device(
        (i2c_port_t)port, dev_addr,
        write_buf, (size_t)write_len,
        read_buf, (size_t)read_len,
        pdMS_TO_TICKS(I2C_TRANSFER_TIMEOUT_MS)
    );
#endif /* WINK_I2C_USE_V6_API */

    xSemaphoreGive(s_i2c_mutex);

    if (err != ESP_OK) {
        return pal_i2c_map_esp_err(err);
    }
#else
    (void)dev_addr; (void)write_buf; (void)write_len; (void)read_buf; (void)read_len;
#endif /* ESP_PLATFORM */
    return WINK_OK;
}

/* ─────────────────────────────────────────────────────────
 * GPIO Pulse In（超声波硬件捕获）
 * ───────────────────────────────────────────────────────── */

wink_status_t pal_gpio_pulse_in(uint16_t pin, bool level,
                                  uint32_t timeout_us, uint32_t *pulse_us) {
    /* FIXME: MVP 阶段暂用 busy-wait（会阻塞 tick）。
     * Phase 4 目标：迁移至 RMT + GPIO 双沿 ISR + 硬件定时器实现非阻塞捕获。
     * 当前实现仅供 avoidance_car 示例跑通，实时性不达标。 */
    if (pulse_us == NULL || pin >= GPIO_NUM_MAX) {
        return WINK_ERR_INVALID_ARG;
    }

    uint64_t start = pal_get_us();
    while (pal_gpio_read(pin) != level) {
        if (pal_get_us() - start > timeout_us) {
            return WINK_ERR_TIMEOUT;
        }
    }

    uint64_t pulse_start = pal_get_us();
    while (pal_gpio_read(pin) == level) {
        if (pal_get_us() - start > timeout_us) {
            return WINK_ERR_TIMEOUT;
        }
    }

    *pulse_us = (uint32_t)(pal_get_us() - pulse_start);
    return WINK_OK;
}
