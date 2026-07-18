#include "dal_ultrasonic.h"

#include "pal_hal.h"
#include "pal_osal.h"
#include "pal_resource.h"
#include "hal/pal_rmt.h"     /* pal_rmt_pulse_capture_init eager warm-up (see init below) */
#include "wink_pt_debug.h"   /* WINK_ASSERT_NONBLOCKING() (ADR-0017 层 3 runtime hook) */

#include <string.h>   /* memcpy（ADR-0008 apply_override 反序列化） */

/* ADR-0017 层 1 例外：本 TU 合法调用多个 WINK_BLOCKING API
 * (pal_os_busy_wait_us, pal_gpio_pulse_in) 以及 blocking 的 dal_ultrasonic_read 自身。
 * 超声波驱动本质需要精确时序 + 脉宽等待。严格模式下 dal_ultrasonic_read 与 pal 阻塞 API
 * 声明全部消失，本文件受同一 #ifndef WINK_STRICT_NONBLOCKING 包围的部分随之消失。 */
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

/* RMT RX backend requires pulse + idle_thres(25ms) headroom before the done
 * interrupt fires; HC-SR04 max pulse ≈25ms (400cm) → total wait ≤ ~51ms.
 * Use 60ms to leave margin for ISR/RTOS scheduling latency on first-measure
 * cold paths (channel init/malloc). The legacy busy-wait backend returned as
 * soon as ECHO fell, but its 30ms bound was too tight once RMT was introduced. */
#define ULTRASONIC_TIMEOUT_US 60000u
#define ULTRASONIC_CM_PER_US  0.017f   /* 声速换算系数 (340m/s, 往返折半) */

/* ---- 两端共享：脉宽(us) -> 距离(cm) ----
 * 非 static 以便单元测试 extern 访问（例外：无副作用纯函数，风险可控）。 */
float dal_pulse_us_to_cm(uint32_t pulse_us) {
    return (float)pulse_us * ULTRASONIC_CM_PER_US;
}

wink_status_t dal_ultrasonic_apply_override(void *dev, const uint8_t *params, uint16_t len) {
    dal_ultrasonic_t *u = (dal_ultrasonic_t *)dev;
    if (u == NULL || params == NULL) { return WINK_ERR_INVALID_ARG; }
    if (len < 4u) { return WINK_ERR_INVALID_ARG; }   /* u16@0 + u16@2 → ≥4B */

    uint16_t trig_pin;
    uint16_t echo_pin;
    memcpy(&trig_pin, params + 0, 2);
    memcpy(&echo_pin, params + 2, 2);

    if (trig_pin == echo_pin) { return WINK_ERR_INVALID_ARG; }   /* 非法不写 */

    u->config.trig_pin = trig_pin;
    u->config.echo_pin = echo_pin;
    return WINK_OK;
}

wink_status_t dal_ultrasonic_deinit(dal_ultrasonic_t *dev) {
    /* ADR-0024 §4 deinit — checked: 1(trig LOW safe-off)/2(both pins reset via
     *   pal_gpio_reset_pin)/3(N/A: trig is output-only, echo has no PAL-registered
     *   ISR — RMT owns the echo signal internally and is stopped in step 4)/
     *   4(RMT deinit force-stops DMA, no burst wait)/5(N/A)/6(N/A)/7(memset clears
     *   distance_cm/capture state/last_ts)/8(NULL+uninit idempotent)/9(RMT force-stop
     *   ≤5ms, well under 50ms budget)/10(signature unified) */
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_OK; }  /* idempotent no-op on un-init dev */

    /* Read fields before any memset. */
    uint16_t trig_pin = dev->config.trig_pin;
    uint16_t echo_pin = dev->config.echo_pin;
    const char *owner = dev->config.owner;
    bool use_rmt = dev->config.use_rmt;

    /* 1. Best-effort pull trig_pin LOW (safe-off semantic, ≤1µs). */
    WINK_IGNORE_UNUSED(pal_gpio_write(trig_pin, false));

    /* 4. Deinitialize RMT hardware capture if RMT was enabled —
     *    this force-stops any in-flight burst without waiting for idle_thres
     *    (ADR-0024 §4 #4 DMA/descriptor cleanup; pal_rmt_pulse_capture_deinit
     *    calls rmt_rx_stop + rmt_del_channel internally). */
    if (use_rmt) {
        pal_rmt_pulse_capture_deinit();
    }

    /* 2. Reset both GPIO pins: disables leftover routing, reverts to Hi-Z,
     *    releases esp_gpio_reserve bitmap (ADR-0024 §4 #2). Both pins must be
     *    reset — trig is an output, echo is the RMT input. */
    pal_gpio_reset_pin(trig_pin);
    pal_gpio_reset_pin(echo_pin);

    /* Release SW resource claims for both pins */
    WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, trig_pin, owner));
    WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, echo_pin, owner));

    /* 7. Clear the instance data completely to guarantee no residual state */
    memset(dev, 0, sizeof(dal_ultrasonic_t));

    return WINK_OK;
}

wink_status_t dal_ultrasonic_init(dal_ultrasonic_t *dev, const dal_ultrasonic_config_t *cfg) {
    if (dev == NULL || cfg == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->owner == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->trig_pin == cfg->echo_pin) {
        return WINK_ERR_INVALID_ARG;
    }

    /* Track A（M1）：GPIO 双引脚冲突治理。trig 成功后 echo 失败必须回滚 trig。 */
    wink_status_t rs = pal_resource_claim(PAL_RESOURCE_GPIO_PIN,
                                          (uint32_t)cfg->trig_pin, cfg->owner);
    if (wink_status_is_error(rs)) { return rs; }
    rs = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->echo_pin, cfg->owner);
    if (wink_status_is_error(rs)) {
        WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN,
                                                 (uint32_t)cfg->trig_pin, cfg->owner));
        return rs;
    }

    /* 深拷贝配置到实例（支持 ADR-0008 Flash 动态覆写） */
    memcpy(&dev->config, cfg, sizeof(dal_ultrasonic_config_t));
    dev->last_distance = 0.0f;
    dev->state = DAL_ULTRASONIC_IDLE;
    dev->last_status = WINK_OK;
    dev->last_pulse_us = 0u;

    /* GPIO 配置（TRIG 输出，ECHO 输入）
     * WASM 仿真下这两个函数也是空操作（pal_hal_wasm.c 实现） */
    wink_status_t status = pal_gpio_init(cfg->trig_pin, PAL_GPIO_OUTPUT_PUSH_PULL);
    if (wink_status_is_error(status)) {
        WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN,
                                                 (uint32_t)cfg->echo_pin, cfg->owner));
        WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN,
                                                 (uint32_t)cfg->trig_pin, cfg->owner));
        return status;
    }
    status = pal_gpio_init(cfg->echo_pin, PAL_GPIO_INPUT);
    if (wink_status_is_error(status)) {
        WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN,
                                                 (uint32_t)cfg->echo_pin, cfg->owner));
        WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN,
                                                 (uint32_t)cfg->trig_pin, cfg->owner));
        return status;
    }

    /* Eager RMT pulse-capture warm-up on ESP32.
     *
     * Motivation (cold-start bug):
     *   pal_gpio_pulse_in() on ESP32 lazily initializes the RMT RX channel on
     *   its first invocation (heap alloc + ISR install + GPIO mux reconfig).
     *   That cold path takes far longer than the ~100us dead-time between a
     *   TRIG rising edge and the start of the ECHO pulse. Result: on the very
     *   first measurement, RMT arms AFTER the echo pulse has already ended,
     *   the receiver sees only idle silence, and the idle_thres timer (25ms)
     *   fires done with a single all-zero "end marker" symbol — visible as
     *   an alarming "1 symbols captured but high pulse=0us" log line before
     *   steady-state measurements succeed.
     *
     *   Doing the init here — eagerly, at DAL init time — moves the cold path
     *   out of the measurement critical section. The first measurement now
     *   finds the channel already active and arm() is fast enough (~few us)
     *   to complete before the mock/real echo pulse arrives.
     *
     * Contract:
     *   - Only attempted when cfg->use_rmt is true; header (dal_ultrasonic.h
     *     line 71) documents this behavior.
     *   - Failure is non-fatal: pal_gpio_pulse_in falls back to busy-wait when
     *     pal_rmt_pulse_capture_is_active() reports false. Host/wasm stubs
     *     return WINK_ERR_UNSUPPORTED here — that is the expected path and
     *     is silently ignored.
     *   - Return value discarded via WINK_IGNORE_UNUSED to satisfy
     *     WINK_WARN_UNUSED_RESULT under -Werror; the DAL init itself must
     *     still succeed to preserve the "GPIO pins claimed" post-condition.
     */
    if (cfg->use_rmt) {
        WINK_IGNORE_UNUSED(pal_rmt_pulse_capture_init(cfg->echo_pin,
                                                       PAL_RMT_EDGE_RISING));
    }

    dev->initialized = true;
    return WINK_OK;
}

/* ADR-0017 层 2 附属：request_measurement 语义上是"非阻塞请求"，但当前实现内部
 * 调用 pal_gpio_pulse_in（WINK_BLOCKING）——严格模式下该 PAL 声明消失，本实现将
 * 无法编译。所以整个函数体也纳入 #ifndef 段一并剔除。未来非阻塞 RMT 后端落地后
 * (dal_ultrasonic_start + dal_ultrasonic_poll) 才可在严格模式保留。 */
#ifndef WINK_STRICT_NONBLOCKING
wink_status_t dal_ultrasonic_request_measurement(dal_ultrasonic_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }

    /* 1. 触发超声波（TRIG 时序）
     * - WASM 仿真：内部委托 js_sim_trigger_ultrasonic 旁路
     * - ESP32 真机：输出 10us GPIO 脉冲
     * 统一 PAL 接口，无平台条件编译 */
    wink_status_t write_status = pal_gpio_write(dev->config.trig_pin, true);
    if (wink_status_is_error(write_status)) {
        dev->last_status = write_status;
        dev->state = DAL_ULTRASONIC_ERROR;
        return WINK_OK;
    }
    pal_os_busy_wait_us(10);
    write_status = pal_gpio_write(dev->config.trig_pin, false);
    if (wink_status_is_error(write_status)) {
        dev->last_status = write_status;
        dev->state = DAL_ULTRASONIC_ERROR;
        return WINK_OK;
    }
    dev->state = DAL_ULTRASONIC_MEASURING;

    /* 2. 捕获 echo 脉宽
     * - WASM 仿真：内部委托 js_sim_measure_echo_pulse_us 物理模拟
     * - ESP32 真机：RMT 硬件捕获（优先）或 GPIO busy-wait
     * PAL 内部处理平台差异，DAL 层透明 */
    uint32_t pulse_us = 0;
    wink_status_t cap = pal_gpio_pulse_in(
        dev->config.echo_pin,
        true,
        ULTRASONIC_TIMEOUT_US,
        &pulse_us
    );

    if (wink_status_is_error(cap)) {
        dev->last_status = cap;
        dev->state = DAL_ULTRASONIC_ERROR;
    } else {
        /* Publish data BEFORE state transitions to READY so cross-core
         * readers (telemetry) always see consistent (distance, status,
         * state=READY) tuple.  Compiler-barrier prevents reordering. */
        dev->last_pulse_us = pulse_us;
        dev->last_distance = dal_pulse_us_to_cm(pulse_us);
        dev->last_status = WINK_OK;
#if defined(ESP_PLATFORM)
        /* Xtensa memw: ensure all prior writes reach RAM before state=READY */
        __asm__ __volatile__("memw" ::: "memory");
#endif
        dev->state = DAL_ULTRASONIC_READY;
    }
    return WINK_OK;   /* request 成功（已触发）；结果经 get_cached 读 */
}
#endif /* WINK_STRICT_NONBLOCKING (request_measurement) */

wink_status_t dal_ultrasonic_get_cached_distance(const dal_ultrasonic_t *dev, float *distance_cm) {
    if (dev == NULL || distance_cm == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }

    /* Snapshot volatile fields to guarantee a consistent read across SMP cores. */
    float              dist  = dev->last_distance;
    wink_status_t      lstat = dev->last_status;
    dal_ultrasonic_state_t st = dev->state;

    switch (st) {
        case DAL_ULTRASONIC_READY:
            *distance_cm = dist;
            return WINK_OK;
        case DAL_ULTRASONIC_MEASURING:
            /* Phase-lock guard: if a previous measurement already succeeded
             * (last_status == WINK_OK), return the cached distance instead
             * of BUSY.  Without this, a telemetry reader whose period is an
             * exact integer multiple of the ultrasonic poll tick (e.g. 2000ms = 4×500ms)
             * can permanently land in the ~28ms MEASURING window because the
             * higher-priority ultrasonic poll task has already claimed the CPU and
             * entered RMT wait.  Reporting stale-but-valid data is the
             * correct telemetry behaviour — BUSY should mean "no data yet",
             * not "data is one measurement-cycle old". */
            if (lstat == WINK_OK) {
                *distance_cm = dist;
                return WINK_OK;
            }
            return WINK_ERR_BUSY;
        case DAL_ULTRASONIC_ERROR:
            return lstat;
        case DAL_ULTRASONIC_IDLE:
        default:
            return WINK_ERR_BUSY;   /* 无测量数据：当作未就绪 */
    }
}

/* @deprecated @blocking —— 见头文件契约；App 10ms tick 禁用，迁移至 request_measurement + get_cached_distance。
 * 所有平台共用同一份代码：统一使用 PAL 接口，无平台条件编译。
 * - WASM 仿真：PAL 内部委托 js_sim_* 物理量旁路
 * - ESP32 真机：PAL 内部用 RMT 或 GPIO busy-wait
 * 单位换算、超时判定与业务逻辑全平台同源（ADR-0003 决策2）。
 *
 * ADR-0017 §落地规则 #2：与 dal_ultrasonic.h 中 #ifndef 包围匹配，严格模式下整段消失。
 * 函数体首行 WINK_ASSERT_NONBLOCKING() 为 T5 阶段 PT-context 检测预留占位（当前 no-op）。 */
#ifndef WINK_STRICT_NONBLOCKING
wink_status_t dal_ultrasonic_read(dal_ultrasonic_t *dev, float *distance_cm) {
    WINK_ASSERT_NONBLOCKING();
    if (dev == NULL || distance_cm == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }

    /* 1. 触发超声波（TRIG 时序） */
    wink_status_t write_status = pal_gpio_write(dev->config.trig_pin, true);
    if (wink_status_is_error(write_status)) {
        return write_status;
    }
    pal_os_busy_wait_us(10);
    write_status = pal_gpio_write(dev->config.trig_pin, false);
    if (wink_status_is_error(write_status)) {
        return write_status;
    }

    /* 2. 测量 ECHO 脉宽（平台差异由 PAL 内部处理） */
    uint32_t pulse_us = 0;
    wink_status_t status = pal_gpio_pulse_in(
        dev->config.echo_pin,
        true,
        ULTRASONIC_TIMEOUT_US,
        &pulse_us
    );
    if (wink_status_is_error(status)) {
        return status;  /* WINK_ERR_TIMEOUT 或其它硬件错误 */
    }

    /* 3. 单位换算：全平台同源代码（ADR-0003 决策2） */
    dev->last_distance = dal_pulse_us_to_cm(pulse_us);
    *distance_cm = dev->last_distance;
    return WINK_OK;
}
#endif  /* WINK_STRICT_NONBLOCKING */
