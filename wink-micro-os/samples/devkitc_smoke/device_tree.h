/**
 * @file device_tree.h
 * @brief DevKitC 冒烟测试设备树：静态实例 + 生命周期入口。
 *
 * 引脚号、config 字面量、actuator thunk、init/deinit 序列全部封装在
 * device_tree.c 中，业务层（app_callbacks.c）仅通过本头文件的 extern 声明
 * 访问实例句柄，并在 app_init 第一行调 wink_device_tree_init()。
 *
 * P0 过渡：SMOKE_TRIG_PIN / SMOKE_ECHO_PIN 仍暴露在头，因为 sim helper
 * (wink_sim_ultrasonic_echo_start) 需要引脚号驱动 echo loopback。P1 codegen 落地
 * 后这两个宏搬进生成的 app_support.c，本头只留实例句柄。
 */
#ifndef DEVICE_TREE_H
#define DEVICE_TREE_H

#include "dal_led.h"
#include "dal_button.h"
#include "dal_ultrasonic.h"
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── P0 过渡：sim helper 需要的引脚（P1 由 codegen 输出后移除） ── */
#define SMOKE_TRIG_PIN    18u
#define SMOKE_ECHO_PIN    19u

/* ── 设备实例句柄（extern 可见，业务层通过名字访问）── */
extern dal_led_t         board_led;
extern dal_button_t      boot_button;
extern dal_ultrasonic_t  smoke_sonar;

/* ── 设备树生命周期 ── */

/**
 * @brief 初始化所有静态设备并注册 actuator safe-off 回调。
 *
 * Init 顺序：board_led → boot_button → smoke_sonar（基础设施 → 传感器）。
 * Actuator 在所有 init 成功后统一注册，保证 registry 中无半初始化设备。
 *
 * @return WINK_OK 全部成功；否则返回首个失败的错误码（已 init 的设备不回滚，
 *         由 Runtime fault 流程接管）。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_device_tree_init(void);

/**
 * @brief 反初始化所有设备：按 init 反序 deinit，并先反注册 actuator。
 *
 * 顺序：unregister actuator → deinit(smoke_sonar) → deinit(boot_button)
 *       → deinit(board_led)。
 *
 * 可在部分初始化的状态下安全调用：已 init 的设备执行 deinit，未 init 的跳过。
 */
void wink_device_tree_deinit(void);

#ifdef __cplusplus
}
#endif
#endif /* DEVICE_TREE_H */
