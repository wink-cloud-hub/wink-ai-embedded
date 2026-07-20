"""YAML rule-pack loader and schema validation for wink lint."""
from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

import yaml

# Top-level keys allowed in SDK rule packs (schema v1).
_SDK_TOP_KEYS = frozenset(
    {
        "version",
        "id",
        "metadata",
        "extends",
        "layers",
        "include_rules",
        "api_rules",
        "path_rules",
        "ignore",
    }
)

# Top-level keys allowed in workspace / CLI overlay packs.
_OVERLAY_TOP_KEYS = frozenset(
    {
        "version",
        "id",
        "overrides",
        "disable_rules",
    }
)

_RULE_LIST_KEYS = ("include_rules", "api_rules", "path_rules")


class LintConfigError(Exception):
    """Raised when a lint config file fails schema or merge validation."""


@dataclass(frozen=True)
class PackInfo:
    """Origin metadata for a loaded rule pack."""

    source: str  # sdk | workspace | cli


@dataclass
class LintConfig:
    """Merged lint configuration from one or more YAML sources."""

    layers: dict[str, dict[str, Any]] = field(default_factory=dict)
    include_rules: list[dict[str, Any]] = field(default_factory=list)
    api_rules: list[dict[str, Any]] = field(default_factory=list)
    path_rules: list[dict[str, Any]] = field(default_factory=list)
    ignore: list[dict[str, Any]] = field(default_factory=list)
    packs: dict[str, PackInfo] = field(default_factory=dict)


def load_configs(paths: list[Path]) -> LintConfig:
    """Load and merge YAML rule packs in order (later overlays earlier)."""
    cfg = LintConfig()
    for path in paths:
        doc = _load_yaml(path)
        if _is_overlay(doc):
            _apply_overlay(cfg, doc, path, _overlay_source(doc))
        else:
            _apply_sdk_pack(cfg, doc, path, "sdk")
    return cfg


def _load_yaml(path: Path) -> dict[str, Any]:
    try:
        raw = path.read_text(encoding="utf-8")
    except OSError as exc:
        raise LintConfigError(f"cannot read config: {path}") from exc
    try:
        doc = yaml.safe_load(raw)
    except yaml.YAMLError as exc:
        raise LintConfigError(f"invalid YAML in {path}") from exc
    if not isinstance(doc, dict):
        raise LintConfigError(f"config root must be a mapping: {path}")
    return doc


def _validate_version(doc: dict[str, Any], path: Path) -> None:
    version = doc.get("version")
    if version != 1:
        raise LintConfigError(
            f"unsupported config version {version!r} in {path} (expected 1)"
        )


def _validate_top_keys(doc: dict[str, Any], allowed: frozenset[str], path: Path) -> None:
    unknown = set(doc.keys()) - allowed
    if unknown:
        keys = ", ".join(sorted(unknown))
        raise LintConfigError(f"unknown top-level key(s) in {path}: {keys}")


def _require_id(doc: dict[str, Any], path: Path) -> None:
    if not doc.get("id"):
        raise LintConfigError(f"missing required id in {path}")


def _is_overlay(doc: dict[str, Any]) -> bool:
    return "overrides" in doc or "disable_rules" in doc


def _overlay_source(doc: dict[str, Any]) -> str:
    if doc.get("id") == "workspace":
        return "workspace"
    return "cli"


def _apply_sdk_pack(
    cfg: LintConfig, doc: dict[str, Any], path: Path, source: str
) -> None:
    _validate_version(doc, path)
    _validate_top_keys(doc, _SDK_TOP_KEYS, path)
    _require_id(doc, path)

    pack_id = doc["id"]
    cfg.packs[pack_id] = PackInfo(source=source)

    if "layers" in doc:
        cfg.layers.update(doc["layers"])

    for key in _RULE_LIST_KEYS:
        rules = doc.get(key, [])
        if not isinstance(rules, list):
            raise LintConfigError(f"{key} must be a list in {path}")
        normalized = [_normalize_rule(rule, source, path) for rule in rules]
        getattr(cfg, key).extend(normalized)

    ignore = doc.get("ignore", [])
    if not isinstance(ignore, list):
        raise LintConfigError(f"ignore must be a list in {path}")
    cfg.ignore.extend(ignore)


def _normalize_rule(
    rule: dict[str, Any], source: str, path: Path
) -> dict[str, Any]:
    if not isinstance(rule, dict) or "id" not in rule:
        raise LintConfigError(f"each rule must be a mapping with an id in {path}")
    out = dict(rule)
    out.setdefault("immutable", False)
    out["rule_source"] = source
    return out


def _apply_overlay(
    cfg: LintConfig, doc: dict[str, Any], path: Path, source: str
) -> None:
    _validate_version(doc, path)
    _validate_top_keys(doc, _OVERLAY_TOP_KEYS, path)
    _require_id(doc, path)

    pack_id = doc["id"]
    cfg.packs[pack_id] = PackInfo(source=source)

    for rule_id in doc.get("disable_rules", []):
        _disable_rule(cfg, rule_id, source)

    overrides = doc.get("overrides", {})
    if not isinstance(overrides, dict):
        raise LintConfigError(f"overrides must be a mapping in {path}")
    for rule_id, override in overrides.items():
        _apply_rule_override(cfg, rule_id, override, source)


def _find_rule(cfg: LintConfig, rule_id: str) -> dict[str, Any] | None:
    for key in _RULE_LIST_KEYS:
        for rule in getattr(cfg, key):
            if rule.get("id") == rule_id:
                return rule
    return None


def _disable_rule(cfg: LintConfig, rule_id: str, source: str) -> None:
    rule = _find_rule(cfg, rule_id)
    if rule is None:
        raise LintConfigError(f"cannot disable unknown rule {rule_id!r}")
    if rule.get("immutable"):
        raise LintConfigError(
            f"cannot disable immutable rule {rule_id!r} via {source} overlay"
        )
    rule["disabled"] = True


def _apply_rule_override(
    cfg: LintConfig, rule_id: str, override: dict[str, Any], source: str
) -> None:
    if not isinstance(override, dict):
        raise LintConfigError(f"override for {rule_id!r} must be a mapping")

    forbidden = set(override.keys()) - {"add_allow_paths"}
    if forbidden:
        keys = ", ".join(sorted(forbidden))
        raise LintConfigError(
            f"overlay for {rule_id!r} contains forbidden key(s): {keys}"
        )

    rule = _find_rule(cfg, rule_id)
    if rule is None:
        raise LintConfigError(f"cannot override unknown rule {rule_id!r}")

    add_allow = override.get("add_allow_paths", [])
    if not isinstance(add_allow, list):
        raise LintConfigError(f"add_allow_paths for {rule_id!r} must be a list")

    if rule.get("immutable"):
        # Immutable rules: only allow_paths append is permitted (§3.6).
        allow_paths = list(rule.get("allow_paths", []))
        allow_paths.extend(add_allow)
        rule["allow_paths"] = allow_paths
        return

    # Non-immutable: still only add_allow_paths in v1 overlay schema.
    allow_paths = list(rule.get("allow_paths", []))
    allow_paths.extend(add_allow)
    rule["allow_paths"] = allow_paths
