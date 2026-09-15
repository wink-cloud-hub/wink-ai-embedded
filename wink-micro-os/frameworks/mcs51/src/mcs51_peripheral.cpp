// SPDX-License-Identifier: GPL-3.0-only
// Task R1: MCS-51 strong peripheral descriptor table definition (ADR-0004).
#include "mcs51_peripheral.h"
#include "mcs51_context.h"

#include <cassert>
#include <cstdlib>
#include <cstring>

// Lower-bound sanity only: the exact worst case (core + chip package
// descriptors) spans TUs and is enforced at registration time below.
static_assert(MCS51_MAX_PERIPHERALS >= 6u,
              "registry must hold core + chip descriptors");

extern "C" {

void mcs51_timer_init(struct Mcu51Context* ctx);
void mcs51_timer_reset(struct Mcu51Context* ctx);
void mcs51_timer_poll(struct Mcu51Context* ctx);
uint64_t mcs51_timer_next_event_us(struct Mcu51Context* ctx);

void mcs51_uart_init(struct Mcu51Context* ctx);
void mcs51_uart_reset(struct Mcu51Context* ctx);
void mcs51_uart_poll(struct Mcu51Context* ctx);
uint64_t mcs51_uart_next_event_us(struct Mcu51Context* ctx);

void mcs51_extint_init(struct Mcu51Context* ctx);
void mcs51_extint_reset(struct Mcu51Context* ctx);
void mcs51_extint_poll(struct Mcu51Context* ctx);
uint64_t mcs51_extint_next_event_us(struct Mcu51Context* ctx);

// M1: core 8051 models run on every family. Chip models (e.g. on-chip ADC,
// hardware signal generator, system protection) are NOT listed here —
// stage4 CPL-10 moved them to chip-package self-registration (the
// chips/*/src/*_register.cpp entries); loops cover core table + registry.
const mcs51_peripheral_desc_t g_mcs51_peripherals[] = {
    {
        "timer",
        mcs51_timer_init,
        mcs51_timer_reset,
        mcs51_timer_poll,
        mcs51_timer_next_event_us,
        MCS51_PHASE_CLOCK,
        MCS51_FAMILY_MASK_ALL
    },
    {
        "uart",
        mcs51_uart_init,
        mcs51_uart_reset,
        mcs51_uart_poll,
        mcs51_uart_next_event_us,
        MCS51_PHASE_RX_DRAIN,
        MCS51_FAMILY_MASK_ALL
    },
    {
        "extint",
        mcs51_extint_init,
        mcs51_extint_reset,
        mcs51_extint_poll,
        mcs51_extint_next_event_us,
        MCS51_PHASE_EXTINT,
        MCS51_FAMILY_MASK_ALL
    }
};

const uint8_t g_mcs51_num_peripherals =
    static_cast<uint8_t>(sizeof(g_mcs51_peripherals) / sizeof(g_mcs51_peripherals[0]));

// ── Stage4 CPL-10: chip self-registration registry (core-owned BSS) ─────────
namespace {

mcs51_peripheral_desc_t s_registered[MCS51_MAX_PERIPHERALS] = {};
uint8_t s_registered_count = 0u;

bool same_desc(const mcs51_peripheral_desc_t* a,
               const mcs51_peripheral_desc_t* b) {
    if (a == b) {
        return true;
    }
    if (a->name != nullptr && b->name != nullptr &&
        std::strcmp(a->name, b->name) == 0) {
        return true;
    }
    return a->init == b->init && a->reset == b->reset &&
           a->poll == b->poll && a->next_event_us == b->next_event_us;
}

}  // namespace

void mcs51_peripheral_register(const mcs51_peripheral_desc_t* desc) {
    if (desc == nullptr) {
        return;
    }
    for (uint8_t i = 0u; i < s_registered_count; ++i) {
        if (same_desc(&s_registered[i], desc)) {
            return;  // idempotent: already registered
        }
    }
    assert(s_registered_count < MCS51_MAX_PERIPHERALS);
    if (s_registered_count >= MCS51_MAX_PERIPHERALS) {
        // Contract failure (ADR-0012 honesty): capacity is a build-time
        // fact; NDEBUG compiles the assert out, so abort unconditionally. A
        // silent drop would disable chip models with no trace.
        assert(0 && "mcs51 peripheral registry overflow");
        std::abort();
    }
    s_registered[s_registered_count] = *desc;  // POD copy, no heap
    ++s_registered_count;
}

uint8_t mcs51_peripheral_registered_count(void) {
    return s_registered_count;
}

const mcs51_peripheral_desc_t* mcs51_peripheral_registered(uint8_t i) {
    return (i < s_registered_count) ? &s_registered[i] : nullptr;
}

void mcs51_peripheral_registry_reset(void) {
    std::memset(s_registered, 0, sizeof(s_registered));
    s_registered_count = 0u;
}

} // extern "C"
