# SPDX-License-Identifier: Apache-2.0
"""
Unit tests for Autonomous Self-Healing Remediator Engine (tools/loop/remediator.py)
Tests 12-state lifecycle, Tier 1 C parsing, H-1 to H-8 rules, PAL additive checks,
pre-flight clean checks, dual-track rollback, and mock agent orchestration.
100% offline and deterministic.
"""
import copy
import json
import shutil
import tempfile
from pathlib import Path
import pytest
import sys

TOOLS_DIR = Path(__file__).resolve().parent.parent.parent / "tools"
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

from loop.safety_checker import HeuristicSafetyChecker, TieredCParser
from loop.remediator import (
    InvestigationWorkspace,
    TransactionalGitTracker,
    ZeroRegressionRunner,
    Remediator,
    RemediatorState,
    DOMAIN_FILE_MAP,
)
from loop.agent import AgentSynthesizer


# -----------------------------------------------------------------------------
# Tier 1 C Parser Tests
# -----------------------------------------------------------------------------

def test_tiered_c_parser_strips_comments_and_strings():
    parser = TieredCParser()
    raw = '''
    // Single line comment
    /* Block
       comment */
    const char *str = "Hello // not a comment";
    char ch = 'x';
    return ESP_OK;
    '''
    clean = parser.strip_comments_and_strings(raw)
    assert "// Single line" not in clean
    assert "Block" not in clean
    assert "Hello" not in clean
    assert "return ESP_OK;" in clean


def test_tiered_c_parser_detects_empty_stubs():
    parser = TieredCParser()
    # Case 1: Trivial return ESP_OK
    stub1 = "ESP_LOGI(TAG, \"init\"); return ESP_OK;"
    assert parser.is_empty_stub(stub1)

    # Case 2: Only log macro, no return
    stub2 = "ESP_LOGW(TAG, \"warning\");"
    assert parser.is_empty_stub(stub2)

    # Case 3: Empty body
    stub3 = "   "
    assert parser.is_empty_stub(stub3)

    # Case 4: Pure variable declaration and return 0
    stub4 = "int ret = 0; return 0;"
    assert parser.is_empty_stub(stub4)

    # Case 5: Genuine functional logic (not a stub)
    legit = """
    s_broker_state = BROKER_STATE_CONNECTED;
    ringbuf_write(&s_rx_buf, data, len);
    return ESP_OK;
    """
    assert not parser.is_empty_stub(legit)


# -----------------------------------------------------------------------------
# Heuristic Safety Checker Tests (H-1 to H-8)
# -----------------------------------------------------------------------------

def test_safety_checker_h8_whitelist_and_blocked_paths():
    checker = HeuristicSafetyChecker()

    # Blocked OSAL path
    bad_patch = """--- a/wink-micro-os/pal/include/osal/pal_osal.h
+++ b/wink-micro-os/pal/include/osal/pal_osal.h
@@ -10,3 +10,4 @@
+int hack;
"""
    ok, errs = checker.validate_patch_scope(bad_patch)
    assert not ok
    assert any("strictly blocked path" in e for e in errs)

    # Whitelisted framework path
    good_patch = """--- a/wink-micro-os/frameworks/esp_idf/src/network/esp_mqtt.c
+++ b/wink-micro-os/frameworks/esp_idf/src/network/esp_mqtt.c
@@ -10,3 +10,4 @@
+int valid;
"""
    ok_good, errs_good = checker.validate_patch_scope(good_patch)
    assert ok_good
    assert len(errs_good) == 0


def test_safety_checker_h5_pal_additive_rules():
    checker = HeuristicSafetyChecker()

    # Violation 1: Deleting existing line in PAL header (ABI break)
    abi_break_patch = """--- a/wink-micro-os/pal/include/hal/pal_i2c.h
+++ b/wink-micro-os/pal/include/hal/pal_i2c.h
@@ -20,3 +20,2 @@
-wink_status_t pal_i2c_master_init(uint8_t port);
+wink_status_t pal_i2c_master_init_v2(uint8_t port);
"""
    ok, errs = checker.validate_pal_additive_increment(abi_break_patch)
    assert not ok
    assert any("H-5 ABI Breaking Violation" in e for e in errs)

    # Violation 2: Vendor header in PAL header
    vendor_leak_patch = """--- a/wink-micro-os/pal/include/hal/pal_dac.h
+++ b/wink-micro-os/pal/include/hal/pal_dac.h
@@ -1,5 +1,6 @@
+#include "esp_err.h"
"""
    ok2, errs2 = checker.validate_pal_additive_increment(vendor_leak_patch)
    assert not ok2
    assert any("H-5 Vendor Neutrality Violation" in e for e in errs2)

    # Valid pure addition in PAL header
    valid_pal_patch = """--- /dev/null
+++ b/wink-micro-os/pal/include/hal/pal_dac.h
@@ -0,0 +1,15 @@
+// SPDX-License-Identifier: LGPL-3.0-only
+#ifndef WINK_PAL_DAC_H
+#define WINK_PAL_DAC_H
+#include "pal/pal.h"
+wink_status_t pal_dac_write(uint8_t channel, uint8_t value);
+#endif
"""
    ok3, errs3 = checker.validate_pal_additive_increment(valid_pal_patch)
    assert ok3
    assert len(errs3) == 0


def test_safety_checker_content_rules_h2_h3_h6():
    checker = HeuristicSafetyChecker()

    # H-2: App name branching
    h2_patch = """--- a/wink-micro-os/frameworks/esp_idf/src/network/esp_mqtt.c
+++ b/wink-micro-os/frameworks/esp_idf/src/network/esp_mqtt.c
@@ -20,3 +20,4 @@
+if (strstr(app_name, "mqtt_tcp")) { return ESP_OK; }
"""
    ok2, errs2 = checker.validate_patch_content(h2_patch)
    assert not ok2
    assert any("H-2 Violation" in e for e in errs2)

    # H-3: Canary mutation bypass
    h3_patch = """--- a/wink-micro-os/frameworks/esp_idf/src/network/esp_mqtt.c
+++ b/wink-micro-os/frameworks/esp_idf/src/network/esp_mqtt.c
@@ -20,3 +20,4 @@
+int canary_bypass = 1;
"""
    ok3, errs3 = checker.validate_patch_content(h3_patch)
    assert not ok3
    assert any("H-3 Violation" in e for e in errs3)

    # H-6: Float PWM duty
    h6_patch = """--- a/wink-micro-app/vendor/esp_idfv61/peripherals/ledc/ledc_basic/main/ledc_basic_main.c
+++ b/wink-micro-app/vendor/esp_idfv61/peripherals/ledc/ledc_basic/main/ledc_basic_main.c
@@ -20,3 +20,4 @@
+pal_pwm_set_duty(channel, 0.5f);
"""
    ok6, errs6 = checker.validate_patch_content(h6_patch)
    assert not ok6
    assert any("H-6 Violation" in e for e in errs6)


# -----------------------------------------------------------------------------
# Investigation Workspace & Synthesis Protocol Tests
# -----------------------------------------------------------------------------

def test_investigation_workspace_lifecycle(tmp_path):
    entry_id = "esp.test.mock_app"
    ws = InvestigationWorkspace(tmp_path, entry_id)

    # Initialize
    state = ws.initialize("Simulation failed: Broker connection refused")
    assert state["schema_version"] == 2
    assert state["current_state"] == RemediatorState.INIT.value
    assert ws.raw_failure_file.is_file()

    # State transitions
    s2 = ws.transition_to(RemediatorState.RCA_AUTHORED)
    assert s2["current_state"] == RemediatorState.RCA_AUTHORED
    assert len(s2["history"]) == 2

    # Synthesis protocol: reject identical
    orig_plan = "## Plan\n1. Modify mqtt broker."
    ok, msg = ws.verify_synthesis_not_mere_append(orig_plan, orig_plan)
    assert not ok
    assert "identical" in msg

    # Synthesis protocol: reject mere trailing append
    mere_append = orig_plan + "\n## Extra notes appended"
    ok_append, msg_append = ws.verify_synthesis_not_mere_append(orig_plan, mere_append)
    assert not ok_append
    assert "only appended text" in msg_append

    # Synthesis protocol: accept legitimate reconstruction with Synthesis Log
    valid_reconstruct = """## Plan (Reconstructed)
1. Add stateful MQTT broker simulator with ring buffer.
2. Comply with zero-regression requirements.

## 4. 评审建议融合记录（Synthesis Log）
- Absorbed Reviewer issue #1: Replaced static stub with ring buffer state machine.
"""
    ok_valid, msg_valid = ws.verify_synthesis_not_mere_append(orig_plan, valid_reconstruct)
    assert ok_valid


# -----------------------------------------------------------------------------
# Transactional Git Tracker Tests
# -----------------------------------------------------------------------------

def test_transactional_git_tracker_rollback(tmp_path):
    tracker = TransactionalGitTracker(tmp_path)

    # Create dummy tracked and untracked files
    dummy_tracked = tmp_path / "modified_file.c"
    dummy_tracked.write_text("original content", encoding="utf-8")

    dummy_created = tmp_path / "new_untracked_file.h"
    dummy_created.write_text("new content", encoding="utf-8")

    assert dummy_created.is_file()

    # Dual-track rollback should unlink created files
    tracker.rollback(modified_files=[], created_files=[dummy_created])
    assert not dummy_created.is_file()


# -----------------------------------------------------------------------------
# Zero Regression Domain Mapping Tests
# -----------------------------------------------------------------------------

def test_zero_regression_domain_detection(tmp_path):
    runner = ZeroRegressionRunner(tmp_path, tmp_path / "dummy.ps1")

    # Network domain detection
    p1 = [Path("wink-micro-os/frameworks/esp_idf/src/network/esp_mqtt.c")]
    assert runner.detect_domain(p1) == "network"

    # Peripheral domain detection
    p2 = [Path("wink-micro-os/pal/include/hal/pal_i2c.h")]
    assert runner.detect_domain(p2) == "peripheral"

    # Timer domain detection
    p3 = [Path("wink-micro-os/frameworks/esp_idf/src/drivers/gptimer.c")]
    assert runner.detect_domain(p3) == "timer"


# -----------------------------------------------------------------------------
# Mock Agent Synthesizer & End-to-End Remediator Test
# -----------------------------------------------------------------------------

class MockAgentSynthesizer(AgentSynthesizer):
    """Simulates Role A and Role B offline with deterministic responses."""

    def __init__(self, workspace_root: Path, audit_verdict: str = "FULLY_COMPLETE"):
        super().__init__(workspace_root)
        self.invocations: list[str] = []
        self.audit_verdict = audit_verdict

    def invoke_agent(self, prompt: str, role: str = "A", timeout_sec: int = 300) -> tuple[int, str]:
        self.invocations.append(role)
        if "[TASK] ESP-IDF 官方示例仿真基线失败 - 根因调查" in prompt:
            # Role A RCA & Plan output
            return 0, """
# Root Cause Analysis
Failure caused by missing MQTT broker simulator.

## Remediation Proposal
Implement stateful mock responder.
```diff
--- a/wink-micro-app/vendor/esp_idfv61/protocols/mqtt/mqtt_tcp/wink-app.json
+++ b/wink-micro-app/vendor/esp_idfv61/protocols/mqtt/mqtt_tcp/wink-app.json
@@ -1,3 +1,4 @@
 {
+  "mock_network": true
 }
```
"""
        elif "[ROLE] 独立对抗审查裁判" in prompt:
            # Role B Review output: Approved
            return 0, """---
verdict: APPROVED
he_checks_passed: true
blocking_issues_count: 0
---

## 对抗审查详细评估
### 1. 架构防腐红线审查
- 无空桩或特判倾向.
"""
        elif "[TASK] 自愈实施后完整性自查与 DoD 对照审计" in prompt:
            if self.audit_verdict == "FULLY_COMPLETE":
                return 0, """
VERDICT: FULLY_COMPLETE
All DoD requirements are 100% satisfied. No further changes needed.
"""
            else:
                return 0, """
VERDICT: GAPS_FOUND
Missing mock timeout property in wink-app.json.

```diff
--- a/wink-micro-app/vendor/esp_idfv61/protocols/mqtt/mqtt_tcp/wink-app.json
+++ b/wink-micro-app/vendor/esp_idfv61/protocols/mqtt/mqtt_tcp/wink-app.json
@@ -4,1 +4,2 @@
+  "timeout_ms": 5000
 }
```
"""
        return 0, "OK"


def test_post_exec_audit_prompt_and_parser(tmp_path):
    synthesizer = AgentSynthesizer(tmp_path)
    app_entry = {"id": "esp.peripherals.gpio"}
    plan = "## Plan\nAdd GPIO pullup"
    patch = "--- a/file.c\n+++ b/file.c\n@@ -1 +1 @@\n+int x;"

    prompt = synthesizer.build_post_exec_audit_prompt(app_entry, plan, patch)
    assert "esp.peripherals.gpio" in prompt
    assert "VERDICT: FULLY_COMPLETE" in prompt
    assert "VERDICT: GAPS_FOUND" in prompt

    # Test parser with FULLY_COMPLETE
    v1, d1 = synthesizer.parse_post_exec_audit_verdict("VERDICT: FULLY_COMPLETE\nAll done.")
    assert v1 == "FULLY_COMPLETE"
    assert d1 is None

    # Test parser with GAPS_FOUND and diff block
    sample_gaps = """
VERDICT: GAPS_FOUND
Omitted header include.
```diff
--- a/wink-micro-app/vendor/esp_idfv61/peripherals/gpio/main/gpio_example_main.c
+++ b/wink-micro-app/vendor/esp_idfv61/peripherals/gpio/main/gpio_example_main.c
@@ -10,3 +10,4 @@
+#include "esp_log.h"
```
"""
    v2, d2 = synthesizer.parse_post_exec_audit_verdict(sample_gaps)
    assert v2 == "GAPS_FOUND"
    assert d2 is not None
    assert "+#include \"esp_log.h\"" in d2


def test_remediator_offline_orchestration(tmp_path, monkeypatch):
    mock_agent = MockAgentSynthesizer(tmp_path, audit_verdict="FULLY_COMPLETE")
    remediator = Remediator(tmp_path, mock_agent, max_attempts=2)

    # Monkeypatch git apply and regression to isolate offline execution
    monkeypatch.setattr(remediator.git_tracker, "pre_flight_check", lambda files: (True, "OK"))
    monkeypatch.setattr(remediator.git_tracker, "apply_patch", lambda pfile: (True, "Applied", [], []))
    monkeypatch.setattr(remediator.regression_runner, "run_l1_domain_regression", lambda d: (True, "L1 OK"))
    monkeypatch.setattr(remediator.regression_runner, "run_l2_golden_regression", lambda: (True, "L2 OK"))

    app_entry = {
        "id": "esp.protocols.mqtt",
        "target_app_dir": "protocols/mqtt/mqtt_tcp",
    }
    app_dir = tmp_path / "wink-micro-app" / "vendor" / "esp_idfv61" / "protocols" / "mqtt" / "mqtt_tcp"
    app_dir.mkdir(parents=True, exist_ok=True)

    ok, msg = remediator.remediate_app(
        app_entry=app_entry,
        app_dir=app_dir,
        failure_log="Connection refused to mqtt://broker:1883",
    )

    assert ok
    assert "Autonomous self-healing patch applied" in msg
    assert "A" in mock_agent.invocations
    assert "B" in mock_agent.invocations

    # Verify state history contains POST_EXEC_AUDITING and not PATCH_SUPPLEMENTING
    ws = InvestigationWorkspace(tmp_path, "esp.protocols.mqtt")
    state_data = json.loads(ws.state_file.read_text(encoding="utf-8"))
    history_states = [h["state"] for h in state_data["history"]]
    assert RemediatorState.POST_EXEC_AUDITING.value in history_states
    assert RemediatorState.PATCH_SUPPLEMENTING.value not in history_states


def test_remediator_post_exec_audit_gaps_supplemented(tmp_path, monkeypatch):
    mock_agent = MockAgentSynthesizer(tmp_path, audit_verdict="GAPS_FOUND")
    remediator = Remediator(tmp_path, mock_agent, max_attempts=2)

    monkeypatch.setattr(remediator.git_tracker, "pre_flight_check", lambda files: (True, "OK"))
    monkeypatch.setattr(remediator.git_tracker, "apply_patch", lambda pfile: (True, "Applied", [], []))
    monkeypatch.setattr(remediator.regression_runner, "run_l1_domain_regression", lambda d: (True, "L1 OK"))
    monkeypatch.setattr(remediator.regression_runner, "run_l2_golden_regression", lambda: (True, "L2 OK"))

    app_entry = {
        "id": "esp.protocols.mqtt_gaps",
        "target_app_dir": "protocols/mqtt/mqtt_tcp",
    }
    app_dir = tmp_path / "wink-micro-app" / "vendor" / "esp_idfv61" / "protocols" / "mqtt" / "mqtt_tcp"
    app_dir.mkdir(parents=True, exist_ok=True)

    ok, msg = remediator.remediate_app(
        app_entry=app_entry,
        app_dir=app_dir,
        failure_log="Connection refused",
    )

    assert ok
    assert "Autonomous self-healing patch applied" in msg

    # Verify state history contains both POST_EXEC_AUDITING and PATCH_SUPPLEMENTING
    ws = InvestigationWorkspace(tmp_path, "esp.protocols.mqtt_gaps")
    state_data = json.loads(ws.state_file.read_text(encoding="utf-8"))
    history_states = [h["state"] for h in state_data["history"]]
    assert RemediatorState.POST_EXEC_AUDITING.value in history_states
    assert RemediatorState.PATCH_SUPPLEMENTING.value in history_states

    # Verify supplementary diff was merged into patch.diff
    patch_content = ws.patch_file.read_text(encoding="utf-8")
    assert "timeout_ms" in patch_content

