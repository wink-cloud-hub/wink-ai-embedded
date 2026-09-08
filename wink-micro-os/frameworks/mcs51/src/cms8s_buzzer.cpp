// SPDX-License-Identifier: Apache-2.0
// CMS8S78xx on-chip Buzzer peripheral model (ADR-0004 static dispatch).
#include "cms8s_buzzer.h"

#include "mcs51_context.h"
#include "mcs51_trap.h"
#include "wink_mcs51_gpio.h"
#include <cstdint>

namespace {

constexpr uint8_t  SFR_BUZDIV = 0xBE;
constexpr uint8_t  SFR_BUZCON = 0xBF;
constexpr uint16_t XSFR_P03CFG = 0xF003;
constexpr uint8_t  GPIO_P03_MUX_BUZZ = 0x05;
constexpr uint16_t BUZZER_PIN = 3u; // P0.3: (0 << 3) | 3

constexpr uint8_t BUZCON_BUZEN_MASK = 0x80u;
constexpr uint8_t BUZCON_BUZCKS_MASK = 0x03u;

struct Cms8sBuzzerPriv {
    bool     running;
    uint8_t  pin_level;
    uint32_t half_period_us;
    uint64_t next_toggle_us;
    uint32_t toggle_count;
};

static Cms8sBuzzerPriv s_buzzer = {};

void update_buzzer_state(Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();

    const uint8_t buzcon = ctx->sfr_shadow[SFR_BUZCON];
    const uint8_t buzdiv = ctx->sfr_shadow[SFR_BUZDIV];
    const bool buzen = (buzcon & BUZCON_BUZEN_MASK) != 0;
    const uint8_t p03cfg = ctx->xdata_shadow[XSFR_P03CFG];

    const bool should_run = buzen && (buzdiv > 0) && (p03cfg == GPIO_P03_MUX_BUZZ);

    if (should_run) {
        const uint8_t cks_sel = buzcon & BUZCON_BUZCKS_MASK;
        const uint32_t prescaler = 8u << cks_sel; // 8, 16, 32, 64
        const uint32_t clk_hz = ctx->clock_hz ? ctx->clock_hz : 24000000u;
        uint64_t half_us = (static_cast<uint64_t>(prescaler) * buzdiv * 1000000ull) / clk_hz;
        if (half_us == 0) half_us = 1;
        s_buzzer.half_period_us = static_cast<uint32_t>(half_us);

        if (!s_buzzer.running) {
            s_buzzer.running = true;
            s_buzzer.pin_level = 1u;
            js_pal_gpio_write(BUZZER_PIN, true, MCS51_DRIVE_SUPPLY);
            s_buzzer.next_toggle_us = ctx->virtual_us + s_buzzer.half_period_us;
        }
    } else {
        if (s_buzzer.running) {
            s_buzzer.running = false;
            s_buzzer.next_toggle_us = UINT64_MAX;
            s_buzzer.pin_level = 0u;
            js_pal_gpio_write(BUZZER_PIN, false, MCS51_DRIVE_SUPPLY);
        }
    }
}

void on_buzzer_sfr_write(Mcu51Context* ctx, uint8_t addr, uint8_t old_val, uint8_t new_val) {
    (void)addr;
    (void)old_val;
    (void)new_val;
    update_buzzer_state(ctx);
}

}  // namespace

extern "C" {

void cms8s_buzzer_reset(struct Mcu51Context* ctx) {
    (void)ctx;
    s_buzzer.running = false;
    s_buzzer.pin_level = 0u;
    s_buzzer.half_period_us = 0u;
    s_buzzer.next_toggle_us = UINT64_MAX;
    s_buzzer.toggle_count = 0u;
}

void cms8s_buzzer_init(struct Mcu51Context* ctx) {
    cms8s_buzzer_reset(ctx);
    mcs51_trap_register_sfr_write(SFR_BUZCON, on_buzzer_sfr_write);
    mcs51_trap_register_sfr_write(SFR_BUZDIV, on_buzzer_sfr_write);
}

void cms8s_buzzer_poll(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();

    update_buzzer_state(ctx);

    if (!s_buzzer.running || s_buzzer.next_toggle_us == UINT64_MAX) {
        return;
    }

    constexpr uint32_t MAX_TOGGLES_PER_POLL = 1000u;
    uint32_t toggles = 0u;
    while (s_buzzer.running && ctx->virtual_us >= s_buzzer.next_toggle_us) {
        s_buzzer.pin_level ^= 1u;
        js_pal_gpio_write(BUZZER_PIN, s_buzzer.pin_level != 0, MCS51_DRIVE_SUPPLY);
        s_buzzer.toggle_count++;
        s_buzzer.next_toggle_us += s_buzzer.half_period_us;
        if (++toggles >= MAX_TOGGLES_PER_POLL) {
            if (ctx->virtual_us >= s_buzzer.next_toggle_us) {
                s_buzzer.next_toggle_us = ctx->virtual_us + s_buzzer.half_period_us;
            }
            break;
        }
    }
}

uint64_t cms8s_buzzer_next_event_us(struct Mcu51Context* ctx) {
    (void)ctx;
    if (s_buzzer.running && s_buzzer.next_toggle_us != UINT64_MAX) {
        return s_buzzer.next_toggle_us;
    }
    return UINT64_MAX;
}

bool cms8s_buzzer_is_running(void) {
    return s_buzzer.running;
}

uint32_t cms8s_buzzer_toggle_count(void) {
    return s_buzzer.toggle_count;
}

uint32_t cms8s_buzzer_half_period_us(void) {
    return s_buzzer.half_period_us;
}

}  // extern "C"
