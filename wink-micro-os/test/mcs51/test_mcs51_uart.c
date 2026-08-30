/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 M3 host test: UART SBUF write emits bytes to the console sink and the
 * C-ABI capture buffer, with TI set synchronously so the classic Keil idiom
 * `SBUF = c; while(!TI); TI = 0;` closes on the first TI read (AD-2).
 *
 * The unmodified Keil sample (uart_printf.c) sends "MCS51-UART-OK\r\n" three
 * times. We assert the exact captured byte sequence via the framework C ABI
 * (no stdout parsing); the PASS line also surfaces on stdout. ctest gates on
 * the process exit code.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "wink_runtime.h"
#include "wink_app.h"
#include "wink_status.h"

extern const wink_app_callbacks_t *wink_app_get_callbacks(void);

/* C-ABI observability from the mcs51 framework. */
extern uint32_t wink_mcs51_uart_byte_count(void);
extern uint8_t  wink_mcs51_uart_byte_at(uint32_t i);

/* Host fallback for the channel-2 live bridge (mcs51_uni_bridge.cpp, host
 * only): records every SBUF write routed via js_pal_uart_write. */
extern uint32_t wink_mcs51_host_uart_tx_count(void);
extern uint8_t  wink_mcs51_host_uart_tx_byte(uint32_t i);
extern uint8_t  wink_mcs51_host_uart_tx_port(uint32_t i);

#define RUN_TICKS 100u

/* Exactly what the sample sends ("MCS51-UART-OK\r\n" three times). */
#define UART_LINE    "MCS51-UART-OK\r\n"

void setUp(void) {}
void tearDown(void) {}

int main(void) {
    const wink_app_callbacks_t *cb = wink_app_get_callbacks();
    if (cb == NULL || cb->loop == NULL) {
        printf("[mcs51] FAIL: callbacks/loop not bound\n");
        return 1;
    }

    wink_status_t st = wink_runtime_run(cb, RUN_TICKS);
    if (st != WINK_OK) {
        printf("[mcs51] FAIL: runtime run returned %d\n", (int)st);
        return 1;
    }

    int fails = 0;

    static const char expected[] = UART_LINE UART_LINE UART_LINE;
    uint32_t want_len = (uint32_t)(sizeof(expected) - 1u);
    uint32_t got_len  = wink_mcs51_uart_byte_count();

    if (got_len != want_len) {
        printf("[mcs51] FAIL: UART captured %u bytes, want %u\n",
               (unsigned)got_len, (unsigned)want_len);
        fails++;
    }

    uint32_t check = got_len < want_len ? got_len : want_len;
    for (uint32_t i = 0; i < check; i++) {
        if (wink_mcs51_uart_byte_at(i) != (uint8_t)expected[i]) {
            printf("[mcs51] FAIL: byte %u is 0x%02X ('%c'), want 0x%02X ('%c')\n",
                   (unsigned)i,
                   (unsigned)wink_mcs51_uart_byte_at(i),
                   (char)wink_mcs51_uart_byte_at(i),
                   (unsigned)(uint8_t)expected[i], (char)expected[i]);
            fails++;
            if (fails > 5) {
                break;  // do not flood on a total mismatch
            }
        }
    }

    /* Live channel-2 route: every SBUF write must also reach js_pal_uart_write
     * (host recording fallback), port 0, same byte sequence. */
    uint32_t tx_len = wink_mcs51_host_uart_tx_count();
    if (tx_len != want_len) {
        printf("[mcs51] FAIL: js_pal_uart_write saw %u bytes, want %u\n",
               (unsigned)tx_len, (unsigned)want_len);
        fails++;
    }
    for (uint32_t i = 0; i < tx_len && i < want_len; i++) {
        if (wink_mcs51_host_uart_tx_port(i) != 0u) {
            printf("[mcs51] FAIL: tx byte %u routed to port %u, want 0\n",
                   (unsigned)i, (unsigned)wink_mcs51_host_uart_tx_port(i));
            fails++;
            break;
        }
        if (wink_mcs51_host_uart_tx_byte(i) != (uint8_t)expected[i]) {
            printf("[mcs51] FAIL: tx byte %u is 0x%02X, want 0x%02X\n",
                   (unsigned)i, (unsigned)wink_mcs51_host_uart_tx_byte(i),
                   (unsigned)(uint8_t)expected[i]);
            fails++;
            if (fails > 8) {
                break;
            }
        }
    }

    if (fails) {
        return 1;
    }
    printf("[mcs51] PASS: UART captured %u bytes == 3x \"MCS51-UART-OK\\r\\n\" "
           "(TI set synchronously, console + capture + live ch2 route)\n",
           (unsigned)got_len);
    return 0;
}
