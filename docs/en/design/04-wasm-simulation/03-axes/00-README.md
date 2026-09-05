# Ⅱb Fidelity Axes Thin Index (axes)

<!-- i18n-meta
source: docs/zh/design/04-wasm-simulation/03-axes/00-README.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

| Item | Content |
|---|---|
| Layer | Ⅱb Axes A~F Fidelity Perspective |
| Status | **Active** (Switched 2026-08-02; Active Wasm simulation SSOT) |
| Responsibility | Documents what each axis guarantees, its upper bounds, and links; **prohibits** becoming a second implementation SSOT |

## Why a Dedicated Directory

Product narratives assert consistency along Axes A~F, while engineering implementation evolves along subsystem mechanisms. Physical separation ensures:

- Modifying clock algorithms $\rightarrow$ Touch only `02-mechanisms/02-virtual-clock.md`
- Modifying "What we promise for Axis B" $\rightarrow$ Touch only `B-timebase.md` + overview criteria if necessary

## Axis ↔ Mechanism Cardinality (Fixed)

| Relationship | Cardinality | Description |
|---|---|---|
| Axis $\rightarrow$ primary mechanism | **Exactly 1** | Every axis must have one and only one primary home |
| Mechanism $\rightarrow$ axis as primary | **0 or 1** | Prohibited for one mechanism to serve as primary for two axes |
| Axis $\rightarrow$ secondary mechanism | 0..N | Optional |
| Cross-cutting mechanism | primary = 0 | e.g. `01-sandbox`, `10-bridge`: Not primary for any axis |

Example: `06-physical-degradation` can be a **secondary** for A/F, but must not serve as primary for both axes simultaneously.

## Asymmetry with Overview (Iron Rule)

| Location | What to Write | What Not to Write |
|---|---|---|
| [`../01-overview/02-axes-af.md`](../01-overview/02-axes-af.md) | Letter **definitions** + cross-axis comparison (upper bounds **abbreviated**) | Mechanism algorithms, expanded upper bound long text |
| This directory `X-*.md` | Problem statement may **echo verbatim** one line; upper bounds **expanded**; primary/secondary; relevant C | **Altering definition wording**; pasting algorithms; status symbols; maturity tags |

## Fixed Template per Axis Page (Populated during Migration)

1. **Question Addressed** (May echo verbatim with overview table)
2. **Primary Mechanism (primary)** — Must match table below; prohibited from using "other file `#sub-anchor`" as sole primary link
3. **Secondary Mechanisms (secondary)** — Optional; **only** Ⅱa mechanisms (or contract ADRs), prohibited from putting overview/assurance directories into secondary
4. **Typical Upper Bounds / Non-Verifiable Scope** (Expanded version)
5. **Relevant Scenario C** ($\rightarrow$ assurance spec; status checked only in checklist)

> **Wave 4 Links**: Section §5 of each axis links C numbers to 3.0 [`../04-assurance/01-consistency-spec.md`](../04-assurance/01-consistency-spec.md); testability status is checked only in [`../04-assurance/02-consistency-checklist.md`](../04-assurance/02-consistency-checklist.md).

## Anti-Rot Guardrails (See Root 00 §5)

- Prohibited fenced code blocks; recommended $\le 120$ lines
- Prohibited scenario status symbols and mechanism maturity tags
- Axis page primary links must match table below (no discrepancies between the two)
- secondary $\ne$ Tier Ⅲ assurance; definition origin $\ne$ secondary

## Primary Home Mapping (Axis Pages Must Match This)

| File | Axis | Primary Home |
|---|---|---|
| [A-peripheral-source.md](./A-peripheral-source.md) | A | `08-channel-routing` |
| [B-timebase.md](./B-timebase.md) | B | `02-virtual-clock` |
| [C-timer-semantics.md](./C-timer-semantics.md) | C | `09-timer-and-pwm-semantics` |
| [D-interrupt-model.md](./D-interrupt-model.md) | D | `04-interrupt-model` |
| [E-scheduler-concurrency.md](./E-scheduler-concurrency.md) | E | `03-scheduler-and-concurrency` |
| [F-fault-and-observation.md](./F-fault-and-observation.md) | F | `05-memory-and-faults` |

Axis letter definitions reside in [`../01-overview/02-axes-af.md`](../01-overview/02-axes-af.md).
