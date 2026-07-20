"""Extract #include directives with stable 1-based line numbers."""
from __future__ import annotations

import re

from tools.lint.engine.lexer import strip_comments_and_strings

_INCLUDE_RE = re.compile(
    r'^\s*#\s*include\s+(?:<([^>]+)>|"([^"]+)")'
)


def extract_includes(text: str) -> list[tuple[int, str, str]]:
    """Return (line, header, form) where form is 'quote' or 'angle'.

    Pipeline (tech-design §5.1):
    1. strip UTF-8 BOM
    2. strip comments and strings (preserve newlines)
    3. join backslash continuations into logical lines
    4. match #include (whitespace allowed between # and include)
    """
    if text.startswith("\ufeff"):
        text = text[1:]

    cleaned = strip_comments_and_strings(text)
    results: list[tuple[int, str, str]] = []
    for line_no, logical in _logical_lines_with_numbers(cleaned):
        m = _INCLUDE_RE.match(logical)
        if not m:
            continue
        if m.group(1) is not None:
            results.append((line_no, m.group(1), "angle"))
        else:
            results.append((line_no, m.group(2), "quote"))
    return results


def _logical_lines_with_numbers(text: str) -> list[tuple[int, str]]:
    """Join \\\n continuations; each logical line keeps its first physical line no."""
    physical = text.split("\n")
    out: list[tuple[int, str]] = []
    i = 0
    while i < len(physical):
        start = i + 1
        chunks: list[str] = []
        while True:
            line = physical[i]
            if line.endswith("\\") and i + 1 < len(physical):
                chunks.append(line[:-1])
                i += 1
                continue
            chunks.append(line)
            break
        # Mirror join_continuations: backslash-newline becomes a single space.
        logical = " ".join(chunks) if len(chunks) > 1 else chunks[0]
        out.append((start, logical))
        i += 1
    return out
