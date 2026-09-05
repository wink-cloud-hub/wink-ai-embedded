# Axis F — Fault & Observability

<!-- i18n-meta
source: docs/zh/design/04-wasm-simulation/03-axes/F-fault-and-observation.md
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

OOM, WDT, race conditions, and Trace telemetry

Definition: Refer to Axis F in [`../01-overview/02-axes-af.md`](../01-overview/02-axes-af.md).

---

## 2. Primary Mechanism

- [`../02-mechanisms/05-memory-and-faults.md`](../02-mechanisms/05-memory-and-faults.md) — Heap quotas, fault latching, safe-off execution, and sanitizers

---

## 3. Secondary Mechanisms

- Physical degradation and fault injection $\rightarrow$ [`../02-mechanisms/06-physical-degradation.md`](../02-mechanisms/06-physical-degradation.md)
- Accuracy Modes, observability planes, and lifecycle evidence validity $\rightarrow$ [`../02-mechanisms/11-accuracy-observation-lifecycle.md`](../02-mechanisms/11-accuracy-observation-lifecycle.md)

---

## 4. Typical Upper Bounds (Expanded)

1. **Governance Scope**: For checklist scenarios designated as physical hardware/HIL exclusive, simulation test passes **cannot** serve as release sign-offs ([`../01-overview/03-production-contract.md`](../01-overview/03-production-contract.md)).
2. **Quotas & OOM**: Fixed heap capping is a design contract; quota enforcement requires linker flag completion.
3. **Fault Domains & Power**: ABI stubs must not claim domain isolation or energy modeling until implementations close.
4. **Observability Grading**: `behavioral`, `timing`, and `cycle` modes carry different evidentiary weight; traces collected under incorrect modes cannot claim higher fidelity.
5. **Reset Boundaries**: Hot reset and cold boot coverage differ; skipping physical resets does not validate cold-boot scenarios.
6. **Cross-Axis Claims**: Button debouncing requires **A + B + F**; bus faults require **A + D + F**. Golden trace tolerances are governed by [ADR-0055](../../../decisions/unisim/0055-sim-fp-determinism-and-golden-policy.md).

---

## 5. Associated C Scenarios

Scenario specifications reside in [`../04-assurance/01-consistency-spec.md`](../04-assurance/01-consistency-spec.md). Statuses reside exclusively in [`../04-assurance/02-consistency-checklist.md`](../04-assurance/02-consistency-checklist.md).

- **C6** — Stack / Heap / Memory Safety
- **C15** — Host↔Wasm Boundary Integrity
- **C25** — Floating-Point / Numerics & Compiler UB
- (Gates) Frequently intersects with **C5** (Soft WDT) and **C13** (Lifecycle / Reset)
