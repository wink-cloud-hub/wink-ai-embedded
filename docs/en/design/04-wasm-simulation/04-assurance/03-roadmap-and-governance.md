# Evolution Roadmap, CI Tiers & Documentation Governance

<!-- i18n-meta
source: docs/zh/design/04-wasm-simulation/04-assurance/03-roadmap-and-governance.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

| Item | Content |
|---|---|
| Document Level | ① Design Specification (UniSim 3.0 / assurance) |
| Document Status | **Active** (Switched 2026-08-02; Active Wasm simulation SSOT) |
| SSOT Responsibility | Phase milestones, CI 3 tiers, **Mechanism Maturity Master Table**, maintenance procedures, **Code $\rightarrow$ Docs Sync**, **ADR Backport Index**, **Golden Vector Governance** unique home |
| Associated Code | `wink-tools/wink.py`; `.github/workflows/clang-tidy.yml` (Complete PR/nightly simulation matrix entry to be verified) |
| Last Audit | 2026-08-02 |
| Governing ADRs | 0003, 0009, 0013, 0014, 0042, 0045, 0047, [0053](../../../decisions/unisim/0053-sim-same-timestamp-event-total-order.md), [0054](../../../decisions/unisim/0054-sim-uart-async-rx-model-boundary.md), [0055](../../../decisions/unisim/0055-sim-fp-determinism-and-golden-policy.md) |
| Migrated From | `04-wasm-simulation-2.0/13-roadmap-and-governance.md` |

> Mechanism maturity vocabulary definition see root [`00-README.md` §3.2](../00-README.md); document lifecycle vocabulary see root §3.1. Whether scenarios can be verified is checked solely in [`02-consistency-checklist.md`](./02-consistency-checklist.md).

---

## 0. Frozen Governance Directives (Effective Prior to Migration)

### 0.1 Code Modification $\rightarrow$ Mandatory Documentation Refresh

When API signatures, behavioral semantics, or ABI change in paths listed under any `02-mechanisms/*` (or related assurance docs) **Associated Code**:

1. Update corresponding document body text in the **same PR**, and refresh header **Last Audit**;
2. Reviewers check: Whether ABI/contract diffs in `wink-micro-os/osal/wasm/`, `targets/wasm/`, or `@wink-ai/unisim` accompany documentation diffs;
3. Target automation: Modified code under associated paths without touching corresponding `.md` $\rightarrow$ CI / `wink lint` **warning** (to be implemented post-migration).

Detailed rules and operational checklists also see §5 below; root summary see [`../00-README.md` §4.4](../00-README.md).

### 0.2 ADR Accepted $\rightarrow$ Backport to Design Specifications (Auditable)

docs-adr mandates immediate backporting to Layer ① upon ADR Acceptance. Operational checklist:

1. Reverse-lookup affected documents via the header **Governing ADRs** field of each file (this field serves as the backport check index);
2. Update body text and (if needed) the **Governing ADRs** list itself;
3. Refresh **Last Audit**;
4. If maturity is affected, synchronize the maturity master table in this file with the header "Landed" line of mechanisms.

---

## 1. Core Divergence Dimensions (Expanded)

| Dimension | Simulation Baseline | Physical Hardware | Escape Hazard | Primary Scenarios |
|---|---|---|---|---|
| Concurrency Model | Single virtual core, cooperative yield | Preemption + possible dual core | Hides lock-free race conditions | C3, C9, C16 |
| Interrupt Timing | tick/yield Poll | Arbitrary preemption, nesting | Invalid modifications outside critical sections | C4, C15 |
| Buses & I/O | Partial synchronous 0µs; drop injection exists, fault state machine weak | Asynchronous DMA + rich faults | Window races, unhandled bus lockups | C7, C8, C18, C19 |
| Time Advancement | Virtual clock fast-forwarding | Continuous wall clock + crystal drift | Dropped edges, double stepping, wrap-around | C2, C14, C21 |
| Host Boundary | Asyncify / JS plant | No such layer | Simulation false-greens, dropped interrupts | C14, C15 |
| Lifecycle | Worker hot reuse | Power-on default | Stale state mistaken for cold boot | C13, C23 |
| Resource Topology | Easily overlooked mutual exclusion | Exclusive hardware blocks | Pin / timer resource conflicts board-level explosion | C17 |
| Memory / ABI | Large heap, relaxed alignment | Small SRAM, strict alignment | OOM / padding escapes | C6, C12, C24, C25 |

### 1.1 Mechanism Maturity Master Table (as of 2026-08-02)

Vocabulary: Root [`00-README.md` §3.2](../00-README.md). This table is the **mechanism landing** index; scenario testability is checked solely in [`02-consistency-checklist.md`](./02-consistency-checklist.md). The **Overall Status** column aligns with the header "Landed" line of the corresponding `02-mechanisms/*` (or overview) document; sub-item notes must not elevate the overall tag.

| Mechanism | Document | Overall Status | Notes / Sub-items |
|---|---|---|---|
| Layered Architecture / Code Map | [`../01-overview/01-architecture.md`](../01-overview/01-architecture.md) | Landed | Descriptive; contains no unlanded capability commitments |
| Worker / Asyncify / Execution Modes | [`../02-mechanisms/01-sandbox-and-execution.md`](../02-mechanisms/01-sandbox-and-execution.md) | Landed | INTERACTIVE + HEADLESS |
| Single Gate Clock / Fast-Forward | [`../02-mechanisms/02-virtual-clock.md`](../02-mechanisms/02-virtual-clock.md) | Landed | Ultrasonic Zero-Yield target path $\rightarrow$ Partial (see 08) |
| Cooperative Scheduler / Fiber / WCET | [`../02-mechanisms/03-scheduler-and-concurrency.md`](../02-mechanisms/03-scheduler-and-concurrency.md) | Landed | Chaotic PRNG $\rightarrow$ Planned; SMP $\rightarrow$ Rejected / requires new ADR |
| IRQ Poll / Critical Section Replay | [`../02-mechanisms/04-interrupt-model.md`](../02-mechanisms/04-interrupt-model.md) | Landed | Overflow Fail-Loud hardening $\rightarrow$ Partial; priority nesting untestable (boundary) |
| Fault Latch / Safe-Off / SafeWrap | [`../02-mechanisms/05-memory-and-faults.md`](../02-mechanisms/05-memory-and-faults.md) | Partial | Sub-items Fault/safe-off **Landed**; overall header Partial |
| Fixed Heap Quota (ADR-0045) | [`../02-mechanisms/05-memory-and-faults.md`](../02-mechanisms/05-memory-and-faults.md) | Planned | 3 CMake flags not checked out |
| Fault Domains / Power Modeling | [`../02-mechanisms/05-memory-and-faults.md`](../02-mechanisms/05-memory-and-faults.md) | Stub | Wave 3 ABI frozen; see also 06 |
| Physical Degradation (Jitter/RC/Warmup/I2C Drop) | [`../02-mechanisms/06-physical-degradation.md`](../02-mechanisms/06-physical-degradation.md) | Partial | Sub-items Jitter/RC/Warmup/I2C/PRNG **Landed**; Fault domains/Power **Stub**; Crystal ppm **Non-goal** |
| PinArbiter / Configuration Boundaries | [`../02-mechanisms/07-peripheral-registry.md`](../02-mechanisms/07-peripheral-registry.md) | Partial | PinArbiter / TS $\leftrightarrow$ ABI **Landed**; SchemaForm · Canvas **Partial** |
| Power Domain Lifecycles | [`../02-mechanisms/07-peripheral-registry.md`](../02-mechanisms/07-peripheral-registry.md) | Planned | Design surface |
| Channel 1 Pin / 1b PWM / 2 Bus Routing | [`../02-mechanisms/08-channel-routing.md`](../02-mechanisms/08-channel-routing.md) | Partial | Channels 1/1b/2 **Landed**; Ultrasonic edges **Partial** (`distanceCm` shortcut **Deprecated**); 3/4 **Planned** |
| Axis C Timers / PWM Semantics / FOC Soft Stepping | [`../02-mechanisms/09-timer-and-pwm-semantics.md`](../02-mechanisms/09-timer-and-pwm-semantics.md) | Partial | duty **Landed**; `pal_hwtimer`/FOC soft stepping **Partial ~ Planned** (no symbols in tree); General capture **Planned** |
| Wasm↔JS ABI Contracts & Anti-Drift | [`../02-mechanisms/10-wasm-js-bridge-abi.md`](../02-mechanisms/10-wasm-js-bridge-abi.md) | Landed | Individual export capabilities may be Stubs (such as power) |
| Accuracy Mode (behavioral/timing/cycle) | [`../02-mechanisms/11-accuracy-observation-lifecycle.md`](../02-mechanisms/11-accuracy-observation-lifecycle.md) | Partial | SSOT established; CI mandatory evidence chain pending reinforcement |
| Observability Plane (VCD/Recorder/BusAnalyzer) | [`../02-mechanisms/11-accuracy-observation-lifecycle.md`](../02-mechanisms/11-accuracy-observation-lifecycle.md) | Partial | Evidence validity rules documented |
| Lifecycle / Reset / Multi-Wasm Boundaries | [`../02-mechanisms/11-accuracy-observation-lifecycle.md`](../02-mechanisms/11-accuracy-observation-lifecycle.md) | Partial | Recommended reset sequence written; cold boot mandatory gates remain weak |

When modifying this table: Synchronize the corresponding mechanisms / overview header "Landed" lines, and record in §6 revision history.

---

## 2. Evolution Stage Milestones

> Status SSOT is [`02-consistency-checklist.md`](./02-consistency-checklist.md). The table below outlines "what to build and what fidelity level to achieve". **No Phase may claim completion without meeting its exit criteria.**

| Phase | Core Capability | Fidelity Level | Primary C Categories |
|---|---|---|---|
| **Phase 1 (MVP+)** | Virtual clock, physical injection, fast-forward contract hardening | Virtual-Time Fidelity | C1, C2, C13, C14, C15 |
| **Phase 2 (Wave A)** | Heap quota, Sanitizers, stack watermark | Resource Parity | C6, C25 |
| **Phase 3 (Wave B)** | Single-source drivers, async DMA, bus fault state machines | Protocol Parity | C7, C8, C18, C19 |
| **Phase 4 (Wave C)** | Chaotic scheduler, TSan, soft WDT, OS semantics nailed down | Temporal Parity | C3, C4, C5, C9, C16, C20 |
| **Phase 5 (Wave D)** | Instruction / ABI dual-track | Instruction Parity | C12, C24 |
| **Continuous / On-Demand** | Resource conflict gates, NVS tearing, low-power declarations | — | C17, C21, C22, C23 |
| **Hardware / HIL** | Dual-core, hard real-time, SPICE, cache/DMA | — | C9, C10, C11, C22, C24 |

### 2.1 Phase Exit Criteria

Each condition must be verifiable (with verification entry points or explicit deliverables). Unmet conditions leave the Phase status as "In Progress".

| Phase | Exit Criteria (Must All Be Satisfied) |
|---|---|
| **Phase 1** | (1) C2 / C14 core sub-items have bound verification entry points and PR gates cover single clock Gate; (2) C13.1 cold boot vectors reproducible (two successive `INIT` calls with zero business residue); (3) C15 Fail-Loud paths have regression entries |
| **Phase 2** | (1) ADR-0045 fixed-heap 3 flags merged into CMake and C6.1 entry is not `Pending`; (2) Tier 1 ASan Pass stably green; (3) C25.3 / ADR-0055: At least 1 physical golden vector annotated with `fp_mode` |
| **Phase 3** | (1) C8 asynchronous transfer window has reproducible test cases (no longer falsely completed via synchronous return); (2) C18 at least one bus fault injection suite in Tier 2; (3) C7.4 Bypass audit has reproducible command |
| **Phase 4** | (1) Known race condition suites trigger Fault under chaos + TSan (or shadow equivalent); (2) Zero escapes inside critical sections (suite documented); (3) C5.2 soft WDT detects lockups; (4) C16 primitive comparison table has test-locked subset |
| **Phase 5** | (1) Nightly ABI/dual-track sampling suite operational; (2) At least 1 padding/alignment-sensitive case fails or warns on nightly track; (3) Daytime track does not claim instruction level |

**Hardware / HIL** and **Continuous / On-Demand** do not carry "Complete" tags; governed by release checklist checkboxes.

---

## 3. CI Test Suite Layering (Test Tiers)

C1~C25 total **102** sub-scenarios (98 during Wave 4C migration; augmented with C1.5 / C2.5 / C13.5 / C21.4 in this wave). The layering goal is for fast tests to block PRs, slower tests to block nightly, and expensive tests on-demand/pre-release.

> **Evidence Status**: "Core Coverage" in the table below is considered **aspirational** for sub-items whose verification entry remains `Pending`, and must not be construed as "already guarded in CI". Progress is governed by the verification entry column in [`02`](./02-consistency-checklist.md).

| Tier | Target Latency | Trigger | Core Coverage | Validation Method |
|---|---|---|---|---|
| **Tier 1 (PR Gate)** | <15s | Per push / PR | C1, C2, C5.1, C6 (ASan/Heap quota), C13.1 (Cold boot), C14.1 (Single write source), C15 (Fail-Loud) | Static compilation assertions, fast HEADLESS unit tests, ASan Pass 1 |
| **Tier 2 (Nightly Chaos)** | <15min | Nightly CI | C3 (Chaos + TSan shadow), C4 (Interrupt preemption), C7, C8 (Async DMA), C16 (OS semantics), C18, C19 | Multi-seed chaos regression, TSan concurrency, bus fault injection |
| **Tier 3 (Deep / HIL)** | On-Demand / Weekly | Pre-release / Hardware pipeline | C10 (FOC motor HIL), C12 (RISC-V32 interpreter binary diff), C22 (Low power), C24 (DMA RAM) | Instruction-level interpreter dual-track trace diff, real hardware HIL automated test benches |

**Sanitizer Pass** ([ADR-0045](../../../decisions/unisim/0045-sim-stack-guard-heap-limits-and-sanitizers.md)): `python wink-tools/wink.py test` matrix includes Pass 3 (ASan Pass); Windows MinGW without `libasan` degrades to `-fsanitize-undefined-trap-on-error`, full ASan on Clang/Emscripten. FP / golden **tolerance policy** see [ADR-0055](../../../decisions/unisim/0055-sim-fp-determinism-and-golden-policy.md); golden **file governance** see §4.1 below.

---

## 4. Sub-Scenario Maintenance Procedures

1. **Adding Sub-Scenarios**: Author all five fields in [`01-consistency-spec.md`](./01-consistency-spec.md) per §0.1 (🚫 / hardware-HIL exclusive can use field exemptions), then add a status row in [`02-consistency-checklist.md`](./02-consistency-checklist.md) (defaults to unverified/weakly verified, verification entry `Pending`) with anchor link back to `01`.
2. **Engine Delivery**: Update only implementation/issue tracking and the status column of `02`; **prohibited** to change `02` status without updating the specification in `01`.
3. **Intentionally Uncovered**: Must appear **simultaneously** in the "Boundary" field of that sub-scenario in `01` **and** the "Explicitly Excluded Scopes" list of `02`.
4. **Prohibited Double-Writing**: Sub-scenario body text from `01` is not repeated in `02`; `02` provides only status + one-line gap + verification entry pointer. **Prohibited** to paste C scenario specification body text into this roadmap.
5. **Conflict Resolution Process**: If discrepancies arise between `01` (spec), `02` (status), and code, take **code** as the final source of truth: modify `01` first $\rightarrow$ then modify `02` $\rightarrow$ record in §6 revision history of this file.
6. **Mechanism Maturity Changes**: When capability tags change, must synchronize **§1.1 master table** + corresponding mechanisms header "Landed" line; tags can only use vocabulary from root [`00-README.md` §3.2](../00-README.md).
7. **✅ Promotion Gate**: PRs promoting `02` status to ✅ must: (a) have verification entry not `Pending`/`N/A`; (b) PR description cite corresponding acceptance oracle from `01`; (c) Reviewer can reproduce via entry. Existing ✅ + `Pending` represent "claimed support", and adding entries is not considered a new feature but must state which oracle is bound in PR notes.
8. **Single Symbol in Status Column**: Prohibited dual symbols; secondary semantics written in "Residual Gaps" (see `02` §0.1).

### 4.1 Golden Vector Governance

> The SSOT for FP / cross-host tolerance policy remains [ADR-0055](../../../decisions/unisim/0055-sim-fp-determinism-and-golden-policy.md). This section governs **how vector files are modified**, without rewriting tolerance formulas.

1. **Check-In**: Golden vectors (including integer traces and floating-point vectors annotated with `fp_mode`) are placed under version control; prohibited from existing only on local developer machines.
2. **Updates**: PRs must explain "why this is not a regression" (algorithm contract change / seed or consumption order change / host matrix expansion, etc.); associate with C sub-scenario ID (such as C2.5, C25.3).
3. **Preventing Accidental Greens**: CI **fails immediately on red** upon golden diff; prohibited from silently rebasing/overwriting baselines in CI scripts; local `--update-golden` flags must not enter default PR gate paths.
4. **Review**: Non-trivial golden updates require at least one reviewer; PRNG global order or physical parameter table changes are high-risk, requiring synchronization across mechanisms `06` and assurance entries.

---

## 5. Documentation Governance (2.0 $\rightarrow$ 3.0)

### 5.1 2.0 $\rightarrow$ 3.0 Mapping Table

| 2.0 | 3.0 |
|---|---|
| `00-README.md` (A~F / Criteria / Vocabulary) | [`../01-overview/`](../01-overview/) + Root [`../00-README.md`](../00-README.md) §3 |
| `01-architecture.md` | [`../01-overview/01-architecture.md`](../01-overview/01-architecture.md) |
| `02`…`10`, `15` Mechanism docs | [`../02-mechanisms/01`](../02-mechanisms/01-sandbox-and-execution.md)…[`11`](../02-mechanisms/11-accuracy-observation-lifecycle.md) (**09 timer semantics** split from old channel doc into independent `09-timer-and-pwm-semantics`) |
| `11` / `12` / `13` | This directory `01` / `02` / `03` (This file) |
| (No independent axis pages) | [`../03-axes/`](../03-axes/) Ⅱb Thin Index |

Historical 1.0 (`04-wasm-simulation/`) comparison notes remain archived in 2.0 `13` §5.1; this document no longer duplicates the 1.0 $\rightarrow$ 2.0 table.

### 5.2 3.0 Four-Tier SSOT Redlines

| Information | Unique Home |
|---|---|
| Axes A~F Definitions & Production Criteria | [`../01-overview/02-axes-af.md`](../01-overview/02-axes-af.md), [`../01-overview/03-production-contract.md`](../01-overview/03-production-contract.md) |
| **Implementation Maturity Vocabulary** | Root [`../00-README.md` §3.2](../00-README.md) |
| **Mechanism Maturity Master Table** | **This Document §1.1** |
| Mechanism Implementation Body Text | [`../02-mechanisms/`](../02-mechanisms/00-README.md) |
| Lean Axis Bounds / Indices | [`../03-axes/`](../03-axes/00-README.md) |
| C Scenario Mechanism / Spec / Oracle Body Text | [`01-consistency-spec.md`](./01-consistency-spec.md) |
| C Scenario Testability Status + Verification Entries | [`02-consistency-checklist.md`](./02-consistency-checklist.md) |
| Phase / CI / Maintenance / Code $\rightarrow$ Docs / ADR Backport / Golden Governance | **This Document** (§0 + §2–§4) |

Cross-file references use anchor links without duplicating body text. Path discipline: UniSim side uses `@wink-ai/unisim` module-level contract descriptions, referenced uniformly per SDK export specifications to avoid hardcoding internal workspace paths across packages.

---

## 6. Revision History

| Date | Changes |
|---|---|
| 2026-08-02 | (2.0 Historical Summary) Reorganized into 2.0 doc package; introduced maturity vocabulary and master table; P1 closed Accuracy/`15`; Axis C dedicated spec merged into channel doc and subsequently split into `09-timer…` in 3.0. |
| 2026-08-02 | **Wave 4A**: Migrated to 3.0 `04-assurance/03`; paths remapped to mechanisms/overview; maturity master table aligned with header "Landed"; Governing ADRs augmented with 0053/0054/0055. |
| 2026-08-02 | **Wave 4C**: Migrated [`02-consistency-checklist.md`](./02-consistency-checklist.md) status matrix (98 sub-items); redeemed `#cN` anchors; assurance layer closed. |
| 2026-08-02 | **Assurance Review Fixes**: Sub-items 98 $\rightarrow$ 102 (C1.5/C2.5/C13.5/C21.4); checklist verification entry column + single-symbol discipline; Phase exit criteria; ✅ promotion gate; Tier aspirational annotations; §4.1 golden governance. |
| 2026-08-02 | **§7 Quality Gates Passed, 3.0 Switched to Active**: C25.1/C25.2/C25.3 completed 5 fields; 1.0/2.0 archived (Archived); `docs/design/README.md` and `CLAUDE.md` entry points directed to 3.0. Tier-C items (reverse tests / UART async RX / ban fast-math CI, etc.) remain code/CI follow-ups without blocking document Active status. |
