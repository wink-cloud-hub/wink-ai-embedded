#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""MCS-51 chip-manifest loader (Stage6 S6-2, PLAN-20260911-MCS51-S6, CPL-16).

Single access point for `tools/manifests/chips/*.yaml`: the gate / cleanup /
lint scripts read chip facts from here and nowhere else. Missing or malformed
manifests fail fast — the stage6 contract is "no manifest, no verdict".

The files use a deliberately tiny YAML subset (nested maps, scalar values,
inline lists/maps, scalar block lists) which is parsed with PyYAML when
available and by a bundled parser otherwise, so the build-time cleanup pass
does not depend on PyYAML being installed.

Public API:
    load_chip_manifests()      -> {family: manifest}
    manifest_for_family(name)  -> manifest (KeyError-style ManifestError)
    manifest_for_mcu(mcu)      -> manifest (family key or alias)
    family_index()             -> {mcu_or_family: family}
    forbid_patterns()          -> [(family, regex_string), ...]
    cleanup_header_patterns()  -> [regex_string, ...]
    allow_header_patterns()    -> [regex_string, ...]
    standard_header_regexes()  -> [regex_string, ...]  (reg51/52/regx52)
"""
from __future__ import annotations

import json
import re
from pathlib import Path

HERE = Path(__file__).resolve().parent
CHIPS_DIR = HERE / "manifests" / "chips"
SCHEMA_PATH = CHIPS_DIR / "schema.json"

TOP_LEVEL = {"family", "aliases", "headers", "memory", "sdcc_gate"}
HEADER_FIELDS = {
    "vendor_include_dir", "allow_regex", "forbid_in_core_regex",
    "cleanup_alias_regex",
}
MEMORY_FIELDS = {"xram_bytes", "xsfr_window"}
SDCC_FIELDS = {
    "mem_limits", "vendor_device_header", "vendor_stddriver_dir",
    "stddriver_link",
}
MEM_LIMIT_FIELDS = {"code_max", "iram_max", "xdata_max"}

# Standard Intel 8051/8052 Keil device headers: generic dialect, always
# normalized by the cleanup pass regardless of family manifest.
STANDARD_HEADER_REGEXES = [r"regx?5[12]"]


class ManifestError(RuntimeError):
    """Raised on missing/malformed chip manifests (fail-fast, no default)."""


# ---------------------------------------------------------------------------
# YAML subset parsing (PyYAML preferred).
# ---------------------------------------------------------------------------

def _split_top_level(text: str) -> list[str]:
    parts, depth, quote, buf = [], 0, "", []
    i = 0
    while i < len(text):
        c = text[i]
        if quote:
            buf.append(c)
            if c == "\\" and i + 1 < len(text) and quote == '"':
                buf.append(text[i + 1])
                i += 2
                continue
            if c == quote:
                quote = ""
            i += 1
            continue
        if c in "\"'":
            quote = c
            buf.append(c)
        elif c in "[{":
            depth += 1
            buf.append(c)
        elif c in "]}":
            depth -= 1
            buf.append(c)
        elif c == "," and depth == 0:
            parts.append("".join(buf).strip())
            buf = []
        else:
            buf.append(c)
        i += 1
    if buf:
        parts.append("".join(buf).strip())
    return [p for p in parts if p]


def _unquote(text: str) -> str:
    if len(text) >= 2 and text[0] == text[-1] and text[0] in "\"'":
        body = text[1:-1]
        if text[0] == '"':
            body = body.replace('\\"', '"')
            # Only the escapes our manifests use.
            body = body.replace("\\\\", "\\")
            body = body.replace("\\n", "\n")
        return body
    return text


def _parse_scalar(text: str):
    text = text.strip()
    if text == "":
        return None
    lowered = text.lower()
    if lowered in ("null", "~"):
        return None
    if lowered == "true":
        return True
    if lowered == "false":
        return False
    if text[0] in "\"'" and text[-1] == text[0]:
        return _unquote(text)
    if text.startswith("[") and text.endswith("]"):
        inner = text[1:-1].strip()
        return [_parse_scalar(p) for p in _split_top_level(inner)] if inner else []
    if text.startswith("{") and text.endswith("}"):
        inner = text[1:-1].strip()
        out = {}
        if inner:
            for part in _split_top_level(inner):
                key, _, value = part.partition(":")
                if not _:
                    raise ManifestError(f"bad inline map entry: {part!r}")
                out[key.strip()] = _parse_scalar(value)
        return out
    if re.fullmatch(r"0[xX][0-9A-Fa-f]+", text):
        return int(text, 16)
    if re.fullmatch(r"-?\d+", text):
        return int(text)
    if re.fullmatch(r"-?\d+\.\d+", text):
        return float(text)
    return text


def _parse_block(lines: list[tuple[int, str]], idx: int, indent: int):
    if lines[idx][1].startswith("- "):
        items = []
        while (idx < len(lines) and lines[idx][0] == indent
               and lines[idx][1].startswith("- ")):
            items.append(_parse_scalar(lines[idx][1][2:].strip()))
            idx += 1
        return items, idx
    out: dict = {}
    while idx < len(lines) and lines[idx][0] == indent:
        key, sep, rest = lines[idx][1].partition(":")
        if not sep:
            raise ManifestError(f"expected 'key:' near {lines[idx][1]!r}")
        key = key.strip()
        rest = rest.strip()
        idx += 1
        if rest:
            out[key] = _parse_scalar(rest)
            continue
        if idx < len(lines) and lines[idx][0] > indent:
            child, idx = _parse_block(lines, idx, lines[idx][0])
            out[key] = child
        else:
            out[key] = None
    return out, idx


def _strip_inline_comment(line: str) -> str:
    quote = ""
    i = 0
    while i < len(line):
        c = line[i]
        if quote:
            if c == "\\" and quote == '"':
                i += 2
                continue
            if c == quote:
                quote = ""
        elif c in "\"'":
            quote = c
        elif c == "#" and (i == 0 or line[i - 1] in " \t"):
            return line[:i]
        i += 1
    return line


def _mini_yaml(text: str):
    lines: list[tuple[int, str]] = []
    for raw in text.splitlines():
        stripped = _strip_inline_comment(raw).strip()
        if not stripped or stripped.startswith("#"):
            continue
        indent = len(raw) - len(raw.lstrip(" "))
        lines.append((indent, stripped))
    if not lines:
        return {}
    value, idx = _parse_block(lines, 0, lines[0][0])
    if idx != len(lines):
        raise ManifestError(f"trailing unparsed manifest lines: {lines[idx:]}")
    return value


def _load_yaml(path: Path):
    text = path.read_text(encoding="utf-8")
    try:
        import yaml  # type: ignore
    except ImportError:
        return _mini_yaml(text)
    try:
        return yaml.safe_load(text)
    except Exception as exc:  # pragma: no cover - malformed file path
        raise ManifestError(f"{path}: YAML parse error: {exc}") from exc


# ---------------------------------------------------------------------------
# Validation.
# ---------------------------------------------------------------------------

def _require(cond: bool, path: Path, detail: str) -> None:
    if not cond:
        raise ManifestError(f"{path}: {detail}")


def _validate(m, path: Path) -> None:
    _require(isinstance(m, dict), path, "manifest root must be a mapping")
    extra = set(m) - TOP_LEVEL
    missing = TOP_LEVEL - set(m)
    _require(not extra, path, f"unknown top-level field(s): {sorted(extra)}")
    _require(not missing, path, f"missing top-level field(s): {sorted(missing)}")
    _require(m["family"] == path.stem, path,
             f"family '{m['family']}' != filename stem '{path.stem}'")
    _require(isinstance(m["aliases"], list)
             and all(isinstance(a, str) for a in m["aliases"]),
             path, "aliases must be a list of strings")
    headers = m["headers"]
    _require(isinstance(headers, dict), path, "headers must be a mapping")
    _require(set(headers) == HEADER_FIELDS, path,
             f"headers fields must be exactly {sorted(HEADER_FIELDS)}")
    _require(isinstance(headers["vendor_include_dir"], str), path,
             "headers.vendor_include_dir must be a string")
    for key in ("allow_regex", "forbid_in_core_regex", "cleanup_alias_regex"):
        _require(isinstance(headers[key], list)
                 and all(isinstance(r, str) for r in headers[key]),
                 path, f"headers.{key} must be a list of regex strings")
        for rx in headers[key]:
            try:
                re.compile(rx)
            except re.error as exc:
                raise ManifestError(f"{path}: bad regex {rx!r}: {exc}") from exc
    memory = m["memory"]
    _require(isinstance(memory, dict) and set(memory) == MEMORY_FIELDS, path,
             f"memory fields must be exactly {sorted(MEMORY_FIELDS)}")
    xram = memory["xram_bytes"]
    _require(xram is None or (isinstance(xram, int) and xram > 0), path,
             "memory.xram_bytes must be a positive int or null")
    window = memory["xsfr_window"]
    _require(window is None
             or (isinstance(window, list) and len(window) == 2
                 and all(isinstance(v, int) for v in window)),
             path, "memory.xsfr_window must be [base, size] or null")
    sdcc = m["sdcc_gate"]
    _require(isinstance(sdcc, dict) and set(sdcc) == SDCC_FIELDS, path,
             f"sdcc_gate fields must be exactly {sorted(SDCC_FIELDS)}")
    limits = sdcc["mem_limits"]
    _require(isinstance(limits, dict) and set(limits) == MEM_LIMIT_FIELDS,
             path, f"sdcc_gate.mem_limits fields must be exactly "
                   f"{sorted(MEM_LIMIT_FIELDS)}")
    for key in ("code_max", "iram_max"):
        _require(isinstance(limits[key], int) and limits[key] > 0, path,
                 f"sdcc_gate.mem_limits.{key} must be a positive int")
    _require(limits["xdata_max"] is None
             or (isinstance(limits["xdata_max"], int)
                 and limits["xdata_max"] > 0),
             path, "sdcc_gate.mem_limits.xdata_max must be a positive int or null")
    for key in ("vendor_device_header", "vendor_stddriver_dir"):
        _require(sdcc[key] is None or isinstance(sdcc[key], str), path,
                 f"sdcc_gate.{key} must be a string or null")
    _require(isinstance(sdcc["stddriver_link"], list)
             and all(isinstance(v, str) for v in sdcc["stddriver_link"]),
             path, "sdcc_gate.stddriver_link must be a list of strings")


# ---------------------------------------------------------------------------
# Public API (cached).
# ---------------------------------------------------------------------------

_CACHE: dict | None = None


def load_chip_manifests() -> dict[str, dict]:
    """Return {family: manifest}; raises ManifestError when none exist."""
    global _CACHE
    if _CACHE is not None:
        return _CACHE
    if not CHIPS_DIR.is_dir():
        raise ManifestError(f"chip manifest directory missing: {CHIPS_DIR}")
    manifests: dict[str, dict] = {}
    paths = sorted(CHIPS_DIR.glob("*.yaml")) + sorted(CHIPS_DIR.glob("*.yml"))
    for path in paths:
        manifest = _load_yaml(path)
        _validate(manifest, path)
        family = manifest["family"]
        if family in manifests:
            raise ManifestError(f"duplicate family '{family}' in {path}")
        manifests[family] = manifest
    if not manifests:
        raise ManifestError(f"no chip manifests found in {CHIPS_DIR}")
    _CACHE = manifests
    return manifests


def family_index() -> dict[str, str]:
    """{mcu or alias or family -> family key}; alias collisions fail fast."""
    index: dict[str, str] = {}
    for family, manifest in load_chip_manifests().items():
        for name in [family] + list(manifest["aliases"]):
            name = name.lower()
            if name in index and index[name] != family:
                raise ManifestError(
                    f"mcu alias '{name}' claimed by {index[name]} and {family}")
            index[name] = family
    return index


def manifest_for_mcu(mcu: str) -> dict:
    """Manifest for a wink-app.json mcu value (family key or alias)."""
    key = (mcu or "").strip().lower()
    if not key:
        raise ManifestError("empty mcu: cannot select a chip manifest")
    index = family_index()
    if key not in index:
        raise ManifestError(
            f"unknown mcu '{mcu}' (no chip manifest family/alias matches; "
            f"known: {', '.join(sorted(index))})")
    return load_chip_manifests()[index[key]]


def manifest_for_family(family: str) -> dict:
    manifests = load_chip_manifests()
    if family not in manifests:
        raise ManifestError(
            f"no manifest for family '{family}' "
            f"(known: {', '.join(sorted(manifests))})")
    return manifests[family]


def forbid_patterns() -> list[tuple[str, str]]:
    """[(family, regex), ...] residue patterns for generic core files."""
    out: list[tuple[str, str]] = []
    for family, manifest in load_chip_manifests().items():
        for rx in manifest["headers"]["forbid_in_core_regex"]:
            out.append((family, rx))
    return out


def cleanup_header_patterns() -> list[str]:
    """Vendor device-header alternation for the cleanup pass (plus standard)."""
    out = list(STANDARD_HEADER_REGEXES)
    for manifest in load_chip_manifests().values():
        out.extend(manifest["headers"]["cleanup_alias_regex"])
    return out


def allow_header_patterns() -> list[str]:
    """Header-name regexes that may exist in vendor/chip scope (lint facts)."""
    out = list(STANDARD_HEADER_REGEXES)
    for manifest in load_chip_manifests().values():
        out.extend(manifest["headers"]["allow_regex"])
    return out


def header_hint_patterns() -> list[str]:
    """Full header-name regexes for lint content gates (standard + manifest).

    Callers compile the joined alternation (usually with re.IGNORECASE):
    a file including any of these is MCS-51 source.
    """
    out = [r"regx?5[12]\.h", r"wink_mcu\.h", r"absacc\.h"]
    for manifest in load_chip_manifests().values():
        out.extend(manifest["headers"]["allow_regex"])
    return out


def schema_version() -> str:
    """Schema $id when present (diagnostics only)."""
    try:
        data = json.loads(SCHEMA_PATH.read_text(encoding="utf-8"))
        return str(data.get("$id", "unknown"))
    except OSError:
        return "missing"
