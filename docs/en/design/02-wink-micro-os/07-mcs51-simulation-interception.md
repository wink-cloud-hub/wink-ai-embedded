# 2.7 MCS-51/8051 Zero-Code Simulation Interception Layer (frameworks/mcs51)

> **Status: Active (M0–M6 Track A Verified; Production wasm linking + headless live validation Phase 0 Verified, 2026-08-29, ADR-0075 Accepted)**
> This document is a Layer-① Living Specification. The single source of truth is governed by:
> - ADRs: [ADR-0070](../../decisions/core/0070-mcs51-zero-code-simulation-interception-layer.md) (Umbrella, Accepted), [ADR-0071](../../decisions/core/0071-sfr-proxy-rmw-edge-data-plane.md) (SFR Data Plane, Accepted), [ADR-0072](../../decisions/core/0072-dual-clock-domain-and-quota-catchup.md) (Dual Clock Domains), [ADR-0073](../../decisions/core/0073-cms8s-adc-real-register-map-supersedes-ssot.md) (CMS8S78xx ADC Real Register Map, Accepted), [ADR-0074](../../decisions/core/0074-mcs51-channel1-external-read-pin.md) (Channel-1 External Digital Read-Pin Seam, Accepted), [ADR-0075](../../decisions/core/0075-mcs51-production-wasm-target-headless.md) (Production Wasm Link + Headless Phase 0, Accepted)
> - Technical Designs: [`docs/tech-designs/mcs51/`](../../tech-designs/mcs51/) (Overview + Data Plane + Timing Plane + User Manual + Master Plan)
> - Implementation Plan: [`2026-08-27-mcs51-zero-code-simulation-plan.md`](../../implementation-plans/core/2026-08-27-mcs51-zero-code-simulation-plan.md)
> - Review Records: [`2026-08-29-mcs51-simulation-layer-review.md`](../../reviews/core/2026-08-29-mcs51-simulation-layer-review.md) (Layer-④)
> - Roadmap Decision: [ADR-0076](../../decisions/core/0076-mcs51-sim-backends-native-vs-iss-channel-roadmap.md) (Dual Backend Native vs ISS + Channel Integration Roadmap, Accepted 2026-08-30; see §2.5)

## 0. Progress Dashboard (SSOT Index)

> Legend: ✅ Verified (Production / CTest Evidence)  🔶 Stubbed / Console / Injected  ❌ Unbuilt (Roadmap in ADR-0076)

### ① Internal Peripheral / Mechanism Models (Host 23/23, Wasm 10/10 CTest Closed-Loop)

| Capability | Status | Evidence / ADR |
|---|---|---|
| SFR/sbit proxy + diff edges + Read-Latch/Pin + RMW red line | ✅ | ADR-0071/0074, M4 CTest |
| Virtual clock + fiber coroutine + quota catch-up (functional µs) | ✅ | ADR-0072, M2 |
| Timer0/Timer1 functional model (lazy overflow, ISR vectors 1/3, mode 0/1/2) | ✅ | blinky_timer0 |
| UART **TX** (SBUF→putchar/console, set TI, vector 4) | ✅ (Console + Live Channel in ②) | uart_printf, mcs51_uart_hello |
| **External Interrupt INT0/1 Model** (P3.2/P3.3 external edge→ITx edge/level→latch IE0/IE1→dispatch vector 0/2, 10ms sampling throttle, fiber rendezvous, undriven idle-HIGH, re-entrancy guard) | ✅ | ADR-0076 T3, test_mcs51_extint (7 unit cases) / int0 e2e |
| **UART RX Model** (fiber context drain injection queue→latch SBUF shadow+set RI+dispatch vector 4) | ✅ (Host+Wasm CTest closed-loop; Live channel in ② ✅) | ADR-0076 T2/T2.3, uart_echo |
| **ADC0832** external bit-bang slave (3-wire FSM, `on_read` injects DO) | ✅ | adc0832_read, iron_ntc |
| **CMS8S78xx On-Chip ADC** real register map (0-cycle, vector 19, vendor adc.c compiles unmodified) | ✅ | ADR-0073, tier-b |
| XDATA/XSFR aperture, ABSACC, dialect erase (REGX52/REG_CMS8S), 28-vector table | ✅ | §3.1 CTest |
| **iron_ntc thermal closed-loop** (ADC0832 sensing + NTC LUT + relay bang-bang + open/short fail-safe) | ✅ | M6 e2e |
| Board-level codegen `mcs51_board_config.h` + production wasm auto-linking | ✅ | ADR-0075 |
| Timer external counting C/T, Timer0 mode3 | ❌ (idle + STRICT warning) | ADR-0076 D2 (Class A) |

### ② Connected to UniSim Live Channels (`js_pal_*` → PinArbiter / Real Plugins)

| Channel | Status | Evidence / Gap |
|---|---|---|
| Ch1 Digital **Write** (MCU→Plugin, LED/Relay) | ✅ | Headless 7/7 (`js_pal_gpio_write`) |
| Ch1 Digital **Read** (Plugin→MCU, Button Read-Pin 3-path) | ✅ | Headless 7/7 (`js_pal_gpio_read_state`) |
| Ch3 Analog **ADC** | ✅ C-side seam (`js_pal_adc_read_norm(32+ch)`→12-bit) + cross-repo live bridge connected: sister `wink-ai` `afc54d68` (`js_pal_adc_read_norm` bridged to `arbiter.readAnalog(pin)`) + `81b94565` (headless `AdcDomainHandler` bound to shared PinArbiter). Headless `mcs51_analog_threshold`: `INPUT_ANALOG adcChannel:32` 0.8→0.2→0.8 → CMS8S on-chip 12-bit ADC → threshold toggles P1.0 LED, `ASSERT_POINT` **8/8** (T4) | ADR-0076 Class A (Landed) |
| Ch2 UART **TX** | ✅ SBUF write→`js_pal_uart_write`→UARTBus TX timeline (synchronous TI, vector 4); headless `mcs51_uart_hello` `ASSERT_BUS_PAYLOAD` PASS (T1/T5) | ADR-0076 Class A |
| Ch2 UART **RX** | ✅ Framework model (fiber drain queue→SBUF+RI+vector 4, host+wasm ctest) + **Live channel connected**: sister `wink-ai` `cf19d412` (`UartBus.sendToFirmware` prioritizes `wink_mcs51_uart_rx_push`, fallback `pal_wasm_push_uart_rx_byte`). Headless `mcs51_uart_echo`: `INPUT_BUS` pushes "A"/"BC" → vector-4 ISR receives → polled TX echo, `ASSERT_BUS_PAYLOAD` **4/4** (T2.3) | ADR-0076 Class A (Landed) |
| Ch2 Bit-bang **I2C / SPI Slave** | ❌ Unbuilt (ADC0832 serves as SPI slave blueprint) | ADR-0076 Class A |
| Ch1 External Interrupt **INT0/1** (Vector 0/2) | ✅ External edge→latch IE0/IE1 by IT0/IT1→dispatch vector 0/2; host unit test + Keil e2e (host+wasm/Node) + headless `mcs51_button_led_int` **10/10** PASS (T3/T5) | ADR-0076 Class A |
| Ch1b **PWM** Duty (8051 lacks hardware PWM) | ❌ Edges emitted, duty deduced by edge measurement (⚠️) | §2.4, ADR-0076 |
| Timed Edge Injection Queue (DHT / NEC IR / Ultrasonic ECHO / Soft UART RX) | ❌ Unbuilt (World currently updates at 10ms boundaries) | ADR-0076 D2 High leverage |
| Ch4 WS2812 / Camera (Sub-µs cycle encoding) | ❌ Infeasible under native functional clock | ADR-0076 Class B → ISS Backend |

---

## 1. Positioning (Two-Axis Model)

MCU compatibility is divided along two orthogonal axes:
- **Axis A Real Hardware Port** (`targets/<mcu>/`): Wink core natively runs on physical microcontrollers (ESP32/Host/Wasm, STM32 in progress).
- **Axis B Simulation Interception Layer** (`frameworks/<eco>/`): Foreign ecosystem code runs under Wink on Host/Wasm. Existing: `frameworks/arduino/`; **8051 Support = New `frameworks/mcs51/`, isomorphic to Arduino framework**.

Core Principle: **Wink core does NOT run on physical 8051 hardware**. User Keil C51 source code executes unmodified via C++17 sandboxing (`-x c++`) + SFR Proxies + Fiber coroutines inside UniSim with high functional fidelity. Under `ESP_PLATFORM` target builds, the entire `frameworks/mcs51` tree is excluded (0 bytes added to physical ESP32 firmware).

---

## 2. Architectural Highlights

- Four `extern "C"` language boundaries: `main` remapping, `WINK_ISR` static registration, `mcs51_trap` C-ABI POD table, `wink_app_get_callbacks`/`mcs51_adc` injection contract.
- Data plane: SFR Shadow memory + `WinkSfr`/`WinkSfrBitProxy`, differential edge dispatch, Read-Latch vs Read-Pin, linear pin indexing `(port<<3)|bit` → PinArbiter immediate notification (`js_pal_gpio_write`); **Channel-1 External Digital Read-Pin Seam (ADR-0074)**: Plain read queries external levels via `js_pal_gpio_read_state`, fallback to latch for HiZ.
- Timing plane: Host 100Hz master clock / Virtual µs slave clock 1:1 mapping, quota force-yield + Catch-Up reconciliation, Trap 4 Red Lines (zero delay / no yield / pure FSM / clock decoupling).
- Peripheral models: ADC0832 (3-wire DIO phased FSM), CMS8S78xx on-chip 12-bit ADC (0-cycle instant penetration, ADR-0073), Dual UART sinks (stdout / JS Console).
- Board SSOT: `wink-app.json` generates `mcs51_board_config.h` + `device-tree.json` via `wink-tools` codegen.

---

## 3. Directory Layout, ABI Surface & Verification Matrix

```
frameworks/mcs51/
  include/   REGX52.H (Dialect erasure + WinkSfr/WinkSbit + main/ISR remapping boundary)
             REG_CMS8S.H (CMS8S78xx SFR/XSFR proxies + verbatim masks)
             cms8s78xx.h (Tier-b shim masking Keil device header)
             mcs51_adc.h / ADC0832.H / mcs51_trap.h / mcs51_isr.h
             absacc.h (XBYTE/XWORD WinkXByteProxy), mcs51_xsfr.hpp
  src/       mcs51_bridge.cpp (init / seam), mcs51_sfr.cpp (shadow + proxy + edge dispatch)
             mcs51_adc.cpp (12-bit injection rail), mcs51_adc0832.cpp (3-wire FSM)
             cms8s_adc.cpp (on-chip ADC 0-cycle model), mcs51_isr.cpp (28-vector table)
             mcs51_clock.cpp / mcs51_timer.cpp / mcs51_uart.cpp
             mcs51_xdata.cpp (XRAM + XSFR aperture), mcs51_unsupported.cpp
  tools/mcs51_cleanup.py (Keil .c → .cpp sanitizer: ISR rewriting, UTF-8/GBK)
test/mcs51/
  samples/   blinky, blinky_timer0, uart_printf, gpio_in_out, adc0832_read,
             cms8s_adc_test, iron_ntc (unmodified Keil user source)
  apps/iron_ntc/wink-app.json (Board-level codegen SSOT input)
  unit/      Data plane / clock / static init / STRICT / CMS8S ADC unit tests
  wasm/      add_wink_wasm_mcs51_test.cmake + Node harness + wasm drivers
  test_mcs51_*_e2e.c (Host+Wasm shared closed-loop test drivers)
```

- Verification: MSVC/MinGW Host mcs51 CTest 23/23, Emscripten/Wasm + Node 10/10, Headless Stage 2 carriers all PASSED.
