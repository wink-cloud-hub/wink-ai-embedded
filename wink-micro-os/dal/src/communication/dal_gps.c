#include "dal_gps.h"
#include "pal_hal.h"
#include "pal_resource.h"
#include <string.h>

wink_status_t dal_gps_init(dal_gps_t *dev, const dal_gps_config_t *cfg) {
    if (dev == NULL || cfg == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->owner == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->baudrate == 0 || cfg->uart_port >= 3u) { return WINK_ERR_INVALID_ARG; }

    /* Track A（M1）：UART 端口冲突治理。两 GPS 若配同 uart_port 不同 owner 则 BUSY。 */
    wink_status_t rs = pal_resource_claim(PAL_RESOURCE_UART_PORT,
                                          (uint32_t)cfg->uart_port, cfg->owner);
    if (wink_status_is_error(rs)) { return rs; }

    /* TODO: 真实 UART 初始化 + NMEA 解析器初始化 */
    memcpy(&dev->config, cfg, sizeof(dal_gps_config_t));
    memset(&dev->last_position, 0, sizeof(dal_gps_position_t));
    dev->last_fix_time_ms = 0;
    dev->initialized = true;
    return WINK_OK;
}

wink_status_t dal_gps_poll(dal_gps_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }

    /* TODO: UART 非阻塞读取 + NMEA 解析（GGA/RMC 语句） */
    return WINK_OK;
}

wink_status_t dal_gps_get_position(const dal_gps_t *dev, dal_gps_position_t *pos) {
    if (dev == NULL || pos == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    if (!dev->last_position.fix_valid) { return WINK_ERR_EMPTY; }

    memcpy(pos, &dev->last_position, sizeof(dal_gps_position_t));
    return WINK_OK;
}
