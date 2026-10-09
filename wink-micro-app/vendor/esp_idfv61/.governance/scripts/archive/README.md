# Archived Governance Scripts (GAP-07)

This directory contains one-off historical and maintenance scripts that have been executed and retired.

## Rules & Boundaries
1. **Read-Only**: Files in this directory are archived for audit trail and historical reference only.
2. **Import Prohibited (`ERR_ARCHIVED_MODULE_IMPORT`)**: Active codebase modules MUST NOT import or depend on any script within `scripts/archive/`.
3. **Execution Caution**: Do not re-run these scripts against the active production checklist or repository without explicit review.

## Inventory
- `clear_quarantine_and_certify_blink.py`: Sprint 0 Task T5.1 one-off script used to certify `esp.get_started.blink` and clear legacy quarantine debts.
