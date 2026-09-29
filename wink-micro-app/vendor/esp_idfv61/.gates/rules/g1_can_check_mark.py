# SPDX-License-Identifier: Apache-2.0
"""
g1_can_check_mark.py
====================
Gate 1 Rule: 6-factor checkmark closed-loop referee and quarantine debt triage.
"""

from pathlib import Path
from datetime import datetime, timezone

RULE_ID = "g1.can_check_mark"
NULL_HASH_64 = "0" * 64


def evaluate_dependency_closure(sample_entry: dict, execution_config: dict, catalog: dict) -> tuple[str, list[str]]:
    capabilities = catalog.get("capabilities", {})
    closure = set()
    worklist = list(sample_entry.get("required_capabilities", []))

    while worklist:
        cap_id = worklist.pop(0)
        if cap_id not in capabilities:
            return "blocked", []
        if cap_id in closure:
            continue

        cap_def = capabilities[cap_id]
        cap_status = cap_def.get("status", "planned")
        if cap_status not in ("implemented", "verified"):
            return "blocked", []

        closure.add(cap_id)

        depends_on = cap_def.get("depends_on", {})
        for req in depends_on.get("mandatory", []):
            if req not in closure:
                worklist.append(req)

        for cond in depends_on.get("conditional", []):
            when = cond.get("when", {})
            matches = True
            try:
                for k, expected in when.items():
                    if k not in ("profile", "backend", "target_soc"):
                        raise ValueError(f"Disallowed conditional attribute: {k}")
                    actual = execution_config.get(k)
                    if isinstance(expected, list):
                        if actual not in expected:
                            matches = False
                            break
                    else:
                        if actual != expected:
                            matches = False
                            break
            except Exception:
                return "unknown", []

            if matches:
                for req in cond.get("requires", []):
                    if req not in closure:
                        worklist.append(req)

    return "satisfied", sorted(list(closure))


def run(context: dict, config: dict | None = None) -> list[dict]:
    findings = []
    catalog = context.get("catalog", {})
    now_utc = context.get("now_utc") or datetime.now(timezone.utc)
    ws_root = Path(context.get("workspace_root", "."))

    # Build quarantine lookup
    quarantine_raw = context.get("quarantine", {}).get("quarantined_entries", [])
    quarantine_map = {}
    for q in quarantine_raw:
        qid = q.get("id")
        qcfg = q.get("config_id")
        quarantine_map[(qid, qcfg)] = q
        quarantine_map[(qid, None)] = q

    for entry in context["manifest"].get("entries", []):
        display_id = entry.get("display_id")
        entry_id = entry.get("id")

        for ex in entry.get("executions", []):
            cid = ex.get("config_id")
            deliv = ex.get("delivery_state")
            if deliv != "verified":
                continue

            # Check if this verified execution is in the quarantine debt allowlist
            q_item = quarantine_map.get((entry_id, cid)) or quarantine_map.get((entry_id, None))
            is_quarantined = q_item is not None

            if is_quarantined:
                exp_str = q_item.get("grace_period_expires", "")
                try:
                    exp_dt = datetime.fromisoformat(exp_str.replace("Z", "+00:00"))
                except Exception:
                    exp_dt = None

                if exp_dt and now_utc > exp_dt:
                    findings.append({
                        "rule_id": RULE_ID,
                        "severity": "error",
                        "entry_id": entry_id,
                        "display_id": display_id,
                        "config_id": cid,
                        "file_path": None,
                        "message": (
                            f"Entry #{display_id} ({entry_id}) config '{cid}' quarantine debt "
                            f"grace period expired at {exp_str}! Must revert to planned."
                        ),
                    })
                else:
                    findings.append({
                        "rule_id": RULE_ID,
                        "severity": "warning",
                        "entry_id": entry_id,
                        "display_id": display_id,
                        "config_id": cid,
                        "file_path": None,
                        "message": (
                            f"Entry #{display_id} ({entry_id}) config '{cid}' is in quarantine debt "
                            f"allowlist (TTL expires {exp_str})"
                        ),
                    })
                continue

            # Non-quarantined entry: Strict 6-Factor Checkmark Evaluation
            # Factor 1: Scope
            if entry.get("scope", {}).get("inclusion") != "in_scope":
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": entry_id,
                    "display_id": display_id,
                    "config_id": cid,
                    "file_path": None,
                    "message": f"Factor 1 Failed: delivery_state='verified' but scope.inclusion is not 'in_scope'",
                })

            # Factor 2: Audit
            audit = entry.get("audit", {})
            if audit.get("verdict") != "audited":
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": entry_id,
                    "display_id": display_id,
                    "config_id": cid,
                    "file_path": None,
                    "message": f"Factor 2 Failed: delivery_state='verified' but audit.verdict is '{audit.get('verdict')}'",
                })
            elif cid not in audit.get("audited_configs", []):
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": entry_id,
                    "display_id": display_id,
                    "config_id": cid,
                    "file_path": None,
                    "message": f"Factor 2 Failed: config '{cid}' is verified but not covered in audit.audited_configs",
                })

            # Factor 3: Dependency Closure
            dep_status, closure = evaluate_dependency_closure(entry, ex, catalog)
            if dep_status != "satisfied":
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": entry_id,
                    "display_id": display_id,
                    "config_id": cid,
                    "file_path": None,
                    "message": f"Factor 3 Failed: Dependency closure status is '{dep_status}' (expected 'satisfied')",
                })

            # Factor 5: Evidence & Non-empty Hashes
            evidence = ex.get("evidence")
            if not evidence or not isinstance(evidence, dict):
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": entry_id,
                    "display_id": display_id,
                    "config_id": cid,
                    "file_path": None,
                    "message": "Factor 5 Failed: delivery_state='verified' but evidence is null or not an object",
                })
            else:
                assets_sha = evidence.get("assets_sha256")
                if not assets_sha or len(assets_sha) != 64 or assets_sha == NULL_HASH_64:
                    findings.append({
                        "rule_id": RULE_ID,
                        "severity": "error",
                        "entry_id": entry_id,
                        "display_id": display_id,
                        "config_id": cid,
                        "file_path": None,
                        "message": f"Factor 5 Failed: assets_sha256 is invalid or empty ('{assets_sha}')",
                    })

                scenario_sha = evidence.get("scenario_sha256")
                if not scenario_sha or len(scenario_sha) != 64 or scenario_sha == NULL_HASH_64:
                    findings.append({
                        "rule_id": RULE_ID,
                        "severity": "error",
                        "entry_id": entry_id,
                        "display_id": display_id,
                        "config_id": cid,
                        "file_path": None,
                        "message": f"Factor 5 Failed: scenario_sha256 is invalid or empty ('{scenario_sha}')",
                    })

                # Factor 6: Execution report confirms success
                rep_ref = evidence.get("execution_report_ref")
                if not rep_ref or not isinstance(rep_ref, str):
                    findings.append({
                        "rule_id": RULE_ID,
                        "severity": "error",
                        "entry_id": entry_id,
                        "display_id": display_id,
                        "config_id": cid,
                        "file_path": None,
                        "message": "Factor 6 Failed: execution_report_ref is missing",
                    })
                elif "fail" in rep_ref.lower() or "error" in rep_ref.lower():
                    findings.append({
                        "rule_id": RULE_ID,
                        "severity": "error",
                        "entry_id": entry_id,
                        "display_id": display_id,
                        "config_id": cid,
                        "file_path": None,
                        "message": f"Factor 6 Failed: execution report '{rep_ref}' indicates test failure",
                    })

            # Check scenario file existence on disk if declared
            sc_path_rel = ex.get("acceptance", {}).get("scenario_path")
            if sc_path_rel:
                # Try relative to workspace root or esp_idfv61 app directory
                p_cand1 = ws_root / "wink-micro-app" / "vendor" / "esp_idfv61" / sc_path_rel
                p_cand2 = ws_root / sc_path_rel
                if not p_cand1.exists() and not p_cand2.exists():
                    findings.append({
                        "rule_id": RULE_ID,
                        "severity": "error",
                        "entry_id": entry_id,
                        "display_id": display_id,
                        "config_id": cid,
                        "file_path": sc_path_rel,
                        "message": f"Scenario script '{sc_path_rel}' does not exist on disk",
                    })

    return findings
