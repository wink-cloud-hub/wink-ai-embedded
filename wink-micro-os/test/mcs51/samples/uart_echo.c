/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 UART RX/TX echo — UNMODIFIED Keil C51 user source (Stage 2 T2).
 *
 * Standard 8051 serial echo: SCON mode 1 with REN enabled, EA+ES on. The UART
 * ISR (vector 4) receives a byte on RI, stashes it in a one-deep mailbox and
 * clears RI; the main loop retransmits any stashed byte via the polled
 * `SBUF = c; while(!TI); TI = 0;` idiom (TX stays in task context, never in
 * ISR context). At the functional level there is no baud/timer model — RX
 * bytes are injected by the simulation channel and SBUF writes emit
 * synchronously. The cleanup pass emits a .cpp copy; this original is never
 * edited in place.
 */
#include <REGX52.H>

static unsigned char echo_byte;
static unsigned char echo_pending;

void UART_ISR(void) interrupt 4 {
    /* RX is interrupt-driven; TX stays polled in the main loop, so the ISR
     * handles RI only — clearing TI here would make the task's `while(!TI)`
     * poll hang (the standard textbook RX-irq/TX-poll split). */
    if (RI) {
        echo_byte = SBUF;   /* read received byte (SBUF shadow) */
        RI = 0;             /* hardware does NOT auto-clear RI */
        echo_pending = 1;
    }
}

void main(void) {
    /* Enable the UART interrupt source before REN so the first received byte
     * is already gated into the ISR (hardware-valid init order). */
    echo_pending = 0;
    EA = 1;
    ES = 1;
    SCON = 0x50;            /* mode 1 (8-bit UART), REN enabled; TI=RI=0 */
    while (1) {
        if (echo_pending) {
            unsigned char c = echo_byte;
            echo_pending = 0;
            SBUF = c;       /* echo back -> emitted, TI set synchronously */
            while (!TI);
            TI = 0;
        }
        _nop_();            /* cooperative microstep / RX drain point */
    }
}
