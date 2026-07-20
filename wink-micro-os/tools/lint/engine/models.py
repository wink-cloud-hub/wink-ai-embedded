"""Finding model for lint violations."""
from __future__ import annotations

import hashlib
import re
from dataclasses import dataclass


def _normalize_snippet(snippet: str | None) -> str:
    if snippet is None:
        return ""
    text = snippet.replace("\t", " ")
    return re.sub(r"\s+", " ", text).strip()


def _compute_fingerprint(
    rule_id: str, path: str, line: int | None, snippet: str | None
) -> str:
    line_part = "" if line is None else str(line)
    payload = f"{rule_id}|{path}|{line_part}|{_normalize_snippet(snippet)}"
    return hashlib.sha1(payload.encode("utf-8")).hexdigest()


@dataclass(frozen=True)
class Finding:
    rule_id: str
    severity: str
    path: str
    line: int | None
    column: int | None
    message: str
    snippet: str | None
    help: str | None
    refs: tuple[str, ...]
    allowlisted: bool = False
    rule_source: str = "sdk"
    fingerprint: str = ""

    def __post_init__(self) -> None:
        if not self.fingerprint:
            object.__setattr__(
                self,
                "fingerprint",
                _compute_fingerprint(self.rule_id, self.path, self.line, self.snippet),
            )
