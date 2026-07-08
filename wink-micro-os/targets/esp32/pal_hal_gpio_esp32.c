/**
 * @file pal_hal_gpio_esp32.c
 * @brief ESP32 target 的 pal_hal.h GPIO 子集实现：init/read/write/
 *        enable_interrupt_ex/disable_interrupt/pulse_in + GPIO ISR wrapper.
 *
 * 由 targets/esp32/pal_hal_esp32.c 拆出（PLAN-20260701-PAL-TARGET-P1-MAINT Task 2）。
 * 契约不变：仅物理位置调整；见 pal/include/pal_hal.h。
 *
 * ✅ R-4：全文件仅 1 处最外层 `#if defined(ESP_PLATFORM)`，包住所有 IDF-私有
 *   头文件与实现体；非 ESP-IDF 编译走 else 分支的 stub。
 *
 * ⚠️ 跨 TU：pal_esp32_gpio_synchronize_all() 是本 TU 唯一暴露的 target-private
 *   跨 TU 函数（供 pal_irq_esp32.c 的 pal_irq_synchronize(~0U) 调用），
 *   声明在 pal_hal_internal_esp32.h；s_gpio_irq_in_flight[] 保持 file-scope。
 */
#include "pal_hal.h"
#include "pal_irq.h"        /* pal_irq_prio_t used by pal_gpio_enable_interrupt_ex */
#include "pal_osal.h"       /* pal_os_get_us() for pulse_in busy-wait + synchronize_all */
#include "pal_resource.h"
#include "pal_atomic_esp32.h"       /* target-private atomic + memory barrier helpers */
#include "pal_hal_internal_esp32.h" /* pal_esp32_gpio_synchronize_all() declaration */
#include "hal/pal_rmt.h"

/* ADR-0017 层 1 例外：本 TU 是 pal_gpio_pulse_in 的 target 实现，其内部合法调用
 * WINK_BLOCKING 的 pal_rmt_pulse_capture_wait（RMT 后端）与 pal_os_busy_wait_us
 * （busy-wait 回退）。抑制 -Wdeprecated-declarations 使 -Werror 下仍能编译；
 * 严格模式（-DWINK_STRICT_NONBLOCKING=1）下相关 API 声明消失，本 TU 会链接失败——那是设计意图。 */
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#if defined(ESP_PLATFORM)
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "soc/gpio_struct.h"
#include "esp_rom_gpio.h"
#include "esp_idf_version.h"
#include "soc/gpio_sig_map.h"
#include "soc/io_mux_reg.h"
#include "soc/gpio_periph.h"

#ifndef SIG_GPIO_OUT_IDX
#define SIG_GPIO_OUT_IDX 256
#endif

_Static_assert((gpio_num_t)GPIO_NUM_NC == -1,
    "GPIO_NUM_NC must be -1 for wink_pin_t sign-compatibility");

/* ─────────────────────────────────────────────────────────
 * SMP 安全的 GPIO 分发表 + ISR wrapper（ADR-IRQ-002 / ADR-IRQ-004）
 *
 * 竞态场景修复：
 *   Core 0 正在执行 gpio_isr_wrapper，刚读取 s_gpio_isr[pin]
 *   此时 Core 1 调用 pal_gpio_disable_interrupt，将 s_gpio_isr_arg 置空
 *   Core 0 后续读取到 NULL arg，导致空指针解引用崩溃
 *
 * 解决方案：所有读写分发表的路径都必须持有 s_gpio_table_mux 自旋锁。
 * ISR 上下文使用 portENTER_CRITICAL_ISR()。
 * ───────────────────────────────────────────────────────── */

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

/* v2.2 G3（Phase 1.5，2026-07-01）：GPIO ISR service 的首次锁定状态。
 * 由 s_gpio_table_mux 同步。一旦 initialized=true，进程生命周期内不再释放
 * （见 Phase 1.5 §1.2：拒绝 disable→uninstall 方案以规避 TOCTOU / SMP UAF）。 */
static bool           s_gpio_service_initialized = false;
static pal_irq_prio_t s_gpio_service_prio        = PAL_IRQ_PRIO_NORMAL;

/* SMP ISR 同步 count：每个 pin 一份，由 gpio_isr_wrapper 在入/出口原子递增/递减，
 * 由 pal_esp32_gpio_synchronize_all() 供 pal_irq_esp32.c 的
 * pal_irq_synchronize(~0U) 全量等待。 */
static volatile uint32_t s_gpio_irq_in_flight[GPIO_NUM_MAX] = {0};

/* Track the last mode configured via pal_gpio_init for each pin. We use this
 * (instead of an IDF API) to decide whether pal_gpio_enable_interrupt_ex needs
 * to promote a pure OUTPUT pin to INPUT_OUTPUT — IDF has no public gpio_get_direction
 * on all supported versions, and pins configured by other peripherals (LEDC/RMT)
 * before our PAL are treated conservatively (we do NOT auto-promote those).
 * s_gpio_mode_known[pin] is true iff pal_gpio_init has configured this pin.
 * When false we cannot infer the electrical direction and leave the pin alone. */
static pal_gpio_mode_t s_gpio_mode[GPIO_NUM_MAX];
static bool            s_gpio_mode_known[GPIO_NUM_MAX];

void pal_esp32_gpio_synchronize_all(uint64_t timeout_us)
{
    for (uint32_t i = 0; i < GPIO_NUM_MAX; i++) {
        uint64_t start = pal_os_get_us();
        while (Atomic_Load_u32(&s_gpio_irq_in_flight[i]) > 0) {
            if (pal_os_get_us() - start > timeout_us) {
                ESP_LOGE("pal_irq", "synchronize timeout on gpio=%lu",
                         (unsigned long)i);
                break;
            }
        }
    }
}

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

/* ─────────────────────────────────────────────────────────
 * GPIO 基础接口实现
 * ───────────────────────────────────────────────────────── */

wink_status_t pal_gpio_init(wink_pin_t pin, pal_gpio_mode_t mode) {
    if (pin < 0 || pin >= GPIO_NUM_MAX) { return WINK_ERR_INVALID_ARG; }

    /* Track A（M1）：DAL 是资源占用 SSOT，PAL 层不再自 claim GPIO 引脚。 */

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
        case PAL_GPIO_INPUT_OUTPUT:
            /* Bidirectional: driver + input buffer both enabled.
             * Callers use this when the same pin must be both driven by
             * software and observed (e.g. mock echo pulse feeds RMT capture
             * on the same GPIO). */
            cfg.mode = GPIO_MODE_INPUT_OUTPUT;
            break;
        default:
            return WINK_ERR_INVALID_ARG;
    }

    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) { return WINK_ERR_HARDWARE; }
    /* Record mode so pal_gpio_enable_interrupt_ex can decide whether to
     * promote OUTPUT pins to INPUT_OUTPUT (see critical pin-driver caution
     * in that function). */
    s_gpio_mode[pin] = mode;
    s_gpio_mode_known[pin] = true;
    return WINK_OK;
}

void pal_gpio_reset_pin(wink_pin_t pin) {
    /* ADR-0024 §4 checkitem 2: deinit must (a) disable interrupt on the pin,
     * (b) revert to Hi-Z INPUT, (c) release esp_gpio_reserve bitmap so the
     * next init doesn't fail with "gpio already reserved".
     * gpio_reset_pin() does all three (it calls gpio_isr_handler_remove()
     * only if the ISR service is installed; it then calls gpio_config to
     * INPUT+pull-disabled; it clears the reservation bit).
     * Callers that already called pal_gpio_disable_interrupt +
     * pal_gpio_synchronize_interrupt beforehand are fine —
     * gpio_reset_pin is idempotent on an unreserved pin. */
    if (pin < 0 || pin >= GPIO_NUM_MAX || !GPIO_IS_VALID_GPIO((gpio_num_t)pin)) {
        return;
    }
    gpio_reset_pin((gpio_num_t)pin);
    /* Invalidate our cached mode so pal_gpio_set_direction correctly refuses
     * direction swaps on an unconfigured pin (matches "after deinit we no
     * longer know the state" invariant). */
    s_gpio_mode_known[pin] = false;

    /* Also tear down our ISR dispatch entry if we had one — gpio_reset_pin
     * only uninstalls the GPIO per-pin ISR via gpio_isr_handler_remove,
     * it doesn't touch our s_gpio_isr/s_gpio_isr_arg cache. */
    portENTER_CRITICAL(&s_gpio_table_mux);
    s_gpio_isr[pin] = NULL;
    s_gpio_isr_arg[pin] = NULL;
    portEXIT_CRITICAL(&s_gpio_table_mux);
}

wink_status_t pal_gpio_set_direction(wink_pin_t pin, pal_gpio_mode_t mode) {
    if (pin < 0 || pin >= GPIO_NUM_MAX || !GPIO_IS_VALID_GPIO(pin)) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!s_gpio_mode_known[pin]) {
        /* Pin was never pal_gpio_init'd; refuse to set direction on an
         * unconfigured pin to avoid silently changing a pin owned by
         * another peripheral (LEDC/RMT/etc.). */
        return WINK_ERR_INVALID_STATE;
    }

    gpio_mode_t idf_mode;
    switch (mode) {
        case PAL_GPIO_INPUT:             idf_mode = GPIO_MODE_INPUT;    break;
        case PAL_GPIO_INPUT_PULLUP:      idf_mode = GPIO_MODE_INPUT;    break;
        case PAL_GPIO_INPUT_PULLDOWN:    idf_mode = GPIO_MODE_INPUT;    break;
        case PAL_GPIO_OUTPUT_PUSH_PULL:  idf_mode = GPIO_MODE_OUTPUT;   break;
        case PAL_GPIO_OUTPUT_OPEN_DRAIN: idf_mode = GPIO_MODE_OUTPUT_OD;break;
        case PAL_GPIO_INPUT_OUTPUT:      idf_mode = GPIO_MODE_INPUT_OUTPUT; break;
        default: return WINK_ERR_INVALID_ARG;
    }

    esp_err_t err = gpio_set_direction((gpio_num_t)pin, idf_mode);
    if (err == ESP_ERR_INVALID_ARG) { return WINK_ERR_INVALID_ARG; }
    if (err != ESP_OK) { return WINK_ERR_HARDWARE; }

    /* Update pull-up/down only if caller explicitly requested a pull mode,
     * since set_direction alone does not touch pull config. */
    if (mode == PAL_GPIO_INPUT_PULLUP) {
        (void)gpio_pullup_en((gpio_num_t)pin);
        (void)gpio_pulldown_dis((gpio_num_t)pin);
    } else if (mode == PAL_GPIO_INPUT_PULLDOWN) {
        (void)gpio_pullup_dis((gpio_num_t)pin);
        (void)gpio_pulldown_en((gpio_num_t)pin);
    }

    s_gpio_mode[pin] = mode;
    return WINK_OK;
}

wink_status_t pal_gpio_write(wink_pin_t pin, bool level) {
    if (pin < 0 || pin >= GPIO_NUM_MAX || !GPIO_IS_VALID_GPIO(pin)) {
        return WINK_ERR_INVALID_ARG;
    }
    esp_err_t err = gpio_set_level((gpio_num_t)pin, level ? 1 : 0);
    if (err == ESP_ERR_INVALID_ARG) { return WINK_ERR_INVALID_ARG; }
    if (err != ESP_OK) { return WINK_ERR_IO; }
    return WINK_OK;
}

wink_status_t pal_gpio_read(wink_pin_t pin, bool *out_level) {
    if (out_level == NULL) { return WINK_ERR_INVALID_ARG; }
    *out_level = false;
    if (pin < 0 || pin >= GPIO_NUM_MAX || !GPIO_IS_VALID_GPIO(pin)) {
        return WINK_ERR_INVALID_ARG;
    }
    int val = gpio_get_level((gpio_num_t)pin);
    if (val < 0) { return WINK_ERR_IO; }
    *out_level = (val != 0);
    return WINK_OK;
}

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
    /* ADR-0018：优先级枚举收窄到 LOW/NORMAL/HIGH（1..3）。
     * P1-P5-9: 使用命名常量边界，与 pal_irq_esp32.c / pal_hal_host.c / pal_irq_wasm.c
     * 的 prio 入参校验保持字节一致。 */
    if (prio < PAL_IRQ_PRIO_LOW || prio > PAL_IRQ_PRIO_HIGH) { return WINK_ERR_INVALID_ARG; }

    /* v2.2 G3（Phase 1.5，2026-07-01）：GPIO service 首次锁定 prio。
     * 若已锁定，后续 prio 必须一致；不一致返回 WINK_ERR_INVALID_ARG。
     * gpio_install_isr_service 仅在首次安装时决定硬件优先级，无法为每个 pin
     * 单独指定，因此 prio 只能在首次注册时锁定一次。 */
    portENTER_CRITICAL(&s_gpio_table_mux);
    if (s_gpio_service_initialized) {
        if (prio != s_gpio_service_prio) {
            portEXIT_CRITICAL(&s_gpio_table_mux);
            return WINK_ERR_INVALID_ARG;   /* G3: prio 冲突，本次拒接 */
        }
    }
    portEXIT_CRITICAL(&s_gpio_table_mux);

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

    /* ADR-0018 prio → ESP_INTR_FLAG_LEVELn 映射（3 级收窄版）。
     * LOW/NORMAL/HIGH 均位于 configMAX_SYSCALL_INTERRUPT_PRIORITY 内，
     * 全部 RTOS 安全，可调 xxxFromISR API。 */
    static const int s_gpio_prio_flag_map[PAL_IRQ_PRIO_COUNT] = {
        [PAL_IRQ_PRIO_LOW]     = ESP_INTR_FLAG_LEVEL1,
        [PAL_IRQ_PRIO_NORMAL]  = ESP_INTR_FLAG_LEVEL2,
        [PAL_IRQ_PRIO_HIGH]    = ESP_INTR_FLAG_LEVEL3,
    };

    /* 首次安装 ISR service：加上 IRAM 属性使 wrapper 在 Flash cache 禁用时仍可运行
     * （项目的 gpio_isr_wrapper 已 IRAM_ATTR，配合此 flag 满足 IDF 要求）。 */
    if (!s_gpio_service_initialized) {
        int intr_flags = s_gpio_prio_flag_map[prio] | ESP_INTR_FLAG_IRAM;
        esp_err_t err = gpio_install_isr_service(intr_flags);
        if (err == ESP_OK) {
            portENTER_CRITICAL(&s_gpio_table_mux);
            s_gpio_service_prio        = prio;
            s_gpio_service_initialized = true;
            portEXIT_CRITICAL(&s_gpio_table_mux);
        } else if (err == ESP_ERR_INVALID_STATE) {
            /* Service 已被其它路径（如 IDF 内部或第三方库）安装过。
             * 我们无从得知对方 flag，硬件优先级不由我们控制；但为了保证
             * API 层契约一致（后续 mismatched prio 仍应拒接），仍锁定跟踪状态。 */
            ESP_LOGI("pal_hal", "GPIO ISR service already installed externally; "
                                "locking pal tracker to prio=%d", (int)prio);
            portENTER_CRITICAL(&s_gpio_table_mux);
            s_gpio_service_prio        = prio;
            s_gpio_service_initialized = true;
            portEXIT_CRITICAL(&s_gpio_table_mux);
        } else {
            return WINK_ERR_HARDWARE;
        }
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

    /* Ensure interrupts can fire on pins configured as pure OUTPUT (where
     * the input buffer is disabled at reset and edge detection cannot
     * observe the pin's own drive or any external signal). Only promote
     * pins we ourselves configured as OUTPUT or OUTPUT_OD via pal_gpio_init;
     * leave pins we never configured (or configured as INPUT-family) alone.
     *
     * Unconditionally setting INPUT_OUTPUT on an INPUT pull-up pin (e.g.
     * the BOOT button on GPIO0) would connect the output driver with OUT
     * register = 0, fighting the external pull-up and causing a stuck-low
     * pad, spurious falling edges, and an WDT reset loop.
     *
     * Pins owned by other peripherals (LEDC/RMT) will show as "unknown"
     * because they don't go through pal_gpio_init; we leave them alone —
     * those peripherals manage their own GPIO routing.
     *
     * Callers needing self-edge visibility on a software-driven pin can
     * pal_gpio_init(pin, PAL_GPIO_INPUT_OUTPUT) before enabling interrupts
     * (as S10 does for TRIG/ECHO); this promotion is a safety net for
     * plain-OUTPUT callers. */
    if (s_gpio_mode_known[pin]) {
        if (s_gpio_mode[pin] == PAL_GPIO_OUTPUT_PUSH_PULL) {
            (void)gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT_OUTPUT);
            s_gpio_mode[pin] = PAL_GPIO_INPUT_OUTPUT;
        } else if (s_gpio_mode[pin] == PAL_GPIO_OUTPUT_OPEN_DRAIN) {
            (void)gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT_OUTPUT_OD);
            s_gpio_mode[pin] = PAL_GPIO_INPUT_OUTPUT;  /* conservative; no OD variant in pal_gpio_mode_t */
        }
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
    return WINK_OK;
}

wink_status_t pal_gpio_disable_interrupt(wink_pin_t pin) {
    if (pin < 0 || pin >= GPIO_NUM_MAX) { return WINK_ERR_INVALID_ARG; }

    esp_err_t err = gpio_set_intr_type((gpio_num_t)pin, GPIO_INTR_DISABLE);
    if (err != ESP_OK) { return WINK_ERR_HARDWARE; }

    (void)gpio_isr_handler_remove((gpio_num_t)pin);

    /* ✅ SMP 安全：持有自旋锁清空分发表
     * 必须在 remove handler 之后清空，防止竞态条件 */
    portENTER_CRITICAL(&s_gpio_table_mux);
    s_gpio_isr[pin] = NULL;
    s_gpio_isr_arg[pin] = NULL;
    portEXIT_CRITICAL(&s_gpio_table_mux);
    return WINK_OK;
}

/* P1-P5-10: 对外暴露的 GPIO ISR 同步原语——忙等待指定 pin 的 in-flight
 * ISR 计数归零，供调用方在 disable 后安全释放 ISR 使用的资源。
 * 内部复用 pal_esp32_gpio_synchronize_all() 的等待循环（超时相同）。 */
#define PAL_GPIO_SYNC_TIMEOUT_US 100000ULL  /* 100ms，与 pal_irq_esp32.c 保持一致 */

wink_status_t pal_gpio_synchronize_interrupt(wink_pin_t pin) {
    if (pin < 0 || pin >= GPIO_NUM_MAX) { return WINK_ERR_INVALID_ARG; }

    uint64_t start = pal_os_get_us();
    while (Atomic_Load_u32(&s_gpio_irq_in_flight[pin]) > 0) {
        if (pal_os_get_us() - start > PAL_GPIO_SYNC_TIMEOUT_US) {
            ESP_LOGE("pal_hal", "synchronize_interrupt timeout on gpio=%d",
                     (int)pin);
            break;
        }
    }
    /* 确保后续内存释放不会被编译器重排到等待之前 */
    esp_memory_barrier();
    return WINK_OK;
}

/* ─────────────────────────────────────────────────────────
 * GPIO Pulse In（超声波硬件捕获 - 当前 busy-wait 回退）
 * ───────────────────────────────────────────────────────── */

static wink_pin_t s_rmt_echo_pin_cache = -1;   /* 缓存当前 RMT 绑定 pin（真源在 pal_rmt_esp32.c 的 s_capture_pin，此缓存仅作同-pin 快速判断） */

/* After rmt_new_rx_channel() (called from pal_rmt_pulse_capture_init on a fresh
 * or re-built channel), ESP-IDF internally calls gpio_set_direction(pin,
 * GPIO_MODE_INPUT), which clears the output-enable bit previously set by
 * pal_test_enable_hardware_loopback(). If the pin is supposed to be in
 * INPUT_OUTPUT loopback mode (e.g. sim_echo driving ECHO high while RMT
 * listens), we must re-apply the output driver + GPIO-out signal routing
 * BEFORE rmt_receive() arms the receiver -- otherwise gpio_set_level() in the
 * sim task cannot drive the pad, RMT sees no edge, and every capture times out.
 *
 * Only pins that pal_test_enable_hardware_loopback() previously configured
 * (recorded in s_gpio_mode[]) get restored; plain INPUT pins are left alone. */
static void pal_restore_loopback_direction_if_needed(wink_pin_t pin) {
    if (pin < 0 || pin >= GPIO_NUM_MAX) { return; }
    if (!s_gpio_mode_known[pin]) { return; }
    if (s_gpio_mode[pin] != PAL_GPIO_INPUT_OUTPUT) { return; }
    /* Full re-apply via gpio_config() (MUST match pal_test_enable_hardware_loopback
     * same-pin path exactly):
     *   - gpio_set_direction() alone only toggles OE/IE bits but does NOT reset
     *     the IOMUX FUNC_SEL mux. rmt_new_rx_channel() / rmt_del_channel() on
     *     IDF v6 can leave the pin muxed to the RMT peripheral, in which case
     *     GPIO-out writes never reach the pad and RMT sees no edges → num_sym=0.
     *   - gpio_config(mode=INPUT_OUTPUT, intr=DISABLED, pulls=off) implicitly
     *     re-selects GPIO as the pin function, clears leftover interrupt state,
     *     and enables both driver and input buffer — the same post-state as
     *     pal_test_enable_hardware_loopback left the pin in before RMT was
     *     torn down.
     *   - Then belt-and-suspenders: force output mux back to GPIO peripheral
     *     (some IDF versions touch OUT-side routing on rmt channel delete)
     *     and re-enable pad input buffer.
     */
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    (void)gpio_config(&cfg);
    esp_rom_gpio_connect_out_signal((gpio_num_t)pin, SIG_GPIO_OUT_IDX, false, false);
    PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[pin]);
    /* Diagnostic: log first 3 restores per pin — verify pad reads back low and
     * that a forced HIGH write is observable on the input buffer (proves the
     * loopback path is intact after RMT re-init, including post-S9 cold-start). */
    static uint8_t s_restore_log_count[GPIO_NUM_MAX];
    if (s_restore_log_count[pin] < 3) {
        s_restore_log_count[pin]++;
        int lvl_before = gpio_get_level((gpio_num_t)pin);
        (void)gpio_set_level((gpio_num_t)pin, 1);
        int lvl_high = gpio_get_level((gpio_num_t)pin);
        (void)gpio_set_level((gpio_num_t)pin, 0);
        int lvl_after = gpio_get_level((gpio_num_t)pin);
        esp_rom_printf("[pal_gpio] restore loopback pin=%d (#%u) levels: before=%d hi=%d after=%d\n",
                       (int)pin, (unsigned)s_restore_log_count[pin],
                       lvl_before, lvl_high, lvl_after);
    }
}

static wink_status_t pal_gpio_pulse_in_busy_wait(wink_pin_t pin, bool level,
                                                 uint32_t timeout_us, uint32_t *pulse_us) {
    if (pulse_us == NULL || pin < 0 || pin >= GPIO_NUM_MAX) {
        return WINK_ERR_INVALID_ARG;
    }
    *pulse_us = 0;

    uint64_t start = pal_os_get_us();
    bool current_val = false;

    while (1) {
        wink_status_t st = pal_gpio_read(pin, &current_val);
        if (wink_status_is_error(st)) {
            return WINK_ERR_IO;
        }
        if (current_val == level) {
            break;
        }
        if (pal_os_get_us() - start > timeout_us) {
            return WINK_ERR_TIMEOUT;
        }
    }

    uint64_t pulse_start = pal_os_get_us();
    while (1) {
        wink_status_t st = pal_gpio_read(pin, &current_val);
        if (wink_status_is_error(st)) {
            return WINK_ERR_IO;
        }
        if (current_val != level) {
            break;
        }
        if (pal_os_get_us() - start > timeout_us) {
            return WINK_ERR_TIMEOUT;
        }
    }

    *pulse_us = (uint32_t)(pal_os_get_us() - pulse_start);
    return WINK_OK;
}

wink_status_t pal_gpio_pulse_in(wink_pin_t pin, bool level,
                                uint32_t timeout_us, uint32_t *pulse_us) {
    if (pulse_us == NULL || pin < 0 || pin >= GPIO_NUM_MAX) {
        return WINK_ERR_INVALID_ARG;
    }
    *pulse_us = 0;

    /* SSOT：RMT 初始化状态由 pal_rmt_pulse_capture_is_active 报告。
     * pal_rmt_pulse_capture_init 内部已处理：同 pin 幂等返 OK / 不同 pin 先 deinit 再重建。
     * 失败路径（init 返错误）：is_active 返回 false，自动降级到 busy-wait。*/
    bool rmt_ready = pal_rmt_pulse_capture_is_active();
    if (!rmt_ready || s_rmt_echo_pin_cache != pin) {
        /* HC-SR04 语义：ECHO 高电平脉宽 → RISING 起。 */
        if (pal_rmt_pulse_capture_init(pin, PAL_RMT_EDGE_RISING) == WINK_OK) {
            rmt_ready = true;
            s_rmt_echo_pin_cache = pin;
            /* pal_rmt_pulse_capture_init calls rmt_new_rx_channel() on fresh/
             * re-built channels; IDF internally gpio_set_direction(pin, INPUT)
             * which would kill the output driver on a loopback pin. Restore
             * INPUT_OUTPUT direction if pal_test_enable_hardware_loopback()
             * previously set this pin up for self-loop. */
            pal_restore_loopback_direction_if_needed(pin);
        } else {
            rmt_ready = false;
            s_rmt_echo_pin_cache = -1;
        }
    }

    /* 3. RMT 可用就走 RMT 测量；否则降级为 busy-wait */
    if (rmt_ready && s_rmt_echo_pin_cache == pin) {
        return pal_rmt_pulse_capture_wait(timeout_us, pulse_us);
    }

    return pal_gpio_pulse_in_busy_wait(pin, level, timeout_us, pulse_us);
}

wink_status_t pal_test_enable_hardware_loopback(wink_pin_t pin_out, wink_pin_t pin_in) {
    if (pin_out < 0 || pin_out >= GPIO_NUM_MAX || !GPIO_IS_VALID_GPIO(pin_out) ||
        pin_in < 0 || pin_in >= GPIO_NUM_MAX || !GPIO_IS_VALID_GPIO(pin_in)) {
        return WINK_ERR_INVALID_ARG;
    }

    if (pin_out == pin_in) {
        /* Same pin: route pin to INPUT_OUTPUT + GPIO-out so input buffer sees
         * own drive (software self-loop). The previous PIN_INPUT_ENABLE-only
         * one-liner enabled the pad input buffer but did NOT feed the
         * internally-driven output back to the input channel, so RMT /
         * gpio_get_level on the same pin could not observe the chip's own
         * transitions (breaks S9 self-loop test). We now:
         *   (1) reconfigure the pin as INPUT_OUTPUT via gpio_config — this
         *       enables both output driver and input buffer at once, and in
         *       this mode gpio_get_level(pin) reads back the value the chip
         *       itself is driving.
         *   (2) force the output mux back to the GPIO peripheral signal
         *       (SIG_GPIO_OUT_IDX). Idempotent if pin is already in GPIO-out
         *       mode; forces it back if held by LEDC/UART/etc. — which is
         *       what the caller wants for a loopback test.
         *   (3) keep PIN_INPUT_ENABLE as belt-and-suspenders (redundant with
         *       INPUT_OUTPUT mode but harmless).
         * Pullup/pulldown left disabled: caller manages pull state. */
        gpio_config_t cfg_self = {
            .pin_bit_mask = (1ULL << pin_out),
            .mode = GPIO_MODE_INPUT_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        esp_err_t err = gpio_config(&cfg_self);
        if (err != ESP_OK) { return WINK_ERR_HARDWARE; }
        esp_rom_gpio_connect_out_signal((gpio_num_t)pin_out, SIG_GPIO_OUT_IDX, false, false);
        PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[pin_out]);
        /* Record in our PAL mode cache so later code (e.g. pal_gpio_pulse_in
         * after a lazy RMT re-init) can detect that this pin is in loopback
         * mode and needs direction restored. Pin_in==pin_out for self-loop. */
        s_gpio_mode[pin_out] = PAL_GPIO_INPUT_OUTPUT;
        s_gpio_mode_known[pin_out] = true;
        return WINK_OK;
    }

    /* Different-pin cross-route: caller must ensure pin_out is in GPIO mode
     * (not LEDC/UART/etc.) so the signal we sample from func_out_sel_cfg is
     * a usable GPIO-out signal for pin_in to pick up. */
    gpio_config_t config_in = {
        .pin_bit_mask = (1ULL << pin_in),
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    esp_err_t err = gpio_config(&config_in);
    if (err != ESP_OK) { return WINK_ERR_HARDWARE; }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    uint32_t sig = GPIO.func_out_sel_cfg[pin_out].func_sel;
#else
    uint32_t sig = GPIO.func_out[pin_out].func;
#endif
    esp_rom_gpio_connect_out_signal((gpio_num_t)pin_in, sig, false, false);
    return WINK_OK;
}

wink_status_t pal_test_disable_hardware_loopback(wink_pin_t pin_out, wink_pin_t pin_in) {
    if (pin_out < 0 || pin_out >= GPIO_NUM_MAX || !GPIO_IS_VALID_GPIO(pin_out) ||
        pin_in < 0 || pin_in >= GPIO_NUM_MAX || !GPIO_IS_VALID_GPIO(pin_in)) {
        return WINK_ERR_INVALID_ARG;
    }

    if (pin_out == pin_in) {
        /* Restore pin to plain INPUT; callers must reconfigure direction
         * after disable if a different mode is required. We do NOT restore
         * to GPIO_MODE_OUTPUT because we cannot know whether the pin was
         * output or input before enable — INPUT is the safer default
         * (avoids accidentally driving a line that something else now owns).
         * Re-routing the output mux back to SIG_GPIO_OUT_IDX is idempotent
         * and provides a safe known state. */
        PIN_INPUT_DISABLE(GPIO_PIN_MUX_REG[pin_out]);
        gpio_config_t cfg_self = {
            .pin_bit_mask = (1ULL << pin_out),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        (void)gpio_config(&cfg_self);
        esp_rom_gpio_connect_out_signal((gpio_num_t)pin_out, SIG_GPIO_OUT_IDX, false, false);
        return WINK_OK;
    }

    esp_rom_gpio_connect_out_signal((gpio_num_t)pin_in, SIG_GPIO_OUT_IDX, false, false);
    gpio_config_t config_in = {
        .pin_bit_mask = (1ULL << pin_in),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    (void)gpio_config(&config_in);
    return WINK_OK;
}

#else /* !ESP_PLATFORM: non-IDF stub for static analysis. */

wink_status_t pal_gpio_init(wink_pin_t pin, pal_gpio_mode_t mode)
{ (void)pin; (void)mode; return WINK_ERR_UNSUPPORTED; }

void pal_gpio_reset_pin(wink_pin_t pin) { (void)pin; }

wink_status_t pal_gpio_set_direction(wink_pin_t pin, pal_gpio_mode_t mode)
{ (void)pin; (void)mode; return WINK_ERR_UNSUPPORTED; }

wink_status_t pal_gpio_write(wink_pin_t pin, bool level) { (void)pin; (void)level; return WINK_ERR_UNSUPPORTED; }
wink_status_t pal_gpio_read(wink_pin_t pin, bool *out_level) {
    if (out_level != NULL) { *out_level = false; }
    (void)pin;
    return WINK_ERR_UNSUPPORTED;
}

wink_status_t pal_gpio_enable_interrupt_ex(wink_pin_t pin,
                                            pal_gpio_intr_t intr_type,
                                            pal_irq_prio_t prio,
                                            pal_gpio_isr_t callback,
                                            void *arg)
{ (void)pin; (void)intr_type; (void)prio; (void)callback; (void)arg;
  return WINK_ERR_UNSUPPORTED; }

wink_status_t pal_gpio_disable_interrupt(wink_pin_t pin)
{ (void)pin; return WINK_ERR_UNSUPPORTED; }

wink_status_t pal_gpio_synchronize_interrupt(wink_pin_t pin)
{ (void)pin; return WINK_ERR_UNSUPPORTED; }

wink_status_t pal_gpio_pulse_in(wink_pin_t pin, bool level,
                                  uint32_t timeout_us, uint32_t *pulse_us)
{ (void)pin; (void)level; (void)timeout_us; (void)pulse_us;
  return WINK_ERR_UNSUPPORTED; }

wink_status_t pal_test_enable_hardware_loopback(wink_pin_t pin_out, wink_pin_t pin_in)
{ (void)pin_out; (void)pin_in; return WINK_ERR_UNSUPPORTED; }

wink_status_t pal_test_disable_hardware_loopback(wink_pin_t pin_out, wink_pin_t pin_in)
{ (void)pin_out; (void)pin_in; return WINK_ERR_UNSUPPORTED; }

#endif /* ESP_PLATFORM */
