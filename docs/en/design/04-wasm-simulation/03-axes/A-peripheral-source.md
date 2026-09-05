# Axis A — Peripheral Physical Source

<!-- i18n-meta
source: docs/zh/design/04-wasm-simulation/03-axes/A-peripheral-source.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

| Item | Content |
|---|---|
| Layer | Ⅱb Lean Axis Index |
| Status | **Active** (Switched 2026-08-02; Active Wasm simulation SSOT) |
| Definition Source | [`../01-overview/02-axes-af.md`](../01-overview/02-axes-af.md) (Wording strictly frozen) |

---

## 1. Question Addressed

Where sensor/actuator/bus data originates

Definition: Refer to Axis A in [`../01-overview/02-axes-af.md`](../01-overview/02-axes-af.md).

---

## 2. Primary Mechanism

- [`../02-mechanisms/08-channel-routing.md`](../02-mechanisms/08-channel-routing.md) — 4-Channel Data Plane & Peripheral Selection (How physical quantities enter firmware)

---

## 3. Secondary Mechanisms

- Configuration plane (Registry / PinArbiter / Schematics) $\rightarrow$ [`../02-mechanisms/07-peripheral-registry.md`](../02-mechanisms/07-peripheral-registry.md)
- Physical degradation and bus fault injection $\rightarrow$ [`../02-mechanisms/06-physical-degradation.md`](../02-mechanisms/06-physical-degradation.md)

---

## 4. Typical Upper Bounds (Expanded)

1. **Model Scope**: Does not simulate analog frontends, impedance, or power rail dynamics; electrical validation requires real hardware or HIL ([`../01-overview/03-production-contract.md`](../01-overview/03-production-contract.md)).
2. **Channel Coverage**: Channel 1 (Pin edges) and Channel 2 (Bus payloads) are primary; Channel 1b (PWM) provides **duty routing** (Hardware carrier cycles belong to Axis C, see [`C-timer-semantics.md`](./C-timer-semantics.md)).
3. **Pending Channels**: Channel 3 (Analog) and Channel 4 (Buffer) are architectural placeholders.
4. **UART**: Master TX and transactional modes are Landed; asynchronous RX byte timing and RX IRQs are Planned.
5. **Bypass Discipline**: Substitutes physical data sources only, strictly banning DAL business bypasses.
6. **Cross-Axis Claims**: ECHO edge capture requires **A + B** (+ `timing` Accuracy Mode); Servo/PWM output requires **A + C**; I2C packet drops require **A + D + F**.

---

## 5. Associated C Scenarios

Scenario specifications reside in [`../04-assurance/01-consistency-spec.md`](../04-assurance/01-consistency-spec.md). Statuses reside exclusively in [`../04-assurance/02-consistency-checklist.md`](../04-assurance/02-consistency-checklist.md).

- **C1** — Business Causality & State Machine Logic
- **C7** — Bus Protocol Frames / CRC / Error Recovery
- **C17** — Peripheral Resource Mutual Exclusion & Clock Coupling
- **C8** — DMA / Bus Asynchronous Transfer Windows; **C18** — Bus Fault State Machines
