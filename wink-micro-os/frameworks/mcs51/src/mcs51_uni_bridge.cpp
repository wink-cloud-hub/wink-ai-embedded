// SPDX-License-Identifier: LGPL-3.0-only
// Host-side fallback for the UniSim channel imports (ADR-0071 D3 / AD-8).
//
// Under emscripten these two symbols are JS imports (wink_sim_js.js in
// production, mcs51_wasm_node_stub.js in the bounded Node tests): the C side
// never defines them. On host (MSVC/GCC) there is no JS data plane, so the
// compat library supplies fallbacks: channel-1 pin notifications are recorded
// (tests assert the instant-notify path fired, with the exact linear pin id),
// and channel-3 analog pulls read 0.0 (tests inject via mcs51_adc_set_value,
// which overrides the pull).
#include <stdint.h>

#include "mcs51_bus_abi.h"

#ifndef __EMSCRIPTEN__

extern "C" {

// Mirrors targets/wasm/wasm_bridge.h:
//   void js_pal_gpio_write(uint16_t, bool, uint8_t strength) (ADR-0077).
#define MCS51_HOST_NOTIFY_LOG_SIZE 128u

static uint32_t s_host_gpio_notifies;
static uint16_t s_notify_pin[MCS51_HOST_NOTIFY_LOG_SIZE];
static uint8_t  s_notify_level[MCS51_HOST_NOTIFY_LOG_SIZE];
static uint8_t  s_notify_strength[MCS51_HOST_NOTIFY_LOG_SIZE];

extern uint64_t wink_mcs51_virtual_us(void);
extern void wink_mcs51_pwm_meter_update(uint16_t pin, uint8_t level, uint64_t timestamp_us);

void js_pal_gpio_write(uint16_t pin, bool level, uint8_t strength) {
    uint32_t i = s_host_gpio_notifies;
    if (i < MCS51_HOST_NOTIFY_LOG_SIZE) {
        s_notify_pin[i] = pin;
        s_notify_level[i] = level ? 1u : 0u;
        // Default 0 -> SUPPLY so a caller that omits strength still reads
        // push-pull (mirrors the host `strength ?? SUPPLY` skew fallback).
        s_notify_strength[i] = strength != 0u ? strength : 3u;
    }
    ++s_host_gpio_notifies;
    wink_mcs51_pwm_meter_update(pin, level ? 1u : 0u, wink_mcs51_virtual_us());
}

// P3 (PLAN-20260912-MCS51-P3-TRIS): MCU-driver release records. Host has no
// arbiter; the chip model's TRIS 1->0 / open-drain / reset releases land here
// so tests can assert the exact pin was released.
#define MCS51_HOST_RELEASE_LOG_SIZE 128u
static uint32_t s_host_gpio_releases;
static uint16_t s_release_pin[MCS51_HOST_RELEASE_LOG_SIZE];

void js_pal_gpio_release_mcu(uint16_t pin) {
    uint32_t i = s_host_gpio_releases;
    if (i < MCS51_HOST_RELEASE_LOG_SIZE) {
        s_release_pin[i] = pin;
    }
    ++s_host_gpio_releases;
}

// Channel-3 analog pull override (A-02 host test seam): per rail-key norm
// override (Stage1 dual-space partition, see mcs51_adc.h: keys 0~31 are MCU
// physical pins, 32~63 board channels; this [64] table already covers both,
// it is the partitioned space itself). has-flag false = no override
// (returns 0.0, legacy).
static float s_host_analog_norm[64];
static bool s_host_analog_has[64];

float js_pal_adc_read_norm(uint16_t pin) {
    if (pin < 64u && s_host_analog_has[pin]) {
        return s_host_analog_norm[pin];
    }
    return 0.0f;
}

void wink_mcs51_host_set_analog_norm(uint16_t pin, float norm) {
    if (pin < 64u) {
        s_host_analog_norm[pin] = norm;
        s_host_analog_has[pin] = true;
    }
}

void wink_mcs51_host_analog_reset(void) {
    for (uint32_t i = 0; i < 64u; ++i) {
        s_host_analog_norm[i] = 0.0f;
        s_host_analog_has[i] = false;
    }
}

// Channel-2 UART TX (mirrors targets/wasm/wasm_bridge.h:
// void js_pal_uart_write(uint8_t port, const uint8_t* buf, uint32_t len)).
// Host has no JS data plane: record the (port, byte) stream so tests can
// assert the live SBUF -> UARTBus route fired; production JS routes it to
// the UARTBus plugin via wink_sim_js.js.
#define MCS51_HOST_UART_LOG_SIZE 256u
static uint32_t s_host_uart_tx;
static uint8_t  s_uart_tx_port[MCS51_HOST_UART_LOG_SIZE];
static uint8_t  s_uart_tx_byte[MCS51_HOST_UART_LOG_SIZE];

void js_pal_uart_write(uint8_t port, const uint8_t* buf, uint32_t len) {
    for (uint32_t i = 0; i < len; ++i) {
        uint32_t j = s_host_uart_tx;
        if (j < MCS51_HOST_UART_LOG_SIZE) {
            s_uart_tx_port[j] = port;
            s_uart_tx_byte[j] = buf ? buf[i] : 0u;
        }
        ++s_host_uart_tx;
    }
}

// Channel-2 SPI transfer (mirrors targets/wasm/wasm_bridge.h:
//   bool js_pal_spi_transfer(uint8_t port, uint16_t device_id,
//                            const uint8_t* tx_buf, uint32_t len,
//                            uint8_t* rx_buf, uint8_t mode, uint32_t sck_hz)).
// Phase 2 prerequisite: the on-chip SPI master model will route byte
// exchanges here once the UniSim SPI session contract (ADR-0087, T2.2a)
// lands. Host has no JS bus data plane, so the fallback is fail-closed —
// report "not executed" instead of faking device bytes (ADR-0012 honesty over
// silent degradation). Emscripten builds import the JS implementation instead
// of linking this definition.
bool js_pal_spi_transfer(uint8_t port, uint16_t device_id,
                         const uint8_t* tx_buf, uint32_t len,
                         uint8_t* rx_buf, uint8_t mode, uint32_t sck_hz) {
    (void)port;
    (void)device_id;
    (void)tx_buf;
    (void)len;
    (void)rx_buf;
    (void)mode;
    (void)sck_hz;
    return false;
}

// Channel-2 I2C transfer (mirrors targets/wasm/wasm_bridge.h:
//   bool js_pal_i2c_transfer(uint8_t port, uint16_t dev_addr,
//                            const uint8_t* write_buf, uint32_t write_len,
//                            uint8_t* read_buf, uint32_t read_len)).
// Phase 2 prerequisite: the on-chip I2C master model will route whole
// transactions here (and per-byte sessions via the ADR-0086 ABI) once the
// UniSim engine wiring lands. Host has no JS bus data plane: fail closed
// (ADR-0012). Emscripten builds import the JS implementation instead.
bool js_pal_i2c_transfer(uint8_t port, uint16_t dev_addr,
                         const uint8_t* write_buf, uint32_t write_len,
                         uint8_t* read_buf, uint32_t read_len) {
    (void)port;
    (void)dev_addr;
    (void)write_buf;
    (void)write_len;
    (void)read_buf;
    (void)read_len;
    return false;
}

// ── CH2 Phase 2 bus mock (ADR-0085/0086/0087, plan T2.3-D) ────────────────
//
// The on-chip controller models route their commands through the status-
// returning ABI. Host has no JS bus engine, so these fallbacks answer
// WINK_ERR_UNSUPPORTED until a test enables the scriptable mock below; the
// model then keeps its Phase 1 in-chip fallback (fail closed, ADR-0012).
// With the mock enabled the tests drive the model data plane deterministically:
// address/write ACK-NACK injection, a scripted read byte, and session
// accounting. Emscripten builds import the JS implementations instead of
// linking these definitions.

#define MCS51_HOST_BUS_SESSION_MAX 4u

// ---- I2C scriptable mock ----
static bool s_host_i2c_mock_enabled = false;
static bool s_host_i2c_addr_ack = true;
static bool s_host_i2c_write_ack = true;
static uint8_t s_host_i2c_read_byte = 0x00u;
static bool s_host_i2c_session_active[MCS51_HOST_BUS_SESSION_MAX];
static bool s_host_i2c_session_addr_nack[MCS51_HOST_BUS_SESSION_MAX];
static uint8_t s_host_i2c_session_dir[MCS51_HOST_BUS_SESSION_MAX];
static uint32_t s_host_i2c_open_count;
static uint32_t s_host_i2c_restart_count;
static uint32_t s_host_i2c_write_count;
static uint32_t s_host_i2c_read_count;
static uint32_t s_host_i2c_close_count;

static void host_i2c_clear_sessions(void) {
    for (uint32_t i = 0; i < MCS51_HOST_BUS_SESSION_MAX; ++i) {
        s_host_i2c_session_active[i] = false;
        s_host_i2c_session_addr_nack[i] = false;
        s_host_i2c_session_dir[i] = 0u;
    }
}

void wink_mcs51_host_i2c_mock_reset(void) {
    host_i2c_clear_sessions();
    s_host_i2c_mock_enabled = false;
    s_host_i2c_addr_ack = true;
    s_host_i2c_write_ack = true;
    s_host_i2c_read_byte = 0x00u;
    s_host_i2c_open_count = 0u;
    s_host_i2c_restart_count = 0u;
    s_host_i2c_write_count = 0u;
    s_host_i2c_read_count = 0u;
    s_host_i2c_close_count = 0u;
}

void wink_mcs51_host_i2c_mock_enable(bool enable) {
    s_host_i2c_mock_enabled = enable;
}
void wink_mcs51_host_i2c_set_addr_ack(bool ack) {
    s_host_i2c_addr_ack = ack;
}
void wink_mcs51_host_i2c_set_write_ack(bool ack) {
    s_host_i2c_write_ack = ack;
}
void wink_mcs51_host_i2c_set_read_byte(uint8_t value) {
    s_host_i2c_read_byte = value;
}
uint32_t wink_mcs51_host_i2c_open_count(void) {
    return s_host_i2c_open_count;
}
uint32_t wink_mcs51_host_i2c_restart_count(void) {
    return s_host_i2c_restart_count;
}
uint32_t wink_mcs51_host_i2c_write_count(void) {
    return s_host_i2c_write_count;
}
uint32_t wink_mcs51_host_i2c_read_count(void) {
    return s_host_i2c_read_count;
}
uint32_t wink_mcs51_host_i2c_close_count(void) {
    return s_host_i2c_close_count;
}

wink_status_t js_pal_i2c_transfer_ex(uint8_t port, uint16_t dev_addr,
                                     const uint8_t* write_buf,
                                     uint32_t write_len, uint8_t* read_buf,
                                     uint32_t read_len,
                                     pal_i2c_result_t* result) {
    if (!s_host_i2c_mock_enabled) {
        return WINK_ERR_UNSUPPORTED;
    }
    if (result == NULL || port != 0u || dev_addr > 0x7Fu) {
        return WINK_ERR_INVALID_ARG;
    }
    if (write_len == 0u && read_len == 0u) {
        return WINK_ERR_INVALID_ARG;
    }
    if (write_len > 0u && write_buf == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (read_len > 0u && read_buf == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    *result = pal_i2c_result_t{};
    if (!s_host_i2c_addr_ack) {
        result->addr_nack = 1u;
        return WINK_OK;
    }
    if (write_len > 0u && !s_host_i2c_write_ack) {
        result->nack_bits = 1u;
    }
    for (uint32_t i = 0; i < read_len; ++i) {
        read_buf[i] = s_host_i2c_read_byte;
    }
    return WINK_OK;
}

wink_status_t js_pal_i2c_session_open(uint8_t port, uint16_t dev_addr,
                                      uint8_t direction,
                                      uint8_t* out_session,
                                      pal_i2c_result_t* result) {
    if (!s_host_i2c_mock_enabled) {
        return WINK_ERR_UNSUPPORTED;
    }
    if (out_session == NULL || result == NULL || port != 0u ||
        direction > 1u || dev_addr > 0x7Fu) {
        return WINK_ERR_INVALID_ARG;
    }
    *out_session = PAL_I2C_SESSION_INVALID;
    *result = pal_i2c_result_t{};
    uint32_t slot = MCS51_HOST_BUS_SESSION_MAX;
    for (uint32_t i = 0; i < MCS51_HOST_BUS_SESSION_MAX; ++i) {
        if (!s_host_i2c_session_active[i]) {
            slot = i;
            break;
        }
    }
    if (slot == MCS51_HOST_BUS_SESSION_MAX) {
        return WINK_ERR_FULL;
    }
    s_host_i2c_session_active[slot] = true;
    s_host_i2c_session_addr_nack[slot] = !s_host_i2c_addr_ack;
    s_host_i2c_session_dir[slot] = direction;
    *out_session = static_cast<uint8_t>(slot);
    result->addr_nack = s_host_i2c_session_addr_nack[slot] ? 1u : 0u;
    ++s_host_i2c_open_count;
    return WINK_OK;
}

wink_status_t js_pal_i2c_session_restart(uint8_t session_id,
                                         uint16_t dev_addr,
                                         uint8_t direction,
                                         pal_i2c_result_t* result) {
    if (!s_host_i2c_mock_enabled) {
        return WINK_ERR_UNSUPPORTED;
    }
    if (result == NULL || session_id >= MCS51_HOST_BUS_SESSION_MAX ||
        !s_host_i2c_session_active[session_id] || direction > 1u ||
        dev_addr > 0x7Fu) {
        return WINK_ERR_INVALID_ARG;
    }
    *result = pal_i2c_result_t{};
    s_host_i2c_session_addr_nack[session_id] = !s_host_i2c_addr_ack;
    s_host_i2c_session_dir[session_id] = direction;
    result->addr_nack = s_host_i2c_session_addr_nack[session_id] ? 1u : 0u;
    ++s_host_i2c_restart_count;
    return WINK_OK;
}

wink_status_t js_pal_i2c_session_write(uint8_t session_id,
                                       const uint8_t* buf, uint32_t len,
                                       pal_i2c_result_t* result) {
    if (!s_host_i2c_mock_enabled) {
        return WINK_ERR_UNSUPPORTED;
    }
    if (result == NULL || buf == NULL || len == 0u ||
        session_id >= MCS51_HOST_BUS_SESSION_MAX ||
        !s_host_i2c_session_active[session_id]) {
        return WINK_ERR_INVALID_ARG;
    }
    *result = pal_i2c_result_t{};
    if (s_host_i2c_session_addr_nack[session_id]) {
        return WINK_ERR_INVALID_STATE;
    }
    if (!s_host_i2c_write_ack) {
        result->nack_bits = 1u;  // first write byte NACKed
    }
    ++s_host_i2c_write_count;
    return WINK_OK;
}

wink_status_t js_pal_i2c_session_read(uint8_t session_id, uint8_t* buf,
                                      uint32_t len, uint8_t ack_mode,
                                      pal_i2c_result_t* result) {
    if (!s_host_i2c_mock_enabled) {
        return WINK_ERR_UNSUPPORTED;
    }
    if (result == NULL || buf == NULL || len == 0u ||
        session_id >= MCS51_HOST_BUS_SESSION_MAX ||
        !s_host_i2c_session_active[session_id] || ack_mode > 2u) {
        return WINK_ERR_INVALID_ARG;
    }
    *result = pal_i2c_result_t{};
    if (s_host_i2c_session_addr_nack[session_id]) {
        return WINK_ERR_INVALID_STATE;
    }
    for (uint32_t i = 0; i < len; ++i) {
        buf[i] = s_host_i2c_read_byte;
    }
    ++s_host_i2c_read_count;
    return WINK_OK;
}

wink_status_t js_pal_i2c_session_close(uint8_t session_id) {
    // Idempotent by contract: any handle returns WINK_OK.
    if (!s_host_i2c_mock_enabled) {
        return WINK_OK;
    }
    if (session_id < MCS51_HOST_BUS_SESSION_MAX &&
        s_host_i2c_session_active[session_id]) {
        s_host_i2c_session_active[session_id] = false;
        ++s_host_i2c_close_count;
    }
    return WINK_OK;
}

// ---- SPI scriptable mock ----
static bool s_host_spi_mock_enabled = false;
static uint8_t s_host_spi_rx_value = 0xFFu;
static bool s_host_spi_session_active[MCS51_HOST_BUS_SESSION_MAX];
static uint32_t s_host_spi_open_count;
static uint32_t s_host_spi_transfer_count;
static uint32_t s_host_spi_close_count;

void wink_mcs51_host_spi_mock_reset(void) {
    for (uint32_t i = 0; i < MCS51_HOST_BUS_SESSION_MAX; ++i) {
        s_host_spi_session_active[i] = false;
    }
    s_host_spi_mock_enabled = false;
    s_host_spi_rx_value = 0xFFu;
    s_host_spi_open_count = 0u;
    s_host_spi_transfer_count = 0u;
    s_host_spi_close_count = 0u;
}

void wink_mcs51_host_spi_mock_enable(bool enable) {
    s_host_spi_mock_enabled = enable;
}
void wink_mcs51_host_spi_set_rx_value(uint8_t value) {
    s_host_spi_rx_value = value;
}
uint32_t wink_mcs51_host_spi_open_count(void) {
    return s_host_spi_open_count;
}
uint32_t wink_mcs51_host_spi_transfer_count(void) {
    return s_host_spi_transfer_count;
}
uint32_t wink_mcs51_host_spi_close_count(void) {
    return s_host_spi_close_count;
}

wink_status_t js_pal_spi_transfer_ex(uint8_t port, uint16_t device_id,
                                     const uint8_t* tx_buf, uint32_t len,
                                     uint8_t* rx_buf, uint8_t mode,
                                     uint32_t sck_hz) {
    if (!s_host_spi_mock_enabled) {
        return WINK_ERR_UNSUPPORTED;
    }
    (void)port;
    (void)device_id;
    (void)tx_buf;
    (void)mode;
    (void)sck_hz;
    if (len == 0u || rx_buf == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    for (uint32_t i = 0; i < len; ++i) {
        rx_buf[i] = s_host_spi_rx_value;
    }
    return WINK_OK;
}

wink_status_t js_pal_spi_session_open(uint8_t port, uint16_t device_id,
                                      uint8_t mode, uint32_t sck_hz,
                                      uint8_t* out_session) {
    if (!s_host_spi_mock_enabled) {
        return WINK_ERR_UNSUPPORTED;
    }
    (void)port;
    (void)device_id;
    (void)mode;
    (void)sck_hz;
    if (out_session == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    *out_session = PAL_SPI_SESSION_INVALID;
    uint32_t slot = MCS51_HOST_BUS_SESSION_MAX;
    for (uint32_t i = 0; i < MCS51_HOST_BUS_SESSION_MAX; ++i) {
        if (!s_host_spi_session_active[i]) {
            slot = i;
            break;
        }
    }
    if (slot == MCS51_HOST_BUS_SESSION_MAX) {
        return WINK_ERR_FULL;
    }
    s_host_spi_session_active[slot] = true;
    *out_session = static_cast<uint8_t>(slot);
    ++s_host_spi_open_count;
    return WINK_OK;
}

wink_status_t js_pal_spi_session_transfer(uint8_t session_id,
                                          const uint8_t* tx_buf,
                                          uint8_t* rx_buf, uint32_t len) {
    if (!s_host_spi_mock_enabled) {
        return WINK_ERR_UNSUPPORTED;
    }
    if (tx_buf == NULL || rx_buf == NULL || len == 0u ||
        session_id >= MCS51_HOST_BUS_SESSION_MAX ||
        !s_host_spi_session_active[session_id]) {
        return WINK_ERR_INVALID_ARG;
    }
    for (uint32_t i = 0; i < len; ++i) {
        rx_buf[i] = s_host_spi_rx_value;
    }
    ++s_host_spi_transfer_count;
    return WINK_OK;
}

wink_status_t js_pal_spi_session_close(uint8_t session_id) {
    // Idempotent by contract: any handle returns WINK_OK.
    if (!s_host_spi_mock_enabled) {
        return WINK_OK;
    }
    if (session_id < MCS51_HOST_BUS_SESSION_MAX &&
        s_host_spi_session_active[session_id]) {
        s_host_spi_session_active[session_id] = false;
        ++s_host_spi_close_count;
    }
    return WINK_OK;
}

// Channel-1 read direction (external digital level driven by the JS
// PinArbiter / an input plugin). No JS data plane on host, so the compat
// library supplies a scriptable fallback. State codes mirror the platform
// JS_GPIO_STATE_* enum (0 low, 1 high, 2 HiZ, 3 conflict); the array lazily
// initialises to HiZ (2 = no external driver -> the proxy falls back to the
// latch), so tests that never inject still see latch semantics.
#define MCS51_HOST_EXT_HIZ 2u
static uint8_t s_host_ext_pin[32];
static bool s_host_ext_pin_ready = false;

static void host_ext_pin_ensure_init(void) {
    if (!s_host_ext_pin_ready) {
        for (uint32_t i = 0; i < 32u; ++i) {
            s_host_ext_pin[i] = MCS51_HOST_EXT_HIZ;
        }
        s_host_ext_pin_ready = true;
    }
}

uint8_t js_pal_gpio_read_state(uint16_t pin) {
    host_ext_pin_ensure_init();
    return (pin < 32u) ? s_host_ext_pin[pin] : MCS51_HOST_EXT_HIZ;
}

// Test injection for the channel-1 external Read-Pin path. state uses the
// JS_GPIO_STATE_* codes (0 low / 1 high / 2 HiZ).
void wink_mcs51_host_set_ext_pin(uint16_t pin, uint8_t state) {
    host_ext_pin_ensure_init();
    if (pin < 32u) {
        s_host_ext_pin[pin] = state;
    }
}
void wink_mcs51_host_ext_pins_reset(void) {
    host_ext_pin_ensure_init();
    for (uint32_t i = 0; i < 32u; ++i) {
        s_host_ext_pin[i] = MCS51_HOST_EXT_HIZ;
    }
}

// Test observability for the channel-2 UART TX route (SBUF write ->
// js_pal_uart_write). Mirrors the gpio notify log accessors.
uint32_t wink_mcs51_host_uart_tx_count(void) {
    return s_host_uart_tx;
}
uint8_t wink_mcs51_host_uart_tx_byte(uint32_t i) {
    return (i < MCS51_HOST_UART_LOG_SIZE) ? s_uart_tx_byte[i] : 0u;
}
uint8_t wink_mcs51_host_uart_tx_port(uint32_t i) {
    return (i < MCS51_HOST_UART_LOG_SIZE) ? s_uart_tx_port[i] : 0xFFu;
}
void wink_mcs51_host_uart_tx_reset(void) {
    s_host_uart_tx = 0;
}

// Test observability for the channel-1 instant-notification path.
uint32_t wink_mcs51_host_gpio_notify_count(void) {
    return s_host_gpio_notifies;
}
void wink_mcs51_host_gpio_notify_reset(void) {
    s_host_gpio_notifies = 0;
}
uint16_t wink_mcs51_host_gpio_notify_pin(uint32_t i) {
    return (i < MCS51_HOST_NOTIFY_LOG_SIZE) ? s_notify_pin[i] : 0xFFFFu;
}
uint8_t wink_mcs51_host_gpio_notify_level(uint32_t i) {
    return (i < MCS51_HOST_NOTIFY_LOG_SIZE) ? s_notify_level[i] : 0u;
}
uint8_t wink_mcs51_host_gpio_notify_strength(uint32_t i) {
    return (i < MCS51_HOST_NOTIFY_LOG_SIZE) ? s_notify_strength[i] : 0u;
}

// P3: release-side observability (TRIS 1->0 / open-drain / reset).
uint32_t wink_mcs51_host_gpio_release_count(void) {
    return s_host_gpio_releases;
}
uint16_t wink_mcs51_host_gpio_release_pin(uint32_t i) {
    return (i < MCS51_HOST_RELEASE_LOG_SIZE) ? s_release_pin[i] : 0xFFFFu;
}
void wink_mcs51_host_gpio_release_reset(void) {
    s_host_gpio_releases = 0u;
    for (uint32_t i = 0; i < MCS51_HOST_RELEASE_LOG_SIZE; ++i) {
        s_release_pin[i] = 0u;
    }
}

}  // extern "C"

#endif  // __EMSCRIPTEN__
