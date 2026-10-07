# SPDX-License-Identifier: Apache-2.0
"""
g5_network_assertion_quality.py
===============================
Gate 5 Rule: Verifies the assertion quality of network and protocol scenarios.
Ensures that any scenario declaring INJECT_NET_FIXTURE defines valid routes,
and that accompanying ASSERT_POINT steps verify semantic payloads, status codes,
or event boundaries rather than trivial empty assertions.
"""

import json
from pathlib import Path
from gate_context import is_candidate_artifact

RULE_ID = "g5.network_assertion_quality"


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
        has_net_fixture = any(s.get("type") == "INJECT_NET_FIXTURE" for s in steps)

        if not has_net_fixture:
            # 非网络测试场景跳过本规则检查
            continue

        # 检查网络夹具定义
        fixture_steps = [s for s in steps if s.get("type") == "INJECT_NET_FIXTURE"]
        for idx, fstep in enumerate(fixture_steps):
            routes = fstep.get("routes", [])
            requests = fstep.get("requests", [])
            if not routes and not requests:
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": None,
                    "display_id": None,
                    "config_id": None,
                    "file_path": str(sc_file.relative_to(ws_root)),
                    "message": f"Step[{idx}] INJECT_NET_FIXTURE declares neither routes nor requests",
                })
            for r_idx, route in enumerate(routes):
                if not route.get("url_prefix"):
                    findings.append({
                        "rule_id": RULE_ID,
                        "severity": "error",
                        "entry_id": None,
                        "display_id": None,
                        "config_id": None,
                        "file_path": str(sc_file.relative_to(ws_root)),
                        "message": f"Route[{r_idx}] missing required 'url_prefix'",
                    })
            for req_idx, req in enumerate(requests):
                if not req.get("uri") and not req.get("url"):
                    findings.append({
                        "rule_id": RULE_ID,
                        "severity": "error",
                        "entry_id": None,
                        "display_id": None,
                        "config_id": None,
                        "file_path": str(sc_file.relative_to(ws_root)),
                        "message": f"Request[{req_idx}] missing required 'uri' or 'url'",
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
                "message": "Network scenario declares INJECT_NET_FIXTURE but has zero ASSERT_POINT steps",
            })
        else:
            # 确保至少一个断言包含实质性 matcher (非 None，非空串)
            has_meaningful_matcher = any(
                s.get("matcher") is not None and s.get("matcher") != "" for s in assert_steps
            )
            if not has_meaningful_matcher:
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": None,
                    "display_id": None,
                    "config_id": None,
                    "file_path": str(sc_file.relative_to(ws_root)),
                    "message": "Network scenario assertions lack meaningful non-empty matchers (anti-greenwashing gate)",
                })

    return findings
