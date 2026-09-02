/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 UART "HELLO" beacon — UNMODIFIED Keil C51 user source (Stage 2 T1/T5
 * headless proof).
 *
 * Production wasm-sim app: the firmware puts the on-chip UART in mode 1 (8-bit,
 * SCON = 0x40) and repeatedly transmits the string "HELLO\n" by writing each
 * byte to SBUF and busy-waiting on TI (the standard polled-TX idiom; TX stays
 * in task context, no UART ISR). At the functional level there is no baud-rate
 * timer model — a SBUF write emits synchronously and sets TI immediately.
 *
 * Every SBUF write crosses the live channel-2 route: js_pal_uart_write(0, ...)
 * -> the UniSim UARTBus TX timeline (mcs51_uart.cpp, Stage 2 T1). The headless
 * scenario asserts the bus payload with an ASSERT_BUS_PAYLOAD step (the bus
 * spy = UARTBus timeline + BusAnalyzer), proving real firmware output reaches
 * the simulated serial bus with no test-injection API. Built as a real
 * production app (cleanup -> .cpp -> links wink_mcs51_compat ->
 * wink_simulator.{js,wasm}); this original is never edited in place.
 */
#include <wink_mcu.h>

static const char HELLO[] = "HELLO\n";

static void uart_send(char c) {
    SBUF = c;       /* emits on channel 2; TI set synchronously by the model */
    while (!TI) {
        _nop_();    /* cooperative microstep while the byte "shifts out" */
    }
    TI = 0;         /* hardware does NOT auto-clear TI */
}

static void uart_send_str(const char *s) {
    while (*s) {
        uart_send(*s);
        s++;
    }
}

void main(void) {
    SCON = 0x40;    /* UART mode 1 (8-bit UART), REN=0 (TX only), TI=RI=0 */
    while (1) {
        uart_send_str(HELLO);
        /* Idle between beacons: spin on _nop_() (functional microsteps) so the
         * cooperative fiber yields and virtual time advances. */
        {
            unsigned int d;
            for (d = 0; d < 20000u; d++) {
                _nop_();
            }
        }
    }
}
