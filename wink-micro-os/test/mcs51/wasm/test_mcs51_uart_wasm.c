/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 M3 wasm test: UART SBUF write under emscripten fibers (ASYNCIFY),
 * mirroring test/mcs51/test_mcs51_uart.c. The unmodified Keil sample sends
 * "MCS51-UART-OK\r\n" three times via the polled SBUF/TI idiom.
 *
 * The model emits each byte through plain putchar (emscripten libc maps stdout
 * to Node fd 1, so the banner itself appears in the ctest-captured stdout) AND
 * into the C-ABI capture buffer. We assert the exact sequence in C (ctest gates
 * on the exit code, not on Node-side text matching) and echo the captured
 * bytes through the driver's own printf for the captured log. Built by
 * test/mcs51/wasm/add_wink_wasm_mcs51_test.cmake; no Unity — own main.
 */
#include <stdint.h>
#include <stdio.h>

#include "wink_runtime.h"
#include "wink_app.h"
#include "wink_status.h"

extern const wink_app_callbacks_t *wink_app_get_callbacks(void);

/* C-ABI observability from the mcs51 framework. */
extern uint32_t wink_mcs51_uart_byte_count(void);
extern uint8_t  wink_mcs51_uart_byte_at(uint32_t i);

#define RUN_TICKS 100u

/* Exactly what the sample sends ("MCS51-UART-OK\r\n" three times). */
#define UART_LINE    "MCS51-UART-OK\r\n"

int main(void) {
    const wink_app_callbacks_t *cb = wink_app_get_callbacks();
    if (cb == NULL || cb->loop == NULL) {
        printf("[mcs51-wasm] FAIL: callbacks/loop not bound\n");
        return 1;
    }

    wink_status_t st = wink_runtime_run(cb, RUN_TICKS);
    if (st != WINK_OK) {
        printf("[mcs51-wasm] FAIL: runtime run returned %d\n", (int)st);
        return 1;
    }

    int fails = 0;

    static const char expected[] = UART_LINE UART_LINE UART_LINE;
    uint32_t want_len = (uint32_t)(sizeof(expected) - 1u);
    uint32_t got_len  = wink_mcs51_uart_byte_count();

    if (got_len != want_len) {
        printf("[mcs51-wasm] FAIL: UART captured %u bytes, want %u\n",
               (unsigned)got_len, (unsigned)want_len);
        fails++;
    }

    /* Echo captured bytes to stdout (visible in the Node ctest log). */
    printf("[mcs51-wasm] captured %u bytes: ", (unsigned)got_len);
    for (uint32_t i = 0; i < got_len; i++) {
        uint8_t b = wink_mcs51_uart_byte_at(i);
        putchar((b == '\r' || b == '\n') ? '.' : (char)b);
    }
    putchar('\n');

    uint32_t check = got_len < want_len ? got_len : want_len;
    for (uint32_t i = 0; i < check; i++) {
        if (wink_mcs51_uart_byte_at(i) != (uint8_t)expected[i]) {
            printf("[mcs51-wasm] FAIL: byte %u is 0x%02X, want 0x%02X\n",
                   (unsigned)i, (unsigned)wink_mcs51_uart_byte_at(i),
                   (unsigned)(uint8_t)expected[i]);
            fails++;
            if (fails > 5) {
                break;
            }
        }
    }

    if (fails) {
        return 1;
    }
    printf("[mcs51-wasm] PASS: UART captured %u bytes == 3x \"MCS51-UART-OK\" "
           "(TI synchronous, stdout + capture sink under node)\n",
           (unsigned)got_len);
    return 0;
}
