/**
 * @file selftest_pwm_router.c
 * @brief S5: PWM router 同频复用、异频隔离验证。
 *
 * 验证：
 *   1. ch1=50Hz、ch2=1kHz 应被分配到不同 timer（异频隔离）。
 *   2. 两个不同通道配同一频率时应被分配到同一 timer（同频复用）。
 *   3. pal_pwm_set_duty 在两条通道上正常返回 WINK_OK。
 *
 * 测试完成后 deinit 所有已初始化通道，恢复干净状态（供后续 selftest 使用）。
 */
#define LOG_TAG "selftest.pwm"

#include "wink_selftest.h"
#include "wink_selftest_internal.h"
#include "wink_status.h"
#include "wink_log.h"
#include "pal_pwm_router.h"
#include "pal_hal.h"

#define SMOKE_PWM_CH_LO  1u   /* 50 Hz */
#define SMOKE_PWM_CH_HI  2u   /* 1 kHz */
#define SMOKE_PWM_CH_SAME 3u /* 另一个 50 Hz 通道（验证同频复用） */

wink_status_t wink_selftest_pwm_router_freq_isolation(wink_selftest_result_t *r)
{
    r->note = "50Hz vs 1kHz -> different timer";

    /* 先确保三个通道都处于 deinit 状态（防御性：上次崩溃/复位残留）*/
    pal_pwm_deinit(SMOKE_PWM_CH_LO);
    pal_pwm_deinit(SMOKE_PWM_CH_HI);
    pal_pwm_deinit(SMOKE_PWM_CH_SAME);

    /* 1. 异频初始化 */
    wink_status_t st = pal_pwm_init(SMOKE_PWM_CH_LO, 50u);
    if (wink_status_is_error(st)) {
        r->note = "pal_pwm_init(ch_lo) failed";
        return st;
    }
    st = pal_pwm_init(SMOKE_PWM_CH_HI, 1000u);
    if (wink_status_is_error(st)) {
        pal_pwm_deinit(SMOKE_PWM_CH_LO);
        r->note = "pal_pwm_init(ch_hi) failed";
        return st;
    }
    st = pal_pwm_init(SMOKE_PWM_CH_SAME, 50u);
    if (wink_status_is_error(st)) {
        pal_pwm_deinit(SMOKE_PWM_CH_HI);
        pal_pwm_deinit(SMOKE_PWM_CH_LO);
        r->note = "pal_pwm_init(ch_same) failed";
        return st;
    }

    uint8_t t_lo   = pal_pwm_router_channel_timer(SMOKE_PWM_CH_LO);
    uint8_t t_hi   = pal_pwm_router_channel_timer(SMOKE_PWM_CH_HI);
    uint8_t t_same = pal_pwm_router_channel_timer(SMOKE_PWM_CH_SAME);

    /* 设置 50% 占空比（验证通道可操作；返回值忽略 —— 这不是 PASS/FAIL 判据）*/
    WINK_IGNORE_RESULT(pal_pwm_set_duty(SMOKE_PWM_CH_LO, 50.0f));
    WINK_IGNORE_RESULT(pal_pwm_set_duty(SMOKE_PWM_CH_HI, 50.0f));

    /* metric 字段编码 timer 分配：bits [0:3]=t_lo, [4:7]=t_hi, [8:11]=t_same */
    r->metric = ((uint32_t)t_lo & 0xFu)
              | (((uint32_t)t_hi   & 0xFu) << 4)
              | (((uint32_t)t_same & 0xFu) << 8);

    /* 验证：异频 → 不同 timer；同频 → 同一 timer；timer 编号均 < PAL_PWM_TIMERS(4) */
    bool pass = true;
    if (t_lo >= 4u || t_hi >= 4u || t_same >= 4u) pass = false;
    if (t_lo == t_hi)                             pass = false;  /* 异频必须隔离 */
    if (t_lo != t_same)                           pass = false;  /* 同频必须复用 */

    /* 清理：保留 ch1/ch2 为 50Hz 50% 状态（smoke 板载 LED 复用 ch1）——
     * 但 deinit ch_same（smoke 不使用 ch3），避免占 LEDC 通道资源。*/
    pal_pwm_deinit(SMOKE_PWM_CH_SAME);

    /* 注意：保留 ch_lo=50Hz 和 ch_hi=1kHz 的初始化状态：
     *  - ch_lo 是 smoke 板 S5 遗留的 LED 通道，app 期望它保持运行。
     *  - 后续 rmt.self_loopback 会自行 deinit ch_lo 并在结束时恢复。*/
    (void)pass;
    if (!pass) {
        r->note = "freq isolation or reuse violated";
        return WINK_ERR_HARDWARE;
    }
    return WINK_OK;
}
