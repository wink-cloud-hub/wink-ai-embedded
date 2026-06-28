/* PAL→DAL→runtime→App 端到端：注册样本回调 → 跑 N tick → 注入近障 → 验证舵机偏转 + trace。
 * 等价 ADR-0003 计划附录 B Task T1（App 三层联动）。
 * 注意：本测试不 link Unity（用断言宏自实现），作为 app_avoidance_car_e2e 的 main。 */
#include "wink_runtime.h"
#include "wink_trace.h"
#include "dal_servo.h"
#include "device_tree.h"
#include "host_test_ctrl.h"

extern const wink_app_callbacks_t *wink_app_get_callbacks(void);

/* targets/host 的 sim_* 经 host_test_ctrl；dal_ultrasonic 走真机分支需 echo 时序注入。
 * 为让 app_loop 的 radar 读到"近障"，注入一个近距 echo 脉宽。
 * 简化：直接经 sim_set_echo_timing 注入 ~588us(≈10cm) 脉宽。 */
#define E2E_PASS()      do { extern int puts(const char*); puts("E2E PASS"); return 0; } while(0)
#define E2E_FAIL(msg)   do { extern int puts(const char*); puts("E2E FAIL: " msg); return 1; } while(0)

int main(void) {
    wink_trace_reset();
    const wink_app_callbacks_t *cb = wink_app_get_callbacks();

    /* tick 1：无障碍（echo 远）→ 舵机应复位 90°
     * 注意：sim_reset_time() 会清零 echo_pin，故必须先 reset 再 set_echo_pin，
     * 否则 pal_gpio_read 因 pin 不匹配恒返回 false 导致协作式轮询死循环。 */
    sim_reset_time();
    sim_set_echo_pin(front_radar.echo_pin);
    sim_set_echo_timing(100, 5882);   /* ≈100cm，无近障 */
    {
        wink_status_t s = wink_runtime_run(cb, 1);
        (void)s;
    }
    if (neck_servo.current_angle != 90.0f) E2E_FAIL("servo not 90 when clear");

    /* tick 2：近障（echo ≈10cm = 588us）→ 舵机应扫到 180°
     * 每个 tick 先 reset（重置虚拟时钟，使 echo 边沿可达）再重新注入 pin/timing。 */
    sim_reset_time();
    sim_set_echo_pin(front_radar.echo_pin);
    sim_set_echo_timing(100, 588);    /* ≈10cm < 20cm 阈值 */
    {
        wink_status_t s = wink_runtime_run(cb, 1);
        (void)s;
    }
    if (neck_servo.current_angle != 180.0f) E2E_FAIL("servo not 180 on near obstacle");

    E2E_PASS();
}
