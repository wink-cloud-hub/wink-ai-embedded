# SPDX-License-Identifier: Apache-2.0
"""
Unit Tests for Safety Checker Peripheral Plugin Rules (P-1 to P-4)
and Transactional Directory-Tree Rollback
"""
from __future__ import annotations

import tempfile
from pathlib import Path
import pytest


from loop.safety_checker import (
    HeuristicSafetyChecker,
    SAFE_WRITE_WHITELIST,
)
from loop.remediator import (
    TransactionalGitTracker,
)


def test_whitelist_allows_peripheral_and_doc_paths():
    """Verify SAFE_WRITE_WHITELIST explicitly includes builtin plugins and doc plans."""
    assert "wink-plugin-peripherals/builtin/" in SAFE_WRITE_WHITELIST
    assert "docs/implementation-plans/wokwi-dal-type-coverage-type/" in SAFE_WRITE_WHITELIST

    checker = HeuristicSafetyChecker()
    valid_patch = """--- a/wink-plugin-peripherals/builtin/imu/package.json
+++ b/wink-plugin-peripherals/builtin/imu/package.json
@@ -0,0 +1,5 @@
+{"name": "@wink/plugin-imu"}
+"""
    ok, errs = checker.validate_patch_scope(valid_patch)
    assert ok is True
    assert len(errs) == 0


def test_rule_p1_plan_first_blocking():
    """Rule P-1: Peripheral plugin additions must be backed by a sub-plan document."""
    checker = HeuristicSafetyChecker()

    # Patch with plugin but NO sub-plan document
    bad_patch = """--- a/wink-plugin-peripherals/builtin/motion/1.0.0/src/simulation.ts
+++ b/wink-plugin-peripherals/builtin/motion/1.0.0/src/simulation.ts
@@ -0,0 +1,5 @@
+export class MotionSim { reset() {} }
+"""
    ok, errs = checker.validate_peripheral_plugin_rules(bad_patch)
    assert ok is False
    assert any("P-1 Plan-First Violation" in e for e in errs)

    # Patch with BOTH plugin and sub-plan document
    good_patch = """--- a/docs/implementation-plans/wokwi-dal-type-coverage-type/15-p2-sensor-motion-plan.md
+++ b/docs/implementation-plans/wokwi-dal-type-coverage-type/15-p2-sensor-motion-plan.md
@@ -0,0 +1,5 @@
+# Motion Plan
--- a/wink-plugin-peripherals/builtin/motion/1.0.0/src/simulation.ts
+++ b/wink-plugin-peripherals/builtin/motion/1.0.0/src/simulation.ts
@@ -0,0 +1,5 @@
+export class MotionSim { reset() {} }
+"""
    ok, errs = checker.validate_peripheral_plugin_rules(good_patch)
    assert ok is True
    assert len(errs) == 0


def test_rule_p2_forbidden_bare_chip_types():
    """Rule P-2: Forbidden bare chip model names as type."""
    checker = HeuristicSafetyChecker()

    # mpu9250 is a bare chip name, forbidden
    bad_patch = """--- a/docs/implementation-plans/wokwi-dal-type-coverage-type/16-p2-sensor-mpu9250-plan.md
+++ b/docs/implementation-plans/wokwi-dal-type-coverage-type/16-p2-sensor-mpu9250-plan.md
@@ -0,0 +1,5 @@
+# Plan
--- a/wink-plugin-peripherals/builtin/mpu9250/1.0.0/src/simulation.ts
+++ b/wink-plugin-peripherals/builtin/mpu9250/1.0.0/src/simulation.ts
@@ -0,0 +1,5 @@
+export class Sim { reset() {} }
+"""
    ok, errs = checker.validate_peripheral_plugin_rules(bad_patch)
    assert ok is False
    assert any("P-2 Type Unification Violation" in e for e in errs)

    # imu is canonical, allowed
    good_patch = """--- a/docs/implementation-plans/wokwi-dal-type-coverage-type/16-p2-sensor-imu-plan.md
+++ b/docs/implementation-plans/wokwi-dal-type-coverage-type/16-p2-sensor-imu-plan.md
@@ -0,0 +1,5 @@
+# Plan
--- a/wink-plugin-peripherals/builtin/imu/1.0.0/src/simulation.ts
+++ b/wink-plugin-peripherals/builtin/imu/1.0.0/src/simulation.ts
@@ -0,0 +1,5 @@
+export class Sim { reset() {} }
+"""
    ok, errs = checker.validate_peripheral_plugin_rules(good_patch)
    assert ok is True


def test_rule_p3_manifest_pintype_validation():
    """Rule P-3: Manifest pinType must be from approved enum."""
    checker = HeuristicSafetyChecker()

    bad_patch = """--- a/docs/implementation-plans/wokwi-dal-type-coverage-type/16-p2-sensor-imu-plan.md
+++ b/docs/implementation-plans/wokwi-dal-type-coverage-type/16-p2-sensor-imu-plan.md
@@ -0,0 +1,5 @@
+# Plan
--- a/wink-plugin-peripherals/builtin/imu/1.0.0/dist/manifest.json
+++ b/wink-plugin-peripherals/builtin/imu/1.0.0/dist/manifest.json
@@ -0,0 +1,10 @@
+{
+  "pins": [
+    {"name": "SCL", "pinType": "invalid_pin_type"},
+    {"name": "SDA", "pinType": "i2c_sda"}
+  ]
+}
+"""
    ok, errs = checker.validate_peripheral_plugin_rules(bad_patch)
    assert ok is False
    assert any("P-3 Manifest Integrity Violation" in e for e in errs)

    good_patch = """--- a/docs/implementation-plans/wokwi-dal-type-coverage-type/16-p2-sensor-imu-plan.md
+++ b/docs/implementation-plans/wokwi-dal-type-coverage-type/16-p2-sensor-imu-plan.md
@@ -0,0 +1,5 @@
+# Plan
--- a/wink-plugin-peripherals/builtin/imu/1.0.0/dist/manifest.json
+++ b/wink-plugin-peripherals/builtin/imu/1.0.0/dist/manifest.json
@@ -0,0 +1,10 @@
+{
+  "pins": [
+    {"name": "SCL", "pinType": "i2c_scl"},
+    {"name": "SDA", "pinType": "i2c_sda"}
+  ]
+}
+"""
    ok, errs = checker.validate_peripheral_plugin_rules(good_patch)
    assert ok is True


def test_rule_p4_lifecycle_reset_validation():
    """Rule P-4: simulation.ts must implement reset() or onAttach state clearing."""
    checker = HeuristicSafetyChecker()

    # Missing reset or state clearing
    bad_patch = """--- a/docs/implementation-plans/wokwi-dal-type-coverage-type/16-p2-sensor-imu-plan.md
+++ b/docs/implementation-plans/wokwi-dal-type-coverage-type/16-p2-sensor-imu-plan.md
@@ -0,0 +1,5 @@
+# Plan
--- a/wink-plugin-peripherals/builtin/imu/1.0.0/src/simulation.ts
+++ b/wink-plugin-peripherals/builtin/imu/1.0.0/src/simulation.ts
@@ -0,0 +1,8 @@
+export class ImuPlugin {
+  onAttach(ctx: any) {
+    // no state reset here
+  }
+}
+"""
    ok, errs = checker.validate_peripheral_plugin_rules(bad_patch)
    assert ok is False
    assert any("P-4 Lifecycle Reset Violation" in e for e in errs)

    # Has reset() method
    good_patch = """--- a/docs/implementation-plans/wokwi-dal-type-coverage-type/16-p2-sensor-imu-plan.md
+++ b/docs/implementation-plans/wokwi-dal-type-coverage-type/16-p2-sensor-imu-plan.md
@@ -0,0 +1,5 @@
+# Plan
--- a/wink-plugin-peripherals/builtin/imu/1.0.0/src/simulation.ts
+++ b/wink-plugin-peripherals/builtin/imu/1.0.0/src/simulation.ts
@@ -0,0 +1,8 @@
+export class ImuPlugin {
+  reset() {
+    this.regs = [];
+  }
+}
+"""
    ok, errs = checker.validate_peripheral_plugin_rules(good_patch)
    assert ok is True


def test_transactional_git_tracker_directory_tree_rollback():
    """Verify recursive deletion of newly created peripheral plugin directories."""
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp_path = Path(tmpdir)
        tracker = TransactionalGitTracker(tmp_path)

        # Create simulated plugin directory
        plugin_root = tmp_path / "wink-plugin-peripherals" / "builtin" / "mock_sensor"
        plugin_src = plugin_root / "1.0.0" / "src"
        plugin_src.mkdir(parents=True, exist_ok=True)
        file_a = plugin_src / "simulation.ts"
        file_a.write_text("export class Mock {}", encoding="utf-8")

        assert plugin_root.is_dir()
        assert file_a.is_file()

        # Execute rollback with file_a as created_files
        tracker.rollback(modified_files=[], created_files=[file_a])

        # Assert entire plugin subtree was cleanly pruned
        assert not file_a.exists()
        assert not plugin_root.exists()
