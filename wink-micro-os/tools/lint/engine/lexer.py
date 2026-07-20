"""Lexical helpers for wink lint: comment stripping and continuations."""
from __future__ import annotations


def strip_comments_and_strings(text: str) -> str:
    """Strip C comments while preserving string/char literals and newlines.

    Despite the historical name, v1 include extraction (tech-design §5.1 and
    Task 3 lexer tests) requires keeping string literal text so that
    ``#include "header.h"`` remains matchable. Comments are replaced with
    spaces; newlines inside block comments are preserved for line numbering.

    String/char literals are still tracked so that ``//`` or ``/*`` inside
    them do not start comments.
    """
    if text.startswith("\ufeff"):
        text = text[1:]

    out: list[str] = []
    i = 0
    n = len(text)
    state = "code"  # code | line_comment | block_comment | dquote | squote

    while i < n:
        ch = text[i]
        nxt = text[i + 1] if i + 1 < n else ""

        if state == "code":
            if ch == "/" and nxt == "/":
                out.append("  ")
                i += 2
                state = "line_comment"
                continue
            if ch == "/" and nxt == "*":
                out.append("  ")
                i += 2
                state = "block_comment"
                continue
            if ch == '"':
                out.append(ch)
                i += 1
                state = "dquote"
                continue
            if ch == "'":
                out.append(ch)
                i += 1
                state = "squote"
                continue
            out.append(ch)
            i += 1
            continue

        if state == "line_comment":
            if ch == "\n":
                out.append("\n")
                state = "code"
            else:
                out.append(" ")
            i += 1
            continue

        if state == "block_comment":
            if ch == "*" and nxt == "/":
                out.append("  ")
                i += 2
                state = "code"
                continue
            out.append("\n" if ch == "\n" else " ")
            i += 1
            continue

        if state == "dquote":
            out.append(ch)
            if ch == "\\" and i + 1 < n:
                out.append(text[i + 1])
                i += 2
                continue
            if ch == '"':
                state = "code"
            i += 1
            continue

        if state == "squote":
            out.append(ch)
            if ch == "\\" and i + 1 < n:
                out.append(text[i + 1])
                i += 2
                continue
            if ch == "'":
                state = "code"
            i += 1
            continue

    return "".join(out)


def strip_strings(text: str) -> str:
    """Replace string/char literal contents with spaces (keep quotes/newlines).

    Used by api_surface packs so literals like ``"malloc"`` do not match.
    Assumes comments were already stripped, or operate on raw text carefully.
    """
    out: list[str] = []
    i = 0
    n = len(text)
    state = "code"

    while i < n:
        ch = text[i]
        if state == "code":
            if ch == '"':
                out.append(ch)
                i += 1
                state = "dquote"
                continue
            if ch == "'":
                out.append(ch)
                i += 1
                state = "squote"
                continue
            out.append(ch)
            i += 1
            continue

        if state == "dquote":
            if ch == "\\" and i + 1 < n:
                out.append("  ")
                i += 2
                continue
            if ch == '"':
                out.append(ch)
                i += 1
                state = "code"
                continue
            out.append("\n" if ch == "\n" else " ")
            i += 1
            continue

        if state == "squote":
            if ch == "\\" and i + 1 < n:
                out.append("  ")
                i += 2
                continue
            if ch == "'":
                out.append(ch)
                i += 1
                state = "code"
                continue
            out.append("\n" if ch == "\n" else " ")
            i += 1
            continue

    return "".join(out)


def join_continuations(text: str) -> str:
    """Merge backslash-newline continuations into a single logical stream."""
    out: list[str] = []
    i = 0
    n = len(text)
    while i < n:
        if text[i] == "\\" and i + 1 < n and text[i + 1] == "\n":
            out.append(" ")
            i += 2
            continue
        if (
            text[i] == "\\"
            and i + 2 < n
            and text[i + 1] == "\r"
            and text[i + 2] == "\n"
        ):
            out.append(" ")
            i += 3
            continue
        out.append(text[i])
        i += 1
    return "".join(out)
