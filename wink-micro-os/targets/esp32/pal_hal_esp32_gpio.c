/**
 * @file pal_hal_esp32_gpio.c
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
 *   声明在 pal_hal_esp32_internal.h；s_gpio_irq_in_flight[] 保持 file-scope。
 */
#include "pal_hal.h"
#include "pal_irq.h"        /* pal_irq_prio_t used by pal_gpio_enable_interrupt_ex */
#include "pal_osal.h"       /* pal_os_get_us() for pulse_in busy-wait + synchronize_all */
#include "pal_resource.h"
#include "pal_atomic_esp32.h"       /* target-private atomic + memory barrier helpers */
#include "pal_hal_esp32_internal.h" /* pal_esp32_gpio_synchronize_all() declaration */

#if defined(ESP_PLATFORM)
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "soc/gpio_struct.h"

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
        default:
            return WINK_ERR_INVALID_ARG;
    }

    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) { return WINK_ERR_HARDWARE; }
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
    if (prio >= PAL_IRQ_PRIO_COUNT) { return WINK_ERR_INVALID_ARG; }

    /* v2.1 G2：ESP32 GPIO ISR 路径同样拒接 REALTIME，避免与 pal_irq_enable 出现
     * "同一 prio 在两条路径上行为不一致"的语义陷阱（参考 ADR-0012 / ADR-IRQ-008）。 */
    if (prio == PAL_IRQ_PRIO_REALTIME) {
        return WINK_ERR_UNSUPPORTED;
    }

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

    /* v2.2 G3 prio → ESP_INTR_FLAG_LEVELn 映射
     * REALTIME 已在入口拒接，映射表不包含它。 */
    static const int s_gpio_prio_flag_map[PAL_IRQ_PRIO_COUNT] = {
        [PAL_IRQ_PRIO_LOWEST]  = ESP_INTR_FLAG_LEVEL1,
        [PAL_IRQ_PRIO_LOW]     = ESP_INTR_FLAG_LEVEL1,
        [PAL_IRQ_PRIO_NORMAL]  = ESP_INTR_FLAG_LEVEL2,
        [PAL_IRQ_PRIO_HIGH]    = ESP_INTR_FLAG_LEVEL3,
        [PAL_IRQ_PRIO_HIGHEST] = ESP_INTR_FLAG_LEVEL3,   /* RTOS 安全边界，见 ADR-IRQ-003 */
        /* PAL_IRQ_PRIO_REALTIME 在入口已拒接，此处不映射 */
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

/* ─────────────────────────────────────────────────────────
 * GPIO Pulse In（超声波硬件捕获 - 当前 busy-wait 回退）
 * ───────────────────────────────────────────────────────── */

wink_status_t pal_gpio_pulse_in(wink_pin_t pin, bool level,
                                  uint32_t timeout_us, uint32_t *pulse_us) {
    /* FIXME: MVP 阶段暂用 busy-wait（会阻塞 tick）。
     * Phase 4 目标：迁移至 RMT + GPIO 双沿 ISR + 硬件定时器实现非阻塞捕获。
     * 当前实现仅供 avoidance_car 示例跑通，实时性不达标。 */
    if (pulse_us == NULL || pin < 0 || pin >= GPIO_NUM_MAX) {
        return WINK_ERR_INVALID_ARG;
    }

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

#else /* !ESP_PLATFORM: non-IDF stub for static analysis. */

wink_status_t pal_gpio_init(wink_pin_t pin, pal_gpio_mode_t mode)
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

wink_status_t pal_gpio_pulse_in(wink_pin_t pin, bool level,
                                  uint32_t timeout_us, uint32_t *pulse_us)
{ (void)pin; (void)level; (void)timeout_us; (void)pulse_us;
  return WINK_ERR_UNSUPPORTED; }

#endif /* ESP_PLATFORM */
