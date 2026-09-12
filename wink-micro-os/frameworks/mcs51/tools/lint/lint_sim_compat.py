# SPDX-License-Identifier: Apache-2.0
"""MCS-51 Wasm simulation-compat pack (external lint pack, group ``mcs51_all``).

Single :class:`GlobalPack` with two passes inside ``run_on_root`` (the runner
executes all FilePacks before GlobalPacks, so a File/Global split could never
share a table):

* Pass 1: build the ISR-written symbol table across the target scope.
* Pass 2: judge polling loops (B-01) and bit-bang timing (B-03).

Rules::

    MCS51-SIM-POLL-DEADLOCK   (warning*) pure-RAM while() invisible to the
                                        single-threaded Wasm sim
    MCS51-SIM-BITBANG-TIMING  (info)     cycle-counted bit-bang not faithful

``*`` B-01 ships as warning first (precision acceptance rule) and will
tighten to error after 1-2 feedback rounds.

Verdict order per loop (all must hold to report; FN preferred over FP):

1. condition identifiers ``V`` non-empty (``while(1)`` skipped);
2. ``V`` has no SFR/sbit member (hardware Shadow Hook covers those);
3. no timeout exit (``&&`` / ``||`` / ``--`` / ``++`` in the condition);
4. no same-file assignment to ``V`` in the loop body;
5. no calls in the loop body (opaque calls conservatively pass, e.g. a
   ``_nop_()`` microstep lets the sim vector the ISR);
6. ``V`` intersects the cross-file ISR-written table.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

from tools.lint.engine.base import LintContext, register_pack
from tools.lint.engine.models import Finding

# Stage6 S6-2 (CPL-16): header-name facts come from the chip manifests; no
# vendor name is hardcoded in this pack.
_TOOLS_DIR = Path(__file__).resolve().parents[1]
if str(_TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(_TOOLS_DIR))
from mcs51_manifest import header_hint_patterns, load_chip_manifests  # noqa: E402

_SOURCE_SUFFIXES = {".c", ".h", ".cc", ".cpp", ".hpp", ".cxx", ".hxx"}
_SKIP_DIRS = {"build", ".git", "node_modules", "__pycache__", "unisim-assets"}

# Fallback when no REG header can be parsed (standard 8051 core sets).
_CORE_SFR = frozenset({
    "P0", "P1", "P2", "P3", "SP", "DPL", "DPH", "PCON", "TCON", "TMOD",
    "TL0", "TL1", "TH0", "TH1", "SCON", "SBUF", "IE", "IP", "PSW", "ACC", "B",
})
_CORE_SBIT = frozenset({
    "IE0", "IT0", "IE1", "IT1", "TR0", "TF0", "TR1", "TF1",
    "RI", "TI", "RB8", "TB8", "REN", "SM0", "SM1", "SM2",
    "EA", "ES", "ET0", "EX0", "ET1", "EX1", "PS", "PT0", "PX0", "PT1", "PX1",
    "RS0", "RS1", "OV", "AC", "CY",
})

_C_KEYWORDS = frozenset({
    "if", "else", "for", "while", "do", "switch", "case", "default", "break",
    "continue", "return", "goto", "sizeof", "typedef", "struct", "union",
    "enum", "static", "extern", "const", "volatile", "unsigned", "signed",
    "void", "char", "short", "int", "long", "float", "double", "inline",
    "sbit", "sfr", "sfr16", "bdata", "idata", "xdata", "pdata", "code",
    "interrupt", "using", "reentrant", "bit", "sfr", "true", "false",
})

_SFR_DECL_RE = re.compile(r"\bsfr\s+(\w+)\s*=")
_SBIT_DECL_RE = re.compile(r"\bsbit\s+(\w+)\s*=")
_INCLUDE_REG_RE = re.compile(r'#\s*include\s*[<"]([^>"]*(?:REG|reg)[^>"]*\.[Hh])\s*[>"]')
_ISR_KEIL_RE = re.compile(
    r"\bvoid\s+(\w+)\s*\(\s*(?:void)?\s*\)\s*interrupt\s+(\d+)(?:\s+using\s+\d+)?"
)
_ISR_WINK_RE = re.compile(r"\bWINK_ISR\s*\(\s*(\d+)\s*\)")
_WRITE_LHS_RE = re.compile(
    r"\b([A-Za-z_]\w*)\s*(?:\[[^\]]*\]\s*)?"
    r"(?:\+\+|\-\-|\+=|\-=|\*=|/=|%=|&=|\|=|\^=|<<=|>>=|(?<![=!<>])=(?!=))"
)
_IDENT_RE = re.compile(r"\b[A-Za-z_]\w*\b")
_CALL_RE = re.compile(r"\b[A-Za-z_]\w*\s*\(")
_TIMEOUT_HINT_RE = re.compile(r"&&|\|\||\+\+|\-\-")

_HW_CACHE: dict[str, tuple[frozenset, frozenset]] = {}


# ---------------------------------------------------------------------------
# C light parsing helpers (masked source keeps offsets/newlines).
# ---------------------------------------------------------------------------

def _mask(source: str) -> str:
    mask = list(source)
    i, n = 0, len(source)
    while i < n:
        c = source[i]
        if c == "/" and i + 1 < n and source[i + 1] == "/":
            while i < n and source[i] != "\n":
                mask[i] = " "
                i += 1
            continue
        if c == "/" and i + 1 < n and source[i + 1] == "*":
            mask[i] = mask[i + 1] = " "
            i += 2
            while i < n and not (source[i] == "*" and i + 1 < n and source[i + 1] == "/"):
                mask[i] = "\n" if source[i] == "\n" else " "
                i += 1
            if i < n:
                mask[i] = mask[i + 1] = " "
                i += 2
            continue
        if c in "\"'":
            q = c
            mask[i] = " "
            i += 1
            while i < n and source[i] != q:
                if source[i] == "\\" and i + 1 < n:
                    mask[i] = " " if source[i + 1] != "\n" else "\n"
                    mask[i + 1] = " " if source[i + 1] != "\n" else "\n"
                    i += 2
                    continue
                mask[i] = "\n" if source[i] == "\n" else " "
                i += 1
            if i < n:
                mask[i] = " "
                i += 1
            continue
        i += 1
    return "".join(mask)


def _match_paren(text: str, open_pos: int) -> int:
    depth, i = 0, open_pos
    while i < len(text):
        if text[i] == "(":
            depth += 1
        elif text[i] == ")":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


def _match_brace(text: str, open_pos: int) -> int:
    depth, i = 0, open_pos
    while i < len(text):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


_HEADER_HINT_RE = re.compile("|".join(header_hint_patterns()), re.IGNORECASE)


def _is_mcs51_text(text: str, rel: str = "") -> bool:
    # Host test harnesses are out of scope (see lint_mcs51_safety).
    norm = rel.replace("\\", "/")
    base = norm.rsplit("/", 1)[-1]
    if base.startswith("test_") or "/unit/" in norm:
        return False
    if "catch_amalgamated.hpp" in text or "TEST_CASE(" in text:
        return False
    return bool(
        _HEADER_HINT_RE.search(text)
        or re.search(r"wink_mcu\.h|absacc\.h", text)
        or re.search(r"\bWINK_ISR\b|\binterrupt\b|\bsbit\b|\bsfr\b", text)
    )


# ---------------------------------------------------------------------------
# Scope + SFR knowledge.
# ---------------------------------------------------------------------------

def _iter_scope_files(ctx: LintContext) -> list[tuple[str, Path]]:
    """Files to analyze: ctx.paths if incremental, else root + micro-app."""
    if ctx.paths:
        out: list[tuple[str, Path]] = []
        for p in ctx.paths:
            abs_p = p if p.is_absolute() else (ctx.root / p)
            abs_p = abs_p.resolve()
            if abs_p.is_file() and abs_p.suffix.lower() in _SOURCE_SUFFIXES:
                try:
                    rel = abs_p.relative_to(ctx.root).as_posix()
                except ValueError:
                    rel = abs_p.as_posix()
                out.append((rel, abs_p))
        return out
    roots = [ctx.root]
    try:
        from tools.paths import embedded_root

        micro_app = embedded_root() / "wink-micro-app"
        if micro_app.is_dir():
            roots.append(micro_app)
    except Exception:
        pass
    seen: set[Path] = set()
    out = []
    for base in roots:
        for abs_p in base.rglob("*"):
            if not abs_p.is_file() or abs_p.suffix.lower() not in _SOURCE_SUFFIXES:
                continue
            try:
                parts = set(abs_p.relative_to(base).parts)
            except ValueError:
                continue
            if parts & _SKIP_DIRS:
                continue
            resolved = abs_p.resolve()
            if resolved in seen:
                continue
            seen.add(resolved)
            anchor = ctx.root if base == ctx.root else base
            try:
                rel = abs_p.relative_to(anchor).as_posix()
            except ValueError:
                rel = abs_p.as_posix()
            out.append((rel, abs_p))
    return out


def _read_text(abs_path: Path) -> str | None:
    try:
        raw = abs_path.read_bytes()
    except OSError:
        return None
    try:
        return raw.decode("utf-8")
    except UnicodeDecodeError:
        return raw.decode("gbk", errors="replace")


def _hw_unions(include_dirs: list[Path]) -> tuple[frozenset, frozenset]:
    """(sfr_union, sbit_union): every REG header + core fallbacks, cached."""
    key = ";".join(str(d) for d in include_dirs) or "<core>"
    if key in _HW_CACHE:
        return _HW_CACHE[key]
    sfrs: set[str] = set(_CORE_SFR)
    sbits: set[str] = set(_CORE_SBIT)
    for include_dir in include_dirs:
        if not include_dir.is_dir():
            continue
        for header in sorted(include_dir.iterdir()):
            if header.is_file() and re.fullmatch(r"[Rr][Ee][Gg].*\.[Hh]", header.name):
                try:
                    text = header.read_text(encoding="utf-8", errors="replace")
                except OSError:
                    continue
                sfrs |= set(_SFR_DECL_RE.findall(text))
                sbits |= set(_SBIT_DECL_RE.findall(text))
    result = (frozenset(sfrs), frozenset(sbits))
    _HW_CACHE[key] = result
    return result


def _resolve_include_dirs() -> list[Path]:
    """Core include + manifest-declared chip include dirs (S6-2)."""
    dirs: list[Path] = []
    try:
        from tools.paths import micro_os_root

        root = micro_os_root()
    except Exception:
        root = None
    if root is None:
        return dirs
    core = root / "frameworks" / "mcs51" / "include"
    if core.is_dir():
        dirs.append(core)
    for manifest in load_chip_manifests().values():
        cand = root / manifest["headers"]["vendor_include_dir"]
        if cand.is_dir():
            dirs.append(cand)
    return dirs


# ---------------------------------------------------------------------------
# Pass 1: ISR-written symbols. Pass 2: loop verdicts.
# ---------------------------------------------------------------------------

def _isr_bodies(masked: str) -> list[str]:
    bodies: list[str] = []
    for m in list(_ISR_KEIL_RE.finditer(masked)) + list(_ISR_WINK_RE.finditer(masked)):
        brace = masked.find("{", m.end() - 1)
        if brace != -1:
            end = _match_brace(masked, brace)
            if end != -1:
                bodies.append(masked[brace + 1:end])
    return bodies


def _condition_idents(cond: str) -> list[str]:
    return [w for w in _IDENT_RE.findall(cond) if w not in _C_KEYWORDS]


def _is_do_while_tail(masked: str, while_pos: int) -> bool:
    """True if this ``while`` closes a ``do { ... } while();``."""
    i = while_pos - 1
    while i >= 0 and masked[i] in " \t\n\r":
        i -= 1
    if i < 0 or masked[i] != "}":
        return False
    depth, j = 0, i
    while j >= 0:
        if masked[j] == "}":
            depth += 1
        elif masked[j] == "{":
            depth -= 1
            if depth == 0:
                return bool(re.search(r"\bdo\s*$", masked[:j]))
        j -= 1
    return False


def _iter_while_loops(masked: str):
    """Yield (cond_text, body_text, keyword_pos) for while loops."""
    for m in re.finditer(r"\bwhile\b", masked):
        # Skip do-while tails: body lives before the keyword, unknown here.
        if _is_do_while_tail(masked, m.start()):
            continue
        paren = masked.find("(", m.end())
        if paren == -1:
            continue
        close = _match_paren(masked, paren)
        if close == -1:
            continue
        cond = masked[paren + 1:close]
        rest = masked[close + 1:].lstrip()
        if rest.startswith(";"):
            yield cond, "", m.start()
            continue
        if rest.startswith("{"):
            rel = len(masked[close + 1:]) - len(rest)
            brace = close + 1 + rel
            end = _match_brace(masked, brace)
            yield cond, (masked[brace + 1:end] if end != -1 else ""), m.start()
        # do-while tail "while(...);" after a block: body unknown -> skip
        # by yielding empty body would FP; instead only handle the two shapes
        # above plus single-statement bodies conservatively skipped.
        continue


class _SimCompat:
    def __init__(self, ctx: LintContext):
        self.ctx = ctx
        self.findings: list[Finding] = []
        sfrs, sbits = _hw_unions(_resolve_include_dirs())
        self.sfr = sfrs | sbits  # any hardware name (poll verdicts)
        self.sbit = sbits  # bit-level names only (bit-bang verdicts)

    def line_of(self, raw: str, pos: int) -> int:
        return raw[:pos].count("\n") + 1

    def emit(self, rel: str, line: int, rule_id: str, severity: str, message: str, help_text: str) -> None:
        self.findings.append(
            Finding(
                rule_id=rule_id, severity=severity, path=rel, line=line,
                column=None, message=message, snippet=None, help=help_text,
                refs=("mcs51-sim",), allowlisted=False, rule_source="sdk",
            )
        )

    def run(self) -> list[Finding]:
        files: list[tuple[str, Path, str, str]] = []
        for rel, abs_p in _iter_scope_files(self.ctx):
            text = _read_text(abs_p)
            if text is None or not _is_mcs51_text(text, rel):
                continue
            files.append((rel, abs_p, text, _mask(text)))
        # Pass 1: ISR-written symbols across the whole scope.
        isr_written: set[str] = set()
        for rel, _abs_p, _text, masked in files:
            for body in _isr_bodies(masked):
                isr_written.update(_WRITE_LHS_RE.findall(body))
        # Pass 2: per-file verdicts.
        for rel, _abs_p, raw, masked in files:
            local_sbits = set(_SBIT_DECL_RE.findall(masked))
            self.check_poll_deadlocks(rel, raw, masked, local_sbits, isr_written)
            self.check_bitbang(rel, raw, masked, local_sbits)
        return self.findings

    def check_poll_deadlocks(
        self, rel: str, raw: str, masked: str,
        local_sbits: set[str], isr_written: set[str],
    ) -> None:
        hw = set(self.sfr) | local_sbits
        for cond, body, pos in _iter_while_loops(masked):
            V = [w for w in _condition_idents(cond) if not re.fullmatch(r"\d+", w)]
            if not V:
                continue  # while(1): intentional super-loop
            if any(w in hw for w in V):
                continue  # SFR/sbit: Shadow Hook drives it
            if _TIMEOUT_HINT_RE.search(cond):
                continue  # timeout/counter exit: not a deadlock
            written_here = set(_WRITE_LHS_RE.findall(body))
            if any(w in written_here for w in V):
                continue
            if _CALL_RE.search(body):
                continue  # opaque call (e.g. _nop_ microstep): may yield/update
            shared = [w for w in V if w in isr_written]
            if not shared:
                continue  # unknown writer: FN preferred over FP
            self.emit(
                rel, self.line_of(raw, pos), "MCS51-SIM-POLL-DEADLOCK", "warning",
                f"Pure-RAM poll 'while(...{shared[0]}...)' is invisible to the "
                f"single-threaded Wasm sim and will deadlock without a yield.",
                "Poll an SFR/sbit instead, or add a 'wink_sim_yield()/_nop_()' "
                "microstep so the ISR can vector.",
            )

    def check_bitbang(
        self, rel: str, raw: str, masked: str, local_sbits: set[str],
    ) -> None:
        # Only bit-level writes count (sbit toggling); whole-byte SFR config
        # writes (SCON = 0x50, TMOD |= ...) are not bit-bang.
        bits = set(self.sbit) | local_sbits
        # Per function body: >=3 hardware-bit writes + a busy delay loop.
        for m in re.finditer(
            r"(?:\b(?:static|inline|void|int|char|uint8_t|uint16_t|uint32_t|bool|unsigned)\b[^;{}()]*?)\s+([a-zA-Z_]\w*)\s*\([^)]*\)\s*(?:reentrant|__reentrant)?\s*\{",
            masked,
        ):
            brace = m.end() - 1
            end = _match_brace(masked, brace)
            if end == -1:
                continue
            body = masked[brace + 1:end]
            writes = [w for w in _WRITE_LHS_RE.findall(body) if w in bits]
            if len(writes) < 3:
                continue
            if not re.search(r"\bfor\s*\([^;]*;[^;]*;[^)]*\)", body) and "delay" not in body.lower():
                continue
            self.emit(
                rel, self.line_of(raw, m.start()), "MCS51-SIM-BITBANG-TIMING", "info",
                f"Function '{m.group(1)}' bit-bangs hardware bits in a delay loop; "
                f"sub-microsecond timing is not faithful in simulation.",
                "Prefer a DAL peripheral interface or hardware timer.",
            )


@register_pack("mcs51_sim_compat", group="mcs51_all", default_enabled=False)
class Mcs51SimCompatPack:
    """GlobalPack for Wasm simulation behavior compat (two passes)."""

    def run_on_root(self, ctx: LintContext) -> list[Finding]:
        return _SimCompat(ctx).run()
