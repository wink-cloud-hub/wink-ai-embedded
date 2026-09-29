# SPDX-License-Identifier: Apache-2.0
"""
refresh_toolchain_lock.py
=========================
Regenerates `.governance/toolchain.lock.yaml`, the fingerprint of the private
wink-ai toolchain that Gate 3 executes.

The toolchain lives in a commercial-secret private repository and is
deliberately not vendored here. This script makes that coupling auditable by
recording the engine's content hash, the informational commit, and the rule-data
hashes of the executed copies.

Usage:
    python .governance/tools/refresh_toolchain_lock.py            # rewrite the lock
    python .governance/tools/refresh_toolchain_lock.py --check    # report drift only
"""

import argparse
import hashlib
import shutil
import subprocess
import sys
from pathlib import Path

import yaml

GOV_DIR = Path(__file__).resolve().parent.parent
REPO_ROOT = GOV_DIR.parents[3]
LOCK_PATH = GOV_DIR / "toolchain.lock.yaml"
MIRROR_RULES = REPO_ROOT / "wink-tools" / "tools" / "lint" / "rules"
RULES_REL = Path("packages") / "wink-tools" / "tools" / "lint" / "rules"


def sha256(p: Path) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def locate_private_repo() -> Path | None:
    candidates = [
        REPO_ROOT.parent / "wink-ai",
        REPO_ROOT.parent.parent / "wink-ai",
        REPO_ROOT / "wink-ai",
    ]
    for c in candidates:
        if (c / RULES_REL).is_dir():
            return c
    return None


def git_info(repo: Path) -> tuple[str, str]:
    try:
        commit = subprocess.run(["git", "-C", str(repo), "rev-parse", "HEAD"],
                                capture_output=True, text=True).stdout.strip()
        branch = subprocess.run(["git", "-C", str(repo), "rev-parse", "--abbrev-ref", "HEAD"],
                                capture_output=True, text=True).stdout.strip()
        remote = subprocess.run(["git", "-C", str(repo), "remote", "get-url", "origin"],
                                capture_output=True, text=True).stdout.strip()
        return commit, branch, remote
    except OSError:
        return "", "", ""


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true",
                    help="Report drift without rewriting the lock")
    ap.add_argument("--sync-mirror", action="store_true",
                    help="Copy the private repo's rule packs over this repo's mirror")
    args = ap.parse_args()

    priv = locate_private_repo()
    if priv is None:
        print("ERROR: private wink-ai repository not found. Expected it as a sibling "
              "of wink-ai-embedded. Looked in:", file=sys.stderr)
        for c in (REPO_ROOT.parent / "wink-ai", REPO_ROOT.parent.parent / "wink-ai"):
            print(f"  {c}", file=sys.stderr)
        return 2

    entry = priv / "packages" / "wink-tools" / "wink.py"
    commit, branch, remote = git_info(priv)

    print(f"private repo : {priv}")
    print(f"entry point  : {entry}")
    print(f"  sha256     : {sha256(entry)}")
    print(f"  commit     : {commit} ({branch})")

    if args.sync_mirror:
        MIRROR_RULES.mkdir(parents=True, exist_ok=True)
        for rule in sorted((priv / RULES_REL).glob("*.yaml")):
            shutil.copy2(rule, MIRROR_RULES / rule.name)
        print(f"\nsynced {len(list((priv / RULES_REL).glob('*.yaml')))} pack(s) into {MIRROR_RULES}")

    print(f"\nrule data (executed copy = private repo) vs mirror (this repo):")
    drift = []
    for rule in sorted((priv / RULES_REL).glob("*.yaml")):
        a = sha256(rule)
        mirror = MIRROR_RULES / rule.name
        if not mirror.exists():
            print(f"  {rule.name:<20} private={a[:12]}  mirror=MISSING  <-- DIVERGENCE")
            drift.append((rule.name, a, None))
            continue
        b = sha256(mirror)
        same = a == b
        print(f"  {rule.name:<20} private={a[:12]}  mirror={b[:12]}  "
              f"{'ok' if same else '<-- DIVERGENCE'}")
        if not same:
            drift.append((rule.name, a, b))

    overlays = sorted(MIRROR_RULES.glob("*.overlay.yaml"))
    print(f"\nproject overlay (the actual customization channel):")
    for ov in overlays or [None]:
        if ov is None:
            print("  (none found)")
        else:
            print(f"  {ov.name:<20} {sha256(ov)[:12]}")

    if args.check:
        return 1 if drift else 0

    lines = [
        "# SPDX-License-Identifier: Apache-2.0",
        "#",
        "# Toolchain fingerprint for the ESP-IDF classification gate system.",
        "#",
        "# WHY THIS EXISTS",
        "# ---------------",
        "# Gate 3 executes the linter from the PRIVATE wink-ai repository",
        "# (wink-ai/packages/wink-tools/wink.py). That repository is deliberately not",
        "# vendored here -- it is a commercial-secret private repo. The consequence is",
        "# that the engine is not version-controlled in this repository, so nothing",
        "# would otherwise detect that the engine underneath a given lint verdict has",
        "# changed. This lock makes the coupling auditable WITHOUT vendoring anything.",
        "#",
        "# MISMATCH SEMANTICS",
        "# ------------------",
        "# A mismatch is a WARNING, never an error. The private toolchain is under",
        "# active development; hard-blocking on every engine change would produce",
        "# alert fatigue and get the check disabled. Refresh deliberately with:",
        "#     python .governance/tools/refresh_toolchain_lock.py",
        "#",
        "# GENERATED FILE -- do not hand-edit.",
        f'spec_version: "1.0"',
        "",
        "toolchain:",
        f'  repository: "{remote}"',
        '  package_relpath: "packages/wink-tools"',
        '  entry_point: "wink.py"',
        f'  pinned_commit: "{commit}"',
        f'  pinned_branch: "{branch}"',
        "  # Authoritative: content hash of the engine entry point actually executed.",
        f'  entry_sha256: "{sha256(entry)}"',
        "",
        "# Rule data lives in BOTH repos; Gate 3 executes the private repo's copy.",
        "# This repo's wink-tools/tools/lint/rules/ is a committed mirror kept for",
        "# reviewability. If the copies diverge, the mirror no longer describes what",
        "# is actually linted.",
        'rule_data_reference: "private_repo"',
        "rule_data:",
    ]
    for rule in sorted((priv / RULES_REL).glob("*.yaml")):
        lines.append(f'  {rule.name}: "{sha256(rule)}"')

    ovs = sorted(MIRROR_RULES.glob("*.overlay.yaml"))
    lines += [
        "",
        "# Project-local overlays: the ONLY customization channel for the rule data.",
        "# wink lint treats a YAML carrying `overrides`/`disable_rules` as an overlay",
        "# rather than a pack and appends add_allow_paths onto the built-in rule, so",
        "# the project customises lint without forking the private toolchain's packs.",
        "# Gate 3 passes these via `--config` after the built-in packs.",
    ]
    if ovs:
        lines.append("overlays:")
        for ov in ovs:
            lines.append(f'  {ov.name}: "{sha256(ov)}"')
    else:
        lines.append("overlays: {}")

    if drift:
        lines += [
            "",
            "known_divergences:",
        ]
        for name, a, b in drift:
            lines += [
                f"  - file: \"{name}\"",
                f"    private_repo_sha256_prefix: \"{a[:12]}\"",
                f"    this_repo_sha256_prefix: \"{(b or 'MISSING')[:12]}\"",
                "    reason: >",
                "      The private repo's copy is the one Gate 3 executes, and the",
                "      mirror committed here is never loaded: tools/lint/cli.py resolves",
                "      rule packs from tools_pkg_root() (the wink-tools package actually",
                "      imported), falling back to <root>/tools/lint/rules only when the",
                "      package itself has no rules directory. Because the package does",
                "      have one, this repo's copy has no effect on any lint verdict.",
                "      For user_surface.yaml specifically, the two copies differ only in",
                "      the allow_paths exemptions of rule APP-NO-DAL-CALL, and the private",
                "      copy's paths do not exist in this repo -- so the exemptions do not",
                "      match and the rule reports the deliberately-exempt counter-example",
                "      samples as violations. Reconciling means editing the SHARED private",
                "      toolchain, which affects every project using it; deliberately NOT",
                "      auto-resolved. To bring the mirror back in line with the source",
                "      of truth run: python .governance/tools/refresh_toolchain_lock.py",
                "      --sync-mirror",
            ]

    LOCK_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")

    # Never publish output we cannot read back. A generator that silently emits
    # malformed YAML is worse than no generator, because the gate that consumes it
    # may then fail open.
    try:
        reparsed = yaml.safe_load(LOCK_PATH.read_text(encoding="utf-8"))
    except Exception as exc:
        print(f"ERROR: generated lock is not valid YAML: {exc}\n"
              f"       {LOCK_PATH} has been left in place for inspection.", file=sys.stderr)
        return 2
    if not isinstance(reparsed, dict) or "toolchain" not in reparsed:
        print(f"ERROR: generated lock failed its own structural check "
              f"(missing 'toolchain' key).", file=sys.stderr)
        return 2

    print(f"Wrote {LOCK_PATH} (re-parsed OK)")
    if drift:
        print(f"  ({len(drift)} divergence(s) recorded under known_divergences)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
