# SPDX-License-Identifier: Apache-2.0
"""
Unit and mutation tests for g1_scenario_semantic_integrity.py.
"""

import json
from pathlib import Path
from rules import g1_scenario_semantic_integrity


def _create_carrier_app(ws: Path, rel_path: str, wink_app_data: dict, scenario_data: dict) -> Path:
    vendor = ws / "wink-micro-app" / "vendor" / "esp_idfv61"
    app_dir = vendor / rel_path
    app_dir.mkdir(parents=True, exist_ok=True)
    (app_dir / "wink-app.json").write_text(json.dumps(wink_app_data), encoding="utf-8")
    scen_dir = app_dir / "unisim-scenarios"
    scen_dir.mkdir(parents=True, exist_ok=True)
    (scen_dir / "test.scenario.json").write_text(json.dumps(scenario_data), encoding="utf-8")
    return app_dir


def test_scenario_semantic_integrity_valid_gpio(tmp_path):
    """Valid GPIO app with declared pin and alternating levels passes."""
    ws = tmp_path
    _create_carrier_app(
        ws,
        "get-started/blink_test",
        {
            "app_name": "blink_test",
            "devices": {"led": {"type": "led", "gpio_pin": 2}},
        },
        {
            "header": {"name": "test"},
            "steps": [
                {"type": "ASSERT_POINT", "timeUs": "50ms", "target": "gpio:2", "matcher": 0},
                {"type": "ASSERT_POINT", "timeUs": "1050ms", "target": "gpio:2", "matcher": 1},
            ],
        },
    )

    ctx = {"workspace_root": str(ws), "manifest": {"entries": []}}
    findings = g1_scenario_semantic_integrity.run(ctx)
    errors = [f for f in findings if f["severity"] == "error"]
    assert len(errors) == 0, f"Expected 0 errors, got: {errors}"


def test_scenario_semantic_integrity_valid_wifi(tmp_path):
    """Valid Wi-Fi STA app with netif IP and state assertions passes."""
    ws = tmp_path
    _create_carrier_app(
        ws,
        "wifi/wifi_sta_test",
        {"app_name": "wifi_sta_test", "devices": {}},
        {
            "header": {"name": "test"},
            "steps": [
                {"type": "INJECT_WIFI_FIXTURE", "timeUs": "0ms", "accessPoints": []},
                {"type": "ASSERT_POINT", "timeUs": "1500ms", "target": "netif:sta:ip", "matcher": "192.168.1.100"},
                {"type": "ASSERT_POINT", "timeUs": "2500ms", "target": "wifi:sta:state", "matcher": "DISCONNECTED"},
            ],
        },
    )

    ctx = {"workspace_root": str(ws), "manifest": {"entries": []}}
    findings = g1_scenario_semantic_integrity.run(ctx)
    errors = [f for f in findings if f["severity"] == "error"]
    assert len(errors) == 0, f"Expected 0 errors, got: {errors}"


def test_negative_degenerate_power_assertions(tmp_path):
    """Intercept false-green app that only asserts static 3.3V power rails."""
    ws = tmp_path
    _create_carrier_app(
        ws,
        "protocols/http_client",
        {"app_name": "http_client_test", "devices": {}},
        {
            "header": {"name": "test"},
            "steps": [
                {"type": "ASSERT_POINT", "timeUs": "500ms", "target": "power:VCC_WIFI", "matcher": 3.3},
                {"type": "ASSERT_POINT", "timeUs": "1000ms", "target": "power:VCC_3V3", "matcher": 3.3},
            ],
        },
    )

    ctx = {"workspace_root": str(ws), "manifest": {"entries": []}}
    findings = g1_scenario_semantic_integrity.run(ctx)
    errors = [f for f in findings if f["severity"] == "error"]
    assert len(errors) >= 1
    assert any("Degenerate assertion detected" in e["message"] for e in errors)


def test_negative_domain_affinity_violation_ble(tmp_path):
    """BLE app with only GPIO assertion violates domain affinity."""
    ws = tmp_path
    _create_carrier_app(
        ws,
        "bluetooth/bleprph",
        {"app_name": "ble_test", "devices": {"led": {"gpio_pin": 2}}},
        {
            "header": {"name": "test"},
            "steps": [
                {"type": "ASSERT_POINT", "timeUs": "500ms", "target": "gpio:2", "matcher": 1},
                {"type": "ASSERT_POINT", "timeUs": "1000ms", "target": "gpio:2", "matcher": 0},
            ],
        },
    )

    ctx = {"workspace_root": str(ws), "manifest": {"entries": []}}
    findings = g1_scenario_semantic_integrity.run(ctx)
    errors = [f for f in findings if f["severity"] == "error"]
    assert any("Domain affinity violation" in e["message"] for e in errors)


def test_negative_undeclared_gpio_pin(tmp_path):
    """Asserting undeclared GPIO pin triggers topology mismatch error."""
    ws = tmp_path
    _create_carrier_app(
        ws,
        "get-started/blink_test",
        {
            "app_name": "blink_test",
            "devices": {"led": {"gpio_pin": 4}},  # declared 4
        },
        {
            "header": {"name": "test"},
            "steps": [
                {"type": "ASSERT_POINT", "timeUs": "50ms", "target": "gpio:99", "matcher": 0},  # undeclared 99
                {"type": "ASSERT_POINT", "timeUs": "1050ms", "target": "gpio:99", "matcher": 1},
            ],
        },
    )

    ctx = {"workspace_root": str(ws), "manifest": {"entries": []}}
    findings = g1_scenario_semantic_integrity.run(ctx)
    errors = [f for f in findings if f["severity"] == "error"]
    assert any("GPIO topology mismatch" in e["message"] for e in errors)


def test_negative_zero_assertions(tmp_path):
    """Scenario with 0 assertions is strictly rejected."""
    ws = tmp_path
    _create_carrier_app(
        ws,
        "peripherals/test_app",
        {"app_name": "test", "devices": {}},
        {
            "header": {"name": "test"},
            "steps": [
                {"type": "DELAY", "timeUs": "100ms"},
            ],
        },
    )

    ctx = {"workspace_root": str(ws), "manifest": {"entries": []}}
    findings = g1_scenario_semantic_integrity.run(ctx)
    errors = [f for f in findings if f["severity"] == "error"]
    assert any("contains 0 assertion steps" in e["message"] for e in errors)


def test_warning_static_point_tautology(tmp_path):
    """Asserting constant without stimulus or transition emits a warning."""
    ws = tmp_path
    _create_carrier_app(
        ws,
        "get-started/blink_test",
        {
            "app_name": "blink_test",
            "devices": {"led": {"gpio_pin": 2}},
        },
        {
            "header": {"name": "test"},
            "steps": [
                {"type": "ASSERT_POINT", "timeUs": "50ms", "target": "gpio:2", "matcher": 1},
                {"type": "ASSERT_POINT", "timeUs": "1000ms", "target": "gpio:2", "matcher": 1},
            ],
        },
    )

    ctx = {"workspace_root": str(ws), "manifest": {"entries": []}}
    findings = g1_scenario_semantic_integrity.run(ctx)
    warnings = [f for f in findings if f["severity"] == "warning"]
    assert any("Static point tautology warning" in w["message"] for w in warnings)


def test_planned_carrier_downgrades_to_warning(tmp_path):
    """An app explicitly in planned state gets warnings instead of blocking errors."""
    ws = tmp_path
    _create_carrier_app(
        ws,
        "protocols/http_client",
        {"app_name": "http_client_test", "devices": {}},
        {
            "header": {"name": "test"},
            "steps": [
                {"type": "ASSERT_POINT", "timeUs": "500ms", "target": "power:VCC_3V3", "matcher": 3.3},
                {"type": "ASSERT_POINT", "timeUs": "1000ms", "target": "power:VCC_3V3", "matcher": 3.3},
            ],
        },
    )

    ctx = {
        "workspace_root": str(ws),
        "manifest": {
            "entries": [
                {
                    "target_app_dir": "protocols/http_client",
                    "executions": [{"config_id": "wasm_sim_standard", "delivery_state": "planned"}],
                }
            ]
        },
    }
    findings = g1_scenario_semantic_integrity.run(ctx)
    errors = [f for f in findings if f["severity"] == "error"]
    warnings = [f for f in findings if f["severity"] == "warning"]
    assert len(errors) == 0, f"Expected 0 errors for planned app, got: {errors}"
    assert len(warnings) >= 1
    assert any("Degenerate assertion detected" in w["message"] for w in warnings)


def test_valid_waveform_assertion_recognized(tmp_path):
    """ASSERT_WAVEFORM is recognized as a valid assertion and matches GPIO domain."""
    ws = tmp_path
    _create_carrier_app(
        ws,
        "get-started/blink_test",
        {
            "app_name": "blink_test",
            "devices": {"led": {"type": "led", "gpio_pin": 2}},
        },
        {
            "header": {"name": "test"},
            "steps": [
                {
                    "type": "ASSERT_WAVEFORM",
                    "windowUs": ["0ms", "1000ms"],
                    "pin": 2,
                    "minEdges": 2,
                    "maxEdges": 10,
                }
            ],
        },
    )

    ctx = {"workspace_root": str(ws), "manifest": {"entries": []}}
    findings = g1_scenario_semantic_integrity.run(ctx)
    errors = [f for f in findings if f["severity"] == "error"]
    assert len(errors) == 0, f"Expected 0 errors, got: {errors}"


def test_valid_sequence_assertion_recognized(tmp_path):
    """ASSERT_SEQUENCE is recognized as a valid assertion and matches domain targets."""
    ws = tmp_path
    _create_carrier_app(
        ws,
        "peripherals/uart_echo",
        {"app_name": "uart_echo", "devices": {}},
        {
            "header": {"name": "test"},
            "steps": [
                {
                    "type": "INPUT_BUS",
                    "timeUs": "100ms",
                    "busType": "uart",
                    "busId": 1,
                    "payload": {"text": "ping"},
                },
                {
                    "type": "ASSERT_SEQUENCE",
                    "windowUs": ["100ms", "500ms"],
                    "ordered": True,
                    "events": [
                        {"busType": "uart", "matcher": "ping"},
                    ],
                },
            ],
        },
    )

    ctx = {"workspace_root": str(ws), "manifest": {"entries": []}}
    findings = g1_scenario_semantic_integrity.run(ctx)
    errors = [f for f in findings if f["severity"] == "error"]
    assert len(errors) == 0, f"Expected 0 errors, got: {errors}"


def test_valid_resource_gate_assertion_recognized(tmp_path):
    """ASSERT_RESOURCE_GATE is recognized as a valid non-empty assertion step."""
    ws = tmp_path
    _create_carrier_app(
        ws,
        "get-started/blink_test",
        {"app_name": "blink_test", "devices": {"led": {"gpio_pin": 2}}},
        {
            "header": {"name": "test"},
            "steps": [
                {"type": "ASSERT_POINT", "timeUs": "100ms", "target": "gpio:2", "matcher": 1},
                {
                    "type": "ASSERT_RESOURCE_GATE",
                    "timeUs": "500ms",
                    "heap": {"minFreeBytes": 10240},
                },
            ],
        },
    )

    ctx = {"workspace_root": str(ws), "manifest": {"entries": []}}
    findings = g1_scenario_semantic_integrity.run(ctx)
    errors = [f for f in findings if f["severity"] == "error"]
    assert len(errors) == 0, f"Expected 0 errors, got: {errors}"


def test_negative_unknown_step_type_rejected(tmp_path):
    """Unknown or unspec'd scenario step types are rejected with blocking errors."""
    ws = tmp_path
    _create_carrier_app(
        ws,
        "get-started/blink_test",
        {"app_name": "blink_test", "devices": {"led": {"gpio_pin": 2}}},
        {
            "header": {"name": "test"},
            "steps": [
                {"type": "NON_EXISTENT_STEP_MAGIC", "timeUs": "50ms"},
                {"type": "ASSERT_POINT", "timeUs": "100ms", "target": "gpio:2", "matcher": 1},
            ],
        },
    )

    ctx = {"workspace_root": str(ws), "manifest": {"entries": []}}
    findings = g1_scenario_semantic_integrity.run(ctx)
    errors = [f for f in findings if f["severity"] == "error"]
    assert any("Unknown or missing scenario step type" in e["message"] for e in errors)


def test_negative_fixture_alone_does_not_satisfy_domain(tmp_path):
    """INJECT_NET_FIXTURE alone without business output assertion violates domain affinity."""
    ws = tmp_path
    _create_carrier_app(
        ws,
        "protocols/http_client",
        {"app_name": "http_test", "devices": {}},
        {
            "header": {"name": "test"},
            "steps": [
                {"type": "INJECT_NET_FIXTURE", "timeUs": "0ms", "protocol": "http"},
                # Only asserts power rail, no http:* target
                {"type": "ASSERT_POINT", "timeUs": "500ms", "target": "power:VCC_WIFI", "matcher": 3.3},
            ],
        },
    )

    ctx = {"workspace_root": str(ws), "manifest": {"entries": []}}
    findings = g1_scenario_semantic_integrity.run(ctx)
    errors = [f for f in findings if f["severity"] == "error"]
    assert any("Domain affinity violation" in e["message"] for e in errors)
    assert any("Degenerate assertion detected" in e["message"] for e in errors)


def test_negative_empty_devices_topology_blocks_gpio(tmp_path):
    """When devices is completely empty ({}), any physical gpio asserting is blocked."""
    ws = tmp_path
    _create_carrier_app(
        ws,
        "get-started/blink_test",
        {
            "app_name": "blink_test",
            "devices": {},  # Completely empty topology
        },
        {
            "header": {"name": "test"},
            "steps": [
                {"type": "ASSERT_POINT", "timeUs": "50ms", "target": "gpio:2", "matcher": 0},
                {"type": "ASSERT_POINT", "timeUs": "1050ms", "target": "gpio:2", "matcher": 1},
            ],
        },
    )

    ctx = {"workspace_root": str(ws), "manifest": {"entries": []}}
    findings = g1_scenario_semantic_integrity.run(ctx)
    errors = [f for f in findings if f["severity"] == "error"]
    assert any("GPIO topology mismatch" in e["message"] for e in errors)


def test_steady_state_pwm_no_tautology_warning(tmp_path):
    """Constant PWM duty cycle verification across time is a steady-state invariant, not a tautology."""
    ws = tmp_path
    _create_carrier_app(
        ws,
        "peripherals/ledc_basic",
        {"app_name": "ledc_basic", "devices": {}},
        {
            "header": {"name": "test"},
            "steps": [
                {"type": "ASSERT_POINT", "timeUs": "50ms", "target": "pwm:0", "matcher": 50},
                {"type": "ASSERT_POINT", "timeUs": "500ms", "target": "pwm:0", "matcher": 50},
                {"type": "ASSERT_POINT", "timeUs": "1000ms", "target": "pwm:0", "matcher": 50},
            ],
        },
    )

    ctx = {"workspace_root": str(ws), "manifest": {"entries": []}}
    findings = g1_scenario_semantic_integrity.run(ctx)
    warnings = [f for f in findings if f["severity"] == "warning"]
    assert not any("Static point tautology warning" in w["message"] for w in warnings)

