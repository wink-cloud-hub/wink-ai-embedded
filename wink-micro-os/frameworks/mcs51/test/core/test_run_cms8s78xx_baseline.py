#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Negative-first tests for the CMS8S78xx baseline batch driver.

Nothing here invokes the real CLI or toolchain: the driver's own decision functions are
what must fail loudly (missing assets, divergent hashes, a clean request outside the
expected build tree, an invalid explicit entry). A driver that only ever returns PASS
would re-create the false-green this plan exists to remove.
"""
from __future__ import annotations

import json
import os
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))

import run_cms8s78xx_baseline as rb  # noqa: E402


class TmpDirCase(unittest.TestCase):
    def setUp(self):
        import tempfile
        self.tmp = Path(tempfile.mkdtemp(prefix="cms8s-baseline-test-"))
        self.addCleanup(lambda: __import__("shutil").rmtree(self.tmp, ignore_errors=True))

    def touch(self, rel: str, data: bytes = b"x") -> Path:
        handle = self.tmp / rel
        handle.parent.mkdir(parents=True, exist_ok=True)
        handle.write_bytes(data)
        return handle


class AssetIdentityTests(TmpDirCase):
    def test_missing_assets_are_recorded_not_complete(self):
        ident = rb.asset_identity(self.tmp / "empty")
        self.assertFalse(ident["complete"])
        for name in rb.ASSET_NAMES:
            self.assertTrue(ident[name]["missing"])
            self.assertIsNone(ident[name]["sha256"])

    def test_present_assets_carry_raw_hash_and_size(self):
        directory = self.tmp / "assets"
        directory.mkdir(parents=True)
        for name in rb.ASSET_NAMES:
            (directory / name).write_bytes(b"abc" * (len(name) % 5 + 1))
        ident = rb.asset_identity(directory)
        self.assertTrue(ident["complete"])
        self.assertEqual(len(ident["wink_simulator.wasm"]["sha256"]), 64)
        self.assertGreater(ident["device-tree.json"]["size_bytes"], 0)


class ReproducibilityTests(TmpDirCase):
    def _round(self, payload: bytes, status="PASS"):
        directory = self.tmp / str(id(payload))
        directory.mkdir(parents=True, exist_ok=True)
        for name in rb.ASSET_NAMES:
            (directory / name).write_bytes(payload)
        return {"status": status, "round": "x", "assets_dir": str(directory),
                "assets": rb.asset_identity(directory)}

    def test_identical_bytes_pass_and_count_222_worth_of_identities(self):
        a, b = self._round(b"same"), self._round(b"same")
        report = rb.compare_rounds(a, b)
        self.assertEqual(report["status"], "PASS")
        self.assertEqual(report["asset_count"], 6)

    def test_one_divergent_asset_fails(self):
        a, b = self._round(b"same"), self._round(b"different")
        report = rb.compare_rounds(a, b)
        self.assertEqual(report["status"], "FAIL")
        self.assertFalse(report["assets"]["wink_simulator.wasm"]["identical"])

    def test_failed_round_blocks_instead_of_passing(self):
        a, b = self._round(b"same", status="FAIL"), self._round(b"same")
        self.assertEqual(rb.compare_rounds(a, b)["status"], "BLOCKED")


class ExecutionBindingTests(TmpDirCase):
    def setUp(self):
        super().setUp()
        directory = self.tmp / "sel"
        directory.mkdir()
        for name in rb.ASSET_NAMES:
            (directory / name).write_bytes(b"selected")
        self.selected = rb.asset_identity(directory)

    def test_matching_bytes_are_bound(self):
        self.assertEqual(rb.execution_identity_status(self.selected, dict(self.selected)), "bound")

    def test_replaced_object_is_identity_mismatch(self):
        after = {k: dict(v) for k, v in self.selected.items() if k in rb.ASSET_NAMES}
        after["wink_simulator.wasm"] = {"sha256": "f" * 64, "size_bytes": 1}
        self.assertEqual(rb.execution_identity_status(self.selected, after), "identity_mismatch")

    def test_absent_object_is_missing_asset(self):
        after = {k: {"sha256": None, "size_bytes": None, "missing": True} for k in rb.ASSET_NAMES}
        self.assertEqual(rb.execution_identity_status(self.selected, after), "missing_asset")

    def test_status_never_claimed_verified_without_load_report(self):
        # The driver must keep load evidence unverified; a byte match is not a load record.
        source = Path(rb.__file__).read_text(encoding="utf-8")
        self.assertIn('load_evidence_status"] = "unverified', source)


class CleanTargetTests(TmpDirCase):
    def test_leaf_inside_expected_tree_is_accepted(self):
        leaf = self.tmp / "build" / "wasm" / "vendor/cms8s78xx/gpio"
        leaf.mkdir(parents=True)
        rb.assert_clean_target(leaf, self.tmp / "build")

    def test_leaf_outside_expected_tree_is_rejected(self):
        outside = self.tmp.parent
        with self.assertRaises(SystemExit) as ctx:
            rb.assert_clean_target(outside, self.tmp / "build")
        self.assertIn("clean_target_error", str(ctx.exception))


class EntryResolutionTests(TmpDirCase):
    def test_explicit_launcher_is_used(self):
        fake = self.tmp / ("winkcli.exe" if os.name == "nt" else "winkcli")
        fake.write_text("# fake\n", encoding="utf-8")
        entry = rb.resolve_entry(str(fake))
        self.assertEqual(entry["mode"], "installed_cli")
        self.assertEqual(entry["reason"], "explicit launcher override")

    def test_invalid_explicit_launcher_never_falls_back_silently(self):
        with self.assertRaises(SystemExit) as ctx:
            rb.resolve_entry(str(self.tmp / "nope"))
        self.assertIn("entry_error", str(ctx.exception))

    def test_installed_entry_from_path(self):
        monkey_path = self.tmp / "bin"
        monkey_path.mkdir()
        fake = monkey_path / ("winkcli.exe" if os.name == "nt" else "winkcli")
        fake.write_text("# fake\n", encoding="utf-8")
        fake.chmod(0o755)
        original_which = rb.shutil.which
        rb.shutil.which = lambda name: str(fake) if name == "winkcli" else None
        try:
            entry = rb.resolve_entry(None)
        finally:
            rb.shutil.which = original_which
        self.assertEqual(entry["mode"], "installed_cli")
        self.assertEqual(entry["env"], {})

    def test_no_entry_at_all_is_an_error(self):
        original_which = rb.shutil.which
        rb.shutil.which = lambda name: None
        try:
            with self.assertRaises(SystemExit) as ctx:
                rb.resolve_entry(None)
        finally:
            rb.shutil.which = original_which
        self.assertIn("entry_error", str(ctx.exception))


class IsolationStageTests(TmpDirCase):
    def _fake_embedded(self) -> Path:
        embedded = self.tmp / "embedded"
        (embedded / "wink-micro-os" / "frameworks").mkdir(parents=True)
        (embedded / "wink-micro-os" / "build" / "stale").mkdir(parents=True)
        (embedded / "wink-micro-os" / "build" / "stale" / "junk.obj").write_bytes(b"old")
        board = embedded / "wink-tools" / "tools" / "codegen" / "boards" / "mcs51"
        board.mkdir(parents=True)
        (board / "stc89c52_devboard.json").write_text("{}\n", encoding="utf-8")
        (embedded / "wink-micro-app" / "common").mkdir(parents=True)
        (embedded / "wink-micro-app" / "common" / "shared.h").write_bytes(b"// shared\n")
        app = embedded / "wink-micro-app" / "vendor" / "cms8s78xx" / "gpio"
        app.mkdir(parents=True)
        (app / "main.c").write_bytes(b"void main(){}")
        (app / "wink-app.json").write_text(json.dumps(
            {"mcu": "cms8s78xx", "upstream": {"source_dir": "docs/vendors/x/code"}}), encoding="utf-8")
        (app / "unisim-assets").mkdir()
        (app / "unisim-assets" / "wink_simulator.wasm").write_bytes(b"stale asset")
        return embedded

    def test_copy_binds_own_sdk_and_excludes_stale_state(self):
        embedded = self._fake_embedded()
        app = embedded / "wink-micro-app" / "vendor" / "cms8s78xx" / "gpio"
        stage = rb.stage_isolated_copy(embedded, app, self.tmp / "iso" / "A")
        self.assertTrue((Path(stage["isolated_sdk_root"]) / "frameworks").is_dir())
        self.assertFalse((Path(stage["isolated_sdk_root"]) / "build").exists())
        self.assertTrue((Path(stage["isolated_app_dir"]) / "main.c").is_file())
        self.assertFalse((Path(stage["isolated_app_dir"]) / "unisim-assets").exists())

    def test_copy_stages_sibling_roots_that_cmake_resolves_via_parent(self):
        # A missing ../wink-tools in the isolated tree silently degrades the board-config
        # step to a nonexistent fallback JSON, so make fails on a phantom dependency.
        embedded = self._fake_embedded()
        app = embedded / "wink-micro-app" / "vendor" / "cms8s78xx" / "gpio"
        stage = rb.stage_isolated_copy(embedded, app, self.tmp / "iso" / "B")
        iso = Path(stage["isolated_embedded_root"])
        self.assertTrue((iso / "wink-tools" / "tools" / "codegen" / "boards" / "mcs51"
                         / "stc89c52_devboard.json").is_file())
        self.assertTrue((iso / "wink-micro-app" / "common" / "shared.h").is_file())
        self.assertEqual(sorted(set(stage["staged_roots"])),
                         ["wink-micro-app/common", "wink-micro-os", "wink-tools"])

    def test_compile_root_prediction_lands_in_isolated_tree(self):
        embedded = self._fake_embedded()
        app = embedded / "wink-micro-app" / "vendor" / "cms8s78xx" / "gpio"
        iso = self.tmp / "iso" / "A"
        leaf = rb.compile_root_for(iso, app.relative_to(embedded))
        self.assertEqual(leaf.as_posix().split("/")[-4:], ["wasm", "vendor", "cms8s78xx", "gpio"])
        self.assertTrue(str(leaf).replace("\\", "/").startswith(iso.as_posix().replace("\\", "/")))


class AppEnumerationTests(TmpDirCase):
    def test_app_without_upstream_declaration_is_rejected(self):
        vendor = self.tmp / "vendor"
        good = vendor / "gpio"
        good.mkdir(parents=True)
        (good / "wink-app.json").write_text(json.dumps(
            {"upstream": {"source_dir": "docs/vendors/x"}}), encoding="utf-8")
        bad = vendor / "orphan"
        bad.mkdir()
        (bad / "wink-app.json").write_text(json.dumps({"mcu": "cms8s78xx"}), encoding="utf-8")
        with self.assertRaises(SystemExit) as ctx:
            rb.enumerate_apps(vendor)
        self.assertIn("manifest_error", str(ctx.exception))

    def test_only_app_directories_are_enumerated(self):
        vendor = self.tmp / "vendor"
        for name in ("acmp0", "gpio"):
            handle = vendor / name
            handle.mkdir(parents=True)
            (handle / "wink-app.json").write_text(json.dumps(
                {"upstream": {"source_dir": "docs/vendors/x"}}), encoding="utf-8")
        (vendor / "README.md").write_text("# note\n", encoding="utf-8")
        self.assertEqual([p.name for p in rb.enumerate_apps(vendor)], ["acmp0", "gpio"])


class ProcessRunnerTests(TmpDirCase):
    def test_exit_code_and_log_are_recorded(self):
        log = self.tmp / "out.log"
        record, _, _ = rb.run_proc([sys.executable, "-c", "print('hello')"], log_path=log)
        self.assertEqual(record["exit_code"], 0)
        self.assertEqual(record["status"], "PASS")
        self.assertIn("hello", record["stdout_tail"])
        self.assertIn("hello", log.read_text(encoding="utf-8"))

    def test_nonzero_exit_is_fail_not_silently_ok(self):
        record, _, _ = rb.run_proc([sys.executable, "-c", "raise SystemExit(3)"])
        self.assertEqual(record["exit_code"], 3)
        self.assertEqual(record["status"], "FAIL")

    def test_timeout_is_its_own_status(self):
        script = "import time; time.sleep(30)"
        record, _, _ = rb.run_proc([sys.executable, "-c", script], timeout=1)
        self.assertEqual(record["status"], "TIMEOUT")
        self.assertIsNone(record["exit_code"])


class PathLeakageTests(TmpDirCase):
    def test_backslash_and_forward_forms_are_both_detected(self):
        directory = self.tmp / "assets"
        directory.mkdir()
        marker = str(self.tmp / "iso" / "round")
        for name in rb.ASSET_NAMES:
            (directory / name).write_bytes(b"harmless\n")
        (directory / "wink_simulator.js").write_bytes(marker.replace("/", "\\").encode())
        (directory / "device-tree.json").write_bytes(marker.encode())
        hits = rb.path_leakage(directory, marker)
        self.assertEqual(sorted(hits), ["device-tree.json", "wink_simulator.js"])

    def test_mapped_prefix_is_not_a_leak(self):
        directory = self.tmp / "mapped"
        directory.mkdir()
        for name in rb.ASSET_NAMES:
            (directory / name).write_bytes(b"/wink-baseline/wink-micro-os/main.c\x00")
        self.assertEqual(rb.path_leakage(directory, str(self.tmp / "iso" / "A")), {})


class ReportPairingTests(TmpDirCase):
    def _scenario(self, name: str, header_name: str, steps: int = 3) -> None:
        payload = {"header": {"name": header_name, "templateId": "vendor_cms8s78xx_" + name},
                   "steps": [{"type": "ASSERT_POINT"} for _ in range(steps)]}
        self.touch(f"scen/{name}.scenario.json",
                   json.dumps(payload).encode("utf-8"))

    def _report(self, rows) -> Path:
        payload = {"timestamp": "t", "total": len(rows), "passed": 0, "failed": 0, "results": rows}
        handle = self.tmp / "scen-run-report.json"
        handle.write_text(json.dumps(payload), encoding="utf-8")
        return handle

    def _row(self, header_name: str, ok=True, steps=3):
        return {"ok": ok, "status": "passed" if ok else "failed",
                "header": {"name": header_name, "templateId": "x", "totalVirtualUs": "2500000"},
                "summary": {"totalSteps": steps, "passedSteps": steps, "failedSteps": 0,
                            "skippedSteps": 0, "errorSteps": 0},
                "stepResults": [{"stepIndex": i, "type": "ASSERT_POINT", "status": "passed"}
                                for i in range(steps)],
                "diagnostics": []}

    def _files(self):
        return rb.scenario_files(self.tmp / "scen")

    def test_every_file_and_row_paired_is_pass(self):
        self._scenario("gpio", "gpio proof")
        self._scenario("led", "led proof")
        report = self._report([self._row("gpio proof"), self._row("led proof")])
        result = rb.match_reports(report, self._files())
        self.assertEqual(result["status"], "PASS")
        self.assertEqual([s["scenario_file"] for s in result["scenarios"]],
                         ["gpio.scenario.json", "led.scenario.json"])
        self.assertEqual(len(result["scenarios"][0]["scenario_sha256"]), 64)

    def test_report_without_a_row_for_a_file_is_a_failure(self):
        self._scenario("gpio", "gpio proof")
        self._scenario("led", "led proof")
        result = rb.match_reports(self._report([self._row("gpio proof")]), self._files())
        self.assertEqual(result["status"], "FAIL")
        self.assertEqual(result["unexecuted_files"], ["led.scenario.json"])

    def test_row_without_a_matching_file_is_a_failure(self):
        self._scenario("gpio", "gpio proof")
        result = rb.match_reports(self._report([self._row("someone else")]), self._files())
        self.assertEqual(result["status"], "FAIL")
        self.assertEqual(result["unmatched_rows"], [{"header_name": "someone else",
                                                    "candidates": 0}])

    def test_zero_step_execution_is_not_a_pass(self):
        self._scenario("gpio", "gpio proof")
        result = rb.match_reports(self._report([self._row("gpio proof", steps=0)]), self._files())
        self.assertEqual(result["status"], "FAIL")
        self.assertEqual(result["vacuous_files"], ["gpio.scenario.json"])

    def test_failing_row_is_a_failure(self):
        self._scenario("gpio", "gpio proof")
        result = rb.match_reports(self._report([self._row("gpio proof", ok=False)]), self._files())
        self.assertEqual(result["status"], "FAIL")

    def test_absent_report_is_recorded_as_missing(self):
        self._scenario("gpio", "gpio proof")
        result = rb.match_reports(self.tmp / "nope.json", self._files())
        self.assertEqual(result["status"], "FAIL")
        self.assertEqual(result["failure"], "missing_report")

    def test_scenario_files_carry_declared_step_counts(self):
        self._scenario("gpio", "gpio proof", steps=7)
        record = self._files()[0]
        self.assertEqual(record["declared_steps"], 7)
        self.assertEqual(record["template_id"], "vendor_cms8s78xx_gpio")


class ScenarioOutcomeTests(TmpDirCase):
    def test_bound_run_with_reports_passes(self):
        self.assertEqual(rb.scenario_outcome("PASS", "bound", 2), ("PASS", None))

    def test_zero_reports_is_a_failure_even_with_green_exit(self):
        self.assertEqual(rb.scenario_outcome("PASS", "bound", 0), ("FAIL", "missing_report"))

    def test_unbound_assets_fail_even_with_reports(self):
        self.assertEqual(rb.scenario_outcome("PASS", "identity_mismatch", 3),
                         ("FAIL", "identity_mismatch"))
        self.assertEqual(rb.scenario_outcome("PASS", "missing_asset", 3),
                         ("FAIL", "missing_asset"))

    def test_engine_failure_status_is_preserved(self):
        self.assertEqual(rb.scenario_outcome("TIMEOUT", "missing_asset", 0), ("TIMEOUT", None))
        self.assertEqual(rb.scenario_outcome("FAIL", "bound", 1), ("FAIL", None))


class EvidenceRootTests(TmpDirCase):
    def test_relative_run_root_becomes_absolute(self):
        self.assertTrue(rb.run_root_of(str(self.tmp)).is_absolute())
        self.assertEqual(rb.run_root_of("relative/prefix"),
                         (Path.cwd() / "relative" / "prefix").resolve())

    def test_negative_suite_is_self_contained_without_a_successful_build(self):
        # A failed pilot must still be able to prove its gates reject broken input.
        run_root = self.tmp / "evidence"
        empty_assets = run_root / "apps" / "gpio" / "builds" / "A" / "assets"
        empty_assets.mkdir(parents=True)
        rounds = {"A": {"assets_dir": str(empty_assets).replace("\\", "/"),
                        "isolation": {"isolated_app_dir": "x/wink-micro-app/vendor/cms8s78xx/gpio"}},
                  "B": {"isolation": {"isolated_app_dir": "x/wink-micro-app/vendor/cms8s78xx/gpio"}}}
        results = rb.run_negatives(run_root, rounds, None)
        self.assertEqual([r["case"] for r in results],
                         ["missing_asset_rejected", "hash_mismatch_rejected",
                          "executed_object_mismatch_rejected", "embedded_build_path_rejected",
                          "scenario_pairing_rejected", "clean_outside_expected_tree_rejected",
                          "iso_path_budget_rejected",
                          "invalid_explicit_launcher_rejected", "missing_report_rejected"])
        self.assertTrue(all(r["status"] == "PASS" for r in results),
                        msg=json.dumps(results, indent=2, default=str))
        self.assertEqual(results[6]["observed"]["batch"]["status"], "PASS")


class ObjectPathBudgetTests(TmpDirCase):
    """MinGW make turns a breached CMAKE_OBJECT_PATH_MAX into a bogus compile error."""

    def test_deep_iso_tree_blocks_before_any_build(self):
        app = self.tmp / "wink-micro-app/vendor/cms8s78xx/epwm_brake_delay_recover"
        app.mkdir(parents=True)
        leaf = rb.compile_root_for(self.tmp / "iso" / ("d" * 150) / "A",
                                   app.relative_to(self.tmp))
        budget = rb.iso_path_budget(leaf)
        self.assertEqual(budget["status"], "BLOCKED")
        self.assertGreater(budget["projected_length"], rb.OBJECT_PATH_LIMIT)

    def test_batch_iso_tree_fits_for_every_app_name(self):
        run_root = rb.run_root_of(str(self.tmp / "artifacts/cms8s78xx-baseline/20261009-120000-abcd1234"))
        iso_base = rb.iso_base_of(run_root)
        apps = [self.tmp / rb.VENDOR_APPS_REL / name for name in
                ("gpio", "epwm_brake_delay_recover", "adc_hardware_trigger")]
        for app in apps:
            leaf = rb.compile_root_for(rb.iso_tree(iso_base, apps, app.name) / "A",
                                       app.relative_to(self.tmp))
            budget = rb.iso_path_budget(leaf)
            self.assertEqual(budget["status"], "PASS", msg=budget["projected_path"])
        token = iso_base.name
        self.assertEqual(iso_base.parts[-3:-1], ("artifacts", ".iso"))
        self.assertTrue(token.startswith("20261009-120000"), msg=token)
        self.assertLessEqual(len(token), 19)
        self.assertEqual(iso_base, rb.iso_base_of(rb.run_root_of(str(run_root))))

    def test_iso_tree_keys_apps_by_frozen_order(self):
        iso_base = rb.iso_base_of(self.tmp / "run" / "20261009-120000-abcd1234")
        apps = [self.tmp / rb.VENDOR_APPS_REL / name for name in ("acmp0", "gpio", "led")]
        self.assertEqual(rb.iso_tree(iso_base, apps, "gpio"), iso_base / "01")
        self.assertEqual(rb.iso_tree(iso_base, apps, "led"), iso_base / "02")

    def test_blocked_round_is_not_recorded_as_a_compile_failure(self):
        app = self.tmp / rb.VENDOR_APPS_REL / "epwm_brake_delay_recover"
        app.mkdir(parents=True)
        entry = {"command_prefix": ["unused"], "env": {}}
        tree = self.tmp / ("z" * 150)
        result = rb.build_round(entry, app, self.tmp, self.tmp / "evidence", app.name,
                                tree, "A", 60)
        self.assertEqual(result["status"], "BLOCKED")
        self.assertEqual(result["failure"], "iso_path_budget")
        self.assertNotIn("isolation", result)
        self.assertFalse((tree / "A").exists())
        comparison = rb.compare_rounds(result, dict(result, round="B"))
        self.assertEqual(comparison["status"], "BLOCKED")

    def test_reused_iso_tree_blocks_instead_of_building_on_stale_objects(self):
        app = self.tmp / rb.VENDOR_APPS_REL / "gpio"
        app.mkdir(parents=True)
        tree = self.tmp / "iso" / "00"
        (tree / "A").mkdir(parents=True)
        (tree / "S").mkdir(parents=True)
        entry = {"command_prefix": ["unused"], "env": {}}
        result = rb.build_round(entry, app, self.tmp, self.tmp / "evidence", app.name,
                                tree, "A", 60)
        self.assertEqual(result["status"], "BLOCKED")
        self.assertEqual(result["failure"], "iso_tree_not_fresh")
        scenario = rb.scenario_run(entry, app, self.tmp, self.tmp / "evidence", app.name, tree,
                                   {"round": "A", "assets": {}, "assets_dir": str(tree)}, 60)
        self.assertEqual(scenario["status"], "BLOCKED")
        self.assertEqual(scenario["failure"], "iso_tree_not_fresh")



if __name__ == "__main__":
    unittest.main()
