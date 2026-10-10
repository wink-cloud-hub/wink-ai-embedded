# SPDX-License-Identifier: Apache-2.0
"""
test_g5_wifi_anti_decay.py
==========================
Unit and mutation tests for Gate 5 Wi-Fi rules:
  - g5_no_wifi_inline_mock (Rule 503)
  - g5_wifi_assertion_quality (Rule 504)
"""

import json
from pathlib import Path
from gates.rules import g5_no_wifi_inline_mock, g5_wifi_assertion_quality


def test_g5_no_wifi_inline_mock_clean_codebase(tmp_path):
    wifi_dir = tmp_path / "wink-micro-os" / "frameworks" / "esp_idf" / "src" / "wifi"
    wifi_dir.mkdir(parents=True)
    c_file = wifi_dir / "clean_wifi.c"
    c_file.write_text(
        '#include "esp_wifi.h"\n'
        'static void configure(void) {\n'
        '    uint32_t ip = ESP_IP4TOADDR(192, 168, 1, 100);\n'
        '}\n',
        encoding="utf-8"
    )

    context = {"workspace_root": str(tmp_path)}
    findings = g5_no_wifi_inline_mock.run(context)
    assert len(findings) == 0


def test_g5_no_wifi_inline_mock_mutation_catches_ipv4_literal(tmp_path):
    wifi_dir = tmp_path / "wink-micro-os" / "frameworks" / "esp_idf" / "src" / "wifi"
    wifi_dir.mkdir(parents=True)
    bad_c = wifi_dir / "bad_wifi.c"
    bad_c.write_text(
        'void setup_ip(void) {\n'
        '    const char *ip = "192.168.1.100";\n'
        '}\n',
        encoding="utf-8"
    )

    context = {"workspace_root": str(tmp_path)}
    findings = g5_no_wifi_inline_mock.run(context)
    assert len(findings) >= 1
    assert "Hardcoded IPv4 literal detected" in findings[0]["message"]


def test_g5_no_wifi_inline_mock_mutation_catches_pwd_branch(tmp_path):
    wifi_dir = tmp_path / "wink-micro-os" / "frameworks" / "esp_idf" / "src" / "wifi"
    wifi_dir.mkdir(parents=True)
    bad_c = wifi_dir / "spaghetti_wifi.c"
    bad_c.write_text(
        'void auth(const char *password) {\n'
        '    if (strcmp(password, "secret123") == 0) {\n'
        '        do_mock();\n'
        '    }\n'
        '}\n',
        encoding="utf-8"
    )

    context = {"workspace_root": str(tmp_path)}
    findings = g5_no_wifi_inline_mock.run(context)
    assert len(findings) >= 1
    assert "Hardcoded password comparison branch detected" in findings[0]["message"]


def test_g5_wifi_assertion_quality_positive(tmp_path):
    sc_dir = tmp_path / "wink-micro-app" / "vendor" / "esp_idfv61" / "wifi" / "wifi_sta" / "unisim-scenarios"
    sc_dir.mkdir(parents=True)
    sc_file = sc_dir / "test.scenario.json"
    sc_data = {
        "header": {"name": "wifi_sta_test"},
        "steps": [
            {
                "type": "INJECT_WIFI_FIXTURE",
                "timeUs": "0ms",
                "accessPoints": [
                    {
                        "ssid": "myssid",
                        "password": "mypassword"
                    }
                ]
            },
            {
                "type": "ASSERT_POINT",
                "timeUs": "1500ms",
                "target": "netif:sta:ip",
                "expected": "192.168.1.100"
            }
        ]
    }
    sc_file.write_text(json.dumps(sc_data), encoding="utf-8")

    context = {"workspace_root": str(tmp_path)}
    findings = g5_wifi_assertion_quality.run(context)
    assert len(findings) == 0


def test_g5_wifi_assertion_quality_catches_empty_aps(tmp_path):
    sc_dir = tmp_path / "wink-micro-app" / "vendor" / "esp_idfv61" / "wifi" / "wifi_sta" / "unisim-scenarios"
    sc_dir.mkdir(parents=True)
    sc_file = sc_dir / "bad.scenario.json"
    sc_data = {
        "header": {"name": "empty_aps"},
        "steps": [
            {
                "type": "INJECT_WIFI_FIXTURE",
                "timeUs": "0ms",
                "accessPoints": []
            },
            {
                "type": "ASSERT_POINT",
                "timeUs": "1000ms",
                "target": "wifi:sta:state",
                "expected": "CONNECTED"
            }
        ]
    }
    sc_file.write_text(json.dumps(sc_data), encoding="utf-8")

    context = {"workspace_root": str(tmp_path)}
    findings = g5_wifi_assertion_quality.run(context)
    assert len(findings) >= 1
    assert "empty accessPoints list" in findings[0]["message"]


def test_g5_wifi_assertion_quality_catches_missing_ssid(tmp_path):
    sc_dir = tmp_path / "wink-micro-app" / "vendor" / "esp_idfv61" / "wifi" / "wifi_sta" / "unisim-scenarios"
    sc_dir.mkdir(parents=True)
    sc_file = sc_dir / "no_ssid.scenario.json"
    sc_data = {
        "header": {"name": "no_ssid"},
        "steps": [
            {
                "type": "INJECT_WIFI_FIXTURE",
                "timeUs": "0ms",
                "accessPoints": [{"password": "pwd"}]
            },
            {
                "type": "ASSERT_POINT",
                "timeUs": "1000ms",
                "target": "wifi:sta:state",
                "expected": "CONNECTED"
            }
        ]
    }
    sc_file.write_text(json.dumps(sc_data), encoding="utf-8")

    context = {"workspace_root": str(tmp_path)}
    findings = g5_wifi_assertion_quality.run(context)
    assert len(findings) >= 1
    assert "missing required 'ssid'" in findings[0]["message"]


def test_g5_wifi_assertion_quality_catches_non_semantic_target(tmp_path):
    sc_dir = tmp_path / "wink-micro-app" / "vendor" / "esp_idfv61" / "wifi" / "wifi_sta" / "unisim-scenarios"
    sc_dir.mkdir(parents=True)
    sc_file = sc_dir / "greenwash.scenario.json"
    sc_data = {
        "header": {"name": "greenwash"},
        "steps": [
            {
                "type": "INJECT_WIFI_FIXTURE",
                "timeUs": "0ms",
                "accessPoints": [{"ssid": "myssid"}]
            },
            {
                "type": "ASSERT_POINT",
                "timeUs": "1000ms",
                "target": "gpio:pin2:voltage",
                "expected": "3.3V"
            }
        ]
    }
    sc_file.write_text(json.dumps(sc_data), encoding="utf-8")

    context = {"workspace_root": str(tmp_path)}
    findings = g5_wifi_assertion_quality.run(context)
    assert len(findings) >= 1
    assert "anti-greenwashing gate" in findings[0]["message"]
