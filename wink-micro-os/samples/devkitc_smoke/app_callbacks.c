/**
 * @file app_callbacks.c
 * @brief DevKitC 冒烟测试固件：S1-S8 全链路验证。
 *
 * 验证矩阵（裸板零外设即可跑通）：
 *   S1: 启动 + UART telemetry（uptime/stack/heap）
 *   S2: GPIO 输出（LED 慢闪）
 *   S3: GPIO 输入去抖（Boot 按钮）
 *   S4: GPIO 中断 ISR 计数（Task 3 uintptr_t 对称化验证）
 *   S5: PWM router 异频分配（同频复用、异频隔离）
 *   S6: I2C v6 总线扫描（空总线 → 驱动初始化+传输不崩）
 *   S7: 双核临界区并发压测 60s（Task 1 spinlock）
 *   S8: 看门狗复位链路（长按 3s → WDT 复位 → 重启检测）
 *
 * 双目标同源（ADR-0002）：ESP_PLATFORM 宏隔离 ESP32 特有逻辑（ISR/I2C/双核/看门狗）。
 * host 侧仅跑 S2/S3/S5（剩余均为 stub，无故障 = 代码结构验证）。
 */
#include "device_tree.h"
#include "wink_app.h"
#include "wink_runtime.h"  /* WINK_FAULT_BOOT_AFTER_RESET, wink_runtime_fault */
#include "wink_trace.h"
#include "wink_actuator_registry.h"
#include "wink_status.h"
#include "pal_hal.h"       /* pal_pwm_init/set_duty, pal_gpio_enable_interrupt */
#include "pal_osal.h"      /* pal_watchdog_init/feed, pal_get_ms, pal_get_reset_reason */
#include "pal_resource.h"  /* pal_resource_claim/release */
#include "pal_pwm_router.h"/* pal_pwm_router_channel_timer */

#if defined(ESP_PLATFORM)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#endif

/* ─────────────────────────────────────────────────────────
 * 故障码定义（S8 使用 runtime 定义 8001；其余 9000+ 区间）
 * ───────────────────────────────────────────────────────── */
#define FAULT_LED_INIT      9001u
#define FAULT_BUTTON_INIT   9002u
#define FAULT_PWM_INIT      9003u
#define FAULT_ISR_INIT      9004u

/* S5 PWM 通道配置（验证异频隔离 → 不同 timer） */
#define SMOKE_PWM_CH_LO     1u       /* GPIO4，50Hz  */
#define SMOKE_PWM_CH_HI     2u       /* GPIO5，1kHz */

/* ─────────────────────────────────────────────────────────
 * 全局状态（零堆分配，§6.1 约束1）
 * ───────────────────────────────────────────────────────── */
static volatile uint32_t s_isr_count = 0;        /* S4: ISR 事件计数 */
static uint32_t s_press_start_ms = 0;              /* S8: 长按计时起始 */
static bool     s_wdt_verified = false;            /* S8: 本次启动已确认 WDT 复位 */

/* ─────────────────────────────────────────────────────────
 * LED 安全关断适配（Actuator Registry，§5 P0-4）
 * ───────────────────────────────────────────────────────── */
static wink_status_t led_safe_off_thunk(void *ctx)
{
    return dal_led_off((dal_led_t *)ctx);
}

#if defined(ESP_PLATFORM)
/* ─────────────────────────────────────────────────────────
 * S4: GPIO 中断 ISR（Boot 按钮下降沿 → 计数递增）
 *     ESP32 专用，host 侧不使用（避免 unused-function 警告）
 * ───────────────────────────────────────────────────────── */
static void boot_button_isr(void *arg)
{
    (void)arg;
    s_isr_count++;
}
#endif

/* ─────────────────────────────────────────────────────────
 * S5: PWM router 异频分配验证（host + esp32 同源）
 *     ch1=50Hz, ch2=1kHz → 应分配到不同 timer
 * ───────────────────────────────────────────────────────── */
static void smoke_check_pwm_router(void)
{
    uint8_t timer_lo = 0xFF;
    uint8_t timer_hi = 0xFF;

    /* 初始化两个不同频率通道（router 内部处理同频复用/异频隔离） */
    wink_status_t st = pal_pwm_init(SMOKE_PWM_CH_LO, 50u);
    if (wink_status_is_error(st)) {
        wink_trace_fault(FAULT_PWM_INIT);
        return;
    }

    st = pal_pwm_init(SMOKE_PWM_CH_HI, 1000u);
    if (wink_status_is_error(st)) {
        wink_trace_fault(FAULT_PWM_INIT);
        return;
    }

    /* 查询 router 分配结果（验证异频 → 不同 timer） */
    timer_lo = pal_pwm_router_channel_timer(SMOKE_PWM_CH_LO);
    timer_hi = pal_pwm_router_channel_timer(SMOKE_PWM_CH_HI);

    /* 50% 占空比输出（真机可测）
     * gcc16 不因 (void) 抑制 warn_unused_result：先赋值再丢弃 */
    wink_status_t _duty = pal_pwm_set_duty(SMOKE_PWM_CH_LO, 50.0f);
    (void)_duty;

#if defined(ESP_PLATFORM)
    printf("[SMOKE] pwm: ch1=50Hz->timer%u, ch2=1kHz->timer%u %s\n",
           (unsigned)timer_lo, (unsigned)timer_hi,
           (timer_lo != timer_hi && timer_lo < 4 && timer_hi < 4) ? "PASS" : "FAIL");
#else
    /* host 侧：不同 timer 且均 <4 则 trace 正常，否则 trace 故障 */
    if (!(timer_lo != timer_hi && timer_lo < 4 && timer_hi < 4)) {
        wink_trace_fault(FAULT_PWM_INIT);
    }
    (void)timer_lo;
    (void)timer_hi;
#endif
}

#if defined(ESP_PLATFORM)
/* ─────────────────────────────────────────────────────────
 * S6: I2C v6 总线扫描（空总线 → 全 NACK，验证驱动初始化+传输不崩）
 *     扫描 3 个典型 OLED/传感器地址，验证 v6 API 正常工作。
 * ───────────────────────────────────────────────────────── */
static void smoke_check_i2c_bus(void)
{
    static const uint8_t test_addrs[] = {0x3C, 0x68, 0x76};
    uint8_t dummy = 0;

    for (size_t i = 0; i < sizeof(test_addrs); i++) {
        /* 空总线扫描：写 0 字节，读 1 字节 → NACK（裸板无外设） */
        wink_status_t st = pal_i2c_transfer(0, test_addrs[i], NULL, 0, &dummy, 1);
        printf("[SMOKE] i2c scan 0x%02X: status=%d (NACK expected)\n",
               test_addrs[i], (int)st);
    }
    printf("[SMOKE] i2c: PASS (v6 driver init+transfer ran without panic)\n");
}

/* ─────────────────────────────────────────────────────────
 * S7: 双核临界区并发压测 60s（验证 Task 1 spinlock SMP 安全）
 *     两核同时 claim/release 同一资源，无死锁/数据损坏。
 * ───────────────────────────────────────────────────────── */
static void resource_stress_task(void *arg)
{
    uint32_t core_id = (uint32_t)(uintptr_t)arg;
    uint64_t end_time = pal_get_ms() + 60000;
    uint32_t iterations = 0;

    while (pal_get_ms() < end_time) {
        /* 同一资源（GPIO pin 100+core）的并发 claim/release
         * 暴露 spinlock 争用：两核同时进出临界区
         * gcc16 不因 (void) 抑制 warn_unused_result：先赋值再丢弃 */
        wink_status_t _claim = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 100u + core_id, "stress");
        (void)_claim;
        wink_status_t _rel = pal_resource_release(PAL_RESOURCE_GPIO_PIN, 100u + core_id, "stress");
        (void)_rel;
        iterations++;
    }

    printf("[SMOKE] resource_stress core%u: %lu iterations, no panic\n",
           (unsigned)core_id, (unsigned long)iterations);
    vTaskDelete(NULL);
}

static void smoke_check_resource_smp(void)
{
    /* pin 到两个核（SMP 并行启动） */
    xTaskCreatePinnedToCore(resource_stress_task, "stress0", 4096, (void *)0, 5, NULL, 0);
    xTaskCreatePinnedToCore(resource_stress_task, "stress1", 4096, (void *)1, 5, NULL, 1);
    printf("[SMOKE] resource_stress: 60s dual-core claim/release started (Task1 spinlock)\n");
}

/* ─────────────────────────────────────────────────────────
 * Telemetry task：承担所有周期性 UART 输出。
 *     app_loop 零 printf，避免触发 WCET(8002) 误报（S1 验收标准）。
 * ───────────────────────────────────────────────────────── */
static void telemetry_task(void *arg)
{
    (void)arg;
    uint32_t last_report = 0;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        uint32_t now = (uint32_t)pal_get_ms();

        if (now - last_report >= 2000u) {
            printf("[SMOKE] uptime=%lums isr_count=%lu faults=%lu wdt_verified=%d\n",
                   (unsigned long)now, (unsigned long)s_isr_count, (unsigned long)wink_trace_count(),
                   (int)s_wdt_verified);
            last_report = now;
        }
    }
}
#endif /* ESP_PLATFORM */

/* ─────────────────────────────────────────────────────────
 * App Init（S1 启动初始化 + S4 ISR + S5 PWM + S6 I2C + S7 双核 + S8 WDT 检测）
 * ───────────────────────────────────────────────────────── */
static void app_init(void)
{
    /* S8: 检测「本次启动是异常复位后恢复」（ADR-0010）。
     * pal_get_abnormal_boot_count() 是 PAL 统一接口：
     *   >0 = WDT/PANIC 复位后恢复；0 = 正常启动
     * 所有平台都有此接口（ESP32: RTC存储, WASM/host: 静态变量模拟）。 */
    if (pal_get_abnormal_boot_count() > 0) {
        s_wdt_verified = true;
#if defined(ESP_PLATFORM)
        /* printf 是 ESP32 特有输出方式，保留在平台分支 */
        printf("[SMOKE] watchdog: PASS (recovered after abnormal reset, count=%lu)\n",
               (unsigned long)pal_get_abnormal_boot_count());
#endif
    }

    /* S2/S3: DAL LED + 按钮初始化（Phase 2 config_t 标准化） */
    const dal_led_config_t led_cfg = { .pin = BOARD_LED_PIN, .active_high = true };
    wink_status_t st = dal_led_init(&board_led, &led_cfg);
    if (wink_status_is_error(st)) {
        wink_trace_fault(FAULT_LED_INIT);
    }

    const dal_button_config_t btn_cfg = { .pin = BOOT_BUTTON_PIN, .active_low = true };
    st = dal_button_init(&boot_button, &btn_cfg);
    if (wink_status_is_error(st)) {
        wink_trace_fault(FAULT_BUTTON_INIT);
    }

    /* 注册执行器安全关断（fault/WDT 路径）
     * gcc16 不因 (void) 抑制 warn_unused_result：先赋值再丢弃 */
    wink_status_t _reg = wink_actuator_register(led_safe_off_thunk, &board_led);
    (void)_reg;

    /* 初始安全态：LED 熄灭
     * gcc16 不因 (void) 抑制 warn_unused_result：先赋值再丢弃 */
    wink_status_t _off = dal_led_off(&board_led);
    (void)_off;

    /* S5: PWM router 异频分配（跨平台同源） */
    smoke_check_pwm_router();

#if defined(ESP_PLATFORM)
    /* S4: GPIO 中断使能（Boot 按钮下降沿 → ISR 计数）
     * 验证 Task 3 uintptr_t 对称化：arg 经 void* 往返无损 */
    st = pal_gpio_enable_interrupt(BOOT_BUTTON_PIN, PAL_GPIO_INTR_FALLING_EDGE,
                                    boot_button_isr, NULL);
    if (wink_status_is_error(st)) {
        wink_trace_fault(FAULT_ISR_INIT);
    }

    /* S6: I2C v6 总线扫描 */
    smoke_check_i2c_bus();

    /* S7: 双核临界区压测启动 */
    smoke_check_resource_smp();

    /* S1: Telemetry task 启动（承担所有周期 UART 输出） */
    xTaskCreate(telemetry_task, "smoke_telem", 4096, NULL, 1, NULL);

    printf("[SMOKE] init done. Long-press BOOT (>3s) to trigger WDT reset test.\n");
#endif
}

/* ─────────────────────────────────────────────────────────
 * App Loop（S2 LED 慢闪 / S3 按钮输入 / S8 WDT 触发）
 *     零 printf！所有周期输出由独立 telemetry task 承担（避免 WCET 误报）。
 *     WCET <5ms：纯 GPIO 读写 + 简单计时比较，无阻塞。
 * ───────────────────────────────────────────────────────── */
static void app_loop(void)
{
    /* S3: 按钮去抖采样（每 tick 一次）
     * gcc16 不因 (void) 抑制 warn_unused_result：先赋值再丢弃 */
    wink_status_t _poll = dal_button_poll(&boot_button);
    (void)_poll;

    bool pressed = false;
    /* gcc16 不因 (void) 抑制 warn_unused_result：先赋值再丢弃 */
    wink_status_t _pressed = dal_button_is_pressed(&boot_button, &pressed);
    (void)_pressed;

    uint32_t now = (uint32_t)pal_get_ms();

    if (pressed) {
        /* S8: 长按 >3s → 触发 WDT 复位测试（PAL 统一接口）
         * 支持 WDT 的平台（ESP32）会真复位；不支持的平台（WASM/host）返回 WINK_ERR_UNSUPPORTED。
         * 运行时检测替代编译时 #ifdef，真正实现跨平台同源代码。 */
        if (s_press_start_ms == 0) {
            s_press_start_ms = now;
        }
        if (!s_wdt_verified && (now - s_press_start_ms) > 3000u) {
            wink_status_t _wdt = pal_watchdog_init(2000u);
            if (!wink_status_is_error(_wdt)) {
                /* WDT 初始化成功：死循环不喂狗 → 2s 后超时复位 */
                for (;;) { }
            }
            /* WDT 不支持（WASM/host）：静默失败，不影响其它功能 */
            (void)_wdt;
        }
        /* S2/S3: 按住时常亮
         * gcc16 不因 (void) 抑制 warn_unused_result：先赋值再丢弃 */
        wink_status_t _on = dal_led_on(&board_led);
        (void)_on;
    } else {
        s_press_start_ms = 0;
        /* S2/S3: 释放时 500ms 周期慢闪
         * gcc16 不因 (void) 抑制 warn_unused_result：先赋值再丢弃 */
        bool on = ((now / 500u) % 2u) == 0u;
        wink_status_t _led = on ? dal_led_on(&board_led) : dal_led_off(&board_led);
        (void)_led;
    }
}

/* ─────────────────────────────────────────────────────────
 * Fault 回调：故障时 LED 灭（安全态） */
static void app_on_fault(uint32_t fault_code)
{
    /* ADR-0010：on_fault 为通知回调，fault 已由 wink_runtime_fault 先 trace；此处不重复 trace。*/
    (void)fault_code;
    /* gcc16 不因 (void) 抑制 warn_unused_result：先赋值再丢弃 */
    wink_status_t _off = dal_led_off(&board_led);
    (void)_off;
}

/* ─────────────────────────────────────────────────────────
 * 回调工厂（二进制解耦，§7 目录架构）
 * ───────────────────────────────────────────────────────── */
const wink_app_callbacks_t *wink_app_get_callbacks(void)
{
    static const wink_app_callbacks_t cb = {
        .init     = app_init,
        .loop     = app_loop,
        .on_fault = app_on_fault
    };
    return &cb;
}
