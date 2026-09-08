// SPDX-License-Identifier: Apache-2.0
// MCS-51 UART functional model (M3, AD-2).
#include "wink_mcs51_uart.h"

#include "mcs51_proxy.hpp"
#include "mcs51_context.h"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_isr.h"

#include <cstdint>
#include <cstdio>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

extern "C" void js_pal_uart_write(uint8_t port, const uint8_t* buf, uint32_t len);

namespace {

constexpr uint8_t SFR_SCON = 0x98;
constexpr uint8_t SFR_SBUF = 0x99;
constexpr uint8_t SFR_IE   = 0xA8;

constexpr uint8_t SCON_TI = 1u;   // SCON.1 transmit-complete flag
constexpr uint8_t SCON_RI = 0u;   // SCON.0 receive-complete flag
constexpr uint8_t SCON_REN = 4u;  // SCON.4 receive enable
constexpr uint8_t IE_ES   = 4u;   // IE.4 UART interrupt enable
constexpr uint8_t IE_EA   = 7u;   // IE.7 global interrupt enable

constexpr uint64_t RX_BYTE_SPACING_US = 1000ull;

inline Mcu51UartState& get_uart(void) {
    return mcs51_get_context()->uart;
}

inline void sfr_set_bit(uint8_t addr, uint8_t bit) {
    mcs51_get_context()->sfr_shadow[addr] =
        static_cast<uint8_t>(mcs51_get_context()->sfr_shadow[addr] | (1u << bit));
}

// Emits one byte to the host recording buffer and js_pal_uart_write.
void on_sbuf_write(void) {
    Mcu51UartState& uart = get_uart();
    uint8_t b = mcs51_get_context()->sfr_shadow[SFR_SBUF];

    putchar(static_cast<int>(b));
    if (b == '\n') {
        fflush(stdout);
    }

    if (uart.count < MCS51_UART_CAPTURE_CAP) {
        uart.capture[uart.count++] = b;
    }
    js_pal_uart_write(0, &b, 1);

    sfr_set_bit(SFR_SCON, SCON_TI);
    mcs51_raise_irq(IRQ_SOURCE_UART0);
}

// Deliver one pending RX byte per the hardware rules. Returns true while more
// bytes may be deliverable. Pure state machine; runs on the fiber context.
bool rx_deliver_one(void) {
    Mcu51UartState& uart = get_uart();
    if (uart.rx_tail == uart.rx_head) {
        return false;  // FIFO empty
    }
    uint8_t scon = mcs51_get_context()->sfr_shadow[SFR_SCON];
    if ((scon & (1u << SCON_REN)) == 0) {
        return false;  // receiver disabled: bytes stay queued until REN=1
    }
    uint64_t now = wink_mcs51_virtual_us();
    if (uart.rx_have_delivered &&
        (now - uart.rx_last_deliver_us) < RX_BYTE_SPACING_US) {
        return false;  // too early; remaining bytes land on later microsteps
    }
    uint8_t b = uart.rx_fifo[uart.rx_tail % MCS51_UART_RX_FIFO_CAP];
    uart.rx_tail++;
    if (scon & (1u << SCON_RI)) {
        if (uart.rx_dropped < 0xFFFFFFFFu) {
            ++uart.rx_dropped;
        }
        return uart.rx_tail != uart.rx_head;
    }
    mcs51_get_context()->sfr_shadow[SFR_SBUF] = b;
    sfr_set_bit(SFR_SCON, SCON_RI);
    uart.rx_last_deliver_us = now;
    uart.rx_have_delivered = true;
    mcs51_raise_irq(IRQ_SOURCE_UART0);
    return uart.rx_tail != uart.rx_head;
}

static void sfr_write_hook_uart(struct Mcu51Context* ctx, uint8_t addr,
                                uint8_t old_val, uint8_t new_val) {
    (void)ctx;
    (void)old_val;
    (void)new_val;
    if (addr == SFR_SBUF) {
        on_sbuf_write();
    }
}

}  // namespace

extern "C" {

uint32_t wink_mcs51_uart_capture_count(void) { return get_uart().count; }
uint8_t  wink_mcs51_uart_capture_byte(uint32_t idx) {
    Mcu51UartState& uart = get_uart();
    return (idx < uart.count) ? uart.capture[idx] : 0u;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void wink_mcs51_uart_rx_push(uint8_t byte) {
    Mcu51UartState& uart = get_uart();
    uint32_t used = uart.rx_head - uart.rx_tail;
    if (used >= MCS51_UART_RX_FIFO_CAP) {
        if (uart.rx_dropped < 0xFFFFFFFFu) {
            ++uart.rx_dropped;
        }
        return;
    }
    uart.rx_fifo[uart.rx_head % MCS51_UART_RX_FIFO_CAP] = byte;
    ++uart.rx_head;
}

void wink_mcs51_uart_rx_drain(void) {
    for (uint32_t i = 0; i < MCS51_UART_RX_FIFO_CAP; ++i) {
        if (!rx_deliver_one()) {
            break;
        }
    }
    mcs51_irq_scan_and_dispatch();
}

uint32_t wink_mcs51_uart_rx_dropped(void) {
    return get_uart().rx_dropped;
}

void wink_mcs51_uart_on_write(uint8_t addr) {
    if (addr == SFR_SBUF) {
        on_sbuf_write();
    }
}

void wink_mcs51_uart_on_read(uint8_t /*addr*/) {}

void mcs51_uart_reset(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    ctx->uart.count = 0;
    ctx->uart.capture[0] = 0;
    ctx->sfr_shadow[SFR_SCON] &=
        static_cast<uint8_t>(~((1u << SCON_TI) | (1u << SCON_RI)));
    ctx->uart.rx_head = 0;
    ctx->uart.rx_tail = 0;
    ctx->uart.rx_dropped = 0;
    ctx->uart.rx_have_delivered = false;
    ctx->uart.rx_last_deliver_us = 0;
}

void wink_mcs51_uart_reset(void) {
    mcs51_uart_reset(mcs51_get_context());
}

void mcs51_uart_init(struct Mcu51Context* ctx) {
    mcs51_uart_reset(ctx);
    mcs51_trap_register_sfr_write(SFR_SBUF, sfr_write_hook_uart);
}

void mcs51_uart_poll(struct Mcu51Context* ctx) {
    (void)ctx;
    wink_mcs51_uart_rx_drain();
}

uint64_t mcs51_uart_next_event_us(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    const Mcu51UartState& uart = ctx->uart;
    if (uart.rx_tail != uart.rx_head) {
        if (!uart.rx_have_delivered) {
            return ctx->virtual_us;
        }
        uint64_t earliest = uart.rx_last_deliver_us + RX_BYTE_SPACING_US;
        return (earliest > ctx->virtual_us) ? earliest : ctx->virtual_us;
    }
    return UINT64_MAX;
}

uint32_t wink_mcs51_uart_byte_count(void) {
    return get_uart().count;
}

uint8_t wink_mcs51_uart_byte_at(uint32_t i) {
    Mcu51UartState& uart = get_uart();
    if (i >= uart.count) {
        return 0;
    }
    return uart.capture[i];
}

}  // extern "C"
