# Ⅲ Assurance & Governance (assurance)

<!-- i18n-meta
source: docs/zh/design/04-wasm-simulation/04-assurance/00-README.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

| Item | Content |
|---|---|
| Layer | Ⅲ Verification Scope / Testability / Evolution |
| Status | **Active** (Switched 2026-08-02; Active Wasm simulation SSOT) |
| Responsibility | Scenario contracts, status matrices, roadmap tables, and maintenance governance; **excludes** engine implementation text |

---

## Dependency Direction (Unidirectional)

```text
01-consistency-spec.md      (Cause: Principles / Solutions / Oracles / Boundaries; 5-field template + 🚫 exemptions; 102 sub-scenarios)
        │
        │  Checklists reference scenario IDs and statuses only; prohibited from authoring solution text
        ▼
02-consistency-checklist.md (Effect: Sole source for ✅/🟡/❌/🚫/— statuses + Verification entry points)

02-mechanisms/* Frontmatter "Landed" tags
        │
        │  Aggregated in roadmap; updates must sync bilaterally
        ▼
03-roadmap-and-governance.md (Maturity tables / Phase exits / CI / Golden governance / Code→Docs sync / ADR backports)
```

Prohibited: Authoring solution text in checklists; declaring conflicting maturity tags in roadmaps; embedding status matrices in specs.

---

## Directory Index

| File | Responsibility | Migrated From 2.0 | Wave Status |
|---|---|---|---|
| [01-consistency-spec.md](./01-consistency-spec.md) | C1~C25; 5-field templates + oracles (includes Error-Code Parity); 102 sub-scenarios | `11` | Wave 4B & Review Patches |
| [02-consistency-checklist.md](./02-consistency-checklist.md) | **Sole source for ✅/🟡/❌/🚫/— statuses** + **Verification Entries** (102 sub-scenarios) | `12` | Wave 4C & Review Patches |
| [03-roadmap-and-governance.md](./03-roadmap-and-governance.md) | Phase exits, CI, maturity table, ✅ promotions, golden governance, ADR backports | `13` | Wave 4A & Review Patches |
