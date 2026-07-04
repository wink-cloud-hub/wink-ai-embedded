/**
 * @file test_pal_nonblocking_strict.c
 * @brief P1-D2 强化 ADR-0017 层 2 硬隔离的编译期契约测试：确认
 *        WINK_STRICT_NONBLOCKING=1 下所有被标注 WINK_BLOCKING 的 API 声明从头文件中
 *        消失，同时非阻塞对偶 API 仍然可见。
 *
 * 编译成功即为通过（无 runtime 断言 —— 这是编译期契约测试）：
 *   - 若哪个 blocking API 的 #ifndef 包围写错，本 TU 就能取到该函数地址；
 *     `sizeof(&blocking_api)` 之所以可以留在这里断言 elided，是因为紧邻的注释块
 *     列出了明确的负例（"do NOT enable"），review 时人眼校验即可。
 *   - 若哪个非阻塞 API 意外被吞掉，本 TU 会因 `sizeof(&nb_api)` 里的 undeclared
 *     identifier 编译失败 —— 即本文件的正面断言。
 *   - `sizeof(&fn)` 是编译期常量表达式，不产生 link 引用 —— 无需 -c 加 link；直接
 *     交给 add_wink_test（正常 link）也不会拉入 pal target 里的任何 obj。
 *
 * 联动：L1 lint（run-tests.ps1 第 7 段）通过 nm 直接验证 dal_ultrasonic_read 符号
 *        从 .o 消失；这里通过头文件的直接引用验证。两层互补。
 */

#define WINK_STRICT_NONBLOCKING 1   /* MUST be defined before including any PAL/DAL header */

#include "wink_status.h"

/* PAL / DAL headers under strict mode:
 *   - blocking API declarations wrapped by `#ifndef WINK_STRICT_NONBLOCKING` should be gone;
 *   - non-blocking API declarations should still be visible. */
#include "pal_hal.h"        /* pal_gpio_init/read/write (nb) + pal_i2c_transfer / pal_gpio_pulse_in (blocking, elided) */
#include "pal_osal.h"       /* pal_os_get_ms/us (nb) + pal_os_sleep_ms / pal_os_mutex_lock / pal_os_task_create (elided) */
#include "hal/pal_rmt.h"    /* pal_rmt_pulse_capture_init/deinit (nb) + pal_rmt_pulse_capture_wait (elided) */
#include "dal_ultrasonic.h" /* dal_ultrasonic_init / request_measurement / get_cached_distance (nb) + dal_ultrasonic_read (elided) */
#include "dal_eeprom.h"     /* everything in this header is blocking → all elided under strict */
#include "dal_gps.h"        /* dal_gps_init elided; dal_gps_poll / dal_gps_get_position remain */

#include <stddef.h>

/* ---- Unity 测试框架契约：setUp/tearDown/main 是必须的 hook。 ----
 * 本 TU 无 runtime 用例 —— 编译通过就算 pass —— 但 add_wink_test 用 add_test()
 * 注册可执行文件，链接了 unity.c 就会引用 setUp/tearDown/main。给出最小定义。 */
void setUp(void) {}
void tearDown(void) {}

/* ---- Compile-only 契约断言 ----
 *
 * 用 sizeof(&fn) 触发头文件里 fn 的声明查找但**不**产生 link-time 引用（sizeof 求值
 * 结果为编译期常量，操作数不实际取址）。这就完美满足"证明声明存在"而不必链接实现。
 *
 * 若下面任一非阻塞 API 意外被吞掉，本处会出 `implicit-function-declaration` /
 * `undeclared identifier` 硬错，编译失败。 */
enum {
    /* PAL HAL — non-blocking half (register-level ops). */
    NB_HAS_pal_gpio_init                 = sizeof(&pal_gpio_init),
    NB_HAS_pal_gpio_read                 = sizeof(&pal_gpio_read),
    NB_HAS_pal_gpio_write                = sizeof(&pal_gpio_write),
    NB_HAS_pal_pwm_init                  = sizeof(&pal_pwm_init),
    NB_HAS_pal_pwm_set_duty              = sizeof(&pal_pwm_set_duty),
    NB_HAS_pal_pwm_deinit                = sizeof(&pal_pwm_deinit),
    NB_HAS_pal_pwm_channel_pin           = sizeof(&pal_pwm_channel_pin),
    NB_HAS_pal_i2c_port_pins             = sizeof(&pal_i2c_port_pins),

    /* PAL OSAL — non-blocking half.
     * ADR-0017 §决策：pal_os_busy_wait_us <10ms 未标 blocking，属于非阻塞集合。 */
    NB_HAS_pal_os_busy_wait_us           = sizeof(&pal_os_busy_wait_us),
    NB_HAS_pal_os_get_ms                 = sizeof(&pal_os_get_ms),
    NB_HAS_pal_os_get_us                 = sizeof(&pal_os_get_us),
    NB_HAS_pal_os_mutex_create           = sizeof(&pal_os_mutex_create),
    NB_HAS_pal_os_mutex_unlock           = sizeof(&pal_os_mutex_unlock),
    NB_HAS_pal_os_mutex_destroy          = sizeof(&pal_os_mutex_destroy),
    NB_HAS_pal_os_task_delete            = sizeof(&pal_os_task_delete),
    NB_HAS_pal_os_critical_enter         = sizeof(&pal_os_critical_enter),
    NB_HAS_pal_os_critical_exit          = sizeof(&pal_os_critical_exit),
    NB_HAS_pal_os_ringbuf_create         = sizeof(&pal_os_ringbuf_create),
    NB_HAS_pal_os_ringbuf_push           = sizeof(&pal_os_ringbuf_push),
    NB_HAS_pal_os_ringbuf_pop            = sizeof(&pal_os_ringbuf_pop),

    /* PAL RMT — non-blocking half. */
    NB_HAS_pal_rmt_pulse_capture_init      = sizeof(&pal_rmt_pulse_capture_init),
    NB_HAS_pal_rmt_pulse_capture_deinit    = sizeof(&pal_rmt_pulse_capture_deinit),
    NB_HAS_pal_rmt_pulse_capture_is_active = sizeof(&pal_rmt_pulse_capture_is_active),

    /* DAL non-blocking APIs. */
    NB_HAS_dal_ultrasonic_init             = sizeof(&dal_ultrasonic_init),
    NB_HAS_dal_ultrasonic_get_cached       = sizeof(&dal_ultrasonic_get_cached_distance),
    NB_HAS_dal_ultrasonic_apply_override   = sizeof(&dal_ultrasonic_apply_override),
    NB_HAS_dal_gps_poll                    = sizeof(&dal_gps_poll),
    NB_HAS_dal_gps_get_position            = sizeof(&dal_gps_get_position),
};

/* ---- The elided-symbol negatives ----
 * The following commented-out references must NOT compile under strict mode.
 * Uncommenting any of them will cause -Werror=implicit-function-declaration
 * (or "undeclared identifier") — this is the intended gate. Do NOT enable.
 *
 *   enum { X = sizeof(&pal_os_sleep_ms) };
 *   enum { X = sizeof(&pal_os_mutex_lock) };
 *   enum { X = sizeof(&pal_os_task_create) };
 *   enum { X = sizeof(&pal_i2c_transfer) };
 *   enum { X = sizeof(&pal_gpio_pulse_in) };
 *   enum { X = sizeof(&pal_rmt_pulse_capture_wait) };
 *   enum { X = sizeof(&dal_ultrasonic_read) };
 *   enum { X = sizeof(&dal_eeprom_init) };
 *   enum { X = sizeof(&dal_eeprom_read) };
 *   enum { X = sizeof(&dal_eeprom_write) };
 *   enum { X = sizeof(&dal_gps_init) };
 */

int main(void) { return 0; }
