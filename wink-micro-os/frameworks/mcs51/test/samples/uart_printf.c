/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 UART — UNMODIFIED Keil C51 user source (M3).
 *
 * Classic 89C52 polled serial transmit: SCON mode 1 (8-bit UART), then the
 * universal Keil send idiom `SBUF = c; while(!TI); TI = 0;`. There is no
 * baud-rate/timer setup at the functional simulation level (AD-2) — the SBUF
 * write emits the byte immediately and TI is set synchronously, so the
 * `while(!TI)` poll closes on its first read. The super-loop is idle after the
 * banner is sent; all bytes are emitted up-front during the run budget. The
 * cleanup pass emits a .cpp copy; this original is never edited in place.
 */
#include <REGX52.H>

void UART_Init(void) {
    SCON = 0x50;    /* mode 1 (8-bit UART), REN enabled; TI=RI=0 */
}

void UART_SendByte(unsigned char c) {
    SBUF = c;       /* load TX byte -> emitted, TI set synchronously */
    while (!TI);    /* wait for transmit complete (closes immediately) */
    TI = 0;         /* software clears TI */
}

void UART_SendString(char *s) {
    while (*s) {
        UART_SendByte(*s);
        s++;
    }
}

void main(void) {
    unsigned char i;
    UART_Init();
    for (i = 0; i < 3; i++) {
        UART_SendString("MCS51-UART-OK\r\n");
    }
    while (1) {
        _nop_();    /* idle loop; interception point keeps the fiber yielding */
    }
}
