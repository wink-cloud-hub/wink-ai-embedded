#include "dal_gps.h"
#include "pal_hal.h"
#include "pal_resource.h"
#include <string.h>

wink_status_t dal_gps_init(dal_gps_t *dev, const dal_gps_config_t *cfg) {
    /* 参数合法性校验（必须保留：契约诚实，避免 NULL 解引用） */
    if (dev == NULL || cfg == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->owner == NULL) { return WINK_ERR_INVALID_ARG; }

    /* @experimental Stub: UART backend + NMEA parser not yet implemented (see P2-P6 PAL_UART).
     * 不 claim UART 资源、不置 initialized=true、不做任何硬件副作用——避免"假成功"反模式
     * （ADR-0012 契约诚实：宁可返 NOT_SUPPORTED，也不要让 caller 误以为硬件已启动）。
     * 未来真实实现到达时：这里会 pal_resource_claim(PAL_RESOURCE_UART_PORT,...) + 配置
     * UART + 启动 NMEA DMA 接收，并置 dev->initialized = true。 */
    memset(dev, 0, sizeof(dal_gps_t));
    return WINK_ERR_UNSUPPORTED;
}

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
