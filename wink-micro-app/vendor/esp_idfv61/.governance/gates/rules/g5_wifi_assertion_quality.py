# SPDX-License-Identifier: Apache-2.0
"""
g5_wifi_assertion_quality.py
============================
Gate 5 Rule 504: Verifies the assertion quality of Wi-Fi scenarios.
Ensures that any scenario declaring INJECT_WIFI_FIXTURE defines valid APs or faults,
and that accompanying ASSERT_POINT steps verify semantic Wi-Fi state/IP signals
(e.g., wifi:sta:state, netif:sta:ip) rather than trivial or empty assertions.
"""

import json
from pathlib import Path
from gate_context import is_candidate_artifact

RULE_ID = "g5.wifi_assertion_quality"

VALID_WIFI_TARGET_PREFIXES = ("wifi:", "netif:")


def run(context: dict, config: dict | None = None) -> list[dict]:
    findings = []
    ws_root = Path(context.get("workspace_root", "."))
    vendor_root = ws_root / "wink-micro-app" / "vendor" / "esp_idfv61"

    if not vendor_root.exists():
        return findings

    scenario_files = list(vendor_root.glob("**/unisim-scenarios/*.scenario.json"))

    for sc_file in scenario_files:
        if is_candidate_artifact(sc_file, vendor_root):
            continue
        try:
            with open(sc_file, "r", encoding="utf-8") as f:
                sc_data = json.load(f)
        except Exception as e:
            findings.append({
                "rule_id": RULE_ID,
                "severity": "error",
                "entry_id": None,
                "display_id": None,
                "config_id": None,
                "file_path": str(sc_file.relative_to(ws_root)),
                "message": f"Malformed scenario JSON: {e}",
            })
            continue

        steps = sc_data.get("steps", [])
        wifi_fixture_steps = [s for s in steps if s.get("type") == "INJECT_WIFI_FIXTURE"]

        if not wifi_fixture_steps:
            # 非 Wi-Fi 注入场景跳过本规则检查
            continue

        for idx, fstep in enumerate(wifi_fixture_steps):
            aps = fstep.get("accessPoints")
            fault = fstep.get("fault")

            if aps is None and fault is None:
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": None,
                    "display_id": None,
                    "config_id": None,
                    "file_path": str(sc_file.relative_to(ws_root)),
                    "message": f"Step[{idx}] INJECT_WIFI_FIXTURE declares neither 'accessPoints' nor 'fault'",
                })
                continue

            if aps is not None:
                if not isinstance(aps, list) or len(aps) == 0:
                    findings.append({
                        "rule_id": RULE_ID,
                        "severity": "error",
                        "entry_id": None,
                        "display_id": None,
                        "config_id": None,
                        "file_path": str(sc_file.relative_to(ws_root)),
                        "message": f"Step[{idx}] INJECT_WIFI_FIXTURE declares empty accessPoints list",
                    })
                for ap_idx, ap in enumerate(aps):
                    if not ap.get("ssid"):
                        findings.append({
                            "rule_id": RULE_ID,
                            "severity": "error",
                            "entry_id": None,
                            "display_id": None,
                            "config_id": None,
                            "file_path": str(sc_file.relative_to(ws_root)),
                            "message": f"Step[{idx}] AP[{ap_idx}] missing required 'ssid'",
                        })

            if fault is not None:
                if not fault.get("targetSsid") or not fault.get("action"):
                    findings.append({
                        "rule_id": RULE_ID,
                        "severity": "error",
                        "entry_id": None,
                        "display_id": None,
                        "config_id": None,
                        "file_path": str(sc_file.relative_to(ws_root)),
                        "message": f"Step[{idx}] fault definition missing 'targetSsid' or 'action'",
                    })

        # 检查断言质量
        assert_steps = [s for s in steps if s.get("type") == "ASSERT_POINT"]
        if not assert_steps:
            findings.append({
                "rule_id": RULE_ID,
                "severity": "error",
                "entry_id": None,
                "display_id": None,
                "config_id": None,
                "file_path": str(sc_file.relative_to(ws_root)),
                "message": "Wi-Fi scenario declares INJECT_WIFI_FIXTURE but has zero ASSERT_POINT steps",
            })
        else:
            has_semantic_target = any(
                any(str(s.get("target", "")).startswith(p) for p in VALID_WIFI_TARGET_PREFIXES)
                for s in assert_steps
            )
            if not has_semantic_target:
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": None,
                    "display_id": None,
                    "config_id": None,
                    "file_path": str(sc_file.relative_to(ws_root)),
                    "message": (
                        "Wi-Fi scenario assertions lack semantic target (must assert on wifi:* or netif:* signals, "
                        "anti-greenwashing gate)"
                    ),
                })

    return findings
