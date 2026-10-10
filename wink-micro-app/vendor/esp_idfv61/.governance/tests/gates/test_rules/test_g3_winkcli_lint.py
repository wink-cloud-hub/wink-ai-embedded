# SPDX-License-Identifier: Apache-2.0
"""
Unit tests for g3_winkcli_lint.py result handling.

The defect these cover: once json.loads() succeeded, proc.returncode was never
re-read, so a crashed or drifted toolchain produced an empty finding list and was
reported as a clean pass -- the single layering-integrity gate failing open.
"""

import json
import re
import sys
from pathlib import Path

from gates.rules import g3_winkcli_lint as g3


class FakeProc:
    def __init__(self, returncode, stdout, stderr=""):
        self.returncode = returncode
        self.stdout = stdout
        self.stderr = stderr


def _run_with(monkeypatch, proc):
    monkeypatch.setattr(g3, "find_winkcli_executable", lambda *a, **k: "fake-winkcli")
    monkeypatch.setattr(g3.subprocess, "run", lambda *a, **k: proc)
    return g3.run({"workspace_root": ".", "changed_files": []}, {})


def test_tool_crash_is_not_a_clean_pass(monkeypatch):
    """rc != 0 with well-formed empty JSON must still fail the gate."""
    out = _run_with(monkeypatch, FakeProc(3, "[]", "fatal: rule pack not found"))
    assert len(out) == 1
    assert out[0]["severity"] == "error"


def test_envelope_drift_is_not_a_clean_pass(monkeypatch):
    """A dict payload means the output contract changed; findings would vanish."""
    payload = json.dumps({"findings": [{"rule_id": "LAYER", "severity": "error",
                                        "path": "a.c", "line": 1, "message": "v"}]})
    out = _run_with(monkeypatch, FakeProc(1, payload))
    assert len(out) == 1
    assert out[0]["severity"] == "error"


def test_envelope_drift_at_zero_exit_is_caught(monkeypatch):
    out = _run_with(monkeypatch, FakeProc(0, json.dumps({"findings": []})))
    assert len(out) == 1
    assert out[0]["severity"] == "error"


def test_real_violation_is_reported(monkeypatch):
    payload = json.dumps([{"rule_id": "LAYER", "severity": "error",
                           "path": "a.c", "line": 1, "message": "real violation"}])
    out = _run_with(monkeypatch, FakeProc(0, payload))
    assert len(out) == 1
    assert out[0]["severity"] == "error"
    assert "real violation" in out[0]["message"]


def test_genuine_clean_run_passes(monkeypatch):
    out = _run_with(monkeypatch, FakeProc(0, "[]"))
    assert out == []


def test_severity_is_normalized_not_dropped(monkeypatch):
    """run_gates buckets by exact string; "Error" must be folded to "error"
    rather than landing in no bucket and affecting nothing."""
    payload = json.dumps([{"rule_id": "X", "severity": "Error",
                           "path": "a.c", "line": 1, "message": "m"}])
    out = _run_with(monkeypatch, FakeProc(0, payload))
    assert len(out) == 1
    assert out[0]["severity"] == "error"


def test_unknown_severity_is_rejected(monkeypatch):
    payload = json.dumps([{"rule_id": "X", "severity": "notice",
                           "path": "a.c", "line": 1, "message": "m"}])
    out = _run_with(monkeypatch, FakeProc(0, payload))
    assert len(out) == 1
    assert out[0]["severity"] == "error"
    assert "unknown severity" in out[0]["message"]


def test_non_object_array_element_is_rejected(monkeypatch):
    out = _run_with(monkeypatch, FakeProc(0, json.dumps(["oops"])))
    assert len(out) == 1
    assert out[0]["severity"] == "error"


def test_toolchain_resolves_via_path_winkcli(tmp_path, monkeypatch):
    """Linter resolves directly via winkcli executable from PATH."""
    ws = tmp_path / "wink-ai-embedded"
    ws.mkdir()

    monkeypatch.setenv("WINK_TOOLS_ROOT", "")
    monkeypatch.delenv("WINK_TOOLS_ROOT", raising=False)
    monkeypatch.setattr(g3.shutil, "which",
                        lambda _n: r"D:\software\python\.pyenv\pyenv-win\shims\winkcli.BAT")

    cmd = g3.find_winkcli_executable(ws)
    assert cmd is not None
    assert cmd[0] == r"D:\software\python\.pyenv\pyenv-win\shims\winkcli.BAT"
    assert cmd[1] == "lint"


def test_env_override_takes_precedence(tmp_path, monkeypatch):
    ws = tmp_path / "ws"
    ws.mkdir()
    override = tmp_path / "override"
    override.mkdir()
    (override / "wink.py").write_text("# pinned\n", encoding="utf-8")
    monkeypatch.setenv("WINK_TOOLS_ROOT", str(override))
    cmd = g3.find_winkcli_executable(ws)
    assert cmd is not None and Path(cmd[1]).parent == override


def test_missing_toolchain_hint_names_canonical_path(tmp_path):
    ws = tmp_path / "ws"
    ws.mkdir()
    hint = g3.toolchain_search_hint(ws)
    assert "winkcli" in hint
    assert "PATH" in hint


def test_missing_toolchain_when_not_on_path(monkeypatch, tmp_path):
    monkeypatch.setattr(g3.shutil, "which", lambda _name: None)
    monkeypatch.delenv("WINK_TOOLS_ROOT", raising=False)

    ws = tmp_path / "ws"
    ws.mkdir()
    assert g3.find_winkcli_executable(ws) is None


# --- confidentiality ---------------------------------------------------------
# The toolchain is a commercial-secret private repository and this repository is
# open source. Publishing its remote, organisation id or commit id here would
# hand out reconnaissance material about internal infrastructure, so none of it
# may appear in committed files -- not in the lock, not in diagnostics, not in a
# generator that gets re-run and re-committed.
#
# These checks are deliberately written with GENERIC patterns rather than the
# actual confidential strings: a test that hardcoded the private host or org id
# would itself be the leak it is meant to prevent.

GIT_REMOTE = re.compile(r"(?:git@[^\s'\"]+|" + "ssh" + r"://\S+|" + "git" + r"://\S+)")


def _assert_no_remote(text: str, what: str) -> None:
    m = GIT_REMOTE.search(text)
    assert m is None, f"{what} discloses a git remote: {m.group(0)!r}"


def test_hint_does_not_disclose_the_private_toolchain_remote():
    from pathlib import Path as _P
    hint = g3.toolchain_search_hint(_P("."))
    _assert_no_remote(hint, "toolchain_search_hint")
    assert "wink-ai" not in hint


def test_lock_file_discloses_no_remote_or_commit():
    import yaml as _yaml
    text = g3.TOOLCHAIN_LOCK.read_text(encoding="utf-8")
    _assert_no_remote(text, "toolchain.lock.yaml")
    data = _yaml.safe_load(text)
    tc = data["toolchain"]
    for banned in ("repository", "remote", "url", "pinned_commit", "pinned_branch"):
        assert banned not in tc, f"toolchain lock must not record {banned!r}"
    assert tc["entry_sha256"], "content hash must still be recorded"


def test_generator_never_writes_the_private_remote():
    gen = (g3.TOOLCHAIN_LOCK.parent / "tools" / "refresh_toolchain_lock.py")
    src = gen.read_text(encoding="utf-8")
    _assert_no_remote(src, "refresh_toolchain_lock.py")
    for emitted in ("  repository:", "  pinned_commit:", "  pinned_branch:"):
        assert f'f\'{emitted}' not in src, \
            f"generator must not emit {emitted.strip()} into the lock"


# --- toolchain fingerprint ---------------------------------------------------

def _write_lock(tmp_path, entry_sha, rules):
    lock = tmp_path / "toolchain.lock.yaml"
    body = [f'spec_version: "1.0"', "toolchain:", f'  entry_sha256: "{entry_sha}"']
    if rules:
        body += ["rule_data:"]
        body += [f'  {k}: "{v}"' for k, v in rules.items()]
    lock.write_text("\n".join(body) + "\n", encoding="utf-8")
    return lock


def test_fingerprint_clean_when_lock_matches(monkeypatch, tmp_path):
    monkeypatch.setattr(g3, "TOOLCHAIN_LOCK", _write_lock(tmp_path, "ab" * 32, None))
    entry = tmp_path / "wink.py"
    entry.write_bytes(b"x")
    real = g3._sha256(entry)
    lock = _write_lock(tmp_path, real, None)
    monkeypatch.setattr(g3, "TOOLCHAIN_LOCK", lock)
    assert g3.check_toolchain_fingerprint(entry, tmp_path) == []


def test_fingerprint_warns_on_engine_drift(monkeypatch, tmp_path):
    monkeypatch.setattr(g3, "TOOLCHAIN_LOCK",
                        _write_lock(tmp_path, "0" * 64, None))
    entry = tmp_path / "wink.py"
    entry.write_bytes(b"x")
    out = g3.check_toolchain_fingerprint(entry, tmp_path)
    assert len(out) == 1
    assert out[0]["severity"] == "warning"
    assert "Toolchain drift" in out[0]["message"]


def test_fingerprint_warns_on_mirror_divergence(monkeypatch, tmp_path):
    priv = tmp_path / "priv"
    (priv / "tools" / "lint" / "rules").mkdir(parents=True)
    (priv / "wink.py").write_bytes(b"x")
    real = g3._sha256(priv / "wink.py")
    (priv / "tools" / "lint" / "rules" / "layering.yaml").write_bytes(b"private")

    lock = _write_lock(tmp_path, real, {"layering.yaml": g3._sha256(
        priv / "tools" / "lint" / "rules" / "layering.yaml")})
    monkeypatch.setattr(g3, "TOOLCHAIN_LOCK", lock)

    ws = tmp_path / "ws"
    mirror = ws / "wink-tools" / "tools" / "lint" / "rules"
    mirror.mkdir(parents=True)
    (mirror / "layering.yaml").write_bytes(b"mirror-differs")

    out = g3.check_toolchain_fingerprint(priv / "wink.py", ws)
    assert len(out) == 1
    assert out[0]["severity"] == "warning"
    assert "mirror divergence" in out[0]["message"].lower()


def test_fingerprint_never_blocks_and_tolerates_missing_lock(monkeypatch, tmp_path):
    monkeypatch.setattr(g3, "TOOLCHAIN_LOCK", tmp_path / "nope.yaml")
    entry = tmp_path / "wink.py"
    entry.write_bytes(b"x")
    assert g3.check_toolchain_fingerprint(entry, tmp_path) == []


def test_corrupt_lock_is_reported_not_silently_ignored(monkeypatch, tmp_path):
    """Regression: a parse error used to `return []`, which made a corrupt
    baseline indistinguishable from a clean run -- the check was effectively off."""
    bad = tmp_path / "toolchain.lock.yaml"
    bad.write_text("spec_version: \"1.0\"\nthis line has no colon\n", encoding="utf-8")
    monkeypatch.setattr(g3, "TOOLCHAIN_LOCK", bad)
    entry = tmp_path / "wink.py"
    entry.write_bytes(b"x")
    out = g3.check_toolchain_fingerprint(entry, tmp_path)
    assert len(out) == 1
    assert out[0]["severity"] == "warning"
    assert "could not be parsed" in out[0]["message"]
    assert "NOT running" in out[0]["message"]


def test_lock_missing_toolchain_key_is_reported(monkeypatch, tmp_path):
    bad = tmp_path / "toolchain.lock.yaml"
    bad.write_text('spec_version: "1.0"\nrule_data: {}\n', encoding="utf-8")
    monkeypatch.setattr(g3, "TOOLCHAIN_LOCK", bad)
    entry = tmp_path / "wink.py"
    entry.write_bytes(b"x")
    out = g3.check_toolchain_fingerprint(entry, tmp_path)
    assert len(out) == 1
    assert "missing the required" in out[0]["message"]


def test_committed_lock_file_is_valid_yaml():
    """The lock is generated; if the generator emits broken YAML the whole
    fingerprint mechanism silently turns off. Guard the artifact itself."""
    lock = Path(g3.TOOLCHAIN_LOCK)
    assert lock.exists(), f"{lock} is missing"
    import yaml as _yaml
    data = _yaml.safe_load(lock.read_text(encoding="utf-8"))
    assert isinstance(data, dict)
    assert "toolchain" in data
    assert (data["toolchain"] or {}).get("entry_sha256")
    assert data.get("rule_data")
    assert "overlays" in data


def test_fingerprint_findings_merged_on_clean_run(monkeypatch, tmp_path):
    """Drift must surface even when the lint itself is clean."""
    entry = tmp_path / "wink.py"
    entry.write_bytes(b"engine-v1")
    monkeypatch.setattr(g3, "_run_lint", lambda c, cfg=None: [])
    monkeypatch.setattr(g3, "find_winkcli_executable",
                        lambda ws: [sys.executable, str(entry), "lint"])
    monkeypatch.setattr(g3, "TOOLCHAIN_LOCK",
                        _write_lock(tmp_path, "0" * 64, None))
    out = g3.run({"workspace_root": str(tmp_path), "changed_files": []}, {})
    assert any(f["rule_id"].endswith("toolchain_drift") for f in out), out


# --- project overlays --------------------------------------------------------

def test_resolve_overlays_skips_missing_and_resolves_relative(tmp_path):
    rules = tmp_path / "wink-tools" / "tools" / "lint" / "rules"
    rules.mkdir(parents=True)
    (rules / "present.overlay.yaml").write_text("version: 1\n", encoding="utf-8")

    got = g3.resolve_overlays(tmp_path, {
        "overlays": ["wink-tools/tools/lint/rules/present.overlay.yaml",
                     "wink-tools/tools/lint/rules/absent.overlay.yaml"]})
    assert [p.name for p in got] == ["present.overlay.yaml"]


def test_resolve_overlays_absent_config_returns_empty(tmp_path):
    assert g3.resolve_overlays(tmp_path, {}) == []


def test_build_cmd_carries_packs_and_overlays_in_order(tmp_path):
    ov = tmp_path / "x.overlay.yaml"
    ov.write_text("version: 1\n", encoding="utf-8")
    cmd = g3._build_cmd(["py", "wink.py", "lint"], tmp_path, ["layering", "api"], [ov])
    assert cmd.count("--pack") == 2
    assert cmd.count("--config") == 1
    # Overlays must come after the packs so they apply to already-loaded rules.
    assert cmd.index("--config") > cmd.index("api")


def test_pr_branch_without_c_changes_still_keeps_config_packs(monkeypatch, tmp_path):
    """Regression: that branch used to rebuild the command and hardcode packs,
    which would silently drop any config-driven pack or overlay."""
    seen = {}

    def fake_run(cmd, **kw):
        seen["cmd"] = cmd
        return FakeProc(0, "[]")

    monkeypatch.setattr(g3, "find_winkcli_executable",
                        lambda ws: [sys.executable, str(tmp_path / "wink.py"), "lint"])
    monkeypatch.setattr(g3.subprocess, "run", fake_run)
    g3.run({"workspace_root": str(tmp_path), "mode": "pr",
            "changed_files": ["docs/README.md"]},
           {"packs": ["layering", "api"], "overlays": []})
    assert "--config" not in seen["cmd"]
    assert seen["cmd"].count("--pack") == 2


def test_overlay_drift_is_reported(monkeypatch, tmp_path):
    rules = tmp_path / "wink-tools" / "tools" / "lint" / "rules"
    rules.mkdir(parents=True)
    ov = rules / "w.overlay.yaml"
    ov.write_text("version: 1\n", encoding="utf-8")

    lock = tmp_path / "toolchain.lock.yaml"
    lock.write_text(
        'spec_version: "1.0"\ntoolchain:\n  entry_sha256: "%s"\noverlays:\n'
        '  w.overlay.yaml: "%s"\n' % ("0" * 64, "0" * 64), encoding="utf-8")
    monkeypatch.setattr(g3, "TOOLCHAIN_LOCK", lock)

    entry = tmp_path / "wink.py"
    entry.write_bytes(b"x")
    out = g3.check_toolchain_fingerprint(entry, tmp_path)
    assert any("Overlay drift" in f["message"] for f in out), out


def test_missing_overlay_is_reported(monkeypatch, tmp_path):
    lock = tmp_path / "toolchain.lock.yaml"
    lock.write_text(
        'spec_version: "1.0"\ntoolchain:\n  entry_sha256: "%s"\noverlays:\n'
        '  gone.overlay.yaml: "%s"\n' % ("0" * 64, "0" * 64), encoding="utf-8")
    monkeypatch.setattr(g3, "TOOLCHAIN_LOCK", lock)
    entry = tmp_path / "wink.py"
    entry.write_bytes(b"x")
    out = g3.check_toolchain_fingerprint(entry, tmp_path)
    assert any("is missing from" in f["message"] for f in out), out
