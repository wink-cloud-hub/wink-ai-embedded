"""Tests for lint YAML config loader and schema validation."""
from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

SDK = Path(__file__).resolve().parents[1].parent  # wink-micro-os
if str(SDK) not in sys.path:
    sys.path.insert(0, str(SDK))

from tools.lint.engine.config import LintConfigError, load_configs  # noqa: E402


class TestLoadConfigs(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.tmp_path = Path(self._tmp.name)

    def tearDown(self):
        self._tmp.cleanup()

    def test_load_layering_stub(self):
        p = self.tmp_path / "layering.yaml"
        p.write_text(
            "version: 1\nid: layering\n"
            "metadata: {owner: 'wink-arch', adr: ['ADR-0043']}\n"
            "layers:\n  bal_public:\n"
            "    roots: ['bal/include']\n    kind: public_header\n"
            "include_rules: []\napi_rules: []\npath_rules: []\nignore: []\n",
            encoding="utf-8",
        )
        cfg = load_configs([p])
        self.assertIn("bal_public", cfg.layers)
        self.assertEqual(cfg.packs["layering"].source, "sdk")

    def test_load_unknown_top_key_raises(self):
        p = self.tmp_path / "bad.yaml"
        p.write_text(
            "version: 1\nid: x\nlayers: {}\nunknown_field: 1\n",
            encoding="utf-8",
        )
        with self.assertRaises(LintConfigError):
            load_configs([p])

    def test_load_invalid_version_raises(self):
        p = self.tmp_path / "bad_version.yaml"
        p.write_text(
            "version: 2\nid: x\nlayers: {}\n"
            "include_rules: []\napi_rules: []\npath_rules: []\nignore: []\n",
            encoding="utf-8",
        )
        with self.assertRaises(LintConfigError):
            load_configs([p])

    def test_workspace_overlay_cannot_disable_immutable(self):
        sdk = self.tmp_path / "layering.yaml"
        sdk.write_text(
            "version: 1\nid: layering\n"
            "metadata: {owner: 'wink-arch', adr: ['ADR-0043']}\n"
            "layers:\n  bal_public:\n"
            "    roots: ['bal/include']\n    kind: public_header\n"
            "include_rules:\n"
            "  - id: BAL-HDR-NO-PAL\n"
            "    in: [bal_public]\n"
            "    deny:\n"
            "      - match: basename\n"
            "        pattern: 'pal_.*\\.h'\n"
            "    message: 'no pal headers'\n"
            "    severity: error\n"
            "    immutable: true\n"
            "api_rules: []\npath_rules: []\nignore: []\n",
            encoding="utf-8",
        )
        overlay = self.tmp_path / "workspace.yaml"
        overlay.write_text(
            "version: 1\nid: workspace\n"
            "disable_rules: [BAL-HDR-NO-PAL]\n",
            encoding="utf-8",
        )
        with self.assertRaises(LintConfigError):
            load_configs([sdk, overlay])

    def test_workspace_overlay_may_append_allow_paths_on_immutable(self):
        sdk = self.tmp_path / "layering.yaml"
        sdk.write_text(
            "version: 1\nid: layering\n"
            "layers:\n  bal_public:\n"
            "    roots: ['bal/include']\n    kind: public_header\n"
            "include_rules:\n"
            "  - id: BAL-HDR-NO-PAL\n"
            "    in: [bal_public]\n"
            "    deny:\n"
            "      - match: basename\n"
            "        pattern: 'pal_.*\\.h'\n"
            "    allow_paths: []\n"
            "    message: 'no pal headers'\n"
            "    severity: error\n"
            "    immutable: true\n"
            "api_rules: []\npath_rules: []\nignore: []\n",
            encoding="utf-8",
        )
        overlay = self.tmp_path / "workspace.yaml"
        overlay.write_text(
            "version: 1\nid: workspace\n"
            "overrides:\n"
            "  BAL-HDR-NO-PAL:\n"
            "    add_allow_paths:\n"
            "      - path: 'vendor_sdk/**'\n"
            "        reason: 'third-party'\n",
            encoding="utf-8",
        )
        cfg = load_configs([sdk, overlay])
        rule = next(r for r in cfg.include_rules if r["id"] == "BAL-HDR-NO-PAL")
        self.assertTrue(rule["immutable"])
        self.assertEqual(len(rule["allow_paths"]), 1)
        self.assertEqual(rule["allow_paths"][0]["path"], "vendor_sdk/**")

    def test_workspace_overlay_forbidden_keys_on_immutable_raises(self):
        sdk = self.tmp_path / "layering.yaml"
        sdk.write_text(
            "version: 1\nid: layering\n"
            "layers:\n  bal_public:\n"
            "    roots: ['bal/include']\n    kind: public_header\n"
            "include_rules:\n"
            "  - id: BAL-HDR-NO-PAL\n"
            "    in: [bal_public]\n"
            "    deny:\n"
            "      - match: basename\n"
            "        pattern: 'pal_.*\\.h'\n"
            "    allow_paths: []\n"
            "    message: 'no pal headers'\n"
            "    severity: error\n"
            "    immutable: true\n"
            "api_rules: []\npath_rules: []\nignore: []\n",
            encoding="utf-8",
        )
        overlay = self.tmp_path / "workspace.yaml"
        overlay.write_text(
            "version: 1\nid: workspace\n"
            "overrides:\n"
            "  BAL-HDR-NO-PAL:\n"
            "    severity: warning\n"
            "    deny:\n"
            "      - match: basename\n"
            "        pattern: 'hal_.*\\.h'\n",
            encoding="utf-8",
        )
        with self.assertRaises(LintConfigError) as ctx:
            load_configs([sdk, overlay])
        self.assertIn("forbidden key", str(ctx.exception).lower())

    def test_extends_non_empty_raises(self):
        p = self.tmp_path / "extends.yaml"
        p.write_text(
            "version: 1\nid: child\n"
            "extends: [layering]\n"
            "layers: {}\n"
            "include_rules: []\napi_rules: []\npath_rules: []\nignore: []\n",
            encoding="utf-8",
        )
        with self.assertRaises(LintConfigError) as ctx:
            load_configs([p])
        self.assertIn("extends is not implemented", str(ctx.exception))

    def test_extends_empty_list_allowed(self):
        p = self.tmp_path / "empty_extends.yaml"
        p.write_text(
            "version: 1\nid: layering\n"
            "extends: []\n"
            "layers:\n  bal_public:\n"
            "    roots: ['bal/include']\n    kind: public_header\n"
            "include_rules: []\napi_rules: []\npath_rules: []\nignore: []\n",
            encoding="utf-8",
        )
        cfg = load_configs([p])
        self.assertIn("bal_public", cfg.layers)


if __name__ == "__main__":
    unittest.main()
