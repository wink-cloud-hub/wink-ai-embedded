# SPDX-License-Identifier: Apache-2.0
"""
Tests for Domain Admission Service (V1-T1)
==========================================
Verifies that the admission service enforces ADR-0012 contract honesty:
- Accepts valid admission packages with valid samples and recognized ABIs.
- Rejects packages missing required files or invalid JSON.
- Classifies missing ABIs or unmodeled mutation operators as CAPABILITY_GAP.
- Rejects non-differentiating or inverted samples.
"""

import json
import shutil
import tempfile
from pathlib import Path

import pytest

from admission_service import AdmissionService, KNOWN_EXPORTED_ABIS


@pytest.fixture
def temp_dir():
    with tempfile.TemporaryDirectory() as td:
        yield Path(td)


def test_admission_service_create_template_and_verify(temp_dir):
    service = AdmissionService()
    pkg_dir = temp_dir / "pkg"
    service.create_template(pkg_dir, "peripherals/test_gpio")

    verdict = service.verify_package(pkg_dir)
    assert verdict.status == "ADMISSION_ACCEPTED"
    assert verdict.domain_id == "peripherals/test_gpio"
    assert verdict.package_sha256 is not None
    assert (pkg_dir / "admission_manifest.json").is_file()

    manifest = json.loads((pkg_dir / "admission_manifest.json").read_text(encoding="utf-8"))
    assert manifest["status"] == "ADMISSION_ACCEPTED"
    assert manifest["domain_id"] == "peripherals/test_gpio"


def test_admission_service_rejects_missing_directory(temp_dir):
    service = AdmissionService()
    non_existent = temp_dir / "non_existent"
    verdict = service.verify_package(non_existent)
    assert verdict.status == "REJECTED"
    assert "directory_exists" in verdict.missing_prerequisites


def test_admission_service_rejects_missing_files(temp_dir):
    service = AdmissionService()
    pkg_dir = temp_dir / "incomplete_pkg"
    pkg_dir.mkdir(parents=True)
    # Only write domain_spec.json, omit proofplan and samples
    (pkg_dir / "domain_spec.json").write_text("{}", encoding="utf-8")

    verdict = service.verify_package(pkg_dir)
    assert verdict.status == "REJECTED"
    assert "proofplan.json" in verdict.missing_prerequisites


def test_admission_service_detects_capability_gap_unmodeled_abi(temp_dir):
    service = AdmissionService()
    pkg_dir = temp_dir / "pkg_unmodeled_abi"
    service.create_template(pkg_dir, "peripherals/fancy_accelerator")

    # Modify domain_spec.json to require an unmodeled ABI
    spec_path = pkg_dir / "domain_spec.json"
    spec = json.loads(spec_path.read_text(encoding="utf-8"))
    spec["observation_mapping"] = ["esp_nonexistent_quantum_crypto_accelerate"]
    spec_path.write_text(json.dumps(spec, indent=2), encoding="utf-8")

    verdict = service.verify_package(pkg_dir)
    assert verdict.status == "CAPABILITY_GAP"
    assert "abi:esp_nonexistent_quantum_crypto_accelerate" in verdict.missing_prerequisites


def test_admission_service_detects_capability_gap_unsupported_operator(temp_dir):
    service = AdmissionService()
    pkg_dir = temp_dir / "pkg_unsupported_op"
    service.create_template(pkg_dir, "peripherals/timer_unsupported")

    # Modify proofplan.json to reference unsupported operator without custom recipe
    plan_path = pkg_dir / "proofplan.json"
    plan = json.loads(plan_path.read_text(encoding="utf-8"))
    plan["claims"][0]["recipe_ids"] = ["MUT-MAGIC-TELEPATHY"]
    plan_path.write_text(json.dumps(plan, indent=2), encoding="utf-8")

    verdict = service.verify_package(pkg_dir)
    assert verdict.status == "CAPABILITY_GAP"
    assert "operator:MUT-MAGIC-TELEPATHY" in verdict.missing_prerequisites


def test_admission_service_rejects_broken_positive_sample(temp_dir):
    service = AdmissionService()
    pkg_dir = temp_dir / "pkg_bad_pos"
    service.create_template(pkg_dir, "peripherals/bad_pos")

    pos_file = pkg_dir / "samples" / "positive_sample.json"
    pos = json.loads(pos_file.read_text(encoding="utf-8"))
    pos["ok"] = False
    pos["status"] = "failed"
    pos_file.write_text(json.dumps(pos, indent=2), encoding="utf-8")

    verdict = service.verify_package(pkg_dir)
    assert verdict.status == "REJECTED"
    assert "valid_positive_sample" in verdict.missing_prerequisites


def test_admission_service_rejects_non_differentiating_negative_sample(temp_dir):
    service = AdmissionService()
    pkg_dir = temp_dir / "pkg_bad_neg"
    service.create_template(pkg_dir, "peripherals/bad_neg")

    # Make negative sample also passing (non-differentiating)
    neg_file = pkg_dir / "samples" / "negative_sample.json"
    neg = json.loads(neg_file.read_text(encoding="utf-8"))
    neg["ok"] = True
    neg["status"] = "passed"
    neg_file.write_text(json.dumps(neg, indent=2), encoding="utf-8")

    verdict = service.verify_package(pkg_dir)
    assert verdict.status == "REJECTED"
    assert "valid_negative_sample_differentiation" in verdict.missing_prerequisites
