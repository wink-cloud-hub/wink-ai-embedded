# SPDX-License-Identifier: Apache-2.0
"""
test_g5_anti_decay.py
=====================
Unit and mutation tests for Gate 5 rules (g5_no_inline_mock and g5_network_assertion_quality).
"""

import json
from pathlib import Path
from rules import g5_no_inline_mock, g5_network_assertion_quality


def test_g5_no_inline_mock_clean_codebase(tmp_path):
    # Context pointing to a clean mock network dir
    net_dir = tmp_path / "wink-micro-os" / "frameworks" / "esp_idf" / "src" / "network"
    net_dir.mkdir(parents=True)
    c_file = net_dir / "clean_http.c"
    c_file.write_text(
        '#include "esp_http_client.h"\n'
        'static void parse(const char *url) {\n'
        '    if (strncmp(url, "http://", 7) == 0) {}\n'
        '}\n',
        encoding="utf-8"
    )

    context = {"workspace_root": str(tmp_path)}
    findings = g5_no_inline_mock.run(context)
    assert len(findings) == 0


def test_g5_no_inline_mock_mutation_catches_banned_domain(tmp_path):
    net_dir = tmp_path / "wink-micro-os" / "frameworks" / "esp_idf" / "src" / "network"
    net_dir.mkdir(parents=True)
    bad_c = net_dir / "bad_http.c"
    bad_c.write_text(
        'void perform(void) {\n'
        '    const char *url = "http://httpbin.org/get";\n'
        '}\n',
        encoding="utf-8"
    )

    context = {"workspace_root": str(tmp_path)}
    findings = g5_no_inline_mock.run(context)
    assert len(findings) >= 1
    assert "Hardcoded business domain/URL detected" in findings[0]["message"]


def test_g5_no_inline_mock_mutation_catches_inline_if_else(tmp_path):
    net_dir = tmp_path / "wink-micro-os" / "frameworks" / "esp_idf" / "src" / "network"
    net_dir.mkdir(parents=True)
    bad_c = net_dir / "spaghetti_http.c"
    bad_c.write_text(
        'void perform(struct client *c) {\n'
        '    if (strstr(c->url, "/ota_firmware")) {\n'
        '        do_mock();\n'
        '    }\n'
        '}\n',
        encoding="utf-8"
    )

    context = {"workspace_root": str(tmp_path)}
    findings = g5_no_inline_mock.run(context)
    assert len(findings) >= 1
    assert "Inlined URL branch check detected" in findings[0]["message"]


def test_g5_network_assertion_quality_positive(tmp_path):
    sc_dir = tmp_path / "wink-micro-app" / "vendor" / "esp_idfv61" / "protocols" / "http" / "unisim-scenarios"
    sc_dir.mkdir(parents=True)
    sc_file = sc_dir / "test.scenario.json"
    sc_data = {
        "header": {"name": "test"},
        "steps": [
            {
                "type": "INJECT_NET_FIXTURE",
                "routes": [{"url_prefix": "http://example/get"}]
            },
            {
                "type": "ASSERT_POINT",
                "matcher": "HTTP_EVENT_ON_FINISH"
            }
        ]
    }
    sc_file.write_text(json.dumps(sc_data), encoding="utf-8")

    context = {"workspace_root": str(tmp_path)}
    findings = g5_network_assertion_quality.run(context)
    assert len(findings) == 0


def test_g5_network_assertion_quality_catches_empty_matcher(tmp_path):
    sc_dir = tmp_path / "wink-micro-app" / "vendor" / "esp_idfv61" / "protocols" / "http" / "unisim-scenarios"
    sc_dir.mkdir(parents=True)
    sc_file = sc_dir / "fake_green.scenario.json"
    sc_data = {
        "header": {"name": "fake_green"},
        "steps": [
            {
                "type": "INJECT_NET_FIXTURE",
                "routes": [{"url_prefix": "http://example/get"}]
            },
            {
                "type": "ASSERT_POINT",
                "matcher": ""
            }
        ]
    }
    sc_file.write_text(json.dumps(sc_data), encoding="utf-8")

    context = {"workspace_root": str(tmp_path)}
    findings = g5_network_assertion_quality.run(context)
    assert len(findings) >= 1
    assert "anti-greenwashing" in findings[0]["message"]
