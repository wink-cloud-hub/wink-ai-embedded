/**
 * @file pal_hal_esp32.c
 * @brief ESP32 PAL HAL 最小残留：仅承载 pal_debug_printf。
 *
 * Task 2（PLAN-20260701-PAL-TARGET-P1-MAINT）完成后的最终形态：
 *   - GPIO 实现 → pal_hal_gpio_esp32.c
 *   - IRQ 实现  → pal_irq_esp32.c
 *   - PWM 实现  → pal_hal_pwm_esp32.c
 *   - I2C 实现  → pal_hal_i2c_esp32.c
 *
 * 保留本 TU 而非合并到其它 TU：pal_debug_printf 是跨 target 的通用调试接口，
 * 位置中立、无 IDF 私有头依赖，作为文件名 SSOT 的锚点也便于未来扩展
 * （例如 pal_dump / pal_panic 等 debug 类接口）。
 *
 * ✅ R-4：本 TU 无 `#if defined(ESP_PLATFORM)`，是同源可编译代码。
 */
#include "pal_debug.h"
#include <stdarg.h>
#include <stdio.h>

void pal_debug_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}
