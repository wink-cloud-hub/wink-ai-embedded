/**
 * @file wink_sim_physical.h
 * @brief ADR-0009 物理特性退化算法库（target 无关，host 试点 Wave 1）。
 *
 * 确定性守卫（ADR-0009 §4.1）：所有时间基准由 caller 传入 pal_get_us() 虚拟时钟值；
 *   PRNG 种子驱动，严禁 rand()/Math.random()/clock()/time()/墙钟。
 * 零编译污染（§4.3）：本单元仅进 pal_host OBJECT；esp32/baremetal/wasm 不链接。
 * 无 libm：RC 低通用离散一阶近似，不用 expf。
 *
 * SIM_TRACE：调试追踪宏（测试时定义 `SIM_TRACE=1` 启用）。
 *   不改变算法行为，不影响确定性，仅输出关键决策点用于问题定位。
 *   可按模块独立控制：`SIM_TRACE_DEBOUNCE=1`、`SIM_TRACE_RC=1` 等。
 */
#ifndef WINK_SIM_PHYSICAL_H
#define WINK_SIM_PHYSICAL_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"

/* === SIM_TRACE 调试追踪宏（默认关闭，不影响确定性） === */
/* 定义 SIM_TRACE=1 启用所有模块追踪；或单独定义各模块追踪 */
#if defined(SIM_TRACE) && SIM_TRACE
#  ifndef SIM_TRACE_DEBOUNCE
#    define SIM_TRACE_DEBOUNCE 1
#  endif
#  ifndef SIM_TRACE_RC
#    define SIM_TRACE_RC 1
#  endif
#  ifndef SIM_TRACE_WARMUP
#    define SIM_TRACE_WARMUP 1
#  endif
#  ifndef SIM_TRACE_BUS
#    define SIM_TRACE_BUS 1
#  endif
#endif

/* 各模块追踪宏（空操作当未启用） */
#if defined(SIM_TRACE_DEBOUNCE) && SIM_TRACE_DEBOUNCE
#  include <stdio.h>
#  define ST_DEBOUNCE_START(pin, target, now, bounce_us) \
    printf("[SIM_DEBOUNCE] pin=%d start: target=%d now=%lluus bounce=%uus\n", \
           pin, target, (unsigned long long)(now), bounce_us)
#  define ST_DEBOUNCE_IN_WINDOW(pin, flip, result) \
    printf("[SIM_DEBOUNCE] pin=%d window: flip=%d level=%d\n", pin, flip, result)
#  define ST_DEBOUNCE_SETTLED(pin, level) \
    printf("[SIM_DEBOUNCE] pin=%d SETTLED: level=%d\n", pin, level)
#else
#  define ST_DEBOUNCE_START(pin, t, n, b)
#  define ST_DEBOUNCE_IN_WINDOW(pin, f, r)
#  define ST_DEBOUNCE_SETTLED(pin, l)
#endif

#if defined(SIM_TRACE_RC) && SIM_TRACE_RC
#  include <stdio.h>
#  define ST_RC_STEP(chan, target, now, tau, out) \
    printf("[SIM_RC] chan=%d: target=%.3f now=%lluus tau=%.3fs out=%.3f\n", \
           chan, target, (unsigned long long)(now), tau, out)
#  define ST_RC_NOISE(chan, noise_amp, noise_val) \
    printf("[SIM_RC] chan=%d noise: amp=%.3f val=%.3f\n", chan, noise_amp, noise_val)
#else
#  define ST_RC_STEP(c, t, n, tau, o)
#  define ST_RC_NOISE(c, a, v)
#endif

#if defined(SIM_TRACE_WARMUP) && SIM_TRACE_WARMUP
#  include <stdio.h>
#  define ST_WARMUP_CHECK(now, power_on, warmup, elapsed, status) \
    printf("[SIM_WARMUP] now=%lluus power_on=%lluus warmup=%uus elapsed=%lluus -> %s\n", \
           (unsigned long long)(now), (unsigned long long)(power_on), warmup, \
           (unsigned long long)(elapsed), \
           (status) == WINK_ERR_BUSY ? "BUSY" : (status) == WINK_ERR_TIMEOUT ? "TIMEOUT" : "OK")
#else
#  define ST_WARMUP_CHECK(n, p, w, e, s)
#endif

#if defined(SIM_TRACE_BUS) && SIM_TRACE_BUS
#  include <stdio.h>
#  define ST_BUS_DROP(permille, rand_val, will_drop) \
    printf("[SIM_BUS] drop: permille=%d rand=%.3f -> %s\n", \
           permille, rand_val, will_drop ? "DROP" : "PASS")
#else
#  define ST_BUS_DROP(p, r, d)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 故障注入配置（§4.2；host 直接填，wasm 将来从 JS JSON 解析）。全 0 = 理想（无退化）。 */
typedef struct {
    uint32_t bounce_us;          /* 按键抖动时长（§3.1），0=禁用 */
    uint32_t warmup_us;          /* 传感器上电预热（§3.2） */
    uint32_t sample_interval_us; /* 最小采样间隔（§3.2） */
    float    adc_noise_v;        /* ADC 噪声幅度 ±V（§3.3），0=禁用 */
    float    rc_tau_s;           /* RC 低通时间常数（§3.3），<=0=禁用 */
    uint16_t i2c_drop_permil;    /* 总线丢包率千分比（§4），0=禁用 */
    uint32_t prng_seed;          /* 确定性 PRNG 种子（§4.1） */
} wink_sim_faults_t;

extern const wink_sim_faults_t WINK_SIM_FAULTS_IDEAL;

/** @brief 确定性 PRNG（LCG）。推进 *seed 并返回 [0,1)。caller 持有 seed。 */
float wink_phys_prng_next(uint32_t *seed);

/** @brief 抖动状态机上下文（caller 每 pin 持有一个）。
 *
 * 语义契约（与 host 注入层 sim_set_gpio_ideal 双语义对齐，§2.3 红线 6）：
 *   - 上电态：stable_level = 初始理想电平（无跃变、不抖）。
 *   - 跃变：caller 改变 target_level 使之 ≠ stable_level → 进入抖动窗。
 * 抖动窗内每次采样强制翻转 bounce_flip（采样周期无关、100% 确定，§3 约束 6）。
 */
typedef struct {
    bool     stable_level;      /* 上次已稳定的电平 */
    bool     in_bounce;         /* 是否正处于抖动期 */
    uint64_t bounce_start_us;   /* 当前抖动期起点 */
    bool     bounce_flip;       /* 抖动期电平翻转位（每次采样取反，强制交替） */
} wink_phys_debounce_ctx_t;

/** @brief 按键抖动状态机（§3.1，强制交替模型）。返回当前物理（抖动后）电平。 */
bool wink_phys_debounce_step(wink_phys_debounce_ctx_t *ctx,
                             bool target_level, uint64_t now_us, uint32_t bounce_us);

/** @brief RC 低通上下文（caller 每通道持有一个）。 */
typedef struct {
    float    current;   /* 当前滤波输出 */
    uint64_t last_us;   /* 上次更新时间 */
    bool     is_initialized; /* 是否已初始化 */
} wink_phys_rc_ctx_t;

/** @brief RC 一阶低通 + 噪声（§3.3，离散近似，无 expf）。返回当前含噪输出。 */
float wink_phys_rc_lowpass(wink_phys_rc_ctx_t *ctx, float target, uint64_t now_us,
                           float tau_s, float noise_v, uint32_t *prng_seed);

/* warmup/采样间隔检查（§3.2）：预热内返回 WINK_ERR_BUSY；采样过近返回 WINK_ERR_TIMEOUT；否则 OK。
 * last_sample_us=NULL → 仅检查预热；时钟回拨→强制 OK+reset last_sample。 */
wink_status_t wink_phys_warmup_check(uint64_t now_us, uint64_t power_on_us,
                                     uint32_t warmup_us, uint32_t sample_interval_us,
                                     uint64_t *last_sample_us);

/** @brief 总线丢包判定（§4）。drop_permil 千分比；PRNG 驱动确定性。true=丢弃。 */
bool wink_phys_bus_drop(uint16_t drop_permil, uint32_t *prng_seed);

#ifdef __cplusplus
}
#endif
#endif /* WINK_SIM_PHYSICAL_H */
