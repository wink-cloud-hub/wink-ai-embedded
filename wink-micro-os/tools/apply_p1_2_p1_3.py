#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
apply_p1_2_p1_3.py - Applies P1.2 (ISR safety) and P1.3 (Wasm parity) packs, rules, and tests to wink-tools.
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
    lint_rules = wink_tools_root / "tools" / "lint" / "rules"
    lint_tests = wink_tools_root / "tools" / "tests"
    lint_cli_file = wink_tools_root / "tools" / "lint" / "cli.py"

    # =========================================================================
    # 0. Update tools/lint/engine/base.py (_reset_registry resets _DISCOVERED)
    # =========================================================================
    base_py = lint_engine / "base.py"
    base_content = base_py.read_text(encoding="utf-8")
    if "tools.lint.engine.runner" not in base_content:
        base_content = base_content.replace(
            "def _reset_registry() -> None:\n    \"\"\"Test helper: clear registry and group mapping.\"\"\"\n    PACK_REGISTRY.clear()\n    _PACK_GROUPS.clear()",
            "def _reset_registry() -> None:\n    \"\"\"Test helper: clear registry and group mapping.\"\"\"\n    try:\n        import tools.lint.engine.runner as runner_mod\n        runner_mod._DISCOVERED = False\n    except Exception:\n        pass\n    PACK_REGISTRY.clear()\n    _PACK_GROUPS.clear()",
        )
        base_py.write_text(base_content, encoding="utf-8")
        print("Updated tools/lint/engine/base.py")

    # =========================================================================
    # 0.1 Update tools/lint/engine/runner.py (_DISCOVERED flag)
    # =========================================================================
    runner_py = lint_engine / "runner.py"
    runner_content = runner_py.read_text(encoding="utf-8")
    runner_py_new = '''"""Lint runner orchestration: discover, classify, run packs, apply allowlist."""
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
_DISCOVERED: bool = False


def _auto_discover_packs(force: bool = False) -> None:
    """Dynamically import all non-underscored modules in tools.lint.packs."""
    global _DISCOVERED
    if _DISCOVERED and not force:
        return
    _DISCOVERED = True
    for _, mod_name, _ in pkgutil.iter_modules(packs_pkg.__path__):
        if mod_name.startswith("_"):
            continue
        full_name = f"tools.lint.packs.{mod_name}"
        if full_name in sys.modules:
            importlib.reload(sys.modules[full_name])
        else:
            importlib.import_module(full_name)


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
    runner_py.write_text(runner_py_new, encoding="utf-8")
    print("Updated tools/lint/engine/runner.py with _DISCOVERED flag")

    # =========================================================================
    # 1. Update tools/lint/engine/config.py to add isr_rules & wasm_rules
    # =========================================================================
    config_py = lint_engine / "config.py"
    cfg_content = config_py.read_text(encoding="utf-8")
    
    if '"isr_rules",' not in cfg_content:
        cfg_content = cfg_content.replace(
            '"user_surface_rules",\n        "ignore",',
            '"user_surface_rules",\n        "isr_rules",\n        "wasm_rules",\n        "ignore",',
        )
        cfg_content = cfg_content.replace(
            '_RULE_LIST_KEYS = (\n    "include_rules",\n    "api_rules",\n    "path_rules",\n    "user_surface_rules",\n)',
            '_RULE_LIST_KEYS = (\n    "include_rules",\n    "api_rules",\n    "path_rules",\n    "user_surface_rules",\n    "isr_rules",\n    "wasm_rules",\n)',
        )
        cfg_content = cfg_content.replace(
            'user_surface_rules: list[dict[str, Any]] = field(default_factory=list)\n    ignore:',
            'user_surface_rules: list[dict[str, Any]] = field(default_factory=list)\n    isr_rules: list[dict[str, Any]] = field(default_factory=list)\n    wasm_rules: list[dict[str, Any]] = field(default_factory=list)\n    ignore:',
        )
        config_py.write_text(cfg_content, encoding="utf-8")
        print("Updated tools/lint/engine/config.py")

    # =========================================================================
    # 2. Update tools/lint/engine/allowlist.py to index isr_rules & wasm_rules
    # =========================================================================
    allowlist_py = lint_engine / "allowlist.py"
    al_content = allowlist_py.read_text(encoding="utf-8")
    if '"isr_rules"' not in al_content:
        al_content = al_content.replace(
            '("include_rules", "api_rules", "path_rules", "user_surface_rules")',
            '("include_rules", "api_rules", "path_rules", "user_surface_rules", "isr_rules", "wasm_rules")',
        )
        allowlist_py.write_text(al_content, encoding="utf-8")
        print("Updated tools/lint/engine/allowlist.py")

    # =========================================================================
    # 3. Update tools/lint/cli.py to support explain for isr_rules & wasm_rules
    # =========================================================================
    cli_content = lint_cli_file.read_text(encoding="utf-8")
    if '"isr_rules"' not in cli_content:
        cli_content = cli_content.replace(
            'for key in ("include_rules", "api_rules", "path_rules", "user_surface_rules"):',
            'for key in ("include_rules", "api_rules", "path_rules", "user_surface_rules", "isr_rules", "wasm_rules"):',
        )
        lint_cli_file.write_text(cli_content, encoding="utf-8")
        print("Updated tools/lint/cli.py")

    # =========================================================================
    # 4. Create tools/lint/rules/isr.yaml
    # =========================================================================
    isr_yaml_content = '''version: 1
id: isr_rules
metadata:
  description: Interrupt Service Routine (ISR) and IRAM execution safety rules (ADR-0047).
  source: sdk
isr_rules:
  - id: ISR-NO-FLASH-CALL
    severity: error
    immutable: true
    refs:
      - ADR-0047
    target_roots:
      - pal
      - targets
      - dal
      - runtime
    deny_calls:
      - malloc
      - calloc
      - realloc
      - free
      - printf
      - vsnprintf
      - sprintf
      - snprintf
      - LOG_E
      - LOG_W
      - LOG_I
      - LOG_D
      - pal_os_sleep_ms
      - vTaskDelay
      - xQueueSend
      - xQueueReceive
      - xSemaphoreTake
      - xSemaphoreGive
      - abort
      - assert
    match_macros:
      - PAL_ISR
      - IRAM_ATTR
      - PAL_IRAM_TEXT
      - PAL_DEFINE_ISR
    except_files:
      - "pal_log_*.c"
    context:
      strip_comments: true
      strip_strings: true
    message: "ISR / IRAM functions must not invoke heap allocation, logging, blocking APIs, or non-FromISR FreeRTOS calls."
    help: "Remove heap allocations and blocking delays from ISR. Use ring buffers or FreeRTOS *FromISR equivalents."

  - id: ISR-IRAM-FLAG-REQUIRED
    severity: error
    immutable: true
    refs:
      - ADR-0047
    target_roots:
      - targets/esp32
    deny_alloc_without_flag:
      func: esp_intr_alloc
      required_flag: ESP_INTR_FLAG_IRAM
    message: "ESP32 hardware interrupt allocation must specify ESP_INTR_FLAG_IRAM."
    help: "Add ESP_INTR_FLAG_IRAM to esp_intr_alloc() flags to prevent cache-disabled crashes."
'''
    (lint_rules / "isr.yaml").write_text(isr_yaml_content, encoding="utf-8")
    print("Created tools/lint/rules/isr.yaml")

    # =========================================================================
    # 5. Create tools/lint/rules/wasm.yaml
    # =========================================================================
    wasm_yaml_content = '''version: 1
id: wasm_parity
metadata:
  description: WebAssembly simulation ABI hash invariance and stub symbol coverage guards.
  source: sdk
wasm_rules:
  - id: WASM-ABI-HASH-MATCH
    severity: error
    immutable: true
    refs:
      - ADR-0003
      - ADR-0012
    message: "PAL_WASM_ABI_HASH in pal_wasm_degradation.c must match normalized SHA256[0:8] hash of wasm_bridge.h."
    help: "Recompute PAL_WASM_ABI_HASH from wasm_bridge.h extern declarations when modifying ABI contract."

  - id: WASM-STUB-COVERAGE
    severity: error
    immutable: true
    refs:
      - ADR-0003
    message: "All js_pal_* extern functions declared in wasm_bridge.h must have matching implementations or stubs in JS."
    help: "Add missing js_pal_* symbol to wink_sim_stub.js or wink_sim_js.js."
'''
    (lint_rules / "wasm.yaml").write_text(wasm_yaml_content, encoding="utf-8")
    print("Created tools/lint/rules/wasm.yaml")

    # =========================================================================
    # 6. Create tools/lint/packs/isr_safety.py
    # =========================================================================
    isr_safety_py = '''"""isr_safety pack: enforces zero-logging, zero-heap-allocation, and IRAM flags in ISRs (ADR-0047)."""
from __future__ import annotations

import fnmatch
import re
from pathlib import Path
from typing import Any, List

from tools.lint.engine.base import GlobalPack, LintContext, register_pack
from tools.lint.engine.config import LintConfig
from tools.lint.engine.lexer import strip_comments_and_strings
from tools.lint.engine.models import Finding

_DEFAULT_FORBIDDEN_CALLS = [
    r"\\bmalloc\\s*\\(",
    r"\\bcalloc\\s*\\(",
    r"\\brealloc\\s*\\(",
    r"\\bfree\\s*\\(",
    r"\\bprintf\\s*\\(",
    r"\\bvsnprintf\\s*\\(",
    r"\\bsprintf\\s*\\(",
    r"\\bsnprintf\\s*\\(",
    r"\\bLOG_E\\s*\\(",
    r"\\bLOG_W\\s*\\(",
    r"\\bLOG_I\\s*\\(",
    r"\\bLOG_D\\s*\\(",
    r"\\bpal_os_sleep_ms\\s*\\(",
    r"\\bvTaskDelay\\s*\\(",
    r"\\bxQueueSend\\s*\\(",
    r"\\bxQueueReceive\\s*\\(",
    r"\\bxSemaphoreTake\\s*\\(",
    r"\\bxSemaphoreGive\\s*\\(",
    r"\\babort\\s*\\(",
    r"\\bassert\\s*\\(",
]

# Function definitions matching ISR attributes or PAL_DEFINE_ISR macro
_ISR_PATTERN = re.compile(
    r"(?:(?:static\\s+)?(?:void|bool|uint32_t|int|wink_status_t)\\s+(?:PAL_ISR|IRAM_ATTR|PAL_IRAM_TEXT)\\s+(\\w+)\\s*\\([^)]*\\)\\s*\\{([^}]+)\\}|"
    r"PAL_DEFINE_ISR\\s*\\(\\s*(\\w+)[^)]*\\)\\s*\\{([^}]+)\\})",
    re.MULTILINE | re.DOTALL,
)


def check_isr_safety(root: Path, cfg: LintConfig) -> List[Finding]:
    """Scan root for ISR zero-allocation, zero-logging, and IRAM flags violations."""
    findings: List[Finding] = []
    
    # 1. Evaluate ISR-NO-FLASH-CALL
    search_dirs = [root / "pal", root / "targets", root / "dal", root / "runtime"]
    if (root / "wink-micro-os").is_dir():
        search_dirs.extend([
            root / "wink-micro-os" / "pal",
            root / "wink-micro-os" / "targets",
            root / "wink-micro-os" / "dal",
            root / "wink-micro-os" / "runtime",
        ])

    for d in search_dirs:
        if not d.exists():
            continue
        for c_file in d.rglob("*.c"):
            if "pal_log_" in c_file.name:
                continue
            rel = _get_rel_path(c_file, root)
            findings.extend(_scan_isr_file(c_file, rel))

    # 2. Evaluate ISR-IRAM-FLAG-REQUIRED for ESP32 target
    esp32_dirs = [root / "targets" / "esp32", root / "wink-micro-os" / "targets" / "esp32"]
    for esp32_dir in esp32_dirs:
        if not esp32_dir.exists():
            continue
        for c_file in esp32_dir.glob("*.c"):
            rel = _get_rel_path(c_file, root)
            findings.extend(_check_iram_flags(c_file, rel))

    return findings


def _get_rel_path(p: Path, root: Path) -> str:
    try:
        return p.resolve().relative_to(root.resolve()).as_posix()
    except ValueError:
        return p.as_posix().replace("\\\\", "/")


def _scan_isr_file(file_path: Path, rel_path: str) -> List[Finding]:
    findings: List[Finding] = []
    try:
        content = file_path.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return findings

    for match in _ISR_PATTERN.finditer(content):
        fn_name = match.group(1) or match.group(3)
        fn_body = match.group(2) or match.group(4)
        clean_body = strip_comments_and_strings(fn_body)
        match_start = match.start(2) if match.group(2) else match.start(4)

        for pat in _DEFAULT_FORBIDDEN_CALLS:
            m = re.search(pat, clean_body)
            if m:
                offset = match_start + m.start()
                line_no = content.count("\\n", 0, offset) + 1
                clean_name = pat.replace("\\\\b", "").replace("\\\\s*\\\\(", "")
                snippet = content.splitlines()[line_no - 1] if line_no <= len(content.splitlines()) else ""
                findings.append(
                    Finding(
                        rule_id="ISR-NO-FLASH-CALL",
                        severity="error",
                        path=rel_path,
                        line=line_no,
                        column=None,
                        message=f"ISR function '{fn_name}' calls forbidden flash/blocking function '{clean_name}()'",
                        snippet=snippet.strip(),
                        help="Remove heap allocation, logging, and blocking APIs from ISR. Use FromISR variants.",
                        refs=("ADR-0047",),
                        allowlisted=False,
                        rule_source="sdk",
                    )
                )
    return findings


def _check_iram_flags(file_path: Path, rel_path: str) -> List[Finding]:
    findings: List[Finding] = []
    try:
        content = file_path.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return findings

    for match in re.finditer(r"esp_intr_alloc\\s*\\([^,]+,\\s*([^,]+),", content):
        flags_arg = match.group(1).strip()
        if "ESP_INTR_FLAG_IRAM" not in flags_arg:
            if not re.search(r"ESP_INTR_FLAG_IRAM", content):
                line_no = content.count("\\n", 0, match.start()) + 1
                snippet = content.splitlines()[line_no - 1] if line_no <= len(content.splitlines()) else ""
                findings.append(
                    Finding(
                        rule_id="ISR-IRAM-FLAG-REQUIRED",
                        severity="error",
                        path=rel_path,
                        line=line_no,
                        column=None,
                        message=f"esp_intr_alloc() called without ESP_INTR_FLAG_IRAM: flags '{flags_arg}'",
                        snippet=snippet.strip(),
                        help="Ensure ESP_INTR_FLAG_IRAM is passed to esp_intr_alloc to prevent flash cache crash.",
                        refs=("ADR-0047",),
                        allowlisted=False,
                        rule_source="sdk",
                    )
                )
    return findings


@register_pack("isr_safety", aliases=("isr",))
class ISRSafetyPack:
    def run_on_root(self, ctx: LintContext) -> List[Finding]:
        return check_isr_safety(ctx.root, ctx.cfg)
'''
    (lint_packs / "isr_safety.py").write_text(isr_safety_py, encoding="utf-8")
    print("Created tools/lint/packs/isr_safety.py")

    # =========================================================================
    # 7. Create tools/lint/packs/wasm_parity.py
    # =========================================================================
    wasm_parity_py = '''"""wasm_parity pack: validates PAL_WASM_ABI_HASH invariance and JS stub symbol coverage (ADR-0003, ADR-0012)."""
from __future__ import annotations

import hashlib
import re
from pathlib import Path
from typing import List, Set

from tools.lint.engine.base import GlobalPack, LintContext, register_pack
from tools.lint.engine.config import LintConfig
from tools.lint.engine.models import Finding


def extract_normalized_decls(header_path: Path) -> List[str]:
    content = header_path.read_text(encoding="utf-8")
    content = re.sub(r"/\\*.*?\\*/", "", content, flags=re.DOTALL)
    content = re.sub(r"//.*", "", content)
    raw_decls = re.findall(r"extern\\s+([^;]+);", content)
    normalized = []
    for d in raw_decls:
        norm = " ".join(d.split())
        if norm and not norm.startswith('"C"'):
            normalized.append(f"extern {norm};")
    return sorted(set(normalized))


def check_wasm_parity(root: Path, cfg: LintConfig) -> List[Finding]:
    """Validate WASM ABI hash matching and JS stub symbol coverage."""
    findings: List[Finding] = []
    
    # Locate wasm files
    bridge_h = None
    degradation_c = None
    stub_js = None
    sim_js = None

    candidates = [
        root / "targets" / "wasm",
        root / "wink-micro-os" / "targets" / "wasm",
    ]
    for cand in candidates:
        if (cand / "wasm_bridge.h").exists():
            bridge_h = cand / "wasm_bridge.h"
            degradation_c = cand / "pal_wasm_degradation.c"
            stub_js = cand / "wink_sim_stub.js"
            sim_js = cand / "wink_sim_js.js"
            break

    if not bridge_h or not bridge_h.exists():
        return findings

    rel_bridge = _get_rel_path(bridge_h, root)
    rel_deg = _get_rel_path(degradation_c, root) if degradation_c and degradation_c.exists() else rel_bridge

    # 1. Rule: WASM-ABI-HASH-MATCH
    if degradation_c and degradation_c.exists():
        decls = extract_normalized_decls(bridge_h)
        data = "\\n".join(decls).encode("utf-8")
        computed_sha = hashlib.sha256(data).hexdigest()
        computed_u32 = int(computed_sha[:8], 16)

        deg_content = degradation_c.read_text(encoding="utf-8")
        hash_match = re.search(r"#define\\s+PAL_WASM_ABI_HASH\\s+(0x[0-9a-fA-F]+u?)", deg_content)
        if not hash_match:
            findings.append(
                Finding(
                    rule_id="WASM-ABI-HASH-MATCH",
                    severity="error",
                    path=rel_deg,
                    line=1,
                    column=None,
                    message="Missing PAL_WASM_ABI_HASH definition in pal_wasm_degradation.c",
                    snippet=None,
                    help="Define PAL_WASM_ABI_HASH with the matching 32-bit SHA256[0:8] hash.",
                    refs=("ADR-0003", "ADR-0012"),
                    allowlisted=False,
                    rule_source="sdk",
                )
            )
        else:
            current_hash_str = hash_match.group(1).rstrip("uU")
            current_u32 = int(current_hash_str, 16)
            if computed_u32 != current_u32:
                findings.append(
                    Finding(
                        rule_id="WASM-ABI-HASH-MATCH",
                        severity="error",
                        path=rel_deg,
                        line=deg_content.count("\\n", 0, hash_match.start()) + 1,
                        column=None,
                        message=f"PAL_WASM_ABI_HASH mismatch: expected 0x{computed_u32:08X}u (SHA256: {computed_sha[:16]}...) but found 0x{current_u32:08X}u",
                        snippet=hash_match.group(0),
                        help="Update PAL_WASM_ABI_HASH in pal_wasm_degradation.c to match wasm_bridge.h contract.",
                        refs=("ADR-0003", "ADR-0012"),
                        allowlisted=False,
                        rule_source="sdk",
                    )
                )

    # 2. Rule: WASM-STUB-COVERAGE
    content_h = bridge_h.read_text(encoding="utf-8")
    symbols: Set[str] = set(re.findall(r"extern\\s+[\\w\\*\\s]+\\s+(js_pal_\\w+)\\s*\\([^)]*\\)\\s*;", content_h))

    stub_content = ""
    if stub_js and stub_js.exists():
        stub_content += stub_js.read_text(encoding="utf-8")
    if sim_js and sim_js.exists():
        stub_content += sim_js.read_text(encoding="utf-8")

    missing = [sym for sym in sorted(symbols) if sym not in stub_content]
    if missing:
        findings.append(
            Finding(
                rule_id="WASM-STUB-COVERAGE",
                severity="error",
                path=rel_bridge,
                line=1,
                column=None,
                message=f"{len(missing)} js_pal_* extern functions not declared in JS stubs: {', '.join(missing)}",
                snippet=None,
                help="Implement or stub all js_pal_* functions in wink_sim_stub.js or wink_sim_js.js.",
                refs=("ADR-0003",),
                allowlisted=False,
                rule_source="sdk",
            )
        )

    return findings


def _get_rel_path(p: Path, root: Path) -> str:
    try:
        return p.resolve().relative_to(root.resolve()).as_posix()
    except ValueError:
        return p.as_posix().replace("\\\\", "/")


@register_pack("wasm_parity", aliases=("wasm",))
class WasmParityPack:
    def run_on_root(self, ctx: LintContext) -> List[Finding]:
        return check_wasm_parity(ctx.root, ctx.cfg)
'''
    (lint_packs / "wasm_parity.py").write_text(wasm_parity_py, encoding="utf-8")
    print("Created tools/lint/packs/wasm_parity.py")

    # =========================================================================
    # 8. Update tools/tests/test_lint_pack_discovery.py to cover all 12 packs
    # =========================================================================
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
        _auto_discover_packs(force=True)

    def test_auto_discover_all_twelve_canonical_packs(self):
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
            "isr_safety",
            "wasm_parity",
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
        self.assertIn("isr", PACK_REGISTRY)
        self.assertEqual(PACK_REGISTRY["isr"].name, "isr_safety")
        self.assertIn("wasm", PACK_REGISTRY)
        self.assertEqual(PACK_REGISTRY["wasm"].name, "wasm_parity")

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
        self.assertIsInstance(PACK_REGISTRY["isr_safety"], GlobalPack)
        self.assertIsInstance(PACK_REGISTRY["wasm_parity"], GlobalPack)

    def test_reset_registry(self):
        _reset_registry()
        self.assertEqual(len(PACK_REGISTRY), 0)
        self.assertEqual(len(_PACK_GROUPS), 0)
        _auto_discover_packs(force=True)
        self.assertTrue(len(PACK_REGISTRY) > 0)


if __name__ == "__main__":
    unittest.main()
'''
    (lint_tests / "test_lint_pack_discovery.py").write_text(test_discovery_content, encoding="utf-8")
    print("Updated test_lint_pack_discovery.py")

    # =========================================================================
    # 9. Create tools/tests/test_lint_isr_safety.py
    # =========================================================================
    test_isr_py = '''"""Unit tests for isr_safety lint pack."""
from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

SDK = Path(__file__).resolve().parents[1]
if str(SDK) not in sys.path:
    sys.path.insert(0, str(SDK))

from tools.lint.engine.config import LintConfig
from tools.lint.packs.isr_safety import check_isr_safety


class TestLintISRSafety(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.cfg = LintConfig()

    def tearDown(self):
        self.tmp.cleanup()

    def test_clean_isr_passes(self):
        pal_dir = self.root / "pal" / "src"
        pal_dir.mkdir(parents=True, exist_ok=True)
        (pal_dir / "clean_isr.c").write_text("""
void PAL_ISR on_timer_tick(void *arg) {
    uint32_t val = 123;
    (void)val;
}
""", encoding="utf-8")
        findings = check_isr_safety(self.root, self.cfg)
        self.assertEqual(findings, [])

    def test_malloc_in_pal_isr_fails(self):
        pal_dir = self.root / "pal" / "src"
        pal_dir.mkdir(parents=True, exist_ok=True)
        (pal_dir / "bad_isr.c").write_text("""
void PAL_ISR on_timer_tick(void *arg) {
    void *p = malloc(10);
    (void)p;
}
""", encoding="utf-8")
        findings = check_isr_safety(self.root, self.cfg)
        self.assertEqual(len(findings), 1)
        self.assertEqual(findings[0].rule_id, "ISR-NO-FLASH-CALL")
        self.assertIn("malloc", findings[0].message)

    def test_pal_define_isr_macro_body_fails(self):
        pal_dir = self.root / "pal" / "src"
        pal_dir.mkdir(parents=True, exist_ok=True)
        (pal_dir / "macro_isr.c").write_text("""
PAL_DEFINE_ISR(my_gpio_isr, void*, arg) {
    printf("hello from isr\\n");
}
""", encoding="utf-8")
        findings = check_isr_safety(self.root, self.cfg)
        self.assertEqual(len(findings), 1)
        self.assertEqual(findings[0].rule_id, "ISR-NO-FLASH-CALL")
        self.assertIn("printf", findings[0].message)

    def test_comments_and_strings_ignored(self):
        pal_dir = self.root / "pal" / "src"
        pal_dir.mkdir(parents=True, exist_ok=True)
        (pal_dir / "comment_isr.c").write_text("""
void PAL_ISR safe_isr(void *arg) {
    // Note: Do NOT call malloc() here!
    /* printf("debug"); */
    const char *msg = "malloc in string";
    (void)msg;
}
""", encoding="utf-8")
        findings = check_isr_safety(self.root, self.cfg)
        self.assertEqual(findings, [])

    def test_pal_log_file_exempted(self):
        pal_dir = self.root / "pal" / "src"
        pal_dir.mkdir(parents=True, exist_ok=True)
        (pal_dir / "pal_log_esp32.c").write_text("""
void PAL_ISR log_flush_isr(void *arg) {
    printf("emergency log");
}
""", encoding="utf-8")
        findings = check_isr_safety(self.root, self.cfg)
        self.assertEqual(findings, [])

    def test_freertos_from_isr_allowed_but_blocking_denied(self):
        pal_dir = self.root / "pal" / "src"
        pal_dir.mkdir(parents=True, exist_ok=True)
        (pal_dir / "queue_isr.c").write_text("""
void PAL_ISR q_isr(void *arg) {
    xQueueSendFromISR(q, item, NULL);  // Allowed
    xQueueSend(q, item, 100);          // Denied
}
""", encoding="utf-8")
        findings = check_isr_safety(self.root, self.cfg)
        self.assertEqual(len(findings), 1)
        self.assertEqual(findings[0].rule_id, "ISR-NO-FLASH-CALL")
        self.assertIn("xQueueSend", findings[0].message)

    def test_esp_intr_alloc_missing_iram_flag(self):
        esp_dir = self.root / "targets" / "esp32"
        esp_dir.mkdir(parents=True, exist_ok=True)
        (esp_dir / "intr.c").write_text("""
void setup_intr(void) {
    esp_intr_alloc(ETS_GPIO_INTR_SOURCE, 0, my_isr, NULL, NULL);
}
""", encoding="utf-8")
        findings = check_isr_safety(self.root, self.cfg)
        self.assertEqual(len(findings), 1)
        self.assertEqual(findings[0].rule_id, "ISR-IRAM-FLAG-REQUIRED")


if __name__ == "__main__":
    unittest.main()
'''
    (lint_tests / "test_lint_isr_safety.py").write_text(test_isr_py, encoding="utf-8")
    print("Created tools/tests/test_lint_isr_safety.py")

    # =========================================================================
    # 10. Create tools/tests/test_lint_wasm_parity.py
    # =========================================================================
    test_wasm_py = '''"""Unit tests for wasm_parity lint pack."""
from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

SDK = Path(__file__).resolve().parents[1]
if str(SDK) not in sys.path:
    sys.path.insert(0, str(SDK))

from tools.lint.engine.config import LintConfig
from tools.lint.packs.wasm_parity import check_wasm_parity


class TestLintWasmParity(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.cfg = LintConfig()
        self.wasm_dir = self.root / "targets" / "wasm"
        self.wasm_dir.mkdir(parents=True, exist_ok=True)

    def tearDown(self):
        self.tmp.cleanup()

    def test_valid_wasm_parity_passes(self):
        (self.wasm_dir / "wasm_bridge.h").write_text("""
#ifndef WASM_BRIDGE_H
#define WASM_BRIDGE_H
extern void js_pal_gpio_write(int pin, int level);
#endif
""", encoding="utf-8")
        (self.wasm_dir / "pal_wasm_degradation.c").write_text("""
#include <stdint.h>
#define PAL_WASM_ABI_HASH 0xACE6B77Du
uint32_t pal_wasm_get_abi_hash(void) {
    return PAL_WASM_ABI_HASH;
}
""", encoding="utf-8")
        (self.wasm_dir / "wink_sim_stub.js").write_text("""
function js_pal_gpio_write(pin, level) {}
""", encoding="utf-8")
        findings = check_wasm_parity(self.root, self.cfg)
        self.assertEqual(findings, [])

    def test_hash_mismatch_fails(self):
        (self.wasm_dir / "wasm_bridge.h").write_text("""
extern void js_pal_gpio_write(int pin, int level);
extern void js_pal_gpio_read(int pin);
""", encoding="utf-8")
        (self.wasm_dir / "pal_wasm_degradation.c").write_text("""
#define PAL_WASM_ABI_HASH 0x12345678u
""", encoding="utf-8")
        (self.wasm_dir / "wink_sim_stub.js").write_text("""
function js_pal_gpio_write() {}
function js_pal_gpio_read() {}
""", encoding="utf-8")
        findings = check_wasm_parity(self.root, self.cfg)
        self.assertEqual(len(findings), 1)
        self.assertEqual(findings[0].rule_id, "WASM-ABI-HASH-MATCH")

    def test_missing_stub_coverage_fails(self):
        (self.wasm_dir / "wasm_bridge.h").write_text("""
extern void js_pal_gpio_write(int pin, int level);
extern void js_pal_unimplemented_symbol(void);
""", encoding="utf-8")
        (self.wasm_dir / "wink_sim_stub.js").write_text("""
function js_pal_gpio_write() {}
""", encoding="utf-8")
        findings = check_wasm_parity(self.root, self.cfg)
        stub_findings = [f for f in findings if f.rule_id == "WASM-STUB-COVERAGE"]
        self.assertEqual(len(stub_findings), 1)
        self.assertIn("js_pal_unimplemented_symbol", stub_findings[0].message)


if __name__ == "__main__":
    unittest.main()
'''
    (lint_tests / "test_lint_wasm_parity.py").write_text(test_wasm_py, encoding="utf-8")
    print("Created tools/tests/test_lint_wasm_parity.py")

    print("\n[P1.2 & P1.3 SUCCESS] All schema, rules, packs, and tests created successfully!")
    return 0

if __name__ == "__main__":
    sys.exit(main())
