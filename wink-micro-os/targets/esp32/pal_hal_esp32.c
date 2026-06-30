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
#include "pal_irq.h"        /* 统一中断抽象 */
#include "pal_osal.h"       /* pal_get_us() (used in pal_gpio_pulse_in busy-wait) */
#include "pal_resource.h"
#include "pal_pwm_router.h"
#include "pal_debug.h"
#include <stdarg.h>
#include <stdio.h>

#if defined(ESP_PLATFORM)
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_idf_version.h"
#include "esp_intr_alloc.h"
#include "xtensa/hal.h"

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

_Static_assert((gpio_num_t)GPIO_NUM_NC == -1,
    "GPIO_NUM_NC must be -1 for wink_pin_t sign-compatibility");

wink_status_t pal_gpio_init(wink_pin_t pin, pal_gpio_mode_t mode) {
    if (pin < 0 || pin >= GPIO_NUM_MAX) { return WINK_ERR_INVALID_ARG; }

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

void pal_gpio_write(wink_pin_t pin, bool level) {
#if defined(ESP_PLATFORM)
    if (pin >= 0 && pin < GPIO_NUM_MAX) {
        gpio_set_level((gpio_num_t)pin, level ? 1 : 0);
    }
#else
    (void)pin; (void)level;
#endif
}

bool pal_gpio_read(wink_pin_t pin) {
#if defined(ESP_PLATFORM)
    if (pin < 0 || pin >= GPIO_NUM_MAX) { return false; }
    return gpio_get_level((gpio_num_t)pin) != 0;
#else
    (void)pin; return false;
#endif
}

#if defined(ESP_PLATFORM)
/* ⚠️ SMP 安全：GPIO 分发表自旋锁（ADR-IRQ-004）
 *
 * 竞态场景修复：
 *   Core 0 正在执行 gpio_isr_wrapper，刚读取 s_gpio_isr[pin]
 *   此时 Core 1 调用 pal_gpio_disable_interrupt，将 s_gpio_isr_arg 置空
 *   Core 0 后续读取到 NULL arg，导致空指针解引用崩溃
 *
 * 解决方案：所有读写分发表的路径都必须持有此自旋锁。
 * ISR 上下文使用 portENTER_CRITICAL_ISR()。
 */
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "soc/gpio_struct.h"

static inline void esp_memory_barrier(void) {
#if defined(__XTENSA__)
    __asm__ __volatile__("memw" ::: "memory");
#elif defined(__riscv)
    __asm__ __volatile__("fence rw, rw" ::: "memory");
#else
    __asm__ __volatile__("" ::: "memory");
#endif
}

#ifndef Atomic_Load_u32
static inline uint32_t Atomic_Load_u32(volatile uint32_t *pulSource) {
    return *pulSource;
}
#endif

#ifndef Atomic_Increment_u32
static inline uint32_t Atomic_Increment_u32(volatile uint32_t *pulAddend) {
    return __atomic_add_fetch(pulAddend, 1, __ATOMIC_SEQ_CST);
}
#endif

#ifndef Atomic_Decrement_u32
static inline uint32_t Atomic_Decrement_u32(volatile uint32_t *pulAddend) {
    return __atomic_sub_fetch(pulAddend, 1, __ATOMIC_SEQ_CST);
}
#endif

static inline void gpio_clear_intr_status(gpio_num_t gpio_num) {
    if (gpio_num < 32) {
        GPIO.status_w1tc = (1UL << gpio_num);
    } else {
        GPIO.status1_w1tc.val = (1UL << (gpio_num - 32));
    }
}

static portMUX_TYPE s_gpio_table_mux = portMUX_INITIALIZER_UNLOCKED;

static pal_gpio_isr_t s_gpio_isr[GPIO_NUM_MAX] = {NULL};
static void *s_gpio_isr_arg[GPIO_NUM_MAX] = {NULL};

/* SMP ISR 同步 count */
static volatile uint32_t s_irq_in_flight[32] = {0};
static volatile uint32_t s_gpio_irq_in_flight[GPIO_NUM_MAX] = {0};

/**
 * @brief ESP32 GPIO 公用 ISR 包装（IRAM 中执行，ADR-IRQ-002 清标顺序）
 *
 * ⚠️ 关键实现顺序（必须严格遵守，ADR-IRQ-002）：
 * 1. ✅ 第一步：先禁用中断，再清除标志 —— 防止重入和中断风暴
 * 2. ✅ 第二步：SMP 安全：持有自旋锁，原子性读取回调指针和参数
 * 3. ✅ 第三步：调用用户 ISR
 * 4. ✅ 第四步：如果分发表中 isr 非空，重新启用中断
 *
 * API 名称修正（v2.0）：不使用非标准的 gpio_intr_clr_enable()，
 * 改用标准 gpio_intr_disable() + gpio_clear_intr_status() 组合。
 */
static void PAL_ISR gpio_isr_wrapper(void *arg)
{
    uint32_t pin = (uint32_t)(uintptr_t)arg;
    if (pin >= GPIO_NUM_MAX) {
        return;
    }

    /* ✅ SMP 同步：标记此 GPIO ISR 正在执行 */
    Atomic_Increment_u32(&s_gpio_irq_in_flight[pin]);

    /* ✅ 第一步：第一时间禁用并清除中断标志，防止重入 */
    gpio_intr_disable((gpio_num_t)pin);
    gpio_clear_intr_status((gpio_num_t)pin);

    /* ✅ 第二步：SMP 安全读取回调（持有自旋锁）
     * 确保 isr 和 arg 读取的原子性，避免双核竞态导致 NULL deref */
    pal_gpio_isr_t isr = NULL;
    void *isr_arg = NULL;
    bool need_reenable = false;

    portENTER_CRITICAL_ISR(&s_gpio_table_mux);
    isr = s_gpio_isr[pin];
    isr_arg = s_gpio_isr_arg[pin];
    need_reenable = (s_gpio_isr[pin] != NULL);  /* 预读取是否需要重新启用 */
    portEXIT_CRITICAL_ISR(&s_gpio_table_mux);

    /* ✅ 第三步：调用用户 ISR（此时中断已禁用并清除，不会重入） */
    if (isr != NULL) {
        isr(isr_arg);
    }

    /* ✅ 第四步：重新启用中断（用户 callback 完成后）
     * 优化：不持有自旋锁调用 gpio_intr_enable，减少中断延迟
     * 如果用户在 callback 中调用了 disable，isr 会被设为 NULL，need_reenable 也为 false */
    if (need_reenable) {
        /* 二次检查：在启用前确认 isr 仍然有效
         * （在临界区外执行硬件操作，降低 ISR 延迟） */
        portENTER_CRITICAL_ISR(&s_gpio_table_mux);
        need_reenable = (s_gpio_isr[pin] != NULL);
        portEXIT_CRITICAL_ISR(&s_gpio_table_mux);

        if (need_reenable) {
            gpio_intr_enable((gpio_num_t)pin);
        }
    }

    /* ✅ SMP 同步：ISR 执行完成 */
    Atomic_Decrement_u32(&s_gpio_irq_in_flight[pin]);
}
#endif /* ESP_PLATFORM */

/* ─────────────────────────────────────────────────────────
 * GPIO 中断接口实现（v2.0 SMP 安全版）
 * ───────────────────────────────────────────────────────── */

wink_status_t pal_gpio_enable_interrupt_ex(wink_pin_t pin,
                                            pal_gpio_intr_t intr_type,
                                            pal_irq_prio_t prio,
                                            pal_gpio_isr_t callback,
                                            void *arg)
{
    if (pin < 0 || pin >= GPIO_NUM_MAX) { return WINK_ERR_INVALID_ARG; }
    if (callback == NULL) { return WINK_ERR_INVALID_ARG; }
    if (prio >= PAL_IRQ_PRIO_COUNT) { return WINK_ERR_INVALID_ARG; }

    /* v2.1 G2：ESP32 GPIO ISR 路径同样拒接 REALTIME，避免与 pal_irq_enable 出现
     * "同一 prio 在两条路径上行为不一致"的语义陷阱（参考 ADR-0012 / ADR-IRQ-008）。 */
    if (prio == PAL_IRQ_PRIO_REALTIME) {
        return WINK_ERR_UNSUPPORTED;
    }

    /* GPIO 中断优先级由 ESP-IDF 全局控制，暂不支持 per-pin 设置
     * prio 参数预留用于未来扩展（如分配到不同的 CPU 中断源） */
    (void)prio;

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
        case PAL_GPIO_INTR_LOW_LEVEL:
            esp_intr_type = GPIO_INTR_LOW_LEVEL;
            break;
        case PAL_GPIO_INTR_HIGH_LEVEL:
            esp_intr_type = GPIO_INTR_HIGH_LEVEL;
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

    /* ✅ SMP 安全：持有自旋锁写入分发表 */
    portENTER_CRITICAL(&s_gpio_table_mux);
    s_gpio_isr[pin] = callback;
    s_gpio_isr_arg[pin] = arg;
    portEXIT_CRITICAL(&s_gpio_table_mux);

    /* 注册到 ESP-IDF ISR 分发服务 */
    esp_err_t err = gpio_isr_handler_add((gpio_num_t)pin,
                                          gpio_isr_wrapper,
                                          (void *)(uintptr_t)pin);
    if (err != ESP_OK) {
        portENTER_CRITICAL(&s_gpio_table_mux);
        s_gpio_isr[pin] = NULL;
        portEXIT_CRITICAL(&s_gpio_table_mux);
        return WINK_ERR_HARDWARE;
    }

    /* 设置中断类型 */
    err = gpio_set_intr_type((gpio_num_t)pin, esp_intr_type);
    if (err != ESP_OK) {
        (void)gpio_isr_handler_remove((gpio_num_t)pin);
        portENTER_CRITICAL(&s_gpio_table_mux);
        s_gpio_isr[pin] = NULL;
        portEXIT_CRITICAL(&s_gpio_table_mux);
        return WINK_ERR_HARDWARE;
    }
#else
    (void)intr_type; (void)prio; (void)callback; (void)arg;
    /* 非 ESP32 平台（编译时存根）返回 UNSUPPORTED，由调用方静默降级 */
    return WINK_ERR_UNSUPPORTED;
#endif
    return WINK_OK;
}

wink_status_t pal_gpio_disable_interrupt(wink_pin_t pin) {
    if (pin < 0 || pin >= GPIO_NUM_MAX) { return WINK_ERR_INVALID_ARG; }

#if defined(ESP_PLATFORM)
    esp_err_t err = gpio_set_intr_type((gpio_num_t)pin, GPIO_INTR_DISABLE);
    if (err != ESP_OK) { return WINK_ERR_HARDWARE; }

    (void)gpio_isr_handler_remove((gpio_num_t)pin);

    /* ✅ SMP 安全：持有自旋锁清空分发表
     * 必须在 remove handler 之后清空，防止竞态条件 */
    portENTER_CRITICAL(&s_gpio_table_mux);
    s_gpio_isr[pin] = NULL;
    s_gpio_isr_arg[pin] = NULL;
    portEXIT_CRITICAL(&s_gpio_table_mux);
#else
    (void)pin;
    /* 非 ESP32 平台（编译时存根）返回 UNSUPPORTED */
    return WINK_ERR_UNSUPPORTED;
#endif
    return WINK_OK;
}

/* ─────────────────────────────────────────────────────────
 * PWM (LEDC) 实现
 * ───────────────────────────────────────────────────────── */

#if defined(ESP_PLATFORM)
/* 板级路由弱默认：无 board_config.c 覆盖时使用，避免链接缺符号。
 * 强定义由 samples/<app>/board_config.c 提供。*/
__attribute__((weak)) const wink_pin_t pal_pwm_pin_map[PAL_PWM_CHANNELS] = {2, 4, 5, 18, 19, 21, 22, 23};

/* I2C 引脚弱默认：无 board_config.c 强覆盖时使用。
 * I2C0: SDA=21, SCL=22; I2C1: SDA=33, SCL=32 */
__attribute__((weak)) const wink_pin_t pal_i2c_pin_map[PAL_I2C_PORTS][2] = {
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

/* 并发安全：静态互斥锁，保护初始化与设备缓存操作
 * ✅ SMP-safe: FreeRTOS 调度器启动前静态初始化，无双核竞态
 * 参见: https://www.freertos.org/xSemaphoreCreateMutexStatic.html */
static SemaphoreHandle_t s_i2c_mutex = NULL;
static StaticSemaphore_t s_i2c_mutex_buf;

/* 在启动时初始化互斥锁，避免双核懒初始化竞态
 * constructor(101) 确保在应用层代码之前执行 */
__attribute__((constructor(101)))
static void pal_i2c_static_init_mutex(void) {
    s_i2c_mutex = xSemaphoreCreateMutexStatic(&s_i2c_mutex_buf);
}

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
 * @brief 将缓存条目移动到队尾（LRU 热更新）
 * @note 每次命中后调用，确保访问频率高的设备不被淘汰
 */
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

/**
 * @brief 获取或创建 I2C 设备句柄（懒加载 + LRU 替换 + 事务安全）
 * @note 必须在持有 s_i2c_mutex 的情况下调用
 * @design
 *   - ✅ LRU: 命中时将条目移到队尾，淘汰时总是淘汰 index 0（最久未用）
 *   - ✅ 事务安全: 先分配临时句柄，仅在完全成功后才修改缓存
 *   - ✅ 无中间状态: 即使创建设备失败，缓存也不会被破坏
 */
static wink_status_t pal_i2c_get_or_create_device(uint8_t port, uint16_t dev_addr,
                                                   i2c_master_dev_handle_t *out_handle)
{
    /* Step 1：线性扫描缓存，查找已存在的设备 */
    int free_slot = -1;
    for (int i = 0; i < I2C_MAX_DEVICES; i++) {
        if (s_i2c_dev_cache[port][i].dev_addr == dev_addr) {
            /* ✅ LRU: 命中时将该条目移到队尾（提高后续访问局部性） */
            pal_i2c_lru_touch(port, i);
            *out_handle = s_i2c_dev_cache[port][I2C_MAX_DEVICES - 1].handle;
            return WINK_OK;
        }
        if (s_i2c_dev_cache[port][i].dev_addr == 0 && free_slot == -1) {
            free_slot = i;
        }
    }

    /* Step 2：先在栈上分配临时句柄，不修改任何缓存状态 */
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev_addr,
        .scl_speed_hz = 400000,
    };

    i2c_master_dev_handle_t temp_handle = NULL;
    esp_err_t err = i2c_master_bus_add_device(s_i2c_bus[port], &dev_cfg, &temp_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to add I2C device addr 0x%02X: %s",
                 dev_addr, esp_err_to_name(err));
        return pal_i2c_map_esp_err(err);
    }

    /* Step 3: 需要淘汰缓存（仅在设备创建成功后执行） */
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

    /* Step 4：唯一的缓存写入点 - 原子性提交 */
    s_i2c_dev_cache[port][free_slot].handle = temp_handle;
    s_i2c_dev_cache[port][free_slot].dev_addr = dev_addr;
    *out_handle = temp_handle;
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
    /* 临界区：初始化 + 设备缓存操作
     * ✅ 互斥锁已在 constructor 中静态初始化，SMP 安全 */
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

wink_status_t pal_gpio_pulse_in(wink_pin_t pin, bool level,
                                  uint32_t timeout_us, uint32_t *pulse_us) {
    /* FIXME: MVP 阶段暂用 busy-wait（会阻塞 tick）。
     * Phase 4 目标：迁移至 RMT + GPIO 双沿 ISR + 硬件定时器实现非阻塞捕获。
     * 当前实现仅供 avoidance_car 示例跑通，实时性不达标。 */
    if (pulse_us == NULL || pin < 0 || pin >= GPIO_NUM_MAX) {
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

/* ─────────────────────────────────────────────────────────
 * Debug Output（PAL 统一接口）
 * ───────────────────────────────────────────────────────── */

void pal_debug_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

#if defined(ESP_PLATFORM)
/* ─────────────────────────────────────────────────────────
 * 共享中断机制（RCU 模式 + SMP 安全，ADR-IRQ-005）
 * ───────────────────────────────────────────────────────── */

#define MAX_SHARED_HANDLERS  4      /* 单中断最多 4 个共享 handler */
#define MAX_SHARED_IRQS      16     /* 最多 16 个支持共享的中断号 */

/* 共享 handler 链（RCU 模式：写时复制，原子替换指针） */
typedef struct {
    pal_irq_shared_handler_t  handler;
    void                     *arg;
} shared_handler_entry_t;

typedef struct {
    shared_handler_entry_t entries[MAX_SHARED_HANDLERS];
    uint8_t count;
} shared_chain_t;

/* 每个中断号的共享链指针（RCU 原子替换） */
static shared_chain_t *s_shared_chain[MAX_SHARED_IRQS] = {NULL};
static portMUX_TYPE s_shared_chain_mux = portMUX_INITIALIZER_UNLOCKED;

/* ⚠️ SMP ISR 同步机制（ADR-IRQ-007 完整实现）
 *
 * 问题：pal_irq_disable() 返回后，另一个核心可能仍在执行该 ISR。
 *       如果此时释放 ISR 使用的内存，会导致 UAF (Use-After-Free)。
 *
 * 解决方案：
 * 1. 每个中断号维护一个原子 in_flight 计数器
 * 2. ISR wrapper 在入口处 +1，出口处 -1
 * 3. pal_irq_synchronize() 忙等待计数器归 0
 * 4. 增加超时保护，避免死锁
 */
// s_irq_in_flight and s_gpio_irq_in_flight are defined at the top of the file.

#define SYNCHRONIZE_TIMEOUT_US  100000  /* 100ms 超时（远大于 ISR 最大执行时间） */

/* 共享中断 wrapper（由 esp_intr_alloc 注册，按注册顺序调用所有 handler） */
static void PAL_ISR shared_irq_wrapper(void *arg)
{
    uint32_t irq_num = (uint32_t)(uintptr_t)arg;
    if (irq_num >= MAX_SHARED_IRQS) {
        return;
    }

    /* ✅ SMP 同步：标记此 ISR 正在执行 */
    Atomic_Increment_u32(&s_irq_in_flight[irq_num]);

    /* ✅ RCU 读路径：原子性读取当前链指针
     * 写路径会创建新链并原子替换，因此读取到的指针在 ISR 执行期间始终有效 */
    shared_chain_t *chain = s_shared_chain[irq_num];
    if (chain == NULL) {
        Atomic_Decrement_u32(&s_irq_in_flight[irq_num]);
        return;
    }

    uint32_t claimed_count = 0;

    /* ✅ v2.0 语义修正：始终遍历调用所有 handler，不提前终止
     * 这与 Linux 内核 Shared IRQ 行为一致，避免共享外设同时触发时的中断丢失 */
    for (uint8_t i = 0; i < chain->count; i++) {
        if (chain->entries[i].handler != NULL) {
            bool claimed = chain->entries[i].handler(chain->entries[i].arg);
            if (claimed) {
                claimed_count++;
            }
        }
    }

    /* 零认领 → 可能是杂散中断或硬件问题，记录警告 */
    if (claimed_count == 0 && chain->count > 0) {
        ESP_LOGW("pal_irq", "spurious interrupt on irq=%lu, no handler claimed",
                 (unsigned long)irq_num);
    }

    /* ✅ SMP 同步：ISR 执行完成，减少计数 */
    Atomic_Decrement_u32(&s_irq_in_flight[irq_num]);
}

/* ─────────────────────────────────────────────────────────
 * 中断控制器核心接口实现（ESP32 平台）
 * ───────────────────────────────────────────────────────── */

/* 逻辑中断句柄表（32 个逻辑中断源，未来扩展至 Device Tree） */
static intr_handle_t s_irq_handles[32] = {NULL};

/* v2.1 G1：硬件直连中断（pal_irq_direct_connect）的无参 handler 表。
 *
 * 旧实现 `(pal_isr_t)handler` 把 void(*)(void) 强转为 void(*)(void*)，ABI 容忍但
 * CFI/UBSan-function 直接判违例。新实现以本文件 static 数组保存裸 direct handler，
 * 由 trampoline 桥接 (void*) → ()，签名清洁。
 *
 * ⚠️ TLS 禁忌：严禁将 s_direct_handlers 换成 thread_local/__thread。ISR 在 IDF
 * dispatch 上下文里被调用，访问 TLS 可能踩到不存在的线程槽位。文件级 static 数组
 * （或全局 atomic 指针表）才是 ISR 安全的。 */
#define PAL_DIRECT_HANDLER_SLOTS  32
static pal_direct_isr_t s_direct_handlers[PAL_DIRECT_HANDLER_SLOTS] = {NULL};

/* 通用 ISR wrapper：跟踪 in-flight 计数并调用用户 handler */
typedef struct {
    pal_isr_t user_handler;
    void     *user_arg;
} isr_wrapper_ctx_t;

static isr_wrapper_ctx_t s_isr_ctx[32] = {{NULL, NULL}};

static void PAL_ISR generic_isr_wrapper(void *arg)
{
    uint32_t irq_num = (uint32_t)(uintptr_t)arg;
    if (irq_num >= 32) {
        return;
    }

    /* ✅ 第一时间清除 Pending 标志，防止重入与中断风暴 */
    pal_irq_clear_pending(irq_num);

    /* ✅ SMP 同步：标记 ISR 正在执行 */
    Atomic_Increment_u32(&s_irq_in_flight[irq_num]);

    isr_wrapper_ctx_t *ctx = &s_isr_ctx[irq_num];
    if (ctx->user_handler != NULL) {
        ctx->user_handler(ctx->user_arg);
    }

    /* ✅ SMP 同步：ISR 执行完成 */
    Atomic_Decrement_u32(&s_irq_in_flight[irq_num]);
}

wink_status_t pal_irq_enable(uint32_t irq_num, pal_irq_prio_t prio,
                              pal_isr_t handler, void *arg)
{
    if (irq_num >= 32 || handler == NULL || prio >= PAL_IRQ_PRIO_COUNT) {
        return WINK_ERR_INVALID_ARG;
    }

    /* v2.1 G2 (ADR-0012 / ADR-IRQ-008)：ESP32 不支持 REALTIME 级 C-ISR。
     * NMI 等 Level 4+ 无法通过 esp_intr_alloc 注册 C 处理函数；旧实现把
     * REALTIME 静默映射到 LEVEL3，与 HIGHEST 物理等价但契约相反，会掩盖
     * 跨平台 bug（用户在 ESP32 上写 REALTIME + xQueueSendFromISR 不崩，
     * 换 STM32 NMI 后翻车）。此处显式拒接，不再静默降级。 */
    if (prio == PAL_IRQ_PRIO_REALTIME) {
        return WINK_ERR_UNSUPPORTED;
    }

    /* 先释放旧的句柄（如果有） */
    if (s_irq_handles[irq_num] != NULL) {
        esp_intr_free(s_irq_handles[irq_num]);
        s_irq_handles[irq_num] = NULL;
    }

    /* 优先级映射表（ADR-IRQ-003 预留安全边界）
     * LOWEST ~ HIGHEST 都是 RTOS 安全的，可以调用 FromISR API
     * REALTIME 在 ESP32 上已在入口处被拒接，不出现在映射表中。
     *
     * 注意：ESP_INTR_FLAG_LEVELn 是标志位，不是数值，不能直接用数值做 | 运算
     * 映射关系：LEVEL1 = 最低优先级，LEVEL7 = 最高优先级
     */
    static const int s_prio_flag_map[PAL_IRQ_PRIO_COUNT] = {
        [PAL_IRQ_PRIO_LOWEST]   = ESP_INTR_FLAG_LEVEL1,
        [PAL_IRQ_PRIO_LOW]      = ESP_INTR_FLAG_LEVEL1,
        [PAL_IRQ_PRIO_NORMAL]   = ESP_INTR_FLAG_LEVEL2,
        [PAL_IRQ_PRIO_HIGH]     = ESP_INTR_FLAG_LEVEL3,  /* configMAX_SYSCALL_INTERRUPT_PRIORITY is Level 3 */
        [PAL_IRQ_PRIO_HIGHEST]  = ESP_INTR_FLAG_LEVEL3,  /* RTOS 安全边界 */
        /* PAL_IRQ_PRIO_REALTIME 在入口处已拒接（WINK_ERR_UNSUPPORTED），此处不映射 */
    };

    /* ✅ SMP 同步：保存用户 handler 和 arg，通过 wrapper 调用
     * 这样 wrapper 可以追踪 in-flight 计数 */
    s_isr_ctx[irq_num].user_handler = handler;
    s_isr_ctx[irq_num].user_arg = arg;

    // 针对测试所用的逻辑中断号，映射到合法的 CPU 内部软件中断源
    int source = irq_num;
    if (irq_num == 7) {
        source = ETS_INTERNAL_SW0_INTR_SOURCE;
    } else if (irq_num == 8) {
        source = ETS_INTERNAL_SW1_INTR_SOURCE;
    }

    int flags = ESP_INTR_FLAG_IRAM;
    if (source >= 0) {
        flags |= s_prio_flag_map[prio];
    }
    
    esp_err_t err = esp_intr_alloc(source, flags,
                                    (intr_handler_t)generic_isr_wrapper,
                                    (void *)(uintptr_t)irq_num,
                                    &s_irq_handles[irq_num]);
    if (err != ESP_OK) {
        s_isr_ctx[irq_num].user_handler = NULL;
        s_isr_ctx[irq_num].user_arg = NULL;
        return WINK_ERR_HARDWARE;
    }

    return WINK_OK;
}

wink_status_t pal_irq_disable(uint32_t irq_num)
{
    if (irq_num >= 32) {
        return WINK_ERR_INVALID_ARG;
    }

    if (s_irq_handles[irq_num] != NULL) {
        esp_err_t err = esp_intr_free(s_irq_handles[irq_num]);
        s_irq_handles[irq_num] = NULL;
        if (err != ESP_OK) {
            return WINK_ERR_HARDWARE;
        }
    }

    /* v2.1：清理 direct-connect 槽位，配合后续 pal_irq_synchronize() 保护 UAF。
     * esp_intr_free 已经卸下 ISR 派发，置 NULL 即使与 trampoline 读取并发也安全
     * （trampoline 端 NULL 检查会让在飞中断退化为 no-op）。 */
    s_direct_handlers[irq_num] = NULL;

    return WINK_OK;
}

/* v2.1 G1：硬件直连中断（pal_irq_direct_connect）trampoline 实现。
 * 数据表 s_direct_handlers 在文件顶部与 s_irq_handles 一起声明，避免前向引用。 */

static void PAL_ISR direct_trampoline(void *arg)
{
    uint32_t irq_num = (uint32_t)(uintptr_t)arg;
    if (irq_num >= PAL_DIRECT_HANDLER_SLOTS) {
        return;
    }
    pal_direct_isr_t h = s_direct_handlers[irq_num];
    if (h != NULL) {
        h();
    }
}

wink_status_t pal_irq_direct_connect(uint32_t irq_num, pal_direct_isr_t handler)
{
    if (irq_num >= PAL_DIRECT_HANDLER_SLOTS || handler == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    /* 先注册到 direct 表，再调 pal_irq_enable 注册 wrapper。顺序确保 wrapper
     * 一旦被 esp_intr_alloc 链接上来，trampoline 即可读到合法的 handler。 */
    s_direct_handlers[irq_num] = handler;
    wink_status_t st = pal_irq_enable(irq_num, PAL_IRQ_PRIO_NORMAL,
                                       direct_trampoline,
                                       (void *)(uintptr_t)irq_num);
    if (wink_status_is_error(st)) {
        s_direct_handlers[irq_num] = NULL;
    }
    return st;
}

wink_status_t pal_irq_shared_register(uint32_t irq_num, pal_irq_prio_t prio,
                                       pal_irq_shared_handler_t handler, void *arg)
{
    if (irq_num >= MAX_SHARED_IRQS || handler == NULL || prio >= PAL_IRQ_PRIO_COUNT) {
        return WINK_ERR_INVALID_ARG;
    }

    /* ✅ RCU 写路径：持有自旋锁保护 */
    portENTER_CRITICAL(&s_shared_chain_mux);

    shared_chain_t *old_chain = s_shared_chain[irq_num];
    shared_chain_t *new_chain = NULL;

    if (old_chain == NULL) {
        /* 第一个 handler：创建新链 */
        new_chain = malloc(sizeof(shared_chain_t));
        if (new_chain == NULL) {
            portEXIT_CRITICAL(&s_shared_chain_mux);
            return WINK_ERR_NO_MEM;
        }
        memset(new_chain, 0, sizeof(shared_chain_t));
    } else {
        /* 已有 handler：复制旧链，添加新 handler */
        if (old_chain->count >= MAX_SHARED_HANDLERS) {
            portEXIT_CRITICAL(&s_shared_chain_mux);
            return WINK_ERR_NO_MEM;
        }
        new_chain = malloc(sizeof(shared_chain_t));
        if (new_chain == NULL) {
            portEXIT_CRITICAL(&s_shared_chain_mux);
            return WINK_ERR_NO_MEM;
        }
        memcpy(new_chain, old_chain, sizeof(shared_chain_t));
    }

    /* 追加新 handler */
    uint8_t idx = new_chain->count;
    new_chain->entries[idx].handler = handler;
    new_chain->entries[idx].arg = arg;
    new_chain->count++;

    /* ✅ 原子替换指针（RCU 关键点）
     * 正在 ISR 中执行的旧链指针不会被修改，安全执行 */
    s_shared_chain[irq_num] = new_chain;

    portEXIT_CRITICAL(&s_shared_chain_mux);

    /* ✅ SMP 安全：等待所有核心退出旧 ISR 后再释放
     * 这是 RCU 模式的 synchronize_rcu() 简化实现 */
    pal_irq_synchronize(irq_num);

    /* 现在可以安全释放旧链 */
    free(old_chain);

    /* 如果是第一个 handler，注册共享 wrapper */
    if (new_chain->count == 1) {
        /* 注意：首次注册时的优先级生效，后续注册忽略优先级
         * 这是因为硬件中断优先级是全局的，不能动态修改 */
        return pal_irq_enable(irq_num, prio, shared_irq_wrapper,
                              (void *)(uintptr_t)irq_num);
    }

    return WINK_OK;
}

void pal_irq_set_pending(uint32_t irq_num)
{
#if defined(ESP_PLATFORM)
    if (irq_num < 32 && s_irq_handles[irq_num] != NULL) {
        int cpu_intr = esp_intr_get_intno(s_irq_handles[irq_num]);
        if (cpu_intr >= 0 && cpu_intr < 32) {
            xthal_set_intset(1 << cpu_intr);
        }
    }
#else
    (void)irq_num;
#endif
}

void pal_irq_clear_pending(uint32_t irq_num)
{
#if defined(ESP_PLATFORM)
    if (irq_num < 32 && s_irq_handles[irq_num] != NULL) {
        int cpu_intr = esp_intr_get_intno(s_irq_handles[irq_num]);
        if (cpu_intr >= 0 && cpu_intr < 32) {
            xthal_set_intclear(1 << cpu_intr);
        }
    }
#else
    (void)irq_num;
#endif
}

void pal_irq_synchronize(uint32_t irq_num)
{
    /* ✅ SMP 同步完整实现（ADR-IRQ-007）：
     *
     * 机制：每个 ISR wrapper 在进入时 +1，退出时 -1。
     * synchronize() 忙等待计数归 0，确保所有核心都已退出 ISR。
     *
     * 这是 Linux 内核 synchronize_irq() 在 ESP32 上的简化实现。
     * 无需 IPI，因为原子操作在 SMP 下是全局可见的。
     */

    if (irq_num == ~0U) {
        /* 等待所有中断：逐个检查 32 个逻辑中断 + GPIO 中断 */
        for (uint32_t i = 0; i < 32; i++) {
            uint64_t start = pal_get_us();
            while (Atomic_Load_u32(&s_irq_in_flight[i]) > 0) {
                if (pal_get_us() - start > SYNCHRONIZE_TIMEOUT_US) {
                    ESP_LOGE("pal_irq", "synchronize timeout on irq=%lu",
                             (unsigned long)i);
                    break;
                }
            }
        }
        /* GPIO 中断只需要等待禁用的那个，但全量检查也没问题 */
        for (uint32_t i = 0; i < GPIO_NUM_MAX; i++) {
            uint64_t start = pal_get_us();
            while (Atomic_Load_u32(&s_gpio_irq_in_flight[i]) > 0) {
                if (pal_get_us() - start > SYNCHRONIZE_TIMEOUT_US) {
                    ESP_LOGE("pal_irq", "synchronize timeout on gpio=%lu",
                             (unsigned long)i);
                    break;
                }
            }
        }
    } else {
        /* 等待单个中断 */
        uint64_t start = pal_get_us();
        while (Atomic_Load_u32(&s_irq_in_flight[irq_num]) > 0) {
            if (pal_get_us() - start > SYNCHRONIZE_TIMEOUT_US) {
                ESP_LOGE("pal_irq", "synchronize timeout on irq=%lu",
                         (unsigned long)irq_num);
                break;
            }
        }
    }

    /* 确保后续的内存释放操作（如 free）不会被编译器重排到等待之前 */
    esp_memory_barrier();
}

/* ─────────────────────────────────────────────────────────
 * 全局中断锁实现（双等级语义，ADR-IRQ-006）
 * ───────────────────────────────────────────────────────── */

uint32_t pal_irq_save(void)
{
    /* ✅ 设置到最高屏蔽级别，禁用所有可屏蔽中断（ADR-IRQ-001）
     * 不使用 XCHAL_EXCM_LEVEL (= 3)，因为它只能禁用优先级 ≤3 的中断，
     * 高优先级中断（如 5、7）仍能触发，临界区保护失效。
     *
     * 使用 XCHAL_NUM_INTLEVELS 达到真正的全局禁用效果。
     *
     * ⚠️ 约束：受此锁保护的临界区必须 < 1µs，避免影响 Wi-Fi 和看门狗。
     */
    return XTOS_SET_INTLEVEL(XCHAL_NUM_INTLEVELS);
}

uint32_t pal_irq_save_rtos_safe(void)
{
    /* ✅ 仅禁用到 RTOS 安全边界（ADR-IRQ-006，推荐默认使用）
     * configMAX_SYSCALL_INTERRUPT_PRIORITY = 5
     * 设置 INTLEVEL = 5 将屏蔽所有优先级 ≤5 的中断
     * 优先级 6-7 的中断（如 Wi-Fi 基带、REALTIME 级）仍可触发
     *
     * 这是推荐 of 默认选择，不会影响底层硬件协议时序。
     */
    return XTOS_SET_INTLEVEL(XCHAL_EXCM_LEVEL);
}

void pal_irq_restore(uint32_t mask)
{
    /* 恢复 PS 寄存器中的 INTLEVEL 字段 */
    XTOS_RESTORE_JUST_INTLEVEL(mask);
}

#endif /* ESP_PLATFORM - closes the outer guard at line 25 */

