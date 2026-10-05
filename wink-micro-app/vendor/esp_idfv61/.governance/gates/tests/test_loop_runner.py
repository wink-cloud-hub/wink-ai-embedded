# SPDX-License-Identifier: Apache-2.0
"""
Unit tests for Autonomous Governance Loop Engine (tools/loop)
"""
import copy
import json
from pathlib import Path
import pytest

import sys
TOOLS_DIR = Path(__file__).resolve().parent.parent.parent / "tools"
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

from loop.mutator import CanaryMutator
from loop.runner import LoopRunner
from loop.agent import AgentSynthesizer


@pytest.fixture
def sample_scenario():
    return {
        "header": {
            "name": "sample test scenario",
            "templateId": "sample",
            "timeoutUs": "1000000",
        },
        "steps": [
            {
                "type": "ASSERT_POINT",
                "timeUs": "100ms",
                "target": "power:VCC_3V3",
                "matcher": 3.3,
                "description": "Aux power rail check",
            },
            {
                "type": "ASSERT_POINT",
                "timeUs": "500ms",
                "target": "timer:0/counter",
                "matcher": {"$between": [400000, 600000]},
                "description": "Core domain assertion",
            },
        ],
    }


def test_canary_mutator_target_identification(sample_scenario):
    mutator = CanaryMutator()
    idx = mutator.identify_target_step(sample_scenario["steps"])
    assert idx == 1  # Should pick domain timer:0/counter, not power:VCC_3V3


def test_canary_mutator_between_shift():
    mutator = CanaryMutator()
    orig = {"$between": [1000, 2000]}
    mutated, desc = mutator.mutate_matcher(orig)
    assert mutated["$between"] == [1002000, 2002000]
    assert "shifted" in desc


def test_canary_mutator_scalar_mutations():
    mutator = CanaryMutator()
    m200, _ = mutator.mutate_matcher(200)
    assert m200 == 404

    m0, _ = mutator.mutate_matcher(0)
    assert m0 == 1

    m1, _ = mutator.mutate_matcher(1)
    assert m1 == 0


def test_canary_mutator_detects_false_green():
    mutator = CanaryMutator()
    meta = {"step_index": 1, "target": "timer:0/counter", "mutation_desc": "shifted"}
    # If exit_code is 0, mutant survived -> False Green!
    killed, msg = mutator.verify_kill(0, "All passed", meta)
    assert not killed
    assert "FALSE GREEN DETECTED" in msg


def test_canary_mutator_confirms_kill():
    mutator = CanaryMutator()
    meta = {"step_index": 1, "target": "timer:0/counter", "mutation_desc": "shifted"}
    killed, msg = mutator.verify_kill(1, "Step #2 FAILED: outside range", meta)
    assert killed
    assert "successfully killed" in msg


def test_canary_mutator_rejects_infra_crash():
    mutator = CanaryMutator()
    meta = {"step_index": 1, "target": "timer:0/counter", "mutation_desc": "shifted"}
    # Build failure must be rejected, not counted as kill
    killed, msg = mutator.verify_kill(2, "Failed to build wasm assets", meta)
    assert not killed
    assert "INFRA_CRASH" in msg

    # Runtime load failure must also be rejected
    killed, msg = mutator.verify_kill(2, "Failed to load runtime module", meta)
    assert not killed
    assert "INFRA_CRASH" in msg


def test_canary_mutator_3d_matrix(tmp_path):
    mutator = CanaryMutator()
    sc_data = {
        "steps": [
            {
                "type": "INJECT_NET_FIXTURE",
                "routes": [{"url_prefix": "http://example.com", "status_code": 200, "body": "ok"}],
            },
            {
                "type": "ASSERT_POINT",
                "target": "http:client:status_code",
                "matcher": 200,
            }
        ]
    }
    sc_file = tmp_path / "test.scenario.json"
    sc_file.write_text(json.dumps(sc_data), encoding="utf-8")

    # 1. Dimension A: Assertion Mutant
    path_a, meta_a = mutator.create_mutant_file(sc_file, dimension="assertion")
    assert path_a.is_file()
    assert meta_a["dimension"] == "assertion"
    assert json.loads(path_a.read_text())["steps"][1]["matcher"] == 404

    # 2. Dimension B: Stimulus Mutant
    path_b, meta_b = mutator.create_mutant_file(sc_file, dimension="stimulus")
    assert path_b.is_file()
    assert meta_b["dimension"] == "stimulus"
    assert json.loads(path_b.read_text())["steps"][0]["routes"][0]["status_code"] == 500

    # 3. Dimension C: Platform Fault Mutant
    path_c, meta_c = mutator.create_mutant_file(sc_file, dimension="platform_fault", fault_domain="i2c", fault_type="nack")
    assert path_c.is_file()
    assert meta_c["dimension"] == "platform_fault"
    mut_c_data = json.loads(path_c.read_text())
    assert mut_c_data["steps"][0]["type"] == "INJECT_PLATFORM_FAULT"
    assert mut_c_data["steps"][0]["domain"] == "i2c"
    assert mut_c_data["steps"][0]["fault"] == "nack"


def test_runner_candidate_selection():
    ws_root = Path(__file__).resolve().parents[6]
    runner = LoopRunner(workspace_root=ws_root, dry_run=True)
    candidates = runner.select_candidates()
    # Verified entries like blink_gpio or gptimer_alarm should be excluded
    candidate_ids = [c["id"] for c in candidates]
    assert "esp.get_started.blink" not in candidate_ids
    assert "esp.peripherals.timer_group.gptimer" not in candidate_ids

    # When explicit app is requested, it can be targeted even if verified
    explicit_candidates = runner.select_candidates(app_name="blink_gpio")
    assert len(explicit_candidates) == 1
    assert explicit_candidates[0]["id"] == "esp.get_started.blink"


def test_agent_synthesizer_prompt_contains_rules():
    ws_root = Path(__file__).resolve().parents[6]
    agent = AgentSynthesizer(workspace_root=ws_root)
    fake_entry = {"id": "esp.test.app", "target_app_dir": "test/app"}
    prompt = agent.build_prompt(fake_entry, ws_root)
    assert "governance-sop-esp/SKILL.md" in prompt
    assert "power:*" in prompt
    assert "反模式红线禁令" in prompt


def test_agent_synthesizer_qoder_model_configuration():
    ws_root = Path(__file__).resolve().parents[6]
    # Default model should be Qwen3.8-Flash
    agent_default = AgentSynthesizer(workspace_root=ws_root)
    assert agent_default.qoder_model == "Qwen3.8-Flash"

    # Custom model override
    agent_custom = AgentSynthesizer(workspace_root=ws_root, qoder_model="Qwen3.8-Max")
    assert agent_custom.qoder_model == "Qwen3.8-Max"

    cmd = agent_default.detect_agent_executable(role="A")
    if cmd and "qoderclicn" in cmd[0]:
        assert "-m" in cmd
        assert "Qwen3.8-Flash" in cmd

