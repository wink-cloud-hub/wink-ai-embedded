# Wasm Simulation & Frontend Runtime Engine (UniSim) — 3.0 SSOT

<!-- i18n-meta
source: docs/zh/design/04-wasm-simulation/00-README.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

| Item | Content |
|---|---|
| Document Level | ① Design Specification (Physical path: `docs/design/04-wasm-simulation/`) |
| **Status** | **Active** (Switched 2026-08-02; Active SSOT entry; 2026-08-11 Amend revised and strengthened) |
| Predecessor | `04-wasm-simulation-2.0/` (2.0, formerly Active; archived and removed 2026-08-02; migration lineage tracked in header "Migrated from" and §6 mapping table); [04-wasm-simulation/](../04-wasm-simulation/) (1.0, **Archived**, historical comparison, no longer evolving) |
| Associated ADRs | 0002, 0003, 0009, 0013, 0014, 0019, 0025, 0040, 0042, 0045, 0047 |
| Associated Code (Overview) | `wink-micro-os/osal/wasm/`, `wink-micro-os/targets/{wasm,common}/`, `@wink-ai/unisim` (**UniSim Simulation Engine Core**; standalone TS SDK package contract, governed per module definition. Module rules in §4.1) |
| Last Audit | 2026-08-11 Amend (Embedded architecture review patch: PWM Channel 1b reclassification, degradation non-loss rule, IRQ/DMA/Timer control plane completion) |

> **Active Entry Point**: UniSim 3.0 was switched to **Active** on 2026-08-02 via §7 quality gates, serving as the active SSOT reading entry for Wasm simulation design. On 2026-08-11, **Amend** revisions were added based on `review.md`: corrected PWM channel classification (Channel 1b), declared that "Behavioral degradation must not destroy pulse width/distance measurement information semantics", and completed the control plane triad (IRQ/DMA/Timer). Corresponding TS-side design is in `wink-ai/packages/unisim/docs/design/unified-peripheral-channel-architecture.md`.

---

## 0. Why UniSim 3.0 Four-Tier Layering over Flat 01–15

UniSim 2.0 used a single numerical sequence to carry three orthogonal indexes (mechanisms / A~F / C scenarios), leaving readers unclear about what the "main axis" was; Axis C was buried in the channel document, and Axis F was scattered across multiple documents, creating inconsistencies between classification commitments and file boundaries.

UniSim 3.0 uses **physical directories** to fix four distinct responsibilities, eliminating double-writing:

| Layer | Directory | Questions Addressed | Density | Allows Body Text? |
|---|---|---|---|---|
| **Ⅰ Overview** | [`01-overview/`](./01-overview/) | Concepts, architecture, methodology, production criteria, terminology | Medium | Yes (Criteria & definitions) |
| **Ⅱa Mechanisms** | [`02-mechanisms/`](./02-mechanisms/) | How engine subsystems are implemented | **Dense** | **Yes (Implementation SSOT)** |
| **Ⅱb Fidelity Axes** | [`03-axes/`](./03-axes/) | What each axis A~F guarantees and where its upper bound lies | **Lean** | Index + bounds only; **Bans** duplicating mechanism details |
| **Ⅲ Assurance** | [`04-assurance/`](./04-assurance/) | What to verify, whether it can be verified, and how to evolve | Medium | Yes (Scenarios / status / procedures) |

```text
Claim "High Fidelity"
    │
    ├─ Read Ⅰ  → Criteria and boundaries (Completeness ≠ Identity)
    ├─ Check Ⅱb by Axis A~F → Upper bound for that axis + links to mechanisms/scenarios
    ├─ Modify engine read Ⅱa → Unique implementation body text
    └─ Assurance read Ⅲ   → Contract C scenarios / status matrix / maturity & CI
```

---

## 1. Directory Tree (Current Stage Deliverables)

```text
04-wasm-simulation/
├── 00-README.md                          ← This file: Main entry + SSOT Iron Rules + Lexicon
├── 01-overview/                          ← Ⅰ Overview & Architecture
│   ├── 00-README.md
│   ├── 01-architecture.md
│   ├── 02-axes-af.md                     ← Single source of truth for A~F definitions
│   ├── 03-production-contract.md
│   ├── 04-methodology.md
│   └── 05-glossary.md                    ← Authoritative glossary definitions
├── 02-mechanisms/                        ← Ⅱa Engine Mechanisms (Implementation SSOT)
│   ├── 00-README.md
│   ├── 01-sandbox-and-execution.md
│   ├── 02-virtual-clock.md
│   ├── 03-scheduler-and-concurrency.md
│   ├── 04-interrupt-model.md
│   ├── 05-memory-and-faults.md
│   ├── 06-physical-degradation.md
│   ├── 07-peripheral-registry.md
│   ├── 08-channel-routing.md             ← Axis A Data Plane (Including PWM routing as Channel 1b)
│   ├── 09-timer-and-pwm-semantics.md     ← Axis C Primary Mechanism (Soft stepping / duty / capture / pal_hwtimer)
│   ├── 10-wasm-js-bridge-abi.md
│   └── 11-accuracy-observation-lifecycle.md
├── 03-axes/                              ← Ⅱb Lean Axis Indexes (No double-writing)
│   ├── 00-README.md
│   ├── A-peripheral-source.md
│   ├── B-timebase.md
│   ├── C-timer-semantics.md
│   ├── D-interrupt-model.md
│   ├── E-scheduler-concurrency.md
│   └── F-fault-and-observation.md
└── 04-assurance/                         ← Ⅲ Assurance & Governance
    ├── 00-README.md
    ├── 01-consistency-spec.md
    ├── 02-consistency-checklist.md
    └── 03-roadmap-and-governance.md
```

---

## 2. SSOT Iron Rules (Hard Constraints for Long-Term Maintainability)

1. **One Meaning, One Location**: The same technical fact is written as body text in only one file; other files only link to it.
2. **Asymmetry between Ⅱa and Ⅱb**: Mechanisms may be written in detail; axis pages contain only fixed template fields. If algorithms/state machines/ABI tables are found pasted into axis pages → immediately move back to `02-mechanisms/`.
3. **Exactly One Primary Home per Axis**: Each axis points to **exactly one** primary mechanism file; secondary links are permitted. A mechanism serves as primary for at most **0 or 1** axis (cross-cutting files like sandbox/bridge may be 0). **Forbidden**: "Sole primary link = sub-anchor in another axis's primary file"; **Forbidden**: Assigning two primaries to the same mechanism.
4. **Overview ↔ Axis Page Asymmetry**:
   - [`01-overview/02-axes-af.md`](./01-overview/02-axes-af.md): Letter **definitions** + cross-axis comparison table (upper bounds are condensed);
   - `03-axes/X`: Problem statement may echo one line verbatim; upper bounds are **expanded versions**; **Forbidden** to rephrase definition wording from overview.
5. **A~F Definitions only in** `02-axes-af.md`; authoritative terminology definitions reside in [`01-overview/05-glossary.md`](./01-overview/05-glossary.md).
6. **Scenario Statuses only in** [`04-assurance/02-consistency-checklist.md`](./04-assurance/02-consistency-checklist.md); specifications do not write ✅/🟡/❌/🚫.
7. **Mechanism Maturity Tags** only use the vocabulary in §3 below; the overall table resides only in [`04-assurance/03-roadmap-and-governance.md`](./04-assurance/03-roadmap-and-governance.md). The header "Landed" line must be verifiable using **Associated Code**.
8. **Body Text Fully Migrated (Active)**: UniSim 3.0 was switched to **Active** on 2026-08-02 via §7 quality gates, with body text of each file serving as the active SSOT; 1.0 is **Archived**, and 2.0 is deleted after archiving. New additions/modifications must still obey the iron rules of this section (one meaning one location, Ⅱa/Ⅱb asymmetry, statuses only in checklist, etc.), and old text **must not** be backfilled from 1.0 causing double-writing.
9. **STRICT_NONBLOCKING**: Discipline and "why" reside in methodology; build implementation resides in sandbox and/or scheduler with **bidirectional links**.
10. **Code Changes Trigger Documentation Updates**: See §4.4; detailed rules and ADR back-reference indexes reside in [`04-assurance/03-roadmap-and-governance.md`](./04-assurance/03-roadmap-and-governance.md).

---

## 3. Two Orthogonal Vocabularies

### 3.1 Document Lifecycle States (Directory / File Level)

> **This vocabulary is defined in this location only**. It describes the status of the **documentation package**, not mechanism implementation.

| Tag | Meaning |
|---|---|
| **Scaffold** | Directory skeleton, iron rules, header, and structural template only; no migrated body text |
| **Migrating** | Currently migrating body text from legacy version; **must not** be used as public active reading entry |
| **Active** | Current SSOT; external entries point to this directory |
| **Archived** | Read-only historical reference, no longer evolving |

Migration convention: When starting migration, 3.0 → **Migrating**, while 2.0 remains **Active**; after passing all §7 quality gates, 3.0 → **Active**, and 2.0 (and confirmed 1.0) → **Archived**. **This switch was completed on 2026-08-02**: 3.0 is the active Active SSOT, 1.0 remains Archived; 2.0 was deleted after archiving (source filenames preserved as plain text in header "Migrated from" line and §6 mapping table).

### 3.2 Implementation Maturity Vocabulary (Mechanism Landing State)

> **This vocabulary is defined in this location only**. Header "Landed" lines in `02-mechanisms/*` and the assurance maturity table must use the following tags.

| Tag | Meaning |
|---|---|
| **Landed** | End-to-end available: Primary path implemented, and header **Associated Code** points to source files/tests |
| **Partial** | Primary path available, but has known shortcuts, gaps, or mode restrictions |
| **Stub** | ABI/interface frozen, behavior is empty or unwired |
| **Planned** | Design/selection only, no implementation commitment |
| **Deprecated** | Code remains, new usage forbidden |

**Orthogonal to scenario testability**: Mechanism landing ≠ a specific Scenario C is verifiable. Scenario markings only use ✅ / 🟡 / ❌ / 🚫.

---

## 4. Mandatory Header Fields for Mechanisms / Axes / Assurance

### 4.1 `02-mechanisms/*.md` (Implementation Documents)

| Field | Requirement |
|---|---|
| **Landed** | One of the terms from the §3.2 vocabulary |
| **Associated Code** | For C side, fill in relative path in this repo; for TS simulation engine side, fill in `@wink-ai/unisim` package-level and logical component name (e.g. `@wink-ai/unisim (PinArbiter)`), annotated at public module granularity; must not be blank for Landed/Partial |
| **Last Audit** | `YYYY-MM-DD`; alignment date between documentation and code |
| **Governing ADRs** | Specific ADR numbers that actually constrain this file (not a blanket copy of the root set) |
| **Supporting Axes** | Primary / secondary axis letter; cross-cutting files may be empty |

### 4.2 `04-assurance/01` and `03`

Also populate **Associated Code** (if applicable) / **Last Audit** / **Governing ADRs**. Checklist focuses on status matrix, but still requires audit date.

### 4.3 `03-axes/*.md`

Do not write Landed; write **Primary Mechanism (primary)** and optional **Secondary Mechanism (secondary)**. The primary link in axis pages must be consistent with the comparison table in [`03-axes/00-README.md`](./03-axes/00-README.md) (no discrepancies between the two).

### 4.4 Code Changes → Refresh Documentation (Preventing Rot)

When **API signatures, behavioral semantics, or ABI** change in paths listed under a file's **Associated Code**:

1. The same PR must update the corresponding mechanism (or assurance) **body text** and refresh **Last Audit**;
2. Reviewers must check for corresponding documentation diffs when reviewing exported ABI/contract changes in `wink-micro-os/osal/wasm/`, `wink-micro-os/targets/wasm/`, or `@wink-ai/unisim`;
3. Goal: CI/`wink lint` issues a **warning** for "modified code under associated paths without touching corresponding `.md`" (to be implemented after migration; procedure belongs in [`04-assurance/03`](./04-assurance/03-roadmap-and-governance.md)).

---

## 5. Lean Axis Page Guardrails (Enforced by Tooling after Migration)

| Rule | Description |
|---|---|
| Forbidden fenced code blocks | Algorithms belong in mechanisms |
| Single file recommended ≤ **120** lines | Exceeding limit → move content back to mechanisms / overview |
| Forbidden ✅/🟡/❌/🚫 | Status belongs only in checklist |
| Forbidden Landed/Partial/Stub/Planned/Deprecated | Maturity belongs only in mechanisms header + roadmap table |
| Primary link ∈ axes README table | Consistent with §4.3 |

---

## 6. Mapping from 2.0 to 3.0 (Migration Checklist)

| 3.0 Target | Primary 2.0 Source |
|---|---|
| `01-overview/01-architecture.md` | `01-architecture.md` |
| `01-overview/02-axes-af.md` | `00-README.md` §1 |
| `01-overview/03-production-contract.md` | `00` §2–§3; `11` Scope section |
| `01-overview/04-methodology.md` | `00` §0/§4; `11` §0.1–§0.3 (STRICT "why") |
| `01-overview/05-glossary.md` | Newly created (extracted terminology across text; no mechanism body copy) |
| `02-mechanisms/01`…`08` | `02`…`09` (timer semantic sections **stripped** when migrating channel doc) |
| `02-mechanisms/09-timer-and-pwm-semantics.md` | `09` §1.4/§5.3 etc. + ADR-0047 related |
| `02-mechanisms/10-wasm-js-bridge-abi.md` | `10-wasm-js-bridge-abi.md` |
| `02-mechanisms/11-accuracy-…` | `15-accuracy-observation-lifecycle.md` |
| `03-axes/A`…`F` | Newly created lean pages (expanded index from 00 §1) |
| `04-assurance/01`…`03` | `11` / `12` / `13` (spec filled per frozen 5-field template; added C index TOC; deduplicated) |

---

## 7. Migration Checklist Gate (Migration "Filled" ≠ Ready to Switch)

> **All passed and switch completed on 2026-08-02**: 3.0 → Active; 1.0 retained as Archived, 2.0 deleted after archiving. The following checkboxes are quality gate records; additions/refactorings must still satisfy corresponding items.

Only after all of the following are satisfied may: 3.0 → **Active**; 2.0 → **Archived**; and explicit disposition of 1.0 (`04-wasm-simulation/`) → **Archived** or deleted (must not have three versions coexisting without declaring active entry).

### 7.1 Overview (Criteria and Definition SSOT)

- [x] A~F definition table in `01-overview/02-axes-af.md` populated (prerequisite for prohibiting rewording on axis pages)
- [x] Production criteria in `03-production-contract.md` finalized (prerequisite for prohibiting other files from writing separate commitments)
- [x] All terms in `05-glossary.md` defined (not TODO; 29 entries)
- [x] Bidirectional STRICT links between `04-methodology.md` and sandbox/scheduler complete

### 7.2 Mechanisms / Axes

- [x] All `02-mechanisms/*` headers have **Associated Code** (cross-repo paths verifiable) + **Last Audit** + **Governing ADRs**
- [x] Axis C primary link points to `09-timer-and-pwm-semantics.md` (not channel sub-anchor)
- [x] Intersections between `08-channel-routing` and `09-timer-…` contain only one sentence + reverse link, no duplicate algorithm text
- [x] Primary link on each axis page matches `03-axes/00-README` comparison table
- [x] Lean axis page lint (or equivalent manual checklist) passed (all six axes ≤42 lines, no fenced code/status symbols/maturity terms)

### 7.3 Assurance / Repository Entry

- [x] Spec contains Scenario C index TOC; sub-scenarios follow five-field template (C25.1/C25.2/C25.3 populated); checklist contains no design body text
- [x] All `C<N>.<M>` sub-item references throughout text (e.g. C1.2/C5.2/C15.5) resolve to spec anchors (102 sub-items, 127 explicit anchors, 0 broken links)
- [x] Roadmap maturity summary table matches header Landed lines; includes code→documentation sync and ADR backfill procedures
- [x] No dead links across repository relative links; CLAUDE.md / design specification entries updated
- [x] 1.0 / 2.0 disposition documented in archive notes (1.0 header Archived banner; 2.0 deleted after archiving, source files traced as plain text)

---

## 8. Migration Record & Next Steps

> **Switched to Active via §7 gates on 2026-08-02**: Wave 1–4 body text fully migrated, 3.0 is the active SSOT, 1.0 retained as Archived, 2.0 deleted after archiving. Below are migration wave records and engineering items continuing post-Active.

1. **Waves 1–4 Fully Completed**: Overview (Wave 1), mechanisms `01`–`11` (Wave 2A–2D, including 08/09 timer semantic atomic split), axes A–F (Wave 3), assurance (Wave 4A–4C + review patches, sub-items 98→102, C1.5/C2.5/C13.5/C21.4) all migrated and closed.
2. **Mechanisms Review Closure**: [implementation-plans/2026-08-02-unisim3-mechanisms-review-closure-plan.md](../../implementation-plans/unisim/2026-08-02-unisim3-mechanisms-review-closure-plan.md) — Tier-A patches ✅; Tier-B [ADR-0053](../../decisions/unisim/0053-sim-same-timestamp-event-total-order.md)/[0054](../../decisions/unisim/0054-sim-uart-async-rx-model-boundary.md)/[0055](../../decisions/unisim/0055-sim-fp-determinism-and-golden-policy.md) **Accepted and backfilled**; Tier-C (same-timestamp total order reverse testing, UART async RX implementation, ban fast-math CI, etc.) continuing as code/CI items without blocking document Active status.
3. **Post-Active Governance**: Implement lean axis page linting + code→doc warnings (§4.4); maintain maturity table and ADR backfills per [`04-assurance/03`](./04-assurance/03-roadmap-and-governance.md).
4. Documentation-level changes no longer alter product code or ADR numbers (unless Accepted backfill).
