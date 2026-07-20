"""Layer path classification for lint engine."""
from __future__ import annotations

import re


def _glob_to_regex(pattern: str) -> re.Pattern[str]:
    pattern = pattern.replace("\\", "/")
    parts: list[str] = []
    i = 0
    while i < len(pattern):
        if pattern[i : i + 2] == "**":
            parts.append(".*")
            i += 2
            if i < len(pattern) and pattern[i] == "/":
                i += 1
        elif pattern[i] == "*":
            parts.append("[^/]*")
            i += 1
        elif pattern[i] == "?":
            parts.append("[^/]")
            i += 1
        else:
            parts.append(re.escape(pattern[i]))
            i += 1
    return re.compile("^" + "".join(parts) + "$")


def _match_glob(path: str, pattern: str) -> bool:
    normalized = path.replace("\\", "/")
    return _glob_to_regex(pattern).match(normalized) is not None


def _ignored_for_classify(rel_path: str, ignore: list) -> bool:
    normalized = rel_path.replace("\\", "/")
    for entry in ignore:
        scope = entry.get("scope", ["classify", "rules"])
        if "classify" not in scope:
            continue
        if _match_glob(normalized, entry["path"]):
            return True
    return False


def classify_file(
    rel_path: str, layers: dict, ignore: list
) -> tuple[str, str] | None:
    normalized = rel_path.replace("\\", "/")
    if _ignored_for_classify(normalized, ignore):
        return None

    best_layer: str | None = None
    best_kind: str | None = None
    best_len = -1

    for layer_id, layer in layers.items():
        for root in layer.get("roots", []):
            root_norm = root.replace("\\", "/").rstrip("/")
            if normalized == root_norm or normalized.startswith(root_norm + "/"):
                if len(root_norm) > best_len:
                    best_len = len(root_norm)
                    best_layer = layer_id
                    best_kind = layer.get("kind", "")

    if best_layer is None or best_kind is None:
        return None
    return best_layer, best_kind
