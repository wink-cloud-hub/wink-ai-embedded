# SPDX-License-Identifier: Apache-2.0
"""Unit tests for multi-domain error matcher (WS-1 Task 1.3)."""
import json
import pytest
from pathlib import Path
from error_matcher import (
    match_error_assertion,
    is_error_matcher,
    validate_no_vague_matcher,
    ESP_ERR_SYMBOLS,
    WINK_STATUS_SYMBOLS,
    POSIX_ERRNO_SYMBOLS,
    NIMBLE_HS_SYMBOLS,
)
from report_contract import validate_scenario_report


def test_esp_err_domain_positive():
    """esp_err correctly matches positive codes and symbols."""
    # By symbol
    ok, msg = match_error_assertion({"domain": "esp_err", "symbol": "ESP_ERR_TIMEOUT"}, 0x107)
    assert ok, msg

    # By raw value
    ok, msg = match_error_assertion({"domain": "esp_err", "raw_value": 0x107}, 263)
    assert ok, msg

    # Hex string
    ok, msg = match_error_assertion({"domain": "esp_err", "symbol": "ESP_ERR_TIMEOUT"}, "0x107")
    assert ok, msg

    # ESP_FAIL is allowed special case (-1)
    ok, msg = match_error_assertion({"domain": "esp_err", "symbol": "ESP_FAIL"}, -1)
    assert ok, msg


def test_esp_err_domain_rejects_negative():
    """esp_err rejects negative error codes like -0x107 / -263 (domain confusion prevention)."""
    ok, msg = match_error_assertion({"domain": "esp_err", "symbol": "ESP_ERR_TIMEOUT"}, -263)
    assert not ok
    assert "ERROR_DOMAIN_MISMATCH" in msg

    ok, msg = match_error_assertion({"domain": "esp_err", "symbol": "ESP_ERR_TIMEOUT"}, -2)
    assert not ok
    assert "ERROR_DOMAIN_MISMATCH" in msg


def test_wink_status_domain_negative():
    """wink_status correctly matches negative codes (ADR-0001)."""
    ok, msg = match_error_assertion({"domain": "wink_status", "symbol": "WINK_ERR_TIMEOUT"}, -2)
    assert ok, msg

    ok, msg = match_error_assertion({"domain": "wink_status", "raw_value": -2}, -2)
    assert ok, msg

    ok, msg = match_error_assertion({"domain": "wink_status", "symbol": "WINK_OK"}, 0)
    assert ok, msg


def test_wink_status_domain_rejects_positive():
    """wink_status rejects positive codes like 263 / 0x107."""
    ok, msg = match_error_assertion({"domain": "wink_status", "symbol": "WINK_ERR_TIMEOUT"}, 263)
    assert not ok
    assert "ERROR_DOMAIN_MISMATCH" in msg


def test_posix_and_nimble_domains():
    """posix_errno and nimble_hs domains match positive codes and reject negative."""
    ok, msg = match_error_assertion({"domain": "posix_errno", "symbol": "ETIMEDOUT"}, 110)
    assert ok, msg
    ok, msg = match_error_assertion({"domain": "posix_errno", "symbol": "ETIMEDOUT"}, -110)
    assert not ok
    assert "ERROR_DOMAIN_MISMATCH" in msg

    ok, msg = match_error_assertion({"domain": "nimble_hs", "symbol": "BLE_HS_ETIMEOUT"}, 5)
    assert ok, msg
    ok, msg = match_error_assertion({"domain": "nimble_hs", "symbol": "BLE_HS_ETIMEOUT"}, -5)
    assert not ok
    assert "ERROR_DOMAIN_MISMATCH" in msg


def test_vague_matcher_rejection():
    """Untyped fuzzy comparisons are rejected."""
    ok, msg = validate_no_vague_matcher({"op": "!=", "value": 0})
    assert not ok
    assert "VAGUE_MATCHER_REJECTED" in msg

    ok, msg = validate_no_vague_matcher({"op": "<", "value": 0})
    assert not ok
    assert "VAGUE_MATCHER_REJECTED" in msg

    # Allowed when typed with domain
    ok, msg = validate_no_vague_matcher({"domain": "esp_err", "symbol": "ESP_ERR_TIMEOUT"})
    assert ok


def test_report_contract_integration(tmp_path: Path):
    """validate_scenario_report uses error_matcher for typed error steps."""
    scenario = {
        "header": {"name": "error_test", "templateId": "tmpl1"},
        "steps": [
            {
                "type": "ASSERT_POINT",
                "target": "uart:0",
                "matcher": {
                    "domain": "esp_err",
                    "symbol": "ESP_ERR_TIMEOUT",
                    "raw_value": 0x107,
                },
            }
        ],
    }
    # Passing report with correct positive esp_err
    passing_report = {
        "total": 1,
        "passed": 1,
        "failed": 0,
        "results": [
            {
                "status": "passed",
                "ok": True,
                "header": {"name": "error_test", "templateId": "tmpl1"},
                "summary": {
                    "totalSteps": 1,
                    "passedSteps": 1,
                    "failedSteps": 0,
                    "errorSteps": 0,
                    "skippedSteps": 0,
                },
                "stepResults": [
                    {
                        "stepIndex": 0,
                        "type": "ASSERT_POINT",
                        "target": "uart:0",
                        "status": "passed",
                        "expected": scenario["steps"][0]["matcher"],
                        "actual": 0x107,
                    }
                ],
                "diagnostics": [],
            }
        ],
    }

    scen_path = tmp_path / "scenario.json"
    rep_path = tmp_path / "report.json"
    scen_path.write_text(json.dumps(scenario), encoding="utf-8")
    rep_path.write_text(json.dumps(passing_report), encoding="utf-8")

    ok, reason = validate_scenario_report(rep_path, scen_path)
    assert ok, reason

    # Negative observation on esp_err domain triggers failure
    passing_report["results"][0]["stepResults"][0]["actual"] = -263
    rep_path.write_text(json.dumps(passing_report), encoding="utf-8")
    ok, reason = validate_scenario_report(rep_path, scen_path)
    assert not ok
    assert "ERROR_DOMAIN_MISMATCH" in reason
