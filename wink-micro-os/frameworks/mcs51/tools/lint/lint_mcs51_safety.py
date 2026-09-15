# SPDX-License-Identifier: GPL-3.0-only
"""MCS-51 / Keil C51 static safety pack (external lint pack, group ``mcs51_all``).

Migrated from ``wink-tools/tools/lint/check_mcs51_safety.py`` into a standard
:class:`FilePack`. Catches "simulation false-pass" bugs before hardware
deployment::

    MCS51-VOLATILE-REQUIRED   (error)    ISR-shared global missing volatile
    MCS51-REENTRANT-OVERLAY   (warning*) shared non-reentrant fn (Keil overlay)
    MCS51-ATOMIC-TEARING      (warning)  multibyte cross-ISR torn read
    MCS51-BARE-ASM            (error)    #pragma asm without #ifdef __C51__
    MCS51-RAW-SFR-PTR         (error)    raw pointer into SFR range
    MCS51-UNSUPPORTED-KEYWORD (error)    bdata / sfr16 dialect keywords
    MCS51-RECURSION           (error)    direct or mutual recursion (Keil overlay)

``*`` REENTRANT ships as warning first (precision acceptance rule) and will
tighten to error after 1-2 feedback rounds.

False-positive fixes vs the original script (precision plan L2):

* ``==``/``!=``/``<=``/``>=`` are no longer mistaken for ISR writes.
* A pure write from main is no longer mistaken for a read.
* ``sfr``/``sbit``/``interrupt`` gating uses word boundaries (``transfer``
  no longer counts as 8051 code).
* ``__reentrant`` (SDCC) is honored next to Keil ``reentrant``.
* Calls guarded by ``EA = 0 ... EA = 1`` (incl. ``IE_EA`` /
  ``__disable_irq``) are exempt from REENTRANT-OVERLAY.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

from tools.lint.engine.base import LintContext, register_pack
from tools.lint.engine.models import Finding

# Stage6 S6-2 (CPL-16): family names + header facts come from the chip
# manifests; this pack hardcodes neither.
_TOOLS_DIR = Path(__file__).resolve().parents[1]
if str(_TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(_TOOLS_DIR))
from mcs51_manifest import header_hint_patterns, load_chip_manifests  # noqa: E402


def _family_hint_names() -> str:
    names: list[str] = []
    for family, manifest in load_chip_manifests().items():
        names.append(family)
        names.extend(manifest["aliases"])
    return "|".join(re.escape(n) for n in names)


# Path hints are intentionally broad (vendor trees carry no mcs51/c51 in
# path); the content gate inside run_on_file is the real filter.
_PATH_HINT_RE = re.compile(
    r"mcs51|c51|8051|keil|" + _family_hint_names(), re.IGNORECASE)


def _applies_to_path(rel: str) -> bool:
    return bool(_PATH_HINT_RE.search(rel.replace("\\", "/")))


# Content gate: word boundaries on alpha indicators so that e.g. "transfer"
# does not match "sfr".
_SOURCE_INDICATOR_RES = (
    re.compile("|".join(header_hint_patterns()), re.IGNORECASE),
    re.compile(r"reg5[12]\.h"),
    re.compile(r"wink_mcu\.h|absacc\.h"),
    re.compile(r"\bWINK_ISR\b|\binterrupt\b|\bsbit\b|\bsfr\b"),
)


def is_mcs51_source(source: str, rel: str = "") -> bool:
    """Check whether a source file represents 8051 / Keil C51 code.

    Content decides (path hints are too weak: host tests live next to c51
    apps). ``rel`` is accepted for API symmetry and ignored.

    Host test harnesses (Catch2 cases driving the sim from the host thread)
    are explicitly out of scope: same-thread calls need no volatile, so
    flagging them would be FP-by-scope.
    """
    norm = rel.replace("\\", "/")
    base = norm.rsplit("/", 1)[-1]
    if base.startswith("test_") or "/unit/" in norm:
        return False
    if "catch_amalgamated.hpp" in source or "TEST_CASE(" in source:
        return False
    return any(rx.search(source) for rx in _SOURCE_INDICATOR_RES)


def strip_c_comments_and_strings(source: str) -> str:
    """Mask comments and string/char literals with spaces (offsets kept)."""
    mask = list(source)
    i = 0
    n = len(source)
    while i < n:
        c = source[i]
        if c == "/" and i + 1 < n and source[i + 1] == "/":
            while i < n and source[i] != "\n":
                if source[i] != "\n":
                    mask[i] = " "
                i += 1
            continue
        if c == "/" and i + 1 < n and source[i + 1] == "*":
            mask[i] = " "
            mask[i + 1] = " "
            i += 2
            while i < n and not (source[i] == "*" and i + 1 < n and source[i + 1] == "/"):
                mask[i] = "\n" if source[i] == "\n" else " "
                i += 1
            if i < n:
                mask[i] = " "
                mask[i + 1] = " "
                i += 2
            continue
        if c == '"':
            mask[i] = " "
            i += 1
            while i < n and source[i] != '"':
                if source[i] == "\\" and i + 1 < n:
                    mask[i] = " "
                    mask[i + 1] = " " if source[i + 1] != "\n" else "\n"
                    i += 2
                    continue
                mask[i] = "\n" if source[i] == "\n" else " "
                i += 1
            if i < n:
                mask[i] = " "
                mask[i + 1] = " "
                i += 1
            continue
        if c == "'":
            mask[i] = " "
            i += 1
            while i < n and source[i] != "'":
                if source[i] == "\\" and i + 1 < n:
                    mask[i] = " "
                    mask[i + 1] = " "
                    i += 2
                    continue
                mask[i] = " "
                i += 1
            if i < n:
                mask[i] = " "
                i += 1
            continue
        i += 1
    return "".join(mask)


def find_matching_brace(content: str, start_pos: int) -> int:
    """Find matching closing brace for an opening brace at start_pos."""
    if start_pos < 0 or start_pos >= len(content):
        return -1
    opening = content[start_pos]
    closing = "}" if opening == "{" else (")" if opening == "(" else "]")
    count = 1
    pos = start_pos + 1
    while pos < len(content) and count > 0:
        if content[pos] == opening:
            count += 1
        elif content[pos] == closing:
            count -= 1
        pos += 1
    return pos - 1 if count == 0 else -1


ISR_KEIL_RE = re.compile(
    r"\bvoid\s+(\w+)\s*\(\s*(?:void)?\s*\)\s*interrupt\s+(\d+)(?:\s+using\s+\d+)?",
    re.MULTILINE,
)
ISR_WINK_RE = re.compile(r"\bWINK_ISR\s*\(\s*(\d+)\s*\)\s*\{?", re.MULTILINE)

# EA guard variants: EA / IE_EA bit stores and IRQ disable macros.
_EA_ZERO_RE = re.compile(r"\b(?:EA|IE_EA)\s*=\s*0\b|__disable_irq\s*\(")
_EA_ONE_RE = re.compile(r"\b(?:EA|IE_EA)\s*=\s*1\b|__enable_irq\s*\(")

_MULTIBYTE_TYPES = (
    "uint16_t", "int16_t", "uint32_t", "int32_t", "unsigned int",
    "unsigned long", "long", "float", "double", "int",
)


def _ea_guarded_spans(body: str) -> list[tuple[int, int]]:
    """Return (start, end) spans of EA=0 ... EA=1 critical sections."""
    zeros = [m.start() for m in _EA_ZERO_RE.finditer(body)]
    ones = [m.start() for m in _EA_ONE_RE.finditer(body)]
    spans: list[tuple[int, int]] = []
    oi = 0
    for z in zeros:
        while oi < len(ones) and ones[oi] <= z:
            oi += 1
        if oi < len(ones):
            spans.append((z, ones[oi]))
            oi += 1
        else:
            spans.append((z, len(body)))
    return spans


def _call_positions(body: str, name: str) -> list[int]:
    return [m.start() for m in re.finditer(rf"\b{re.escape(name)}\s*\(", body)]


def _write_spans(body: str, var_name: str) -> list[tuple[int, int]]:
    """Spans where ``var`` is written (compound ops; ``==`` etc. excluded)."""
    rx = re.compile(
        rf"\b{re.escape(var_name)}\s*(?:\[[^\]]*\]\s*)?"
        r"(?:\+\+|\-\-|\+=|\-=|\*=|/=|%=|&=|\|=|\^=|<<=|>>=|(?<![=!<>])=(?!=))"
    )
    return [(m.start(), m.end()) for m in rx.finditer(body)]


def _is_read_in_body(body: str, var_name: str) -> bool:
    """True if ``var`` is read (mentions outside write spans)."""
    spans = _write_spans(body, var_name)
    for m in re.finditer(rf"\b{re.escape(var_name)}\b", body):
        pos = m.start()
        if not any(s <= pos < e for s, e in spans):
            return True
    return False


class _Analyzer:
    def __init__(self, source: str, rel: str):
        self.raw_source = source
        self.rel = rel
        self.clean_source = strip_c_comments_and_strings(source)
        self.findings: list[Finding] = []

    def line_of(self, pos: int) -> int:
        return self.raw_source[:pos].count("\n") + 1

    def emit(self, line: int, rule_id: str, severity: str, message: str, help_text: str) -> None:
        self.findings.append(
            Finding(
                rule_id=rule_id,
                severity=severity,
                path=self.rel,
                line=line,
                column=None,
                message=message,
                snippet=None,
                help=help_text,
                refs=("mcs51-lint",),
                allowlisted=False,
                rule_source="sdk",
            )
        )

    def analyze(self) -> list[Finding]:
        self.check_bare_inline_assembly()
        self.check_raw_sfr_pointer_access()
        self.check_unsupported_keywords()
        self.check_recursion()
        self.check_isr_concurrency_and_reentrancy()
        return self.findings

    # -- language / hardware rules --------------------------------------
    def check_bare_inline_assembly(self) -> None:
        for m in re.finditer(r"#pragma\s+asm\b", self.clean_source):
            pos = m.start()
            preceding = self.clean_source[max(0, pos - 500):pos]
            if not ("__C51__" in preceding and ("#ifdef" in preceding or "#if" in preceding)):
                self.emit(
                    self.line_of(pos), "MCS51-BARE-ASM", "error",
                    "Keil inline assembly '#pragma asm' is not isolated with '#ifdef __C51__'.",
                    "Enclose with '#ifdef __C51__ ... #pragma asm ... #pragma endasm #else ... #endif'.",
                )

    def check_raw_sfr_pointer_access(self) -> None:
        pattern = re.compile(
            r"\*\s*\(\s*(?:volatile\s+)?(?:unsigned\s+char|uint8_t|char|uint16_t|unsigned\s+int)\s*\*+\s*\)\s*(0x[89A-Fa-f][0-9A-Fa-f]\b)"
        )
        for m in pattern.finditer(self.clean_source):
            self.emit(
                self.line_of(m.start()), "MCS51-RAW-SFR-PTR", "error",
                f"Raw pointer dereference into SFR address {m.group(1)} bypasses the shadow proxy.",
                "Use named SFR identifiers (P1, TCON, SBUF, ...) or the WinkXsfr proxy.",
            )

    def check_unsupported_keywords(self) -> None:
        for kw in ("bdata", "sfr16"):
            for m in re.finditer(rf"\b{kw}\b", self.clean_source):
                self.emit(
                    self.line_of(m.start()), "MCS51-UNSUPPORTED-KEYWORD", "error",
                    f"Proprietary Keil C51 keyword '{kw}' is unsupported by the simulation layer.",
                    f"Replace '{kw}' with standard C types and bitmask operations.",
                )

    # -- concurrency rules ----------------------------------------------
    def extract_functions(self) -> dict:
        functions: dict = {}
        func_regex = re.compile(
            r"(?:\b(?:static|inline|void|int|char|uint8_t|uint16_t|uint32_t|bool|unsigned|float|double|bit)\b[^;{}()]*?)\s+([a-zA-Z_]\w*)\s*\(([^)]*)\)\s*(reentrant|__reentrant)?\s*\{",
            re.MULTILINE,
        )
        for m in func_regex.finditer(self.clean_source):
            name = m.group(1)
            brace_start = m.end() - 1
            brace_end = find_matching_brace(self.clean_source, brace_start)
            if brace_end == -1:
                continue
            functions[name] = {
                "name": name, "is_isr": False, "vector": None,
                "is_reentrant": bool(m.group(3)), "params": m.group(2).strip(),
                "start": brace_start, "end": brace_end,
                "body": self.clean_source[brace_start + 1:brace_end],
                "line": self.line_of(m.start()),
            }
        for m in ISR_KEIL_RE.finditer(self.clean_source):
            name, vector = m.group(1), int(m.group(2))
            brace_open = self.clean_source.find("{", m.end() - 1)
            if brace_open != -1:
                brace_end = find_matching_brace(self.clean_source, brace_open)
                if brace_end != -1:
                    functions[name] = {
                        "name": name, "is_isr": True, "vector": vector,
                        "is_reentrant": False, "params": "",
                        "start": brace_open, "end": brace_end,
                        "body": self.clean_source[brace_open + 1:brace_end],
                        "line": self.line_of(m.start()),
                    }
        for m in ISR_WINK_RE.finditer(self.clean_source):
            vector = int(m.group(1))
            name = f"WINK_ISR_{vector}"
            brace_open = self.clean_source.find("{", m.end() - 1)
            if brace_open != -1:
                brace_end = find_matching_brace(self.clean_source, brace_open)
                if brace_end != -1:
                    functions[name] = {
                        "name": name, "is_isr": True, "vector": vector,
                        "is_reentrant": False, "params": "",
                        "start": brace_open, "end": brace_end,
                        "body": self.clean_source[brace_open + 1:brace_end],
                        "line": self.line_of(m.start()),
                    }
        return functions

    def extract_globals(self, functions: dict) -> dict:
        global_vars: dict = {}
        func_spans = [(f["start"], f["end"]) for f in functions.values()]
        decl_pattern = re.compile(
            r"^(?!\s*#)(?!\s*typedef)(?!\s*struct)(?!\s*enum)\s*([a-zA-Z_][\w\s\*]*?)\s+([a-zA-Z_]\w*)\s*(?:\[[^\]]*\])?\s*(?:=\s*[^;]+)?\s*;",
            re.MULTILINE,
        )
        for m in decl_pattern.finditer(self.clean_source):
            pos = m.start()
            if any(start <= pos <= end for start, end in func_spans):
                continue
            type_str, var_name = m.group(1).strip(), m.group(2).strip()
            if type_str in ("return", "goto", "break", "continue", "typedef", "extern"):
                continue
            if type_str.startswith(("sbit", "sfr", "sfr16", "xsfr")):
                continue
            if "(" in type_str:
                continue
            is_multibyte = any(
                re.search(rf"\b{re.escape(mbt)}\b", type_str) for mbt in _MULTIBYTE_TYPES
            )
            global_vars[var_name] = {
                "name": var_name, "type": type_str,
                "is_volatile": "volatile" in type_str,
                "is_multibyte": is_multibyte,
                "line": self.line_of(pos),
            }
        return global_vars

    def build_call_graph(self, functions: dict) -> dict[str, set[str]]:
        call_graph: dict[str, set[str]] = {name: set() for name in functions}
        func_names = set(functions.keys())
        for name, info in functions.items():
            for target in func_names:
                if target != name and re.search(rf"\b{re.escape(target)}\s*\(", info["body"]):
                    call_graph[name].add(target)
        return call_graph

    def reachable(self, root: str, call_graph: dict[str, set[str]]) -> set[str]:
        visited: set[str] = set()
        queue = [root]
        while queue:
            curr = queue.pop(0)
            if curr in visited or curr not in call_graph:
                continue
            visited.add(curr)
            queue.extend(n for n in call_graph[curr] if n not in visited)
        return visited

    def check_recursion(self) -> None:
        """MCS51-RECURSION (error, GAP-25): Keil C51 compiles non-reentrant
        functions into a static overlay — a recursive call overwrites its own
        locals/parameters, while the host/wasm sim runs it fine on the native
        stack. Both direct self-calls and mutual/indirect cycles are fatal.
        Keil C has no methods, overloads, or namespaces, so a same-name call
        inside the function body is unambiguous (comments/strings are
        already stripped from clean_source)."""
        functions = self.extract_functions()
        if not functions:
            return
        names = set(functions)
        calls: dict[str, set[str]] = {}
        for name, info in functions.items():
            targets = set()
            for target in names:
                if re.search(rf"\b{re.escape(target)}\s*\(", info["body"]):
                    targets.add(target)
            calls[name] = targets
        for name in sorted(names):
            if name in calls[name]:
                self.emit(
                    functions[name]["line"], "MCS51-RECURSION", "error",
                    f"Recursive function '{name}' calls itself: Keil overlay "
                    f"corrupts its own locals (use iteration).",
                    "Rewrite without recursion; add 'reentrant' only if the "
                    "stack budget (SDCC gate) proves it fits — never by default.",
                )
                continue
            path = self._cycle_path(name, calls)
            # One finding per cycle: every member finds the same node set,
            # so only the lexicographically smallest member reports it.
            if path is not None and name == min(path):
                self.emit(
                    functions[name]["line"], "MCS51-RECURSION", "error",
                    f"Mutual recursion cycle {' -> '.join(path)}: Keil overlay "
                    f"corrupts locals (use iteration).",
                    "Break the cycle (inline one leg or hoist shared logic "
                    "into a leaf function).",
                )

    @staticmethod
    def _cycle_path(root: str, calls: dict[str, set[str]]) -> list[str] | None:
        """Shortest call path from root back to root, or None when acyclic."""
        queue: list[list[str]] = [[root]]
        seen: set[str] = set()
        while queue:
            path = queue.pop(0)
            curr = path[-1]
            for nxt in sorted(calls.get(curr, ())):
                if nxt == root and len(path) >= 1:
                    return path + [root]
                if nxt not in seen and nxt not in path:
                    seen.add(nxt)
                    queue.append(path + [nxt])
        return None

    def check_isr_concurrency_and_reentrancy(self) -> None:
        functions = self.extract_functions()
        if not functions:
            return
        global_vars = self.extract_globals(functions)
        call_graph = self.build_call_graph(functions)

        isr_funcs = [f for f in functions.values() if f["is_isr"]]
        main_func = functions.get("main") or functions.get("wink_mcs51_user_main")

        # --- MCS51-REENTRANT-OVERLAY (warning first; tighten to error later)
        if main_func:
            main_reachable = self.reachable(main_func["name"], call_graph)
            main_reachable.discard(main_func["name"])
        else:
            main_reachable = {f["name"] for f in functions.values() if not f["is_isr"]}
        for isr in isr_funcs:
            isr_reachable = self.reachable(isr["name"], call_graph)
            isr_reachable.discard(isr["name"])
            for conflicted in main_reachable.intersection(isr_reachable):
                info = functions[conflicted]
                if info.get("is_reentrant"):
                    continue
                if self._call_ea_guarded(functions, main_func, conflicted):
                    continue
                self.emit(
                    info["line"], "MCS51-REENTRANT-OVERLAY", "warning",
                    f"Function '{conflicted}' is reachable from both main and ISR "
                    f"(vector {isr['vector']}); Keil static overlay may corrupt locals.",
                    f"Split into '{conflicted}_Main'/'{conflicted}_ISR', add 'reentrant', "
                    f"or guard main-side calls with 'EA = 0; ... EA = 1;'.",
                )

        # --- MCS51-VOLATILE-REQUIRED / MCS51-ATOMIC-TEARING
        isr_written: dict[str, list[int]] = {}
        for isr in isr_funcs:
            for var_name in global_vars:
                if _write_spans(isr["body"], var_name):
                    isr_written.setdefault(var_name, []).append(isr["vector"])
        non_isr = [f for f in functions.values() if not f["is_isr"]]
        for var_name, vectors in isr_written.items():
            var_info = global_vars[var_name]
            readers = [f for f in non_isr if _is_read_in_body(f["body"], var_name)]
            if not readers:
                continue
            vec_str = ", ".join(str(v) for v in vectors)
            if not var_info["is_volatile"]:
                self.emit(
                    var_info["line"], "MCS51-VOLATILE-REQUIRED", "error",
                    f"Global '{var_name}' is written in ISR (vector {vec_str}) and read "
                    f"outside ISRs but is NOT volatile.",
                    f"Declare 'volatile {var_info['type']} {var_name};'.",
                )
            if var_info["is_multibyte"]:
                for f in readers:
                    if not self._body_ea_guarded(f["body"]):
                        self.emit(
                            var_info["line"], "MCS51-ATOMIC-TEARING", "warning",
                            f"Multibyte '{var_name}' ({var_info['type']}) is ISR-updated and read "
                            f"in '{f['name']}' without EA=0/1 protection; torn reads possible.",
                            f"Wrap reads with 'EA = 0; val = {var_name}; EA = 1;'.",
                        )
                        break

    @staticmethod
    def _body_ea_guarded(body: str) -> bool:
        return _EA_ZERO_RE.search(body) is not None and _EA_ONE_RE.search(body) is not None

    def _call_ea_guarded(self, functions: dict, main_func: dict | None, target: str) -> bool:
        """True if every main-side call to ``target`` sits in an EA section."""
        callers = (
            [main_func] if main_func
            else [f for f in functions.values() if not f["is_isr"]]
        )
        claimed = False
        for caller in callers:
            positions = _call_positions(caller["body"], target)
            if not positions:
                continue
            claimed = True
            spans = _ea_guarded_spans(caller["body"])
            if not all(any(s <= p <= e for s, e in spans) for p in positions):
                return False
        return claimed


@register_pack("mcs51_safety", group="mcs51_all", default_enabled=False)
class Mcs51SafetyPack:
    """FilePack for Keil C51 language / hardware safety."""

    def applies_to(self, rel: str, layer_id: str | None) -> bool:
        return _applies_to_path(rel)

    def run_on_file(
        self,
        rel: str,
        text: str,
        layer_id: str | None,
        kind: str | None,
        ctx: LintContext,
    ) -> list[Finding]:
        if not is_mcs51_source(text, rel):
            return []
        return _Analyzer(text, rel).analyze()
