// SPDX-License-Identifier: Apache-2.0
// MCS-51 UART functional model (M3, AD-2).
#include "wink_mcs51_uart.h"

#include "mcs51_proxy.hpp"
#include "mcs51_context.h"
#include "mcs51_family.h"
#include "mcs51_sfr_map.h"
#include "mcs51_trap.h"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_isr.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#ifndef WINK_MCS51_STRICT
#include "pal_log.h"
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

extern "C" void js_pal_uart_write(uint8_t port, const uint8_t* buf, uint32_t len);

// Keil C51 printf bridge sink (baseline-exception fix): the vendor shim
// header redirects printf through `char putchar(char)`, but the only
// definition lived in one test TU, so every other TU using the hijacked
// printf failed to link on MinGW. Single definition point in the sim
// library (host/wasm console); the test-local shim is removed to avoid
// a duplicate symbol.
//
// Stage6 review S6-H7: the sink is WEAK so carrier apps that legitimately
// define their own `char putchar(char)` (vendor uart0_printf/uart0_rxtx
// route printf through SBUF) override it instead of colliding at link time.
// GCC/Clang/emcc honor __attribute__((weak)); MSVC (where the framework flag
// chain erases __attribute__) keeps the strong definition — no MSVC-built
// carrier defines putchar.
#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
char putchar(char ch) { return static_cast<char>(fputc(ch, stdout)); }

namespace {

constexpr uint8_t SFR_SCON = 0x98;
constexpr uint8_t SFR_SBUF = 0x99;
constexpr uint8_t SFR_IE   = 0xA8;
constexpr uint8_t SFR_TCON = 0x88;
constexpr uint8_t SFR_TMOD = 0x89;
constexpr uint8_t SFR_PCON = 0x87;
constexpr uint8_t SFR_TH1  = 0x8D;
// Stage4 CPL-04: clock-select / remap / BRT addresses live in the chip
// package. The CKCON divider address stays generic (multi-vendor address,
// per-family semantics resolved by the shadow value itself).
constexpr uint8_t SFR_CKCON = MCS51_SFR_CKCON;

constexpr uint8_t SCON_TI = 1u;   // SCON.1 transmit-complete flag
constexpr uint8_t SCON_RI = 0u;   // SCON.0 receive-complete flag
constexpr uint8_t SCON_REN = 4u;  // SCON.4 receive enable
constexpr uint8_t SCON_SM0 = 7u;  // SCON.7: mode 3 (11-bit) iff set
constexpr uint8_t SCON_SM1 = 6u;  // SCON.6: async modes 1/3 iff set
constexpr uint8_t PCON_SMOD0 = 7u;  // PCON.7: baud doubler
constexpr uint8_t CKCON_T1M = 4u;   // CKCON.4: Timer1 1T select
constexpr uint8_t IE_ES   = 4u;   // IE.4 UART interrupt enable
constexpr uint8_t IE_EA   = 7u;   // IE.7 global interrupt enable
constexpr uint8_t TCON_TR1 = 6u;  // TCON.6 Timer1 run control

constexpr uint64_t RX_BYTE_SPACING_US = 1000ull;

// Per-reason saturating trigger counters + warn-once latches (plain POD
// BSS state, same pattern as mcs51_unsupported.cpp). Indexed 0..3 in
// WINK_MCS51_UART_NOTREADY_* bit order. STRICT builds abort before counting,
// so the counters stay 0 there by design.
uint32_t s_notready_triggered[4] = {};
#ifndef WINK_MCS51_STRICT
bool     s_notready_warned[4] = {};
#endif

// GAP-25 SBUF overwrite (separate counter, M4): TI still set from the
// previous byte when a new write lands. STRICT aborts before counting.
uint32_t s_overwrite_triggered = 0u;
#ifndef WINK_MCS51_STRICT
bool s_overwrite_warned = false;
#endif

constexpr const char* kNotreadyNames[4] = {
    "baud source not running/unmodeled",
    "SCON not async mode 1/3",
    "TXD pin not connected",
    "RXD path not connected (REN=1)",
};

inline uint8_t reason_index(uint32_t reason_bit) {
    switch (reason_bit) {
        case WINK_MCS51_UART_NOTREADY_BAUD: return 0u;
        case WINK_MCS51_UART_NOTREADY_MODE: return 1u;
        case WINK_MCS51_UART_NOTREADY_TXD:  return 2u;
        case WINK_MCS51_UART_NOTREADY_RXD:  return 3u;
        default: return 0xFFu;
    }
}

// Timer1 as UART baud source: TR1 running + TMOD mode 2 (8-bit auto-reload).
// The reload VALUE (TH1) only affects the rate, not readiness, so it is
// deliberately not validated here (rate accuracy is out of scope).
inline bool timer1_baud_ready(const Mcu51Context* ctx) {
    const uint8_t tcon = ctx->sfr_shadow[SFR_TCON];
    const uint8_t tmod = ctx->sfr_shadow[SFR_TMOD];
    return ((tcon >> TCON_TR1) & 1u) != 0 &&
           ((tmod >> 4u) & 0x03u) == 0x02u;
}

// GAP-02 TX/RX-link readiness predicate, STANDARD path (pure: no logging,
// no counters). Classic parts have fixed pins (TXD=P3.1, RXD=P3.0) and
// Timer1 as the only baud source; enhanced families add their source/remap
// branches through the uart_hooks (S4-D3).
uint32_t uart_notready_mask_std(const Mcu51Context* ctx) {
    uint32_t mask = 0u;
    const uint8_t scon = ctx->sfr_shadow[SFR_SCON];
    if (((scon >> SCON_SM1) & 1u) == 0) {
        // Modes 0/2 (shift register / 9-bit sync): no async framing.
        mask |= WINK_MCS51_UART_NOTREADY_MODE;
    }
    if (!timer1_baud_ready(ctx)) {
        mask |= WINK_MCS51_UART_NOTREADY_BAUD;
    }
    return mask;
}

uint32_t uart_notready_mask_impl(const Mcu51Context* ctx) {
    if ((ctx->caps_cache & MCS51_CAP_UART_REMAP) != 0u &&
        ctx->uart_hooks.notready_mask != nullptr) {
        return ctx->uart_hooks.notready_mask(const_cast<Mcu51Context*>(ctx));
    }
    return uart_notready_mask_std(ctx);
}

// GAP-25 SBUF-overwrite policy. Keil idiom clears TI before the next
// write (`while(!TI); TI=0;`); a write landing with TI still set means the
// previous frame was never consumed — on silicon the shift register is
// corrupted. Independent of the A-01 readiness gate below.
void uart_overwrite_policy(void) {
#ifdef WINK_MCS51_STRICT
    assert(0 && "UART SBUF rewritten with TI still set (WINK_MCS51_STRICT)");
    std::abort();
#else
    if (s_overwrite_triggered < 0xFFFFFFFFu) {
        ++s_overwrite_triggered;
    }
    if (!s_overwrite_warned) {
        s_overwrite_warned = true;
        pal_log_w("MCS51", "UART SBUF rewritten with TI still set: "
                           "previous frame unconsumed (GAP-25)");
    }
#endif
}

// Policy on a non-zero readiness mask. STRICT: fail loudly at the exact
// misconfiguration (assert + unconditional abort, mirroring
// mcs51_unsupported.cpp so NDEBUG STRICT builds still trap). Release: warn
// once per reason, count every occurrence (saturating), then fall through
// so the byte is still sent and observable scenarios stay green.
void uart_notready_policy(uint32_t mask) {
#ifdef WINK_MCS51_STRICT
    (void)mask;
    assert(0 && "UART TX link not ready (WINK_MCS51_STRICT)");
    std::abort();
#else
    for (uint8_t i = 0; i < 4; ++i) {
        const uint32_t bit = (1u << i);
        if ((mask & bit) == 0) {
            continue;
        }
        if (s_notready_triggered[i] < 0xFFFFFFFFu) {
            ++s_notready_triggered[i];
        }
        if (!s_notready_warned[i]) {
            s_notready_warned[i] = true;
            pal_log_w("MCS51", "UART TX link not ready: %s", kNotreadyNames[i]);
        }
    }
#endif
}

inline Mcu51UartState& get_uart(void) {
    return mcs51_get_context()->uart;
}

inline void sfr_set_bit(uint8_t addr, uint8_t bit) {
    mcs51_get_context()->sfr_shadow[addr] =
        static_cast<uint8_t>(mcs51_get_context()->sfr_shadow[addr] | (1u << bit));
}

// ADR-0081 D2: STANDARD baud rate in Hz (Timer1 source only). Classic parts
// have no T1M (fixed 12T semantics, T=3); the CKCON divider select applies
// only to families exposing the XSFR window, where the register exists. On
// classic parts 0x8E is an undefined SFR: a stray firmware write there must
// not silently change the baud rate. Returns 0 when uncomputable (zero
// divisor). Enhanced sources (TMR4/TMR2/BRT) live behind the uart_hooks
// (S4-D3).
// ADR-0081 test observability: baud (Hz) used for the most recent charged
// byte; 0 when the last write was unready/uncomputable or none yet.
uint32_t s_last_baud_hz = 0u;

uint32_t uart_baud_hz_std(const Mcu51Context* ctx) {
    const uint32_t fsys = wink_mcs51_get_clock_hz();
    if (fsys == 0u) {
        return 0u;
    }
    const uint32_t smod =
        (((ctx->sfr_shadow[SFR_PCON] >> PCON_SMOD0) & 1u) != 0u) ? 2u : 1u;
    // A-01 guarantees TMOD mode 2 + TR1.
    uint32_t t = 3u;
    const bool has_xsfr =
        mcs51_family_has_xsfr(mcs51_family_desc(ctx->family));
    if (has_xsfr &&
        (((ctx->sfr_shadow[SFR_CKCON] >> CKCON_T1M) & 1u) != 0u)) {
        t = 1u;
    }
    const uint32_t n = 256u - ctx->sfr_shadow[SFR_TH1];
    if (n == 0u) {
        return 0u;
    }
    return (fsys * smod) / (128u * t * n);  // 32*K, K=4
}

uint32_t uart_baud_hz_impl(const Mcu51Context* ctx) {
    if ((ctx->caps_cache & MCS51_CAP_UART_REMAP) != 0u &&
        ctx->uart_hooks.baud_hz != nullptr) {
        return ctx->uart_hooks.baud_hz(const_cast<Mcu51Context*>(ctx));
    }
    return uart_baud_hz_std(ctx);
}

// Emits one byte to the host recording buffer and js_pal_uart_write.
void on_sbuf_write(void) {
    Mcu51Context* ctx = mcs51_get_context();
    // GAP-25: TI still set from the previous byte — the new write would
    // corrupt the in-flight frame on silicon. Counted/aborted here; the
    // byte is still sent below so the failure stays observable.
    if ((ctx->sfr_shadow[SFR_SCON] & (1u << SCON_TI)) != 0u) {
        uart_overwrite_policy();
    }
    // GAP-02 TX-link readiness gate (A-01): a misconfigured link (baud
    // source stopped, wrong SCON mode, unconnected RXD path) must never be
    // silent. The byte is still sent afterwards (release) so existing
    // scenarios keep running while the misconfiguration is visible.
    const uint32_t notready = uart_notready_mask_impl(ctx);
    if (notready != 0u) {
        uart_notready_policy(notready);
    } else {
        // ADR-0081 D1/D4: synchronous per-byte charge before TI. TI is
        // still set synchronously below, so while(!TI) always terminates.
        const uint32_t baud = uart_baud_hz_impl(ctx);
        s_last_baud_hz = baud;
        if (baud == 0u) {
            uart_notready_policy(WINK_MCS51_UART_NOTREADY_BAUD);
        } else {
            const uint8_t scon = ctx->sfr_shadow[SFR_SCON];
            const uint32_t frame_bits =
                (((scon >> SCON_SM0) & 1u) != 0u) ? 11u : 10u;
            wink_mcs51_charge_us(frame_bits * 1000000u / baud);
        }
    }

    Mcu51UartState& uart = get_uart();
    uint8_t b = mcs51_get_context()->sfr_shadow[SFR_SBUF];

    // Console mirror via fputc (NOT putchar): a carrier-overridden putchar
    // writes SBUF, which would re-enter this handler (S6-H7).
    fputc(b, stdout);
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

// M3: C language linkage for the C-ABI hook table (internal linkage comes
// from the enclosing anonymous namespace; `static` must NOT be combined
// with a linkage specification).
extern "C" void sfr_write_hook_uart(struct Mcu51Context* ctx, uint8_t addr,
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

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
uint32_t wink_mcs51_uart_notready_mask(void) {
    return uart_notready_mask_impl(mcs51_get_context());
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
uint32_t wink_mcs51_uart_notready_count(uint32_t reason_bit) {
    const uint8_t idx = reason_index(reason_bit);
    return (idx < 4u) ? s_notready_triggered[idx] : 0u;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
uint32_t wink_mcs51_uart_notready_total(void) {
    // Aggregate across all reason buckets (GAP-10 runner verdict).
    return s_notready_triggered[0] + s_notready_triggered[1] +
           s_notready_triggered[2] + s_notready_triggered[3];
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
uint32_t wink_mcs51_uart_overwrite_total(void) {
    // GAP-25 SBUF rewrite with TI still set (GAP-10 runner verdict).
    return s_overwrite_triggered;
}

// ADR-0081 test observability: baud (Hz) used for the most recent charged
// byte; 0 when the last write was unready/uncomputable or none yet.
uint32_t wink_mcs51_uart_last_baud_hz(void) {
    return s_last_baud_hz;
}

void wink_mcs51_uart_on_write(uint8_t addr) {
    if (addr == SFR_SBUF) {
        on_sbuf_write();
    }
}

void wink_mcs51_uart_on_read(uint8_t /*addr*/) {}

void mcs51_uart_reset(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    for (uint8_t i = 0; i < 4; ++i) {        s_notready_triggered[i] = 0;
#ifndef WINK_MCS51_STRICT
        s_notready_warned[i] = false;
#endif
    }
    s_overwrite_triggered = 0u;
#ifndef WINK_MCS51_STRICT
    s_overwrite_warned = false;
#endif
    ctx->uart.count = 0;
    ctx->uart.capture[0] = 0;
    ctx->sfr_shadow[SFR_SCON] &=
        static_cast<uint8_t>(~((1u << SCON_TI) | (1u << SCON_RI)));
    ctx->uart.rx_head = 0;
    ctx->uart.rx_tail = 0;
    ctx->uart.rx_dropped = 0;
    ctx->uart.rx_have_delivered = false;
    ctx->uart.rx_last_deliver_us = 0;
    s_last_baud_hz = 0u;
    // S4-H2 (reset-rebuilds-registration contract): the SBUF hook lives
    // here, not in init — init delegates to reset, so both paths install it.
    mcs51_trap_register_sfr_write(SFR_SBUF, sfr_write_hook_uart);
}

void wink_mcs51_uart_reset(void) {
    mcs51_uart_reset(mcs51_get_context());
}

void mcs51_uart_init(struct Mcu51Context* ctx) {
    mcs51_uart_reset(ctx);
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
