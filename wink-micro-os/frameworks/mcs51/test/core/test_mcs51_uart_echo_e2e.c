/* SPDX-License-Identifier: Apache-2.0
 * Stage 2 T2 e2e (host + wasm/Node): UART RX -> ISR mailbox -> polled TX echo.
 *
 * The unmodified Keil sample (uart_echo.c) enables REN + EA + ES; its vector-4
 * ISR stashes each received byte and clears RI, and the main loop retransmits
 * it through the SBUF/TI idiom. The test injects a short byte sequence from
 * OUTSIDE the fiber (host: host fallback API at the post-init hook; wasm: the
 * Node driver pushes via the exported wink_mcs51_uart_rx_push KEEPALIVE) and
 * asserts the exact bytes come back through the live channel-2 TX route:
 *   host — mcs51_uni_bridge.cpp js_pal_uart_write recording fallback;
 *   wasm — the Node stub's js_pal_uart_write calls back into the exported
 *          mcs51_wasm_uart_accept_byte sink.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "wink_runtime.h"
#include "wink_app.h"
#include "wink_status.h"
#include "mcs51_trap.h"

extern const wink_app_callbacks_t *wink_app_get_callbacks(void);

/* RX injection entry (mcs51_uart.cpp; KEEPALIVE under emscripten). */
extern void wink_mcs51_uart_rx_push(uint8_t byte);

#define ECHO_SEQ  "Hi-8051"
#define RUN_TICKS 200u

#ifdef __EMSCRIPTEN__
/* TX route sink: the Node stub's js_pal_uart_write calls back into this
 * exported function (kept alive by -sEXPORTED_FUNCTIONS on this test). */
static uint8_t  s_tx_route[128];
static uint32_t s_tx_route_count;
int mcs51_wasm_uart_accept_byte(unsigned b) {
    if (s_tx_route_count < sizeof(s_tx_route)) {
        s_tx_route[s_tx_route_count] = (uint8_t)b;
    }
    ++s_tx_route_count;
    return 0;
}
#else
/* Host fallback observability (mcs51_uni_bridge.cpp). */
extern uint32_t wink_mcs51_host_uart_tx_count(void);
extern uint8_t  wink_mcs51_host_uart_tx_byte(uint32_t i);
#endif

/* Post-init hook: queue the RX bytes before the fiber starts. They drain on
 * the firmware's own microstep points (RI latch + vector 4 on fiber context). */
static void inject_rx(void) {
    const char *s = ECHO_SEQ;
    for (uint32_t i = 0; s[i] != '\0'; i++) {
        wink_mcs51_uart_rx_push((uint8_t)s[i]);
    }
}

void setUp(void) {}
void tearDown(void) {}

int main(void) {
    const wink_app_callbacks_t *cb = wink_app_get_callbacks();
    if (cb == NULL || cb->loop == NULL) {
        printf("[mcs51-echo] FAIL: callbacks/loop not bound\n");
        return 1;
    }
    mcs51_framework_set_post_init_hook(inject_rx);

    wink_status_t st = wink_runtime_run(cb, RUN_TICKS);
    if (st != WINK_OK) {
        printf("[mcs51-echo] FAIL: runtime run returned %d\n", (int)st);
        return 1;
    }

    uint32_t want_len = (uint32_t)(sizeof(ECHO_SEQ) - 1u);
    int fails = 0;

#ifdef __EMSCRIPTEN__
    uint32_t got = s_tx_route_count;
    if (got != want_len) {
        printf("[mcs51-echo-wasm] FAIL: echo route saw %u bytes, want %u\n",
               (unsigned)got, (unsigned)want_len);
        fails++;
    }
    for (uint32_t i = 0; i < got && i < want_len; i++) {
        if (s_tx_route[i] != (uint8_t)ECHO_SEQ[i]) {
            printf("[mcs51-echo-wasm] FAIL: echo byte %u is 0x%02X ('%c'), "
                   "want 0x%02X ('%c')\n", (unsigned)i, (unsigned)s_tx_route[i],
                   (char)s_tx_route[i], (unsigned)(uint8_t)ECHO_SEQ[i],
                   ECHO_SEQ[i]);
            fails++;
            if (fails > 5) break;
        }
    }
    if (!fails) {
        printf("[mcs51-echo-wasm] PASS: RX push -> ISR -> TX echo of \"%s\" "
               "(%u bytes) via live ch2 route\n", ECHO_SEQ, (unsigned)got);
    }
#else
    uint32_t got = wink_mcs51_host_uart_tx_count();
    if (got != want_len) {
        printf("[mcs51-echo] FAIL: echo route saw %u bytes, want %u\n",
               (unsigned)got, (unsigned)want_len);
        fails++;
    }
    for (uint32_t i = 0; i < got && i < want_len; i++) {
        if (wink_mcs51_host_uart_tx_byte(i) != (uint8_t)ECHO_SEQ[i]) {
            printf("[mcs51-echo] FAIL: echo byte %u is 0x%02X ('%c'), "
                   "want 0x%02X ('%c')\n", (unsigned)i,
                   (unsigned)wink_mcs51_host_uart_tx_byte(i),
                   (char)wink_mcs51_host_uart_tx_byte(i),
                   (unsigned)(uint8_t)ECHO_SEQ[i], ECHO_SEQ[i]);
            fails++;
            if (fails > 5) break;
        }
    }
    if (!fails) {
        printf("[mcs51-echo] PASS: RX push -> ISR -> TX echo of \"%s\" "
               "(%u bytes) via live ch2 route\n", ECHO_SEQ, (unsigned)got);
    }
#endif

    return fails ? 1 : 0;
}
