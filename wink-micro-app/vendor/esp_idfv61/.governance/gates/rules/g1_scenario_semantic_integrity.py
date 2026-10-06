# SPDX-License-Identifier: Apache-2.0
"""
g1_scenario_semantic_integrity.py
==================================
Gate 1 Rule: Verifies the semantic integrity, domain affinity, and anti-false-green
validity of headless simulation scenario assertions (*.scenario.json).

Enforces:
1. Non-empty Assertion Rule:
   - Scenario files MUST contain at least one valid assertion step (e.g. ASSERT_POINT,
     ASSERT_EVENT, ASSERT_BUS_PAYLOAD).
2. Anti-Degenerate Assertion Penalty (P-1 Defense):
   - Non-power-management apps MUST NOT rely solely on static power rail assertions
     (e.g., 'power:VCC_3V3 == 3.3', 'power:VCC_WIFI == 3.3').
3. Domain Target Affinity:
   - Scenario assertions MUST target the core capabilities of the app's domain
     (e.g. bluetooth requires 'ble:*', wifi requires 'wifi:*'/'netif:*', http requires
     'http:*' or INJECT_NET_FIXTURE, uart requires 'uart:*', i2c requires 'i2c:*', etc.).
4. Tri-Partite Pin Topology Consistency:
   - Any 'gpio:<pin>' target asserted in a scenario MUST be declared in wink-app.json.
5. Temporal Dynamics / Transition Requirement:
   - Scenarios must not contain only identical, unchanging static point assertions
     without any temporal transition or stimulus-response loop.
"""

import json
import re
from pathlib import Path
from typing import List, Dict, Any, Optional
from gate_context import is_candidate_artifact

RULE_ID = "g1.scenario_semantic_integrity"

ASSERTION_STEP_TYPES = {
    "ASSERT_POINT",
    "ASSERT_RANGE",
    "ASSERT_EVENT",
    "ASSERT_WAVEFORM",
    "ASSERT_BUS_PAYLOAD",
    "ASSERT_PACKET_STREAM",
    "ASSERT_BUFFER_FRAME",
    "ASSERT_RESOURCE_GATE",
    "ASSERT_SEQUENCE",
}

ACTION_STEP_TYPES = {
    "INPUT_BUS",
    "INPUT_PIN",
    "INPUT_EVENT",
    "INPUT_ANALOG",
    "INPUT_BUFFER",
    "INPUT_POWER",
    "INPUT_PLUGIN_EVENT",
    "INPUT_PWM",
    "INJECT_FAULT",
    "INJECT_WIFI_FIXTURE",
    "INJECT_NET_FIXTURE",
    "CLEAR_FAULTS",
    "CONFIG_PIN",
    "INTERRUPT_RAISE",
    "UNREGISTER_BUS_DEVICE",
    "DELAY",
    "WAIT_UNTIL",
    "SYSTEM_CONTROL",
    "RESTORE_SNAPSHOT",
}

DOMAIN_AFFINITY_PATTERNS = [
    {
        "category_keywords": ["bluetooth", "ble", "nimble"],
        "name": "Bluetooth LE / NimBLE",
        "match_fn": lambda targets, steps, text: any(t.startswith("ble:") for t in targets),
        "error_msg": "Bluetooth/BLE application scenarios must assert domain targets matching 'ble:*'.",
    },
    {
        "category_keywords": ["wifi", "wlan", "station", "softap"],
        "name": "Wi-Fi Infrastructure",
        "match_fn": lambda targets, steps, text: any(
            t.startswith("wifi:") or t.startswith("netif:") for t in targets
        ),
        "error_msg": "Wi-Fi application scenarios must assert domain targets matching 'wifi:*' or 'netif:*'.",
    },
    {
        "category_keywords": ["http_client", "protocols/http", "esp_http_client"],
        "name": "HTTP Client",
        "match_fn": lambda targets, steps, text: any(
            t.startswith("http:") or t.startswith("netif:") for t in targets
        ),
        "error_msg": "HTTP application scenarios must assert 'http:*' or 'netif:*' domain targets.",
    },
    {
        "category_keywords": ["mqtt", "mqtt_tcp"],
        "name": "MQTT Protocol",
        "match_fn": lambda targets, steps, text: any(
            t.startswith("mqtt:") or t.startswith("netif:") for t in targets
        ),
        "error_msg": "MQTT application scenarios must assert 'mqtt:*' or 'netif:*' domain targets.",
    },
    {
        "category_keywords": ["uart", "uart_echo"],
        "name": "UART Communication",
        "match_fn": lambda targets, steps, text: any(
            t.startswith("uart:") for t in targets
        ),
        "error_msg": "UART application scenarios must assert 'uart:*' targets or bus payload echoes.",
    },
    {
        "category_keywords": ["i2c", "i2c_basic"],
        "name": "I2C Master/Slave",
        "match_fn": lambda targets, steps, text: any(
            t.startswith("i2c:") for t in targets
        ),
        "error_msg": "I2C application scenarios must assert 'i2c:*' targets or I2C bus transactions.",
    },
    {
        "category_keywords": ["gptimer", "timer_group", "hardware_timer"],
        "name": "Hardware Timer / GPTimer",
        "match_fn": lambda targets, steps, text: any(
            t.startswith("timer:") or t.startswith("gptimer:") for t in targets
        ),
        "error_msg": "Timer application scenarios must assert 'timer:*' or 'gptimer:*' targets.",
    },
    {
        "category_keywords": ["ledc", "pwm"],
        "name": "LEDC / PWM",
        "match_fn": lambda targets, steps, text: any(
            t.startswith("pwm:") or t.startswith("ledc:") for t in targets
        ),
        "error_msg": "LEDC/PWM application scenarios must assert 'pwm:*' or 'ledc:*' targets.",
    },
    {
        "category_keywords": ["blink", "gpio"],
        "name": "GPIO / Blink",
        "match_fn": lambda targets, steps, text: any(t.startswith("gpio:") for t in targets),
        "error_msg": "GPIO/Blink application scenarios must assert 'gpio:<pin>' targets.",
    },
]


def _extract_declared_gpios(wink_app_path: Path) -> set:
    """Extract set of gpio pin numbers declared in wink-app.json devices."""
    declared = set()
    if not wink_app_path.is_file():
        return declared
    try:
        data = json.loads(wink_app_path.read_text(encoding="utf-8"))
        devices = data.get("devices", {})
        if isinstance(devices, dict):
            for dev_name, dev_cfg in devices.items():
                if isinstance(dev_cfg, dict):
                    pin = dev_cfg.get("gpio_pin") or dev_cfg.get("pin")
                    if pin is not None:
                        declared.add(str(pin))
    except Exception:
        pass
    return declared


def _extract_step_targets(step: dict) -> list[str]:
    """Extract all assertion targets declared in an assertion step."""
    stype = step.get("type")
    targets = []
    if not stype:
        return targets

    # Direct target field (ASSERT_POINT, ASSERT_RANGE, ASSERT_EVENT)
    tgt = step.get("target")
    if tgt and isinstance(tgt, str):
        targets.append(tgt)

    if stype == "ASSERT_WAVEFORM":
        pin = step.get("pin")
        if pin is not None:
            pin_str = str(pin)
            targets.append(pin_str if pin_str.startswith("gpio:") else f"gpio:{pin_str}")

    elif stype in ("ASSERT_BUS_PAYLOAD", "ASSERT_PACKET_STREAM"):
        bus_type = step.get("busType")
        if bus_type:
            bus_id = step.get("busId")
            if bus_id is not None:
                targets.append(f"{bus_type}:{bus_id}")
            targets.append(f"{bus_type}:payload")

    elif stype == "ASSERT_BUFFER_FRAME":
        channel = step.get("channel", "framebuffer")
        targets.append(f"framebuffer:{channel}")
        targets.append("display:framebuffer")

    elif stype == "ASSERT_RESOURCE_GATE":
        targets.append("resource:gate")
        for metric in ("heap", "cpu", "power"):
            if metric in step:
                targets.append(f"resource:{metric}")

    elif stype == "ASSERT_SEQUENCE":
        for ev in step.get("events", []):
            if isinstance(ev, dict):
                ev_tgt = ev.get("target")
                if ev_tgt and isinstance(ev_tgt, str):
                    targets.append(ev_tgt)
                ev_bus = ev.get("busType")
                if ev_bus and isinstance(ev_bus, str):
                    targets.append(f"{ev_bus}:payload")

    return targets


def run(context: dict, config: dict | None = None) -> list[dict]:
    findings = []
    ws_root = Path(context.get("workspace_root", "."))
    vendor_root = ws_root / "wink-micro-app" / "vendor" / "esp_idfv61"

    if not vendor_root.is_dir():
        return findings

    manifest = context.get("manifest", {})
    entries = manifest.get("entries", [])
    entries_by_target: Dict[str, dict] = {}
    for e in entries:
        t = e.get("target_app_dir")
        if t:
            norm_t = str(t).replace("\\", "/").strip("/")
            entries_by_target[norm_t] = e

    known_step_types = ASSERTION_STEP_TYPES | ACTION_STEP_TYPES

    # Discover all landed apps
    for manifest_file in sorted(vendor_root.rglob("wink-app.json")):
        if is_candidate_artifact(manifest_file, vendor_root):
            continue
        app_dir = manifest_file.parent
        target_app_dir = app_dir.relative_to(vendor_root).as_posix().strip("/")

        entry = entries_by_target.get(target_app_dir)
        entry_id = entry.get("id") if entry else None
        display_id = entry.get("display_id") if entry else None

        scenarios_dir = app_dir / "unisim-scenarios"
        if not scenarios_dir.is_dir():
            continue

        declared_gpios = _extract_declared_gpios(manifest_file)
        scenario_files = sorted(scenarios_dir.glob("*.scenario.json"))

        for scen_file in scenario_files:
            try:
                scen_data = json.loads(scen_file.read_text(encoding="utf-8"))
            except Exception as ex:
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": entry_id,
                    "display_id": display_id,
                    "config_id": None,
                    "file_path": str(scen_file),
                    "message": f"Malformed scenario JSON in '{scen_file.name}': {ex}",
                })
                continue

            steps = scen_data.get("steps", [])
            if not isinstance(steps, list) or len(steps) == 0:
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": entry_id,
                    "display_id": display_id,
                    "config_id": None,
                    "file_path": str(scen_file),
                    "message": f"Scenario '{scen_file.name}' has no 'steps' defined.",
                })
                continue

            # Validate against unknown step types (Fail-Loud)
            has_unknown_steps = False
            for step in steps:
                stype = step.get("type") if isinstance(step, dict) else None
                if not stype or stype not in known_step_types:
                    findings.append({
                        "rule_id": RULE_ID,
                        "severity": "error",
                        "entry_id": entry_id,
                        "display_id": display_id,
                        "config_id": None,
                        "file_path": str(scen_file),
                        "message": (
                            f"Unknown or missing scenario step type '{stype}' in '{scen_file.name}'. "
                            f"Must be a valid UniSim step type."
                        ),
                    })
                    has_unknown_steps = True

            # Extract assertions
            assertion_steps = [s for s in steps if isinstance(s, dict) and s.get("type") in ASSERTION_STEP_TYPES]
            targets = []
            for s in assertion_steps:
                targets.extend(_extract_step_targets(s))

            # 1. Non-empty Assertion Rule
            if len(assertion_steps) == 0:
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": entry_id,
                    "display_id": display_id,
                    "config_id": None,
                    "file_path": str(scen_file),
                    "message": (
                        f"Scenario '{scen_file.name}' in '{target_app_dir}' contains 0 assertion steps. "
                        f"At least one valid assertion (e.g. ASSERT_POINT, ASSERT_WAVEFORM, ASSERT_BUS_PAYLOAD) is required."
                    ),
                })
                continue

            # Determine blocking severity (Fail-Closed):
            # If the app claims to be 'verified' (or entry is missing/untracked), false green is a blocking ERROR.
            # Only if the entry explicitly declares all executions as unverified ('planned'/'building'/'unsupported')
            # does it downgrade to a WARNING for in-progress drafting.
            executions = entry.get("executions", []) if entry else []
            is_explicitly_unverified = executions and all(
                ex.get("delivery_state") in ("planned", "building", "unsupported") for ex in executions
            )
            assertion_severity = "warning" if is_explicitly_unverified else "error"
            is_power_mgmt_app = "power_save" in target_app_dir or "sleep" in target_app_dir

            # 2. Anti-Degenerate Assertion Penalty (P-1 Defense)
            power_targets = [t for t in targets if t and t.startswith("power:")]
            non_power_targets = [t for t in targets if t and not t.startswith("power:") and not t.startswith("resource:power")]
            if len(assertion_steps) > 0 and len(non_power_targets) == 0 and not is_power_mgmt_app:
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": assertion_severity,
                    "entry_id": entry_id,
                    "display_id": display_id,
                    "config_id": None,
                    "file_path": str(scen_file),
                    "message": (
                        f"Degenerate assertion detected in '{target_app_dir}/{scen_file.name}': "
                        f"all {len(assertion_steps)} assertion(s) are static power rails ({', '.join(set(power_targets))}). "
                        f"Business-level domain assertions matching application logic are required."
                    ),
                })

            # 3. Domain Target Affinity (Output assertions must prove business function)
            target_app_lower = target_app_dir.lower()
            for domain in DOMAIN_AFFINITY_PATTERNS:
                if any(kw in target_app_lower for kw in domain["category_keywords"]):
                    matched = domain["match_fn"](targets, steps, scen_file.read_text(encoding="utf-8"))
                    if not matched:
                        findings.append({
                            "rule_id": RULE_ID,
                            "severity": assertion_severity,
                            "entry_id": entry_id,
                            "display_id": display_id,
                            "config_id": None,
                            "file_path": str(scen_file),
                            "message": (
                                f"Domain affinity violation in '{target_app_dir}/{scen_file.name}': "
                                f"{domain['error_msg']}"
                            ),
                        })
                    break

            # 4. Tri-Partite Pin Topology Consistency (Physical pins must be declared)
            for t in targets:
                if t.startswith("gpio:"):
                    pin_part = t.split(":", 1)[1].strip()
                    pin_str = pin_part.split(":")[0] if ":" in pin_part else pin_part
                    if pin_str.isdigit():
                        if pin_str not in declared_gpios:
                            findings.append({
                                "rule_id": RULE_ID,
                                "severity": "error",
                                "entry_id": entry_id,
                                "display_id": display_id,
                                "config_id": None,
                                "file_path": str(scen_file),
                                "message": (
                                    f"GPIO topology mismatch in '{target_app_dir}/{scen_file.name}': "
                                    f"asserted pin 'gpio:{pin_str}' is not declared in wink-app.json "
                                    f"devices (declared: {sorted(list(declared_gpios))})."
                                ),
                            })

            # 5. Temporal Dynamics / Transition Requirement
            # Steady-state peripheral signals (such as PWM duty cycle) are valid invariants across time.
            point_assertions = [s for s in assertion_steps if s.get("type") == "ASSERT_POINT"]
            if len(point_assertions) >= 2 and len(set(targets)) == 1:
                target_0 = targets[0]
                is_steady_state_peripheral = (
                    target_0.startswith("pwm:") or
                    target_0.startswith("ledc:") or
                    is_power_mgmt_app
                )
                matchers = [str(s.get("matcher")) for s in point_assertions]
                has_stimulus = any(
                    s.get("type", "").startswith("INJECT_") or
                    s.get("type", "").startswith("INPUT_")
                    for s in steps
                )
                if len(set(matchers)) == 1 and not has_stimulus and not is_steady_state_peripheral:
                    findings.append({
                        "rule_id": RULE_ID,
                        "severity": "warning",
                        "entry_id": entry_id,
                        "display_id": display_id,
                        "config_id": None,
                        "file_path": str(scen_file),
                        "message": (
                            f"Static point tautology warning in '{target_app_dir}/{scen_file.name}': "
                            f"all assertions check constant '{target_0} == {matchers[0]}' "
                            f"without state transition or external stimulus."
                        ),
                    })

    return findings
