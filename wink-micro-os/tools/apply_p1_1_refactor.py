#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
apply_p1_1_refactor.py - Applies P1.1 Lint kernel refactoring to wink-tools.
"""
import re
import sys
from pathlib import Path

def main():
    repo_root = Path(__file__).resolve().parent.parent.parent
    wink_tools_candidates = [
        repo_root.parent / "wink-ai" / "packages" / "wink-tools",
        repo_root.parent / "wink-tools",
        repo_root / "wink-tools",
    ]
    
    wink_tools_root = None
    for cand in wink_tools_candidates:
        if (cand / "tools" / "lint").exists():
            wink_tools_root = cand.resolve()
            break
            
    if not wink_tools_root:
        print("Error: Could not find wink-tools root directory.", file=sys.stderr)
        return 1
        
    print(f"Found wink-tools at: {wink_tools_root}")
    lint_engine = wink_tools_root / "tools" / "lint" / "engine"
    lint_packs = wink_tools_root / "tools" / "lint" / "packs"
    lint_tests = wink_tools_root / "tools" / "tests"

    # 1. Create tools/lint/engine/base.py
    base_py_content = '''"""Lint pack protocols, context, and discovery registry."""
from __future__ import annotations

from dataclasses import dataclass
from datetime import date
from pathlib import Path
from typing import Callable, Iterable, List, Optional, Protocol, TypeVar, runtime_checkable

from tools.lint.engine.config import LintConfig
from tools.lint.engine.models import Finding


@dataclass(frozen=True)
class LintContext:
    root: Path
    cfg: LintConfig
    strict: bool = False
    paths: Optional[List[Path]] = None
    today: Optional[date] = None


@runtime_checkable
class FilePack(Protocol):
    """Per-file pack protocol. runner has already done file classification."""
    name: str
    aliases: tuple[str, ...]
    group: Optional[str]
    default_enabled: bool

    def run_on_file(
        self,
        rel: str,
        text: str,
        layer_id: Optional[str],
        kind: Optional[str],
        ctx: LintContext,
    ) -> List[Finding]: ...


@runtime_checkable
class GlobalPack(Protocol):
    """Global/whole-repo pack protocol. Traverses root or specific paths independently."""
    name: str
    aliases: tuple[str, ...]
    group: Optional[str]
    default_enabled: bool

    def run_on_root(self, ctx: LintContext) -> List[Finding]: ...


PACK_REGISTRY: dict[str, object] = {}        # alias/name -> instance
_PACK_GROUPS: dict[str, set[str]] = {}       # group -> {canonical name}

T = TypeVar("T")


def register_pack(
    name: str,
    *,
    aliases: Iterable[str] = (),
    group: Optional[str] = None,
    default_enabled: bool = False,
) -> Callable[[type[T]], type[T]]:
    """Decorator to register a FilePack or GlobalPack class."""
    def decorator(cls: type[T]) -> type[T]:
        inst = cls()
        setattr(inst, "name", name)
        setattr(inst, "aliases", tuple(aliases))
        setattr(inst, "group", group)
        setattr(inst, "default_enabled", default_enabled)

        PACK_REGISTRY[name] = inst
        for a in aliases:
            PACK_REGISTRY[a] = inst
        if group:
            _PACK_GROUPS.setdefault(group, set()).add(name)
        return cls

    return decorator


def _reset_registry() -> None:
    """Test helper: clear registry and group mapping."""
    PACK_REGISTRY.clear()
    _PACK_GROUPS.clear()


def _expand_pack_names(requested: Iterable[str]) -> set[str]:
    """Expand group names (e.g. 'layering') to all canonical pack names."""
    out: set[str] = set()
    for r in requested:
        if r in _PACK_GROUPS:
            out |= _PACK_GROUPS[r]
        else:
            out.add(r)
    return out
'''
    (lint_engine / "base.py").write_text(base_py_content, encoding="utf-8")
    print("Created base.py")

    # 2. Update tools/lint/engine/runner.py
    runner_py_content = '''"""Lint runner orchestration: discover, classify, run packs, apply allowlist."""
from __future__ import annotations

import importlib
import pkgutil
import sys
from datetime import date
from pathlib import Path
from typing import List, Optional

from tools.lint.engine.allowlist import apply_allowlist, resolve_today
from tools.lint.engine.base import (
    FilePack,
    GlobalPack,
    LintContext,
    PACK_REGISTRY,
    _expand_pack_names,
)
from tools.lint.engine.classify import classify_file
from tools.lint.engine.config import LintConfig
from tools.lint.engine.models import Finding
import tools.lint.packs as packs_pkg

_SOURCE_SUFFIXES = {".c", ".h", ".cc", ".cpp", ".hpp", ".cxx", ".hxx"}


def _auto_discover_packs() -> None:
    """Dynamically import all non-underscored modules in tools.lint.packs."""
    if PACK_REGISTRY:
        return
    for _, mod_name, _ in pkgutil.iter_modules(packs_pkg.__path__):
        if mod_name.startswith("_"):
            continue
        full_name = f"tools.lint.packs.{mod_name}"
        if full_name in sys.modules:
            importlib.reload(sys.modules[full_name])
        else:
            importlib.import_module(full_name)
    # Phase 2 extension hook: load entry_points(group="wink_tools.lint_packs") per ADR-0061


def run_lint(
    root: Path,
    cfg: LintConfig,
    packs: list[str] | None = None,
    paths: list[Path] | None = None,
    today: date | None = None,
    *,
    strict: bool = False,
) -> list[Finding]:
    """Run selected packs over ``root`` (or ``paths``) and return Findings."""
    _auto_discover_packs()
    root = root.resolve()
    effective_today = resolve_today(today)
    resolved_paths = [p.resolve() if p.is_absolute() else (root / p).resolve() for p in paths] if paths else None
    ctx = LintContext(
        root=root,
        cfg=cfg,
        strict=strict,
        paths=resolved_paths,
        today=effective_today,
    )

    if packs:
        requested = set(packs)
    else:
        requested = {
            v.name
            for v in PACK_REGISTRY.values()
            if getattr(v, "default_enabled", False) and getattr(v, "name", None)
        }

    if "all" in requested:
        active_packs = {v for k, v in PACK_REGISTRY.items() if k == getattr(v, "name", None)}
    else:
        names = _expand_pack_names(requested)
        active_packs = {PACK_REGISTRY[n] for n in names if n in PACK_REGISTRY}

    file_packs = [p for p in active_packs if isinstance(p, FilePack)]
    global_packs = [p for p in active_packs if isinstance(p, GlobalPack)]

    findings: list[Finding] = []

    if file_packs:
        files = _discover_files(root, paths)
        for rel, abs_path in files:
            classified = classify_file(rel, cfg.layers, cfg.ignore)
            if classified is None:
                continue
            layer_id, kind = classified
            text = abs_path.read_text(encoding="utf-8", errors="replace")
            for p in file_packs:
                findings.extend(p.run_on_file(rel, text, layer_id, kind, ctx))

    for p in global_packs:
        findings.extend(p.run_on_root(ctx))

    return apply_allowlist(findings, cfg, effective_today)


def _discover_files(
    root: Path, paths: list[Path] | None
) -> list[tuple[str, Path]]:
    """Return (posix-rel-path, absolute-path) pairs under root."""
    if paths:
        out: list[tuple[str, Path]] = []
        for p in paths:
            abs_p = p if p.is_absolute() else (root / p)
            abs_p = abs_p.resolve()
            if not abs_p.is_file():
                continue
            try:
                rel = abs_p.relative_to(root).as_posix()
            except ValueError:
                rel = p.as_posix().replace("\\\\", "/")
            if abs_p.suffix.lower() in _SOURCE_SUFFIXES:
                out.append((rel, abs_p))
        return out

    out = []
    for abs_p in root.rglob("*"):
        if not abs_p.is_file():
            continue
        if abs_p.suffix.lower() not in _SOURCE_SUFFIXES:
            continue
        # Skip common junk / build trees
        parts = set(abs_p.relative_to(root).parts)
        if parts & {"build", ".git", "node_modules", "__pycache__"}:
            continue
        rel = abs_p.relative_to(root).as_posix()
        out.append((rel, abs_p))
    return out
'''
    (lint_engine / "runner.py").write_text(runner_py_content, encoding="utf-8")
    print("Updated runner.py")

    # 3. Update packs/__init__.py
    (lint_packs / "__init__.py").write_text('"""Lint rule packs (plugin discovery via @register_pack)."""\n', encoding="utf-8")
    print("Updated packs/__init__.py")

    # Helper function to inject import properly after from __future__ import annotations
    def adapt_pack(file_path: Path, class_snippet: str):
        content = file_path.read_text(encoding="utf-8")
        # Remove any misplaced import at line 1
        content = re.sub(r"^from tools\.lint\.engine\.base import LintContext, register_pack\r?\n", "", content)
        
        if "@register_pack" in content:
            # Strip existing class snippet if re-running
            content = re.sub(r"\n*@register_pack\(.*?\)\s+class\s+\w+Pack:[\s\S]*$", "", content)

        import_stmt = "from tools.lint.engine.base import LintContext, register_pack\n"
        if "from __future__ import annotations" in content:
            content = content.replace("from __future__ import annotations\n", f"from __future__ import annotations\n\n{import_stmt}", 1)
        else:
            content = import_stmt + "\n" + content

        content = content.rstrip() + "\n\n\n" + class_snippet.strip() + "\n"
        file_path.write_text(content, encoding="utf-8")
        print(f"Adapted {file_path.name}")

    # 4. Adapt 10 packs
    adapt_pack(
        lint_packs / "include_graph.py",
        '''@register_pack("include_graph", group="layering", default_enabled=True)
class IncludeGraphPack:
    def run_on_file(
        self,
        rel: str,
        text: str,
        layer_id: str | None,
        kind: str | None,
        ctx: LintContext,
    ) -> list[Finding]:
        if not layer_id:
            return []
        return check_includes(rel, text, layer_id, ctx.cfg, root=ctx.root)
'''
    )

    adapt_pack(
        lint_packs / "path_name.py",
        '''@register_pack("path_name", group="layering", default_enabled=True)
class PathNamePack:
    def run_on_file(
        self,
        rel: str,
        text: str,
        layer_id: str | None,
        kind: str | None,
        ctx: LintContext,
    ) -> list[Finding]:
        if not layer_id:
            return []
        return check_path_names(rel, layer_id, ctx.cfg)
'''
    )

    adapt_pack(
        lint_packs / "regex_ban.py",
        '''@register_pack("regex_ban", group="layering", default_enabled=True)
class RegexBanPack:
    def run_on_file(
        self,
        rel: str,
        text: str,
        layer_id: str | None,
        kind: str | None,
        ctx: LintContext,
    ) -> list[Finding]:
        if not layer_id:
            return []
        return check_regex_bans(rel, text, layer_id, ctx.cfg)
'''
    )

    adapt_pack(
        lint_packs / "api_surface.py",
        '''@register_pack("api_surface", aliases=("api",), default_enabled=True)
class ApiSurfacePack:
    def run_on_file(
        self,
        rel: str,
        text: str,
        layer_id: str | None,
        kind: str | None,
        ctx: LintContext,
    ) -> list[Finding]:
        if not layer_id or not kind:
            return []
        return check_api_surface(rel, text, layer_id, kind, ctx.cfg)
'''
    )

    adapt_pack(
        lint_packs / "legacy_arduino.py",
        '''@register_pack("legacy_arduino", aliases=("arduino",))
class LegacyArduinoPack:
    def run_on_root(self, ctx: LintContext) -> list[Finding]:
        return check_arduino_isolation(ctx.root)
'''
    )

    adapt_pack(
        lint_packs / "drivers.py",
        '''@register_pack("drivers")
class DriversPack:
    def run_on_root(self, ctx: LintContext) -> list[Finding]:
        return check_drivers(ctx.root, strict=ctx.strict)
'''
    )

    adapt_pack(
        lint_packs / "user_surface.py",
        '''@register_pack("user_surface")
class UserSurfacePack:
    def run_on_root(self, ctx: LintContext) -> list[Finding]:
        return check_user_surface(ctx.root, ctx.cfg)
'''
    )

    adapt_pack(
        lint_packs / "abi.py",
        '''@register_pack("abi")
class AbiPack:
    def run_on_root(self, ctx: LintContext) -> list[Finding]:
        return check_abi(ctx.root, strict=ctx.strict)
'''
    )

    adapt_pack(
        lint_packs / "dal.py",
        '''@register_pack("dal")
class DalPack:
    def run_on_root(self, ctx: LintContext) -> list[Finding]:
        return check_dal(ctx.root, strict=ctx.strict, paths=ctx.paths)
'''
    )

    adapt_pack(
        lint_packs / "i18n.py",
        '''@register_pack("i18n")
class I18nPack:
    def run_on_root(self, ctx: LintContext) -> list[Finding]:
        return check_i18n(ctx.root, ctx.cfg, mode="report")
'''
    )

    # 5. Create tools/tests/test_lint_pack_discovery.py
    test_discovery_content = '''"""Tests for lint pack auto-discovery, registry, and group expansion."""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

SDK = Path(__file__).resolve().parents[1]
if str(SDK) not in sys.path:
    sys.path.insert(0, str(SDK))

from tools.lint.engine.base import (
    FilePack,
    GlobalPack,
    PACK_REGISTRY,
    _PACK_GROUPS,
    _expand_pack_names,
    _reset_registry,
)
from tools.lint.engine.runner import _auto_discover_packs


class TestLintPackDiscovery(unittest.TestCase):
    def setUp(self):
        _auto_discover_packs()

    def test_auto_discover_all_ten_canonical_packs(self):
        canonical = {v.name for v in PACK_REGISTRY.values() if hasattr(v, "name")}
        expected_canonical = {
            "include_graph",
            "path_name",
            "regex_ban",
            "api_surface",
            "legacy_arduino",
            "drivers",
            "user_surface",
            "abi",
            "dal",
            "i18n",
        }
        self.assertEqual(canonical, expected_canonical)

    def test_layering_group_expansion(self):
        expanded = _expand_pack_names(["layering"])
        self.assertEqual(expanded, {"include_graph", "path_name", "regex_ban"})

    def test_all_pack_aliases(self):
        self.assertIn("api", PACK_REGISTRY)
        self.assertEqual(PACK_REGISTRY["api"].name, "api_surface")
        self.assertIn("arduino", PACK_REGISTRY)
        self.assertEqual(PACK_REGISTRY["arduino"].name, "legacy_arduino")

    def test_private_dal_submodules_not_in_registry(self):
        for name in PACK_REGISTRY:
            self.assertFalse(name.startswith("dal_"), f"Private pack {name} should not be in registry")

    def test_protocol_types(self):
        self.assertIsInstance(PACK_REGISTRY["include_graph"], FilePack)
        self.assertIsInstance(PACK_REGISTRY["path_name"], FilePack)
        self.assertIsInstance(PACK_REGISTRY["regex_ban"], FilePack)
        self.assertIsInstance(PACK_REGISTRY["api_surface"], FilePack)
        self.assertIsInstance(PACK_REGISTRY["legacy_arduino"], GlobalPack)
        self.assertIsInstance(PACK_REGISTRY["drivers"], GlobalPack)
        self.assertIsInstance(PACK_REGISTRY["user_surface"], GlobalPack)
        self.assertIsInstance(PACK_REGISTRY["abi"], GlobalPack)
        self.assertIsInstance(PACK_REGISTRY["dal"], GlobalPack)
        self.assertIsInstance(PACK_REGISTRY["i18n"], GlobalPack)

    def test_reset_registry(self):
        _reset_registry()
        self.assertEqual(len(PACK_REGISTRY), 0)
        self.assertEqual(len(_PACK_GROUPS), 0)
        _auto_discover_packs()
        self.assertTrue(len(PACK_REGISTRY) > 0)


if __name__ == "__main__":
    unittest.main()
'''
    (lint_tests / "test_lint_pack_discovery.py").write_text(test_discovery_content, encoding="utf-8")
    print("Created test_lint_pack_discovery.py")

    print("\n[P1.1 SUCCESS] All files updated successfully!")
    return 0

if __name__ == "__main__":
    sys.exit(main())
