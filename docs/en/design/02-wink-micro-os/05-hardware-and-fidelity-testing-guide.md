# WinkMicroOS Hardware-Level Simulation & Real Hardware Testing Authenticity Defense Guide

<!-- i18n-meta
source: docs/zh/design/02-wink-micro-os/05-hardware-and-fidelity-testing-guide.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

This document establishes the testing design standards for WinkMicroOS across multiple target configurations. It guides developers and AI coding assistants on executing high-fidelity testing under diverse hardware conditions (no hardware, bare boards without peripherals, fully wired boards), establishing mechanisms to ensure testing authenticity and eliminate "Fake Success" test anti-patterns.

---

## 1. Simulation Testing & Authenticity Defense Standards Without Hardware

When running simulation tests on Host computers or in WebAssembly environments, physical chips and peripherals are absent. To prevent "green unit tests on broken hardware", developers must adhere to the following authenticity defense standards.

### 1.1 Eliminating Stub Fake Success (Contract Honesty)
* **Standard**: If a low-level peripheral driver is not yet implemented (e.g., `dal_gps`, `dal_eeprom` are test stubs), its initialization and read/write APIs **must never** return `WINK_OK` simply to pass compilation.
* **Practice**: Must explicitly return `WINK_ERR_UNSUPPORTED` (per [ADR-0012 Contract Honesty Principle](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)), tagged with `@experimental Stub` in headers and implementations.

### 1.2 Bus-Level Telemetry Mocking
* **Standard**: Tests must not stop at verifying that a function returned success. They must assert the **physical side-effects** produced by the invocation.
* **Practice**:
  * **I2C Verification**: In Host `pal_i2c_transfer` mocks, track transferred addresses and transmission counts (`sim_i2c_transfer_count()`). Tests must assert exact counts (e.g., `dal_ssd1306_flush` must trigger exactly 9 I2C transfers).
  * **PWM Verification**: `pal_pwm_set_duty` must record duty cycle percentages (`sim_last_pwm_duty()`). Tests must verify angle-to-duty conversion formulas against physical hardware specifications.
  * **GPIO Resource Verification**: Assert pin conflicts and double allocation via `pal_resource_is_claimed`, preventing driver pin collision bugs.

### 1.3 Virtual Clock & Timing Waveform Injection
* **Standard**: For time-critical peripherals (ultrasonic distance, pulse width capture), drivers must not mock static values; they must simulate physical level transitions.
* **Practice**:
  * Use `sim_set_echo_timing(rise_us, high_us)` in tests to configure Echo pin rising edges and duration.
  * The driver reads pulse widths via conversion formulas ($\text{Distance} = \text{Pulse Width} \times 0.017$) and asserts physical outputs.

### 1.4 Guarding Against Silent Blocking (Wallclock Check)
* **Standard**: Non-blocking interfaces (e.g., `dal_ultrasonic_request_measurement`) must return within microseconds. To prevent AI code from hiding busy-wait loops (`while(poll)`), tests must enforce wallclock timing assertions.
* **Practice**:
  * Measure total elapsed execution time for 1,000 non-blocking calls using Host `clock()`.
  * Assert total duration is << 100ms. If hidden busy-waits exist, the test fails on timeout.

---

## 2. Bare-Board Hardware Testing Standards Without Peripherals

When developers possess only a bare MCU board (e.g., ESP32 DevKitC) without connected sensors or actuators, the following loopback patterns validate PAL contracts and state machines.

### 2.1 Scenario A: Completely Unwired (Internal GPIO Matrix Loopback)
Utilizes the internal ESP32 GPIO exchange matrix without external jumper wires to **validate register configurations, interrupt assignments, and ISR correctness**.

```text
                    ESP32 Internal (GPIO Matrix)
              ┌──────────────────────────────────────┐
   LEDC PWM ──► [Signal Source] ─ (Same GPIO Pin) ──► RMT ──► Interrupts & ISR
              └──────────────────────────────────────┘
                                 │
                     No External Jumper Wires
```

* **Configuration**:
  1. Assign PWM output and RMT input to the same GPIO pin (e.g., `GPIO4`).
  2. Route internal signals using ESP-IDF ROM calls:
     ```c
     esp_rom_gpio_connect_out_signal(GPIO_NUM_4, ledc_periph_signal, false, false);
     esp_rom_gpio_connect_in_signal(GPIO_NUM_4, rmt_periph_signal, false);
     ```
  3. PWM pulses route directly into RMT, triggering RMT interrupts and ISR handlers.
* **Expected Test Assertion**:
  * Modifying PWM duty cycles causes RMT high-pulse durations to change synchronously. Mismatches pinpoint clock divider regressions on physical hardware.

### 2.2 Scenario B: Single Jumper Wire Loopback (Shadow Task Emulation)
Uses a single jumper wire shorting two pins (e.g., Trig and Echo) to **validate DAL protocol state machines and App business logic**.

```text
              ┌──────────────────────────────────────┐
              │             ESP32 Bare Board         │
              │  ┌──────────┐          ┌──────────┐  │
              │  │ GPIO4    │          │ GPIO5    │  │
              │  │ (Trig)   │          │ (Echo)   │  │
              └──┴───┬──────┴──────────┴──────┬───┴──┘
                     │                        ▲
                     └───────[ Jumper Wire ]──┘
                                 │
                 [Shadow Task Monitors Trig, Emits Echo Pulse]
```

* **Configuration**:
  1. Short `GPIO4 (Trig)` to `GPIO5 (Echo)` with a jumper wire.
  2. Spawn a high-priority FreeRTOS shadow task (`mock_sensor_task`) during startup to simulate ultrasonic hardware:
     * Shadow task polls or awaits interrupts on Trig.
     * When Trig transitions high, shadow task waits 100µs.
     * Shadow task drives Echo high, delays via `esp_rom_delay_us(delay_us)`, and pulls Echo low.
  3. `dal_ultrasonic` executes real measurements against physical pin transitions generated by the shadow task.
* **Expected Test Assertion**:
  * Adjusting `delay_us` in the shadow task simulates varying distances ($2940\mu\text{s} \to 50\text{cm}$).
  * Verifies that obstacle avoidance applications run continuously without lockups and trigger braking when obstacles approach.

---

## 3. Fidelity & Bare-Board Audit Checklist

Before submitting PRs, verify:

- [ ] **No False Success in Host Tests**: Are bus transaction byte lengths, pin ownership claims, or mathematical conversions explicitly asserted? (Rejecting bare `WINK_OK` checks).
- [ ] **STRICT Non-Blocking Compilation**: Under `-DWINK_STRICT_NONBLOCKING=1`, are all `WINK_BLOCKING` functions stripped by the compiler?
- [ ] **Hardware Isolation Verification**: Are hardware-specific delays/ISRs isolated behind `#if defined(ESP_PLATFORM)` with equivalent Host virtual-time tests?
- [ ] **Pin Multiplexing Safety**: Do pin changes pass `test_pal_resource` runtime collision assertions?
