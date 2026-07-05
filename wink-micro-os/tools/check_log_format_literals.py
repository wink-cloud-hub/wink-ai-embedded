#!/usr/bin/env python3
"""Static checker: log macros must use compile-time string literal for `fmt`.

Enforced by PLAN-20260705-LOGGING-HARDENING §4.5 and is a prerequisite for
future Tokenized/Dictionary logging (Pigweed-style hash compression), where
every format string must be known at compile time so the runtime can ship
only a 32-bit hash instead of the full text.

What is checked:
  For every call to
      LOG_E / LOG_W / LOG_I / LOG_D
      pal_log_e / pal_log_w / pal_log_i / pal_log_d
  the format-string argument must be a compile-time string literal. Adjacent
  string literal concatenation is allowed (e.g. "prefix " "suffix" and macro
  expansion that yields a string literal, as long as the final argument after
  preprocessing in this file's view is a "..." token or series thereof).

What is NOT checked (conservative false negatives — by design):
  - We do NOT run the C preprocessor. We use a tokenizer that strips comments
    and stringifies string literals, but does not expand macros. If a
    project-local macro hides a non-literal, we won't catch it. This is a
    deliberate trade-off to keep the tool dependency-free (no libclang).
  - Call sites that split the fmt argument across complex macro wrappers are
    skipped silently rather than false-positive (we only inspect the first
    token that looks like an expression; if we can't prove it's a literal we
    report it).

Exit codes:
  0  all call sites OK (or no call sites found)
  1  one or more non-literal fmt arguments detected
  2  script usage / internal error

Usage:
  python check_log_format_literals.py [--root WINK_MICRO_OS_ROOT] [--verbose]
"""
import argparse
import os
import re
import sys
from typing import Iterable, List, Optional, Tuple

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_ROOT = os.path.dirname(SCRIPT_DIR)  # wink-micro-os/

# Directories NOT to scan (third-party, build artifacts, vendor code, docs).
EXCLUDE_DIRS = {
    ".git", "build", "build-*", "out", "cmake-build-*",
    "test/unity",  # vendored Unity framework
    "docs",
}
# Specific files to skip (e.g. the header where LOG_* macros are defined — its
# body contains the macro definitions themselves which use fmt as a parameter
# name, not a literal; the checker would otherwise false-positive there).
EXCLUDE_FILES = {
    "pal/include/pal_log.h",
}
# File extensions we consider C sources / headers.
SOURCE_EXTS = (".c", ".h", ".cpp", ".hpp")

# Macro/function names whose fmt arg must be a string literal.
#
# (name, arg_index_of_fmt):
#   LOG_E etc.             -> first arg is fmt (no tag param)
#   pal_log_e etc.         -> second arg is fmt (tag, fmt, ...)
CHECKS = [
    ("LOG_E",       0),
    ("LOG_W",       0),
    ("LOG_I",       0),
    ("LOG_D",       0),
    ("pal_log_e",   1),
    ("pal_log_w",   1),
    ("pal_log_i",   1),
    ("pal_log_d",   1),
]


def strip_comments_and_strings(src: str) -> str:
    """Return src with C comments replaced by spaces, but string literals kept.

    This is a lightweight lexer sufficient to find top-level `NAME(` call sites
    without being fooled by appearances inside // /* ... */ comments.
    String contents are kept verbatim so we can later re-parse them; we only
    blank out comment bodies.
    """
    out = []
    i = 0
    n = len(src)
    while i < n:
        c = src[i]
        # line comment
        if c == '/' and i + 1 < n and src[i+1] == '/':
            while i < n and src[i] != '\n':
                out.append(' ')
                i += 1
            continue
        # block comment
        if c == '/' and i + 1 < n and src[i+1] == '*':
            out.append(' ')
            out.append(' ')
            i += 2
            while i < n - 1 and not (src[i] == '*' and src[i+1] == '/'):
                out.append('\n' if src[i] == '\n' else ' ')
                i += 1
            out.append(' ')
            out.append(' ')
            if i < n - 1:
                i += 2
            continue
        # string/char literals: copy verbatim (with escape awareness)
        if c == '"':
            out.append(c)
            i += 1
            while i < n:
                ch = src[i]
                out.append(ch)
                if ch == '\\' and i + 1 < n:
                    out.append(src[i+1])
                    i += 2
                    continue
                i += 1
                if ch == '"':
                    break
            continue
        if c == "'":
            out.append(c)
            i += 1
            while i < n:
                ch = src[i]
                out.append(ch)
                if ch == '\\' and i + 1 < n:
                    out.append(src[i+1])
                    i += 2
                    continue
                i += 1
                if ch == "'":
                    break
            continue
        out.append(c)
        i += 1
    return ''.join(out)


def find_call_sites(src: str) -> Iterable[Tuple[str, int, int]]:
    """Yield (name, paren_open_idx, arg_list_end_paren_idx) for every call-like site.

    A "call-like" site is an identifier immediately followed by '(' (with
    optional whitespace). We match naively — later logic validates if the
    name is one we care about.
    """
    pattern = re.compile(r'\b([A-Za-z_][A-Za-z0-9_]*)\s*\(')
    for m in pattern.finditer(src):
        name = m.group(1)
        open_idx = m.end() - 1  # index of '('
        # find matching close paren, respecting string nesting
        depth = 1
        i = open_idx + 1
        in_str = False
        in_chr = False
        while i < len(src) and depth > 0:
            ch = src[i]
            if in_str:
                if ch == '\\' and i + 1 < len(src):
                    i += 2
                    continue
                if ch == '"':
                    in_str = False
            elif in_chr:
                if ch == '\\' and i + 1 < len(src):
                    i += 2
                    continue
                if ch == "'":
                    in_chr = False
            else:
                if ch == '"':
                    in_str = True
                elif ch == "'":
                    in_chr = True
                elif ch == '(':
                    depth += 1
                elif ch == ')':
                    depth -= 1
                    if depth == 0:
                        yield (name, open_idx, i)
                        break
            i += 1


def split_top_level_args(arg_text: str) -> List[str]:
    """Split a function argument string by top-level commas, respecting
    nested parens and string literals."""
    args = []
    depth = 0
    cur = []
    in_str = False
    in_chr = False
    i = 0
    while i < len(arg_text):
        ch = arg_text[i]
        if in_str:
            cur.append(ch)
            if ch == '\\' and i + 1 < len(arg_text):
                cur.append(arg_text[i+1])
                i += 2
                continue
            if ch == '"':
                in_str = False
        elif in_chr:
            cur.append(ch)
            if ch == '\\' and i + 1 < len(arg_text):
                cur.append(arg_text[i+1])
                i += 2
                continue
            if ch == "'":
                in_chr = False
        else:
            if ch == '"':
                in_str = True
                cur.append(ch)
            elif ch == "'":
                in_chr = True
                cur.append(ch)
            elif ch in '([{':
                depth += 1
                cur.append(ch)
            elif ch in ')]}':
                depth -= 1
                cur.append(ch)
            elif ch == ',' and depth == 0:
                args.append(''.join(cur).strip())
                cur = []
                i += 1
                continue
            else:
                cur.append(ch)
        i += 1
    last = ''.join(cur).strip()
    if last or args:
        args.append(last)
    return args


def _consume_string_literal(s: str, i: int) -> int:
    """Advance i past a single string literal starting at s[i]=='"'.
    Returns index of character after the closing quote.
    Does NOT handle encoding prefix — caller must skip that."""
    assert s[i] == '"'
    i += 1
    while i < len(s):
        ch = s[i]
        if ch == '\\' and i + 1 < len(s):
            i += 2
            continue
        i += 1
        if ch == '"':
            return i
    return i


def _consume_identifier(s: str, i: int) -> int:
    """Advance past a C identifier starting at s[i], return new index."""
    if i >= len(s) or not (s[i].isalpha() or s[i] == '_'):
        return i
    while i < len(s) and (s[i].isalnum() or s[i] == '_'):
        i += 1
    return i


def is_string_literal_expr(expr: str) -> bool:
    """Return True iff expr is (or starts with) one or more adjacent string
    literals (optionally parenthesized or with leading whitespace / a cast
    we can't see because we don't run cpp).

    Adjacent "foo" "bar" concatenation is explicitly allowed (C standard).
    We also accept prefixes like L"", u8"", etc. for future-proofing.

    A single identifier token is allowed between adjacent literals to support
    the common "prefix" MACRO_ARG "suffix" macro-substitution pattern
    (e.g. ASSERT_EQ("SAMPLE FAIL: " msg " (expected=%d, got=%d)")), where
    the macro will substitute a string literal for `msg` at preprocessing time.
    We can't verify that at static-check time without running cpp, so we accept
    the pattern as "compile-time literal after macro expansion".
    """
    s = expr.strip()
    if not s:
        return False
    # peel wrapping parens (one level)
    while s.startswith('(') and s.endswith(')'):
        s = s[1:-1].strip()
    i = 0
    saw_literal = False
    while i < len(s):
        # skip whitespace
        while i < len(s) and s[i].isspace():
            i += 1
        if i >= len(s):
            break
        # optional encoding prefix (L, u8, u, U)
        if s[i] in 'LUu':
            if s[i] == 'u' and i + 1 < len(s) and s[i+1] == '8':
                i += 2
            else:
                i += 1
        if i < len(s) and s[i] == '"':
            i = _consume_string_literal(s, i)
            saw_literal = True
            continue
        # allow a single identifier between literals (macro parameter that
        # will be substituted with a literal at preprocessing time)
        if saw_literal:
            j = _consume_identifier(s, i)
            if j > i:
                i = j
                continue
        # anything else: stop
        break
    while i < len(s) and s[i].isspace():
        i += 1
    return saw_literal and i == len(s)


def scan_file(path: str, rel: str, verbose: bool) -> List[str]:
    """Return list of violation descriptions for this file (empty if clean)."""
    try:
        with open(path, 'r', encoding='utf-8', errors='replace') as f:
            raw = f.read()
    except OSError as e:
        return [f"{rel}:0: cannot read: {e}"]
    src = strip_comments_and_strings(raw)
    violations: List[str] = []
    for name, open_i, close_i in find_call_sites(src):
        needle = next((entry for entry in CHECKS if entry[0] == name), None)
        if needle is None:
            continue
        _n, fmt_idx = needle
        arg_text = src[open_i + 1:close_i]
        args = split_top_level_args(arg_text)
        if len(args) <= fmt_idx:
            # macro/template call with zero args — malformed; skip silently
            # (real compiler will catch it; not our job)
            continue
        fmt_arg = args[fmt_idx]
        if not is_string_literal_expr(fmt_arg):
            # compute line number
            line = src.count('\n', 0, open_i) + 1
            snippet = fmt_arg.strip()
            if len(snippet) > 60:
                snippet = snippet[:57] + "..."
            violations.append(
                f"{rel}:{line}: {name}(...) fmt arg must be a string literal, "
                f"got: {snippet}"
            )
    return violations


def _is_excluded_dir(d: str) -> bool:
    if d in EXCLUDE_DIRS:
        return True
    for pat in EXCLUDE_DIRS:
        if pat.endswith('*') and d.startswith(pat[:-1]):
            return True
    return False


def iter_sources(root: str) -> Iterable[Tuple[str, str]]:
    for dirpath, dirnames, filenames in os.walk(root):
        # prune excluded dirs (in-place for os.walk)
        dirnames[:] = [d for d in dirnames if not _is_excluded_dir(d)]
        for fn in filenames:
            if fn.endswith(SOURCE_EXTS):
                full = os.path.join(dirpath, fn)
                rel = os.path.relpath(full, root).replace('\\', '/')
                if rel in EXCLUDE_FILES:
                    continue
                yield full, rel


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", default=DEFAULT_ROOT,
                        help="wink-micro-os source root (default: script's grandparent)")
    parser.add_argument("--verbose", action="store_true",
                        help="print per-file progress")
    args = parser.parse_args(argv)

    root = os.path.abspath(args.root)
    if not os.path.isdir(root):
        print(f"error: root not found: {root}", file=sys.stderr)
        return 2

    all_violations: List[str] = []
    files_scanned = 0
    for full, rel in iter_sources(root):
        files_scanned += 1
        if args.verbose:
            print(f"  scan {rel}", file=sys.stderr)
        all_violations.extend(scan_file(full, rel, args.verbose))

    if all_violations:
        print(f"check_log_format_literals: {len(all_violations)} violation(s) "
              f"in {files_scanned} files:", file=sys.stderr)
        for v in all_violations:
            print(f"  {v}", file=sys.stderr)
        return 1
    if args.verbose or True:
        print(f"check_log_format_literals: OK ({files_scanned} files scanned)",
              file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
