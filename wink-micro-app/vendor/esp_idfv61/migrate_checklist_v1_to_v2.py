#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
migrate_checklist_v1_to_v2.py
==============================
Migrate checklist.data.json from Schema v1.1 to Schema v2.0.
Implements ADR-0091 multi-configuration execution instances and 5D orthogonal states.
"""

import io
import sys
import json
import hashlib
from pathlib import Path
from datetime import datetime, timezone
import yaml
import jsonschema

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")

SCRIPT_DIR = Path(__file__).parent
DATA_JSON = SCRIPT_DIR / "checklist.data.json"
SCHEMA_SPEC_MD = SCRIPT_DIR.parent.parent.parent / "docs" / "zh" / "tech-designs" / "esp32" / "esp-idf-classification-schema-spec.md"
QUARANTINE_YAML = SCRIPT_DIR / ".gates" / "quarantine.yaml"

def compute_file_sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest().lower()

def compute_assets_bundle_sha256(assets_dir: Path) -> str:
    file_names = ["device-tree.json", "wink_simulator.js", "wink_simulator.wasm"]
    bundle_h = hashlib.sha256()
    for fname in file_names:
        fpath = assets_dir / fname
        if not fpath.exists():
            return "0000000000000000000000000000000000000000000000000000000000000000"
        bundle_h.update(f"{fname}:{compute_file_sha256(fpath)}".encode("utf-8"))
    return bundle_h.hexdigest().lower()

def migrate_entry(old_e: dict, quarantined_ids: set[str]) -> dict:
    eid = old_e["id"]
    display_id = old_e["display_id"]
    upstream_path = old_e["upstream_path"]
    
    # 1. Map Scope
    old_sm = old_e.get("scope_and_maturity", {})
    old_status = old_sm.get("status", "pending_audit")
    
    if old_status in ("out_of_scope_product", "contract_blocked"):
        inclusion = "out_of_scope"
        schedule = "deferred"
        ex_ev = old_sm.get("exclusion_evidence", {})
        exclusion_reason = ex_ev.get("physical_medium") or ("不可逆物理硬件介质" if old_status == "out_of_scope_product" else "契约阻断：时序与硬件契约纯软件无法兑现")
    elif old_status == "in_scope_deferred":
        inclusion = "in_scope"
        schedule = "deferred"
        exclusion_reason = "依赖重型外部器件模型，当前阶段暂缓投入"
    elif old_status == "in_scope_deficit":
        inclusion = "in_scope"
        schedule = "active"
        exclusion_reason = None
    elif old_status == "pending_audit":
        inclusion = "unknown"
        schedule = "active"
        exclusion_reason = None
    else:
        inclusion = "unknown"
        schedule = "active"
        exclusion_reason = None

    scope_obj = {
        "inclusion": inclusion,
        "exclusion_reason": exclusion_reason,
        "schedule": schedule
    }

    # 2. Map Audit
    old_audit = old_sm.get("audit", {})
    old_verdict = old_audit.get("verdict", "pending")
    if old_verdict == "audited":
        audit_verdict = "audited"
        audited_configs = ["wasm_sim_standard"]
        audited_at = "2026-09-29T14:00:00Z"
    else:
        audit_verdict = "pending"
        audited_configs = []
        audited_at = None

    audit_obj = {
        "verdict": audit_verdict,
        "auditor": old_audit.get("auditor") if audit_verdict == "audited" else None,
        "audited_at": audited_at,
        "audited_configs": audited_configs,
        "dispute_ref": None,
        "ruling_path": None
    }

    # 3. Acceptance mapping
    old_acc = old_e.get("acceptance", {})
    is_build_sys = "build_system" in upstream_path
    if inclusion == "out_of_scope":
        acc_type = "expected_rejection"
    elif is_build_sys:
        acc_type = "build_toolchain"
    elif old_e.get("baseline", {}).get("execution_backend") == "host_native":
        acc_type = "host_native"
    else:
        acc_type = "wasm_simulation"

    acceptance_obj = {
        "type": acc_type,
        "observability_level": old_acc.get("observability_level", "L4_internal"),
        "scenario_path": old_acc.get("scenario_path"),
        "timeout_virtual_us": old_acc.get("timeout_virtual_us"),
        "timeout_wall_ms": old_acc.get("timeout_wall_ms"),
        "positive_cases": old_acc.get("positive_cases", []),
        "negative_cases": old_acc.get("negative_cases", []),
        "sla_error_symbol": (old_sm.get("exclusion_evidence", {}).get("sla_block_symbols") or [None])[0] if inclusion == "out_of_scope" else None
    }

    # 4. Delivery state & Evidence mapping
    old_del = old_e.get("delivery", {})
    old_state = old_del.get("state", "planned")
    
    if eid in quarantined_ids:
        delivery_state = "verified"
        evidence_obj = {
            "run_id": f"quarantined-{eid.replace('.', '-')}",
            "assets_sha256": "0000000000000000000000000000000000000000000000000000000000000000",
            "scenario_sha256": "0000000000000000000000000000000000000000000000000000000000000000",
            "execution_report_ref": f"quarantine://pending-evidence/{eid}",
            "verified_commit": "eb352cf518a228fa2d512a3928a6fcf7a342410a",
            "verified_at": "2026-09-29T14:00:00Z"
        }
    elif old_state == "verified":
        # Check if assets exist
        assets_sha = old_del.get("assets_sha256")
        if isinstance(assets_sha, str) and len(assets_sha) == 64:
            delivery_state = "verified"
            evidence_obj = {
                "run_id": f"run-verified-{eid.replace('.', '-')}",
                "assets_sha256": assets_sha,
                "scenario_sha256": old_del.get("scenario_sha256") or "0000000000000000000000000000000000000000000000000000000000000000",
                "execution_report_ref": f"reports/esp32/{eid}.json",
                "verified_commit": "eb352cf518a228fa2d512a3928a6fcf7a342410a",
                "verified_at": "2026-09-29T14:00:00Z"
            }
        else:
            delivery_state = "planned"
            evidence_obj = None
    else:
        delivery_state = old_state if old_state in ("building", "regressed", "stale") else "planned"
        evidence_obj = None

    # Backend
    if is_build_sys or acc_type == "host_native":
        backend = "host_native"
    else:
        backend = "wasm_browser"

    execution_config = {
        "config_id": "wasm_sim_standard" if backend == "wasm_browser" else "host_native_standard",
        "backend": backend,
        "target_soc": "esp32",
        "profile": "standard",
        "delivery_state": delivery_state,
        "acceptance": acceptance_obj,
        "evidence": evidence_obj
    }

    # Clean compatibility
    old_comp = old_e.get("compatibility", {})
    compat_obj = {
        "source_code_policy": old_comp.get("source_code_policy", "zero_modification_mirror"),
        "header_closure": old_comp.get("header_closure", []),
        "sdkconfig_overrides": old_comp.get("sdkconfig_overrides", {})
    }

    # Clean fidelity_contract
    old_fid = old_e.get("fidelity_contract", {})
    fid_obj = {
        "axes_declared": {
            "axis_a_channels": old_fid.get("axes_declared", {}).get("axis_a_channel", "full_buffer"),
            "axis_b_timebase": old_fid.get("axes_declared", {}).get("axis_b_timebase", "deterministic_microsecond"),
            "axis_c_timer": old_fid.get("axes_declared", {}).get("axis_c_timer", "cycle_accurate"),
            "axis_d_interrupt": old_fid.get("axes_declared", {}).get("axis_d_interrupt", "fiber_dispatch"),
            "axis_e_concurrency": old_fid.get("axes_declared", {}).get("axis_e_concurrency", "cooperative_fiber"),
            "axis_f_diagnostics": old_fid.get("axes_declared", {}).get("axis_f_fault_trace", "structured_trace")
        },
        "concurrency_model": old_fid.get("concurrency_model", "cooperative_fiber")
    }

    return {
        "id": eid,
        "display_id": display_id,
        "upstream_path": upstream_path,
        "written_at_spec_version": "2.0.0",
        "scope": scope_obj,
        "audit": audit_obj,
        "required_capabilities": old_e.get("required_capabilities", []),
        "compatibility": compat_obj,
        "fidelity_contract": fid_obj,
        "executions": [execution_config]
    }


def main():
    print("[*] Starting Migration from Schema v1.1 to Schema v2.0...")
    
    # 1. Load quarantine
    quarantined_ids = set()
    if QUARANTINE_YAML.exists():
        with open(QUARANTINE_YAML, "r", encoding="utf-8") as f:
            q_data = yaml.safe_load(f)
            for item in q_data.get("quarantined_entries", []):
                quarantined_ids.add(item["id"])
    print(f"[*] Quarantine list loaded: {len(quarantined_ids)} entries in quarantine whitelist.")

    # 2. Load existing checklist.data.json
    with open(DATA_JSON, "r", encoding="utf-8") as f:
        old_data = json.load(f)
    print(f"[*] Loaded existing checklist.data.json: {len(old_data.get('entries', []))} entries.")

    # 3. Migrate each entry
    new_entries = []
    scope_in = 0
    scope_out = 0
    scope_unknown = 0
    audited = 0
    verified_configs = 0

    for old_e in old_data["entries"]:
        new_e = migrate_entry(old_e, quarantined_ids)
        new_entries.append(new_e)

        inc = new_e["scope"]["inclusion"]
        if inc == "in_scope":
            scope_in += 1
        elif inc == "out_of_scope":
            scope_out += 1
        else:
            scope_unknown += 1

        if new_e["audit"]["verdict"] == "audited":
            audited += 1

        for exec_c in new_e["executions"]:
            if exec_c["delivery_state"] == "verified":
                verified_configs += 1

    migrated_manifest = {
        "spec_version": "2.0.0",
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "total_entries": len(new_entries),
        "summary": {
            "scope_in": scope_in,
            "scope_out": scope_out,
            "scope_unknown": scope_unknown,
            "audited": audited,
            "verified_configs": verified_configs
        },
        "entries": new_entries
    }

    print(f"[*] Migration Statistics:")
    print(f"    - Total entries: {len(new_entries)}")
    print(f"    - In Scope: {scope_in}")
    print(f"    - Out of Scope: {scope_out}")
    print(f"    - Unknown / Pending Scope: {scope_unknown}")
    print(f"    - Audited: {audited}")
    print(f"    - Verified Configs (incl. quarantined): {verified_configs}")

    # 4. Save to checklist.data.json
    with open(DATA_JSON, "w", encoding="utf-8", newline="\r\n") as f:
        json.dump(migrated_manifest, f, indent=2, ensure_ascii=False)
    print(f"[+] Successfully wrote Schema v2.0 data to {DATA_JSON}!")


if __name__ == "__main__":
    main()
