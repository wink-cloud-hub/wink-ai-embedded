// SPDX-License-Identifier: GPL-3.0-only
/* External IRQ/JS/health boundaries for the focused UART emcc test.
 * The UART implementation and OSAL ring buffer are real production sources. */
#include "unity.h"
#include "pal_wasm_common.h"
#include "pal_irq.h"
#include "wasm_bridge.h"

bool pal_wasm_is_faulted(void) { return false; }
void pal_wasm_report_oom(const char *site, uint32_t size)
{
    (void)site;
    (void)size;
    TEST_FAIL_MESSAGE("Unexpected UART allocation failure");
}
void pal_wasm_log_fault(uint8_t type, uint16_t port)
{
    (void)type;
    (void)port;
}
void pal_irq_set_pending(uint32_t irq) { (void)irq; }
void pal_wasm_dispatch_pending_irqs(void) {}
void js_pal_uart_write(uint8_t port, const uint8_t *data, uint32_t len)
{
    (void)port;
    (void)data;
    (void)len;
}
