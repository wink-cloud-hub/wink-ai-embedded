# SPDX-License-Identifier: Apache-2.0
"""
Admission Service & Domain Intake Verifier (V1-T1)
==================================================
Validates new domain / capability admission packages before allowing them
to enter the ESP-IDF autonomous governance loop.
Enforces ADR-0012 contract honesty: missing ABIs or unmodeled operators
are explicitly rejected as CAPABILITY_GAP rather than false green.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

VENDOR_ROOT = Path(__file__).resolve().parents[3]

try:
    from loop.mutation_catalog import CATALOG_OPERATORS
except ImportError:
    CATALOG_OPERATORS = {}

# Known exported or simulated ABIs for Wasm / Host simulation
KNOWN_EXPORTED_ABIS = {
    # Core & OS
    "esp_restart", "esp_get_free_heap_size", "esp_timer_get_time",
    # GPIO
    "gpio_set_level", "gpio_get_level", "gpio_config", "gpio_reset_pin",
    # UART
    "uart_write_bytes", "uart_read_bytes", "uart_driver_install", "uart_param_config",
    # GPTimer & HW Timer
    "gptimer_new_timer", "gptimer_set_raw_count", "gptimer_get_raw_count",
    "gptimer_start", "gptimer_stop", "gptimer_set_alarm_action",
    # ADC
    "adc_oneshot_read", "adc_oneshot_new_unit", "adc_oneshot_config_channel",
    # DAC
    "dac_cosine_new_channel", "dac_cosine_start", "dac_cosine_stop", "dac_oneshot_output_voltage",
    # SPI
    "spi_bus_initialize", "spi_bus_add_device", "spi_device_transmit", "spi_device_polling_transmit",
    # I2C
    "i2c_param_config", "i2c_driver_install", "i2c_master_write_read_device",
    # Storage & VFS
    "esp_vfs_spiffs_register", "esp_spiffs_info", "esp_vfs_ram_get_used_bytes",
    # Power & Sleep
    "esp_deep_sleep_start", "esp_light_sleep_start", "esp_sleep_enable_timer_wakeup",
    # LEDC
    "ledc_channel_config", "ledc_timer_config", "ledc_set_duty", "ledc_update_duty",
    # Watchdog
    "esp_task_wdt_init", "esp_task_wdt_add", "esp_task_wdt_reset",
}


@dataclass
class AdmissionVerdict:
    status: str  # ADMISSION_ACCEPTED | CAPABILITY_GAP | REJECTED
    domain_id: str
    message: str
    missing_prerequisites: List[str] = field(default_factory=list)
    package_sha256: Optional[str] = None
    manifest_path: Optional[str] = None

    def to_dict(self) -> Dict[str, Any]:
        return {
            "status": self.status,
            "domain_id": self.domain_id,
            "message": self.message,
            "missing_prerequisites": self.missing_prerequisites,
            "package_sha256": self.package_sha256,
            "manifest_path": self.manifest_path,
        }


def compute_directory_sha256(dir_path: Path) -> str:
    """Compute deterministic SHA-256 digest of all files in directory."""
    hasher = hashlib.sha256()
    for p in sorted(dir_path.rglob("*")):
        if p.is_file() and not p.name.endswith(".tmp") and p.name != "admission_manifest.json":
            rel_posix = p.relative_to(dir_path).as_posix()
            file_hash = hashlib.sha256(p.read_bytes()).hexdigest()
            hasher.update(f"{rel_posix}:{file_hash}\n".encode("utf-8"))
    return hasher.hexdigest()


class AdmissionService:
    """Validates domain intake packages for checklist additions."""

    def __init__(self, vendor_root: Optional[Path] = None):
        self.vendor_root = vendor_root or VENDOR_ROOT

    def verify_package(self, package_dir: Path) -> AdmissionVerdict:
        if not package_dir.is_dir():
            return AdmissionVerdict(
                status="REJECTED",
                domain_id="unknown",
                message=f"Package directory does not exist: {package_dir}",
                missing_prerequisites=["directory_exists"],
            )

        spec_file = package_dir / "domain_spec.json"
        proofplan_file = package_dir / "proofplan.json"
        samples_dir = package_dir / "samples"
        pos_sample_file = samples_dir / "positive_sample.json"
        neg_sample_file = samples_dir / "negative_sample.json"

        # 1. Check required structural files
        missing_files = []
        for req_f in [spec_file, proofplan_file, pos_sample_file, neg_sample_file]:
            if not req_f.is_file():
                missing_files.append(str(req_f.name))

        if missing_files:
            return AdmissionVerdict(
                status="REJECTED",
                domain_id="unknown",
                message=f"Missing required admission files: {', '.join(missing_files)}",
                missing_prerequisites=missing_files,
            )

        # 2. Parse and validate domain_spec.json
        try:
            domain_spec = json.loads(spec_file.read_text(encoding="utf-8"))
        except Exception as exc:
            return AdmissionVerdict(status="REJECTED", domain_id="unknown", message=f"domain_spec.json invalid JSON: {exc}")

        domain_id = domain_spec.get("domain_id", "").strip()
        if not domain_id:
            return AdmissionVerdict(status="REJECTED", domain_id="unknown", message="domain_spec.json missing domain_id")

        source_contract = domain_spec.get("source_contract", {})
        if not source_contract.get("upstream_repo") or not source_contract.get("upstream_commit"):
            return AdmissionVerdict(
                status="REJECTED",
                domain_id=domain_id,
                message="source_contract must specify upstream_repo and upstream_commit",
                missing_prerequisites=["source_contract_integrity"],
            )

        # 3. Parse and validate proofplan.json
        try:
            proofplan = json.loads(proofplan_file.read_text(encoding="utf-8"))
        except Exception as exc:
            return AdmissionVerdict(status="REJECTED", domain_id=domain_id, message=f"proofplan.json invalid JSON: {exc}")

        claims = proofplan.get("claims", [])
        if not claims:
            return AdmissionVerdict(status="REJECTED", domain_id=domain_id, message="proofplan.json has no claims defined")

        # 4. Check observation mapping (ABI exports)
        obs_mapping = domain_spec.get("observation_mapping", [])
        missing_abis = []
        for abi in obs_mapping:
            if abi not in KNOWN_EXPORTED_ABIS:
                missing_abis.append(abi)

        if missing_abis:
            return AdmissionVerdict(
                status="CAPABILITY_GAP",
                domain_id=domain_id,
                message=f"Declared observation mapping requires unmodeled ABIs: {', '.join(missing_abis)}",
                missing_prerequisites=[f"abi:{a}" for a in missing_abis],
            )

        # 5. Check mutation operator availability
        missing_operators = []
        for claim in claims:
            recipe_ids = claim.get("recipe_ids", [])
            for r_id in recipe_ids:
                if r_id not in CATALOG_OPERATORS:
                    # Check if custom recipe is defined in proofplan
                    custom_recipes = proofplan.get("custom_recipes", {})
                    if r_id not in custom_recipes:
                        missing_operators.append(r_id)

        if missing_operators:
            return AdmissionVerdict(
                status="CAPABILITY_GAP",
                domain_id=domain_id,
                message=f"ProofPlan references unsupported mutation operators: {', '.join(missing_operators)}",
                missing_prerequisites=[f"operator:{op}" for op in missing_operators],
            )

        # 6. Validate samples (differentiation and report contract)
        try:
            pos_sample = json.loads(pos_sample_file.read_text(encoding="utf-8"))
            neg_sample = json.loads(neg_sample_file.read_text(encoding="utf-8"))
        except Exception as exc:
            return AdmissionVerdict(status="REJECTED", domain_id=domain_id, message=f"Sample file invalid JSON: {exc}")

        # Check positive sample: must be passing
        if not pos_sample.get("ok", False) or pos_sample.get("status") not in ("passed", "PASS"):
            return AdmissionVerdict(
                status="REJECTED",
                domain_id=domain_id,
                message="positive_sample must indicate passed status",
                missing_prerequisites=["valid_positive_sample"],
            )

        # Check negative sample: must be failing
        if neg_sample.get("ok", True) and neg_sample.get("status") in ("passed", "PASS"):
            return AdmissionVerdict(
                status="REJECTED",
                domain_id=domain_id,
                message="negative_sample must indicate failed / killed status for defect differentiation",
                missing_prerequisites=["valid_negative_sample_differentiation"],
            )

        # All checks passed: seal package
        pkg_sha256 = compute_directory_sha256(package_dir)
        manifest_data = {
            "schema_version": "1.0",
            "domain_id": domain_id,
            "version": domain_spec.get("version", "1.0.0"),
            "status": "ADMISSION_ACCEPTED",
            "package_sha256": pkg_sha256,
            "claims_count": len(claims),
            "observed_abis": obs_mapping,
            "support_matrix": domain_spec.get("support_matrix", []),
        }

        manifest_path = package_dir / "admission_manifest.json"
        manifest_path.write_text(json.dumps(manifest_data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

        return AdmissionVerdict(
            status="ADMISSION_ACCEPTED",
            domain_id=domain_id,
            message="Domain admission package verified and sealed successfully",
            package_sha256=pkg_sha256,
            manifest_path=str(manifest_path),
        )

    def create_template(self, output_dir: Path, domain_id: str) -> Path:
        output_dir.mkdir(parents=True, exist_ok=True)
        samples_dir = output_dir / "samples"
        samples_dir.mkdir(parents=True, exist_ok=True)

        spec = {
            "domain_id": domain_id,
            "version": "1.0.0",
            "source_contract": {
                "upstream_repo": "espressif/esp-idf",
                "upstream_commit": "63439fc0489eac61b9914d64564557f79942ec04",
                "relative_path": f"examples/{domain_id}",
            },
            "config_dependencies": ["CONFIG_IDF_TARGET_ESP32"],
            "observation_mapping": ["gpio_set_level", "gpio_get_level"],
            "support_matrix": ["wasm_sim_standard", "esp32"],
            "reset_persistence_contract": {
                "state_reset": "full_clean",
                "persistent_storage": False,
            },
        }
        (output_dir / "domain_spec.json").write_text(json.dumps(spec, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

        proofplan = {
            "domain_id": domain_id,
            "claims": [
                {
                    "claim_id": f"{domain_id}.basic_operation",
                    "description": "Basic operation verified with baseline, canary, and recovery",
                    "required_checks": ["baseline", "assertion_self_check", "recovery"],
                    "recipe_ids": ["MUT-GPIO-INV"],
                }
            ],
            "custom_recipes": {},
        }
        (output_dir / "proofplan.json").write_text(json.dumps(proofplan, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

        pos_sample = {
            "total": 1,
            "passed": 1,
            "failed": 0,
            "ok": True,
            "status": "passed",
            "results": [
                {
                    "header": "Baseline execution",
                    "ok": True,
                    "status": "passed",
                    "summary": "1/1 step passed",
                    "stepResults": [
                        {"index": 0, "status": "passed", "matcher": "ASSERT_EQ", "expected": 1, "actual": 1}
                    ],
                }
            ],
        }
        (samples_dir / "positive_sample.json").write_text(json.dumps(pos_sample, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

        neg_sample = {
            "total": 1,
            "passed": 0,
            "failed": 1,
            "ok": False,
            "status": "failed",
            "results": [
                {
                    "header": "Canary assertion execution",
                    "ok": False,
                    "status": "failed",
                    "summary": "0/1 step passed",
                    "stepResults": [
                        {"index": 0, "status": "failed", "matcher": "ASSERT_EQ", "expected": 1, "actual": 0}
                    ],
                }
            ],
        }
        (samples_dir / "negative_sample.json").write_text(json.dumps(neg_sample, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

        return output_dir


def main() -> int:
    parser = argparse.ArgumentParser(description="WinkMicroOS ESP-IDF Domain Admission Service (V1-T1)")
    parser.add_argument("--verify", type=str, help="Verify an admission package directory")
    parser.add_argument("--create-template", type=str, help="Generate an admission package template in directory")
    parser.add_argument("--domain", type=str, default="peripherals/sample", help="Domain ID for template")
    args = parser.parse_args()

    service = AdmissionService()

    if args.create_template:
        out_dir = Path(args.create_template).resolve()
        service.create_template(out_dir, args.domain)
        print(f"[admission] Created admission package template at: {out_dir}")
        return 0

    if args.verify:
        pkg_dir = Path(args.verify).resolve()
        verdict = service.verify_package(pkg_dir)
        print("=" * 60)
        print(f"  ADMISSION VERIFICATION RESULT: {verdict.status}")
        print("=" * 60)
        print(f"Domain ID:  {verdict.domain_id}")
        print(f"Message:    {verdict.message}")
        if verdict.missing_prerequisites:
            print(f"Missing:    {', '.join(verdict.missing_prerequisites)}")
        if verdict.package_sha256:
            print(f"SHA-256:    {verdict.package_sha256}")
            print(f"Manifest:   {verdict.manifest_path}")

        if verdict.status == "ADMISSION_ACCEPTED":
            return 0
        elif verdict.status == "CAPABILITY_GAP":
            return 2
        else:
            return 1

    parser.print_help()
    return 1


if __name__ == "__main__":
    sys.exit(main())
