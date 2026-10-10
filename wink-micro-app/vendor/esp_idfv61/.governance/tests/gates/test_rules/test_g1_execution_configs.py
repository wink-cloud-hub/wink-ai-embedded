# SPDX-License-Identifier: Apache-2.0
"""
Unit tests for g1_execution_configs.py.
"""

from gates.rules import g1_execution_configs


def test_execution_configs_positive():
    context = {
        "manifest": {
            "entries": [
                {
                    "id": "esp.get_started.blink",
                    "display_id": 1,
                    "executions": [
                        {
                            "config_id": "wasm_sim_standard",
                            "backend": "wasm_browser",
                            "target_soc": "esp32",
                            "profile": "standard",
                            "delivery_state": "planned",
                            "acceptance": {
                                "type": "wasm_simulation",
                                "observability_level": "L1_ui",
                            },
                        }
                    ],
                }
            ]
        }
    }
    findings = g1_execution_configs.run(context)
    assert len(findings) == 0


def test_execution_configs_missing_array():
    context = {
        "manifest": {
            "entries": [
                {
                    "id": "esp.a",
                    "display_id": 1,
                    "executions": [],
                }
            ]
        }
    }
    findings = g1_execution_configs.run(context)
    assert len(findings) == 1
    assert "must have a non-empty 'executions' array" in findings[0]["message"]


def test_execution_configs_invalid_backend_and_soc():
    context = {
        "manifest": {
            "entries": [
                {
                    "id": "esp.a",
                    "display_id": 1,
                    "executions": [
                        {
                            "config_id": "bad_cfg",
                            "backend": "quantum_computer",
                            "target_soc": "arm_cortex_m",
                            "profile": "standard",
                            "delivery_state": "planned",
                            "acceptance": {
                                "type": "wasm_simulation",
                                "observability_level": "L1_ui",
                            },
                        }
                    ],
                }
            ]
        }
    }
    findings = g1_execution_configs.run(context)
    assert len(findings) == 2
    msgs = " ".join(f["message"] for f in findings)
    assert "invalid backend 'quantum_computer'" in msgs
    assert "invalid target_soc 'arm_cortex_m'" in msgs
