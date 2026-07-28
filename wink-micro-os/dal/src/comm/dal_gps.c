#include "dal_gps.h"
#include "pal_hal.h"
#include "pal_resource.h"
#include "wink_pt_debug.h"   /* WINK_ASSERT_NONBLOCKING() (ADR-0017 层 3 runtime hook) */
#include <string.h>

/* ADR-0017 层 2：dal_gps_init 是 blocking（UART 探测 + 首个 NMEA 等待）。
 * 严格模式下头文件声明消失，实现也必须一起消失以避免符号残留。
 * dal_gps_poll 与 dal_gps_get_position 是 non-blocking，不进 #ifndef 段。 */
#ifndef WINK_STRICT_NONBLOCKING
wink_status_t dal_gps_init(dal_gps_t *dev, const dal_gps_config_t *cfg) {
    /* 参数合法性校验（必须保留：契约诚实，避免 NULL 解引用） */
    if (dev == NULL || cfg == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->owner == NULL) { return WINK_ERR_INVALID_ARG; }

    /* ADR-0017 层 3：blocking API 契约守卫（PT 上下文误调 → assert/fault）。
     * 放在参数校验之后：invalid-arg 快速路径不触发 nonblocking 断言。 */
    WINK_ASSERT_NONBLOCKING();

    /* @experimental Stub: UART backend + NMEA parser not yet implemented (see P2-P6 PAL_UART).
     * 不 claim UART 资源、不置 initialized=true、不做任何硬件副作用——避免"假成功"反模式
     * （ADR-0012 契约诚实：宁可返 NOT_SUPPORTED，也不要让 caller 误以为硬件已启动）。
     * 未来真实实现到达时：这里会 pal_resource_claim(PAL_RESOURCE_UART_PORT,...) + 配置
     * UART + 启动 NMEA DMA 接收，并置 dev->initialized = true。 */
    memset(dev, 0, sizeof(dal_gps_t));
    return WINK_ERR_UNSUPPORTED;
}
#endif /* WINK_STRICT_NONBLOCKING */

wink_status_t dal_gps_poll(dal_gps_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    /* @experimental Stub: UART RX + NMEA parse not implemented. */
    return WINK_ERR_UNSUPPORTED;
}

wink_status_t dal_gps_get_position(const dal_gps_t *dev, dal_gps_position_t *pos) {
    if (dev == NULL || pos == NULL) { return WINK_ERR_INVALID_ARG; }
    /* @experimental Stub: 清零输出以避免 caller 使用未初始化栈值。 */
    memset(pos, 0, sizeof(dal_gps_position_t));
    return WINK_ERR_UNSUPPORTED;
}

wink_status_t dal_gps_deinit(dal_gps_t *dev) {
    /* ADR-0024 §4 deinit — checked: 1(N/A: GPS is a sensor, no actuator safe-off)/
     *   2(N/A: UART RX/TX pins are owned by the UART driver, released by pal_uart_deinit
     *   when that lands; current stub has no GPIO routed)/3(N/A: no GPIO ISR in stub;
     *   future UART RX ISR must be disabled+torn down before UART driver deinit)/
     *   4(N/A: no DMA active in stub; future UART RX DMA must be force-aborted)/
     *   5(N/A)/6(N/A: UART not shared)/7(memset clears nmea buffer+state)/
     *   8(NULL+uninit idempotent)/9(no waits in stub; future UART deinit must be ≤50ms)/
     *   10(signature unified) */
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_OK; }  /* idempotent no-op on un-init dev */

    /* Read fields before memset. */
    uint8_t uart_port = dev->config.uart_port;
    const char *owner = dev->config.owner;

    /* Release UART port SW resource claim (future real impl will also
     * pal_uart_deinit(uart_port) to tear down DMA/ISR and release UART pins). */
    WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_UART_PORT, uart_port, owner));

    /* 7. Clear the instance data completely */
    memset(dev, 0, sizeof(dal_gps_t));

    return WINK_OK;
}
