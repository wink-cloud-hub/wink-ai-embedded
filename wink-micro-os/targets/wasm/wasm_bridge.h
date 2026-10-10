// SPDX-License-Identifier: LGPL-3.0-only
/**
 * @file wasm_bridge.h
 * @brief Wasm-JS bridge contract — single source of truth (SSOT).
 *
 * All C<->JS cross-boundary symbols are declared HERE and nowhere else.
 * Functions are organised by UniSim 3.0 simulation-fidelity axes (A~F).
 * See: docs/design/04-wasm-simulation-3.0/01-overview/02-axes-af.md
 *
 * Axis legend:
 *   A  Peripheral physical source  (sensor/actuator data origin, four channels)
 *   B  Time base                   (virtual clock, delay, pulse timing)
 *   C  Timer / PWM semantics       (HW timer, PWM period, capture)
 *   D  Interrupt model             (ISR scheduling, IRQ queue poll)
 *   E  Scheduler / concurrency     (cooperative single-core, critical sections)
 *   F  Fault & observation         (OOM, WDT, fault log, ABI hash)
 *
 * Direction notation:
 *   C->JS  C firmware calls a JS import      (js_pal_* / js_sim_*)
 *   JS->C  JS host calls a KEEPALIVE export  (pal_wasm_* / pal_os_*)
 *
 * ABI integrity rule: any add/remove/rename of a symbol MUST bump
 * PAL_WASM_ABI_HASH (pal_wasm_degradation.c) and update the matching
 * TypeScript WasmImports / WasmExports declarations.
 */
#ifndef WASM_BRIDGE_H
#define WASM_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
/* Status-code SSOT (WINK_OK / WINK_ERR_*) + the portable _Static_assert shim
 * used by the ABI layout guards below (ADR-0085). The mid-file include near
 * the power-model section keeps its place; this include is idempotent. */
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================
 * AXIS A — Peripheral Physical Source
 *
 * Channel 1 : Pin     digital GPIO edge injection / readback
 * Channel 2 : Bus     I2C / SPI / UART TX (+ async RX planned Phase 2)
 * Channel 1b: PWM     duty-cycle output, modulation sub-axis of channel 1
 * Channel 3 : Analog  normalised [0,1] ADC read from PinArbiter
 * Channel 4 : Buffer  WS2812 / camera frame payload   — Phase 4
 * ====================================================================== */

/* -- CH1: Digital pin  (C->JS imports) --------------------------------- */

/**
 * Drive strength for a digital GPIO write (ADR-0077). Values are the identity
 * mapping of the host PinArbiter DriveStrength (WEAK=1, PULL=2, SUPPLY=3);
 * the JS side does `strength ?? SUPPLY` so an older wasm that omits the arg
 * still resolves to push-pull.
 *   WEAK   : weak internal pull-up / open-drain release — 8051 latch=1, I2C SDA release.
 *   PULL   : external resistor pull-up/down (e.g. 4.7k).
 *   SUPPLY : VCC/GND direct or push-pull strong drive — esp32 output, 8051 latch=0.
 */
typedef enum {
    WINK_DRIVE_WEAK   = 1,
    WINK_DRIVE_PULL   = 2,
    WINK_DRIVE_SUPPLY = 3,
} wink_drive_t;

extern void    js_pal_gpio_write(uint16_t pin, bool level, uint8_t strength);


/**
 * Electrical SSOT read.
 * Returns: 0=LOW  1=HIGH  2=HI-Z  3=CONFLICT
 * Reference: 07-peripheral-registry.md §4.2
 */
enum {
    JS_GPIO_STATE_LOW      = 0,
    JS_GPIO_STATE_HIGH     = 1,
    JS_GPIO_STATE_HIZ      = 2,
    JS_GPIO_STATE_CONFLICT = 3,
};
extern uint8_t js_pal_gpio_read_state(uint16_t pin);

/** Ideal/UI driver injection — driver-id "ideal:ui:{pin}" SUPPLY strength. */
extern void    js_pal_gpio_drive_ideal(uint16_t pin, bool level);
/** Remove only the ideal driver registered by js_pal_gpio_drive_ideal(). */
extern void    js_pal_gpio_release_ideal(uint16_t pin);
/** Remove the MCU driver "mcu:gpio{N}" (INPUT / open-drain release). */
extern void    js_pal_gpio_release_mcu(uint16_t pin);
/** C-to-JS synchronous pin edge notification for C-driven SSOT waveform delivery. */
extern void    js_pal_notify_pin_edge(uint16_t pin, uint8_t level, uint64_t t_us);
/** GPIO write notification bridge (legacy observation hook). */
extern void    js_pal_gpio_on_write(uint8_t pin, uint8_t level);

/* -- CH1b: PWM output  (C->JS import) ---------------------------------- */

/** duty_cycle_percent in [0, 100]. Routed via channel 1b (PWM modulation). */
extern void    js_pal_pwm_set_duty(uint8_t channel, float duty_cycle_percent);
/** basis_points in [0, 10000]. Basis point duty cycle output (ADR-0066). */
extern void    js_pal_pwm_set_duty_bp(uint8_t channel, uint16_t basis_points);

/* -- CH2: Bus — I2C / SPI / UART TX  (C->JS imports) ------------------ */

/**
 * Synchronous I2C transfer executed on the JS bus model.
 * SAFETY: JS implementation MUST NOT hold write_buf/read_buf pointers
 * across calls; they are WASM heap offsets valid only during this call.
 */
extern bool    js_pal_i2c_transfer(uint8_t port, uint16_t dev_addr,
                                    const uint8_t *write_buf, uint32_t write_len,
                                    uint8_t *read_buf,        uint32_t read_len);

/**
 * Structured I2C transfer result (ADR-0085). Layout is FROZEN:
 * sizeof == 8, alignof == 4, natural alignment (no #pragma pack); the
 * 32/16-bit fields are read Little-Endian from the Wasm linear memory
 * (DataView.getUint32(ptr, true) / getUint16(ptr + 4, true) on the JS side).
 *   nack_bits : bit N = slave NACK on write data byte N (first 32 write bytes
 *               only; the read phase never sets bits - the master drives
 *               ACK/NACK as the receiver there).
 *   stretch_us: informational slave stretch / device write-cycle estimate,
 *               0 = none (ADR-0072 dual-clock accounting stays C-side).
 *   addr_nack : 0 = address phase ACKed, 1 = NACKed. ABI polarity: 1 = NACK
 *               (deliberately NOT the I2CMSR.ADD_ACK register polarity).
 *   flags     : PAL_I2C_RESULT_* bits below.
 * The engine zeroes *result before filling it; callers never pre-clear.
 */
typedef struct {
    uint32_t nack_bits;
    uint16_t stretch_us;
    uint8_t  addr_nack;
    uint8_t  flags;
} pal_i2c_result_t;

#define PAL_I2C_ACK   0u  /* unified polarity for addr_nack / nack_bits */
#define PAL_I2C_NACK  1u
#define PAL_I2C_RESULT_ARB_LOST            (1u << 0)
#define PAL_I2C_RESULT_BUS_ERROR           (1u << 1)
#define PAL_I2C_RESULT_NACK_BITS_TRUNCATED (1u << 2)

_Static_assert(sizeof(pal_i2c_result_t) == 8, "pal_i2c_result_t ABI size");
_Static_assert(offsetof(pal_i2c_result_t, nack_bits)  == 0, "ABI layout");
_Static_assert(offsetof(pal_i2c_result_t, stretch_us) == 4, "ABI layout");
_Static_assert(offsetof(pal_i2c_result_t, addr_nack)  == 6, "ABI layout");
_Static_assert(offsetof(pal_i2c_result_t, flags)      == 7, "ABI layout");

/**
 * Status-returning I2C transfer (ADR-0085). `dev_addr` is the right-aligned
 * 7-bit address (no R/W bit); dev_addr > 0x7F is rejected with
 * WINK_ERR_INVALID_ARG. Address/data NACKs are VALID protocol results:
 * WINK_OK with result->addr_nack / result->nack_bits set. A NULL `result`
 * upgrades any NACK/no-response to WINK_ERR_DISCONNECTED (never WINK_OK).
 * SAFETY: write_buf/read_buf/result are WASM heap offsets valid only during
 * this call; the JS side MUST NOT retain them.
 */
extern wink_status_t js_pal_i2c_transfer_ex(uint8_t port, uint16_t dev_addr,
                                            const uint8_t *write_buf, uint32_t write_len,
                                            uint8_t *read_buf,        uint32_t read_len,
                                            pal_i2c_result_t *result);

/* -- CH2 I2C controller-level session stream (ADR-0086) ---------------- */

#define PAL_I2C_SESSION_POOL_MAX 4u     /* engine-global active session pool */
#define PAL_I2C_SESSION_INVALID  0xFFu  /* failed/released handle sentinel */

/* ack_mode: multi-byte read ACK policy (per-byte controllers use 1/2, len=1) */
#define PAL_I2C_ACK_LAST_NACK  0u  /* ACK first len-1 bytes, NACK the last */
#define PAL_I2C_ACK_ALL        1u  /* ACK every byte (controller-driven) */
#define PAL_I2C_NACK_ALL       2u  /* NACK every byte (single-byte terminate) */

/**
 * Session open: START + address phase (ADR-0086 §1). One active session per
 * physical port; the global pool holds up to PAL_I2C_SESSION_POOL_MAX
 * handles. `direction`: 0 = write, 1 = read. On success *out_session holds
 * the handle and *result->addr_nack reports the address phase; on failure
 * *out_session is PAL_I2C_SESSION_INVALID. `result` is mandatory (NULL ->
 * WINK_ERR_INVALID_ARG). Port already busy -> WINK_ERR_BUSY; pool full ->
 * WINK_ERR_FULL.
 */
extern wink_status_t js_pal_i2c_session_open(uint8_t port, uint16_t dev_addr,
                                             uint8_t direction, uint8_t *out_session,
                                             pal_i2c_result_t *result);

/**
 * Repeated START + address phase on an existing session (allows W->R, W->W,
 * R->W, R->R and a dev_addr change with plugin re-resolution). Address NACK
 * leaves the session in ADDR_NACKED where only close/restart are legal.
 */
extern wink_status_t js_pal_i2c_session_restart(uint8_t session_id, uint16_t dev_addr,
                                                uint8_t direction,
                                                pal_i2c_result_t *result);

/**
 * Data phase: write `len` bytes (len == 0 -> WINK_ERR_INVALID_ARG; len > 32
 * reports the first 32 bytes in nack_bits and sets NACK_BITS_TRUNCATED).
 */
extern wink_status_t js_pal_i2c_session_write(uint8_t session_id, const uint8_t *buf,
                                              uint32_t len, pal_i2c_result_t *result);

/**
 * Data phase: read `len` bytes under the `ack_mode` policy (len == 0 ->
 * WINK_ERR_INVALID_ARG). The read phase is master-driven ACK/NACK, so it
 * never reports nack_bits.
 */
extern wink_status_t js_pal_i2c_session_read(uint8_t session_id, uint8_t *buf,
                                             uint32_t len, uint8_t ack_mode,
                                             pal_i2c_result_t *result);

/**
 * Close: STOP + release the handle. Idempotent - any handle (unknown,
 * already released) returns WINK_OK.
 */
extern wink_status_t js_pal_i2c_session_close(uint8_t session_id);

/**
 * Full-duplex SPI transfer (LEGACY bool path).
 * device_id currently carries the C-side `cs_pin` byte - that mapping is
 * DEPRECATED (ADR-0087): new code uses js_pal_spi_transfer_ex or the session
 * stream below, where device_id is the board-bound logical device number.
 * mode in [0, 3].
 * SAFETY: JS implementation MUST NOT hold tx_buf/rx_buf across calls.
 */
extern bool    js_pal_spi_transfer(uint8_t port, uint16_t device_id,
                                    const uint8_t *tx_buf, uint32_t len,
                                    uint8_t *rx_buf, uint8_t mode,
                                    uint32_t sck_hz);

/**
 * Status-returning whole-frame SPI transfer (ADR-0087). One call = one
 * implicit CS-low frame; the engine drives the device-declared CS pin when
 * bound. `device_id` is the board-bound LOGICAL DEVICE NUMBER (NOT the CS
 * pin byte). `len == 0` -> WINK_ERR_INVALID_ARG; unknown device ->
 * WINK_ERR_NOT_FOUND (SPI has no address-phase NACK business result).
 * 8-bit word size only (no data_width parameter).
 * SAFETY: JS implementation MUST NOT hold tx_buf/rx_buf across calls.
 */
extern wink_status_t js_pal_spi_transfer_ex(uint8_t port, uint16_t device_id,
                                            const uint8_t *tx_buf, uint32_t len,
                                            uint8_t *rx_buf, uint8_t mode,
                                            uint32_t sck_hz);

/* -- CH2 SPI controller-level session stream (ADR-0087) ---------------- */

#define PAL_SPI_SESSION_POOL_MAX 4u     /* engine-global active session pool */
#define PAL_SPI_SESSION_INVALID  0xFFu  /* failed/released handle sentinel */

/**
 * Session open: CS assert + frame start (ADR-0087). One active session per
 * physical port; the global pool holds up to PAL_SPI_SESSION_POOL_MAX
 * handles. `device_id` is the board-bound logical device number. On success
 * *out_session holds the handle; on failure it is PAL_SPI_SESSION_INVALID.
 * Port busy -> WINK_ERR_BUSY; pool full -> WINK_ERR_FULL; unbound device ->
 * WINK_ERR_NOT_FOUND (no dead session is allocated).
 */
extern wink_status_t js_pal_spi_session_open(uint8_t port, uint16_t device_id,
                                             uint8_t mode, uint32_t sck_hz,
                                             uint8_t *out_session);

/**
 * Frame-internal full-duplex transfer (len == 0 -> WINK_ERR_INVALID_ARG).
 * Line-level only: whole-frame-only plugins answer WINK_ERR_UNSUPPORTED and
 * the engine never silently degrades to onFrame (ADR-0012).
 */
extern wink_status_t js_pal_spi_session_transfer(uint8_t session_id,
                                                 const uint8_t *tx_buf,
                                                 uint8_t *rx_buf, uint32_t len);

/**
 * Close: CS release (rising edge = WEL/WIP commit point) + frame end.
 * Idempotent - any handle (unknown, already released) returns WINK_OK.
 */
extern wink_status_t js_pal_spi_session_close(uint8_t session_id);

/** UART TX — writes a byte frame to the host. Async RX is Planned (Phase 2). */
extern void    js_pal_uart_write(uint8_t port, const uint8_t *buf, uint32_t len);

/**
 * Transmit an array of RMT pulse symbols to the JS renderer / simulation model.
 * SAFETY: JS implementation MUST NOT hold symbols across calls.
 */
struct pal_rmt_symbol_s;
extern bool    js_pal_rmt_tx(uint8_t channel, const struct pal_rmt_symbol_s *symbols,
                             uint32_t count, uint32_t resolution_hz);

/* -- CH3: Analog ADC  (C->JS import) ----------------------------------- */

/**
 * Read the normalised pin voltage from PinArbiter: return value in [0.0, 1.0].
 * Raw/mV conversion and RC low-pass filtering are performed on the C side
 * inside pal_wasm_adc.c (ADR-0057). The JS side never handles full-scale mV.
 */
extern float   js_pal_adc_read_norm(uint16_t pin);

/* -- Plugin channel read  (Axis A observation, C->JS import) ----------- */

/**
 * Read a named float channel from a UniSim plugin instance.
 * Example: js_sim_get_plugin_channel("ultrasonic:0", "distanceCm")
 *
 * FOR OBSERVATION ONLY. MUST NOT be used as a DAL business bypass.
 * The cm->us shortcut in wasm_dev_ultrasonic.c is @deprecated (see Phase 1).
 */
extern float   js_sim_get_plugin_channel(const char *instance_id,
                                          const char *channel_name);

/* ======================================================================
 * AXIS B — Time Base
 *
 * All simulation time is driven by s_virtual_us (single monotonic counter).
 * Wall-clock sources (Date.now / performance.now) are FORBIDDEN in all
 * logic paths. See: 02-virtual-clock.md
 * ====================================================================== */

/* -- B: OSAL sleep / busy-wait  (C->JS imports, ASYNCIFY_IMPORTS) ------ */

/** Asyncify import. JS MUST return a Promise<void>. */
extern void    js_pal_os_sleep_ms(uint32_t ms);
/** Asyncify import. JS MUST return a Promise<void>. */
extern void    js_pal_os_busy_wait_us(uint32_t us);

/* -- B: Virtual clock exports  (JS->C, KEEPALIVE) ---------------------- */

/** Advance the virtual clock by the given number of microseconds. */
extern void     pal_wasm_advance_virtual_clock(uint64_t us);
/** Returns true when the clock nears the uint64 midpoint rollover. */
extern bool     pal_wasm_is_clock_warning_fired(void);
/** Read the current virtual clock in microseconds (same source as pal_os_get_us). */
extern uint64_t pal_wasm_get_virtual_clock_us(void);
/** OSAL clock direct-read exports — same underlying counter as s_virtual_us. */
extern uint64_t pal_os_get_us(void);
extern uint64_t pal_os_get_ms(void);

/* -- B+A: Pin event queue injection  (JS->C, KEEPALIVE) ---------------- */

/**
 * Inject a digital pin edge into the pin-event queue.
 * delay_us: virtual microseconds from the current clock until the edge fires.
 * level: 0=LOW  1=HIGH
 *
 * Axis A+B: data source is channel 1 (Pin), timing is driven by virtual clock.
 * Typical use: UltrasonicPlugin injects ECHO edges, ButtonPlugin injects bounces.
 */
extern void     pal_wasm_push_pin_event(uint8_t pin, uint64_t delay_us,
                                         uint8_t level);
/** Instant-trigger distance measurement for the ultrasonic device on trig_pin. */
extern void     pal_wasm_trigger_ultrasonic_measurement(uint8_t trig_pin);

/** Batch waveform injection and snapshot state accessors. */
extern void     pal_wasm_push_waveform_edge(uint16_t pin, uint64_t t_us, uint8_t level, uint32_t generation);
extern void     pal_wasm_cancel_waveform_generation(uint16_t pin, uint32_t generation);
extern uint32_t pal_wasm_drain_due_waveform_edges(uint64_t until_us);
extern uint32_t pal_wasm_get_waveform_overflow_count(void);
extern int32_t  pal_wasm_export_waveform_state(uint8_t *out_buf, uint32_t max_len);
extern int32_t  pal_wasm_restore_waveform_state(const uint8_t *in_buf, uint32_t len);

/* ======================================================================
 * AXIS C — Timer / PWM Semantics
 *
 * Software-stepped approximation of hardware timer and PWM capture.
 * Hard-ISR at 10 kHz+ and FOC fast-loop MUST run on real hardware.
 * See: 09-timer-and-pwm-semantics.md
 * ====================================================================== */

/* -- C: PWM duty observation  (JS->C, KEEPALIVE) ----------------------- */

/** Read the last PWM duty cycle (%) written by the firmware on this channel. */
extern float    pal_wasm_get_pwm_duty_percent(uint8_t channel);

/* ======================================================================
 * AXIS D — Interrupt Model
 *
 * Cooperative Asyncify-based interrupt insertion; IRQ queue poll.
 * No true preemption or priority nesting in simulation.
 * See: 04-interrupt-model.md
 * ====================================================================== */

/* -- D: Interrupt poll  (C->JS imports) -------------------------------- */

/**
 * Register a pin-to-ISR mapping on the JS side.
 * JS stores (pin -> callback_index, arg_ptr) and does NOT invoke the ISR
 * synchronously. callback_index is an opaque Wasm Table index.
 */
extern void    js_pal_register_interrupt(uint16_t pin,
                                          uint32_t callback_index,
                                          uint32_t arg_ptr);
/** Remove the ISR mapping for the given pin. */
extern void    js_pal_deregister_interrupt(uint16_t pin);

/**
 * Dequeue one pending interrupt per call (FIFO order).
 * Returns true if an entry was dequeued; call in a loop until false
 * to fully drain the queue.
 */
extern bool    js_pal_poll_interrupt(uint32_t *out_callback_index,
                                      uint32_t *out_arg_ptr);

/* ======================================================================
 * AXIS E — Scheduler / Concurrency  (no dedicated extern symbols)
 *
 * Single cooperative virtual core; no SMP, no true preemption.
 *
 * Calling-convention contract (Master Plan §3 Rule 5):
 *   JS->C writes (e.g. pal_wasm_push_uart_rx_byte added in Phase 2) MUST
 *   only be issued while the C main loop has yielded to the JS scheduler.
 *   Re-entrant calls from within any C synchronous call chain are FORBIDDEN.
 *   JS import implementations with "const uint8_t *" pointer parameters MUST
 *   copy the data (.slice()) before returning; holding a cross-call WASM heap
 *   view is FORBIDDEN (see Phase 4 Task 4.0 for the WS2812 frame case).
 *
 * Contract documented in: ADR-0054, Phase 2 plan Task 2.0.
 * ====================================================================== */

/* ======================================================================
 * AXIS F — Fault & Observation
 *
 * OOM reporting, fault log ring-buffer, ABI hash integrity lock.
 * See: 05-memory-and-faults.md
 * ====================================================================== */

/* -- F: Fault log accessors  (JS->C, KEEPALIVE) ------------------------ */

extern uint32_t pal_wasm_get_fault_log_count(void);
extern void     pal_wasm_reset_fault_log(void);
/** Bulk log unpacking: returns base address of wasm_fault_event_t array (16 bytes per struct). */
extern const void* pal_wasm_get_fault_log_raw_ptr(void);
/** timestamp MUST be handled as a JS BigInt (uint64_t). */
extern uint64_t pal_wasm_fault_event_get_timestamp(uint32_t index);
extern uint8_t  pal_wasm_fault_event_get_type(uint32_t index);
extern uint16_t pal_wasm_fault_event_get_pin_or_bus(uint32_t index);
extern uint32_t pal_wasm_fault_event_get_sequence(uint32_t index);

/* -- CH4: Buffer Payload — WS2812 Framebuffer (C->JS import) ------------ */
/**
 * Zero-copy RGB frame delivery: buf is a WASM heap pointer valid only for the
 * duration of this call. The JS implementation MUST synchronously copy the
 * bytes it needs (e.g. new Uint8Array(memory.buffer).subarray(...).slice())
 * BEFORE returning, and MUST NOT retain a view into the WASM heap across calls
 * (Asyncify/memory growth would invalidate it). pal_ws2812_write forwards the
 * App/DAL buffer directly; there is no intermediate C malloc.
 */
extern void    js_pal_ws2812_write(uint16_t pin, const uint8_t *buf, uint32_t len);

/* -- F: Fault state  (JS->C, KEEPALIVE) -------------------------------- */

/** Returns true when the firmware is in faulted (locked) state. */
extern bool   pal_wasm_is_faulted(void);

/**
 * Inject a host-originated fault into the C fault subsystem (code = 8003).
 * msg may be NULL. msg string is copied via malloc+stringToUTF8 before return.
 */
extern void   pal_wasm_host_fault(uint32_t code, const char *msg_cstr);

/** Report an Out-Of-Memory allocation failure to Axis F fault domain. */
extern void   pal_wasm_report_oom(const char *tag, uint32_t size);

/* -- F: ABI hash lock  (JS->C, KEEPALIVE) ------------------------------ */

/**
 * Returns the ABI layout hash baked into this WASM binary.
 * Hash algorithm: SHA-256 of sorted function-signature lines (excluding
 * comments and blank lines), truncated to the lower 32 bits.
 * Any add/remove/rename of a symbol in this file MUST bump PAL_WASM_ABI_HASH
 * in pal_wasm_degradation.c and update the TypeScript WasmImports/WasmExports.
 */
extern uint32_t pal_wasm_get_abi_hash(void);

/** Export C firmware simulation state buffer for TS ReplayHashCollector. */
extern uint32_t pal_wasm_export_state_hash_buffer(uint8_t *out_buf, uint32_t max_len);

/** Reset app initialization state and all physical/bus/irq devices. */
extern void     pal_wasm_reset_app_state(void);

/* ======================================================================
 * AXES A+F — Physical Degradation Engine  (JS->C, KEEPALIVE)
 *
 * Configures the fidelity level of physical simulation (noise, RC filter,
 * warm-up delay, PRNG seed). Controlled by wink-app.json
 * simulation.fidelityLevel, injected at init time via
 * pal_wasm_set_fidelity_level() added in Phase 3 Task 3.6.
 * ====================================================================== */

extern void     pal_wasm_set_fidelity_level(uint8_t level);
extern void     pal_wasm_set_bounce_us(uint32_t us);
extern void     pal_wasm_set_warmup_us(uint32_t us);
extern void     pal_wasm_set_sample_interval_us(uint32_t us);
extern void     pal_wasm_set_adc_noise_v(float v);
extern void     pal_wasm_set_rc_tau_s(float s);
extern void     pal_wasm_set_i2c_drop_permil(uint16_t permil);
extern void     pal_wasm_set_prng_seed(uint32_t seed);
extern uint32_t pal_wasm_get_prng_state(void);
extern void     pal_wasm_set_prng_state(uint32_t state);
/** Reset all physical degradation state. The ONLY mutator allowed in faulted state. */
extern void     pal_wasm_reset_physical(void);

/* ======================================================================
 * AXES A+F — Peripheral Control / State & Execution Mode  (JS->C, KEEPALIVE)
 * ====================================================================== */

/** Reset all virtual peripheral devices to their initial state. */
extern void     pal_wasm_sim_reset_all_devices(void);

/** Read back the logical GPIO output driven by the firmware (JS->C, bool return). */
extern bool     pal_wasm_gpio_read(uint16_t pin);
/** JS-friendly bool-return I2C transfer wrapper (avoids out-pointer marshalling). */
extern bool     pal_wasm_i2c_transfer(uint8_t port, uint16_t dev_addr,
                                       const uint8_t *write_buf, uint32_t write_len,
                                       uint8_t *read_buf,        uint32_t read_len);
/**
 * ADR-0085 status-returning I2C transfer wrapper (JS->C test/worker surface).
 * Returns the wink_status_t code directly; `out_result` (8-byte
 * pal_i2c_result_t) receives the structured result on success and is zeroed
 * on a rejected call. Mirrors the import of the same name.
 */
extern int32_t  pal_wasm_i2c_transfer_ex(uint8_t port, uint16_t dev_addr,
                                         const uint8_t *write_buf, uint32_t write_len,
                                         uint8_t *read_buf,        uint32_t read_len,
                                         pal_i2c_result_t *out_result);

/**
 * ADR-0087 status-returning SPI whole-frame wrapper (JS->C test/worker
 * surface). Returns the wink_status_t code directly. `device_id` is the
 * board-bound logical device number; unknown device -> WINK_ERR_NOT_FOUND.
 * Mirrors the import of the same name.
 */
extern int32_t  pal_wasm_spi_transfer_ex(uint8_t port, uint16_t device_id,
                                         const uint8_t *tx_buf, uint32_t len,
                                         uint8_t *rx_buf, uint8_t mode,
                                         uint32_t sck_hz);

/** Push a byte from host JS into Wasm UART RX fifo (Async RX, Phase 2). Returns false on overrun. */
extern bool     pal_wasm_push_uart_rx_byte(uint8_t port, uint8_t byte);
/** Inject a UART RX error (flags: 1=FRAMING, 2=PARITY, 4=OVERRUN) */
extern void     pal_wasm_push_uart_rx_error(uint8_t port, uint8_t error_flags);
/** Get number of bytes available in Wasm UART RX fifo. */
extern uint32_t pal_wasm_get_uart_rx_available(uint8_t port);

/** Inject a GPIO input level from the JS host (plugin stimulus / test). */
extern void     pal_wasm_set_gpio_input(uint8_t pin, bool level);
/** Read the GPIO output currently driven by the C firmware. */
extern bool     pal_wasm_get_gpio_output(uint8_t pin);

/** Set execution mode: 0=INTERACTIVE  1=HEADLESS (ADR-0042). */
extern void     pal_wasm_set_sim_mode(uint32_t mode);
extern uint32_t pal_wasm_get_sim_mode(void);

/** Reset sim scheduler and fault latch (called before re-running firmware on RESET). */
extern void     pal_wasm_reset_scheduler_state(void);

/* -- Reset controller (ADR-0082, JS->C exports) ------------------------ */
/** 1 = a reset (POR/SW/ WDT/EXT) is pending sanitization, 0 = none. */
extern int      pal_wasm_has_pending_reset(void);
/** mcs51_reset_reason_t value of the pending reset (0 = none). */
extern int      pal_wasm_get_reset_reason(void);
/** Acknowledge/consume the pending reset after the host handled it. */
extern void     pal_wasm_clear_pending_reset(void);

/* -- Power model  (Wave 3 stub, Axis F) -------------------------------- */
struct wasm_pin_power_model_t;
#include "wink_status.h"
/**
 * Wave 3 stub:
 *   set_pin_power_model — validates arguments but does NOT persist the model.
 *   get_total_energy_mj — always returns 0 (JS BigInt).
 */
extern wink_status_t pal_wasm_set_pin_power_model(
    uint8_t pin, const struct wasm_pin_power_model_t *model);
extern uint64_t      pal_wasm_get_total_energy_mj(void);

/* -- CH5: Soft Edge / Motor Semantics (C<->JS imports/exports) --------- */
extern void     js_pal_pcnt_edge(uint8_t unit, int32_t delta);
extern void     js_pal_mcpwm_capture_edge(uint8_t cap_channel, uint32_t ts_ns, bool rising);
extern uint32_t js_pal_mcpwm_get_duty_ticks(uint8_t mcpwm_unit, uint8_t cmp_id);
extern void     pal_wasm_push_pcnt_edge(uint8_t unit, int32_t delta);

/* ======================================================================
 * CROSS-AXIS UTILITY — Logging
 * ====================================================================== */

/**
 * level: 1=ERROR  2=WARN  3=INFO  4=DEBUG
 * msg is a NUL-terminated UTF-8 string (WASM heap offset).
 * JS MUST NOT hold this pointer after the function returns.
 */
extern void js_pal_log(uint8_t level, const char *msg);

/* ======================================================================
 * DEPRECATED — Scheduled for removal in v3.2.0
 *
 * These symbols inject physical quantities directly into C, bypassing the
 * pulse_in measurement path and breaking Axis A+B fidelity coupling.
 * Migration guide: docs/migration/v3.1-ultrasonic-edge-injection.md
 * ====================================================================== */


#ifdef __cplusplus
}
#endif

#endif /* WASM_BRIDGE_H */
