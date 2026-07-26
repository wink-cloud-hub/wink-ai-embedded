"""tools.cli.commands.doctor — DoctorCommand for probing toolchain capabilities."""
from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path
from typing import Optional

from tools.cli.base import CommandBase
from tools.cli.context import AppContext

_DOCTOR_PROBE_ORDER = [
    "python",
    "jinja2",
    "gcc",
    "cmake",
    "make",
    "emsdk",
    "idf",
    "node",
]

_COL_ITEM = 10
_COL_LOCATION = 44
_COL_STATUS = 22

_VT_ENABLED = sys.platform != "win32" or os.environ.get("FORCE_COLOR") == "1"
_ANSI_BOLD = "\x1b[1m" if _VT_ENABLED else ""
_ANSI_GREEN = "\x1b[32m" if _VT_ENABLED else ""
_ANSI_RED = "\x1b[31m" if _VT_ENABLED else ""
_ANSI_YELLOW = "\x1b[33m" if _VT_ENABLED else ""
_ANSI_RESET = "\x1b[0m" if _VT_ENABLED else ""


def _print_doctor_row(item: str, location: str, status: str) -> None:
    print(f"  {item:<{_COL_ITEM}} {location:<{_COL_LOCATION}} {status}")


def _doctor_required_and_optional() -> tuple[set[str], set[str]]:
    from tools.toolchain.profiles import OPTIONAL_CAPS, PROFILES, expand_profile
    required: set[str] = set()
    optional: set[str] = set()
    for prof_name in PROFILES:
        for cap in expand_profile(prof_name):
            required.add(cap)
    for caps in OPTIONAL_CAPS.values():
        for cap in caps:
            optional.add(cap)
    optional.difference_update(required)
    return required, optional


def _format_location(cap_id: str, result) -> str:
    if result.found and result.path:
        p_str = str(result.path)
        if len(p_str) > _COL_LOCATION:
            return f"...{p_str[-(_COL_LOCATION - 3):]}"
        return p_str
    if result.reason:
        r_str = result.reason
        if len(r_str) > _COL_LOCATION:
            return f"...{r_str[-(_COL_LOCATION - 3):]}"
        return r_str
    return "—"


def _doctor_status_cell(cap_id: str, result, required: set[str], optional: set[str]) -> tuple[str, bool]:
    if result.found:
        ver = f" ({result.version})" if result.version else ""
        return f"{_ANSI_GREEN}✓ OK{ver}{_ANSI_RESET}", False
    if cap_id in required:
        return f"{_ANSI_RED}✗ Missing (required){_ANSI_RESET}", True
    return f"{_ANSI_YELLOW}✗ Missing (optional){_ANSI_RESET}", False


def _probe_all_for_setup(ctx: AppContext):
    """Probe every registered cap and return a list of (id, DetectResult, source_tag)."""
    from tools.toolchain import providers as providers_mod
    from tools.toolchain.resolve import ResolveContext, candidate_paths
    from tools.toolchain.types import DetectResult

    probe_ctx = ResolveContext.snapshot(ctx.workspace_root)
    rows: list[tuple[str, DetectResult, str]] = []
    for cap_id, provider in providers_mod.REGISTRY.items():
        cands = candidate_paths(cap_id, probe_ctx)
        try:
            result = provider.detect(probe_ctx)
        except Exception as exc:
            result = DetectResult(
                found=False, path=None, version=None,
                reason=f"provider raised: {exc}", source=None,
            )
        source_tag = ""
        if result.found:
            if cands:
                source_tag = cands[0][0]
            elif result.source:
                source_tag = result.source
            else:
                source_tag = "PATH"
        else:
            source_tag = "—"
        rows.append((cap_id, result, source_tag))
    return rows, probe_ctx


class DoctorCommand(CommandBase):
    name = "doctor"
    help = "Probe every registered toolchain capability"

    def register_args(self, parser: argparse.ArgumentParser) -> None:
        pass

    def run(self, ctx: AppContext, args: argparse.Namespace) -> Optional[int]:
        if getattr(args, "skip_toolchain_check", False):
            print(
                "[wink] Note: --skip-toolchain-check is ignored for `doctor` "
                "(probing is the whole point of doctor).",
                file=sys.stderr,
            )

        from tools.toolchain import providers as providers_mod
        from tools.toolchain.resolve import ResolveContext
        from tools.toolchain.types import DetectResult

        probe_ctx = ResolveContext.snapshot(ctx.workspace_root)
        required, optional = _doctor_required_and_optional()

        hr = "─" * (_COL_ITEM + _COL_LOCATION + _COL_STATUS + 2)
        print(f"{_ANSI_BOLD}Wink toolchain doctor{_ANSI_RESET}")
        print(hr)
        _print_doctor_row("Item", "Location", "Status")
        print(hr)

        seen: set[str] = set()
        ordered_ids: list[str] = []
        for cap_id in _DOCTOR_PROBE_ORDER:
            if cap_id in providers_mod.REGISTRY and cap_id not in seen:
                ordered_ids.append(cap_id)
                seen.add(cap_id)
        for cap_id in providers_mod.REGISTRY:
            if cap_id not in seen:
                ordered_ids.append(cap_id)
                seen.add(cap_id)

        results: dict[str, DetectResult] = {}
        blocking_missing: list[str] = []
        optional_missing: list[str] = []

        for cap_id in ordered_ids:
            provider = providers_mod.REGISTRY[cap_id]
            try:
                result = provider.detect(probe_ctx)
            except Exception as exc:
                result = DetectResult(
                    found=False, path=None, version=None,
                    reason=f"provider raised: {exc}", source=None,
                )
            results[cap_id] = result
            location = _format_location(cap_id, result)
            status, is_blocking = _doctor_status_cell(cap_id, result, required, optional)
            _print_doctor_row(cap_id, location, status)
            if not result.found:
                if is_blocking:
                    blocking_missing.append(cap_id)
                else:
                    optional_missing.append(cap_id)

        print(hr)
        total = len(ordered_ids)
        installed = sum(1 for r in results.values() if r.found)
        missing_all = blocking_missing + optional_missing
        missing_count = len(missing_all)

        print(f"{_ANSI_BOLD}Summary:{_ANSI_RESET} {total} checked, {installed} installed, {missing_count} missing")

        if missing_all:
            print()
            idx = 1
            for cap_id in blocking_missing:
                provider = providers_mod.REGISTRY[cap_id]
                try:
                    hint_text = provider.hint(probe_ctx)
                except Exception:
                    hint_text = "(no hint available)"
                print(f"  {idx}. {_ANSI_RED}{cap_id}{_ANSI_RESET} — {hint_text}")
                idx += 1
            for cap_id in optional_missing:
                provider = providers_mod.REGISTRY[cap_id]
                try:
                    hint_text = provider.hint(probe_ctx)
                except Exception:
                    hint_text = "(no hint available)"
                print(f"  {idx}. {_ANSI_YELLOW}{cap_id}{_ANSI_RESET} (optional) — {hint_text}")
                idx += 1

            if "idf" in blocking_missing:
                print()
                print("Note: ESP-IDF is never auto-installed by Wink. Install via Espressif IDF Manager (EIM); see preinstall.md §3.")

        return 1 if blocking_missing else 0
