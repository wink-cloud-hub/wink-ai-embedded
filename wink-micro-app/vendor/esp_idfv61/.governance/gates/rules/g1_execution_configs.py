# SPDX-License-Identifier: Apache-2.0
"""
g1_execution_configs.py
=======================
Gate 1 Rule: Verifies that each entry defines valid executions: [...] configurations array (Schema v2.0).
"""

import re

RULE_ID = "g1.execution_configs"

CONFIG_ID_PATTERN = re.compile(r"^[a-z0-9_]+(-[a-z0-9_]+)*$")
VALID_BACKENDS = {"wasm_browser", "wasm_node", "esp32_hardware", "host_native"}
VALID_TARGET_SOCS = {"esp32", "esp32s3", "esp32c3", "esp32c6", "all"}
VALID_PROFILES = {"standard", "minimal", "debug", "coverage", "lite", "pro"}
VALID_DELIVERY_STATES = {"planned", "building", "verified", "stale", "regressed"}
VALID_ACCEPTANCE_TYPES = {"wasm_simulation", "host_native", "build_toolchain", "expected_rejection", "differential_parity"}
VALID_OBSERVABILITY_LEVELS = {"L1_ui", "L2_log", "L3_probe", "L4_internal", "LX_deadlock"}


def run(context: dict, config: dict | None = None) -> list[dict]:
    findings = []

    for entry in context["manifest"].get("entries", []):
        display_id = entry.get("display_id")
        entry_id = entry.get("id")
        executions = entry.get("executions")

        if not executions or not isinstance(executions, list) or len(executions) == 0:
            findings.append({
                "rule_id": RULE_ID,
                "severity": "error",
                "entry_id": entry_id,
                "display_id": display_id,
                "config_id": None,
                "file_path": None,
                "message": f"Entry #{display_id} ({entry_id}) must have a non-empty 'executions' array",
            })
            continue

        seen_config_ids = set()
        for idx, ex in enumerate(executions):
            if not isinstance(ex, dict):
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": entry_id,
                    "display_id": display_id,
                    "config_id": None,
                    "file_path": None,
                    "message": f"Entry #{display_id} executions[{idx}] is not an object",
                })
                continue

            cid = ex.get("config_id")
            if not cid or not isinstance(cid, str) or not CONFIG_ID_PATTERN.match(cid):
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": entry_id,
                    "display_id": display_id,
                    "config_id": str(cid),
                    "file_path": None,
                    "message": f"Entry #{display_id} has invalid config_id '{cid}'",
                })
            elif cid in seen_config_ids:
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": entry_id,
                    "display_id": display_id,
                    "config_id": cid,
                    "file_path": None,
                    "message": f"Entry #{display_id} has duplicate config_id '{cid}'",
                })
            else:
                seen_config_ids.add(cid)

            backend = ex.get("backend")
            if backend not in VALID_BACKENDS:
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": entry_id,
                    "display_id": display_id,
                    "config_id": cid,
                    "file_path": None,
                    "message": f"Entry #{display_id} config '{cid}' has invalid backend '{backend}'",
                })

            target_soc = ex.get("target_soc")
            if target_soc not in VALID_TARGET_SOCS:
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": entry_id,
                    "display_id": display_id,
                    "config_id": cid,
                    "file_path": None,
                    "message": f"Entry #{display_id} config '{cid}' has invalid target_soc '{target_soc}'",
                })

            profile = ex.get("profile")
            if profile not in VALID_PROFILES:
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": entry_id,
                    "display_id": display_id,
                    "config_id": cid,
                    "file_path": None,
                    "message": f"Entry #{display_id} config '{cid}' has invalid profile '{profile}'",
                })

            deliv = ex.get("delivery_state")
            if deliv not in VALID_DELIVERY_STATES:
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": entry_id,
                    "display_id": display_id,
                    "config_id": cid,
                    "file_path": None,
                    "message": f"Entry #{display_id} config '{cid}' has invalid delivery_state '{deliv}'",
                })

            acceptance = ex.get("acceptance")
            if not acceptance or not isinstance(acceptance, dict):
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": entry_id,
                    "display_id": display_id,
                    "config_id": cid,
                    "file_path": None,
                    "message": f"Entry #{display_id} config '{cid}' missing valid acceptance definition",
                })
            else:
                acc_type = acceptance.get("type")
                if acc_type not in VALID_ACCEPTANCE_TYPES:
                    findings.append({
                        "rule_id": RULE_ID,
                        "severity": "error",
                        "entry_id": entry_id,
                        "display_id": display_id,
                        "config_id": cid,
                        "file_path": None,
                        "message": f"Entry #{display_id} config '{cid}' has invalid acceptance.type '{acc_type}'",
                    })
                obs_lvl = acceptance.get("observability_level")
                if obs_lvl not in VALID_OBSERVABILITY_LEVELS:
                    findings.append({
                        "rule_id": RULE_ID,
                        "severity": "error",
                        "entry_id": entry_id,
                        "display_id": display_id,
                        "config_id": cid,
                        "file_path": None,
                        "message": f"Entry #{display_id} config '{cid}' has invalid acceptance.observability_level '{obs_lvl}'",
                    })

    return findings
